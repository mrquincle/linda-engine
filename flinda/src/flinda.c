/**
 * The flinda engine returns a fitness value for the topology of the network. It
 * is the idea that the developmental engine returns a diverse range of topologies,
 * so generated topologies are judged on their similarity with existing ones to 
 * create a spectrum of topologies that is diverse enough.
 * 
 * This engine can be used instead of the delta3d simulator. It is not meant to run
 * in parallel with the physical engine. The actual run might very well dwell upon
 * a certain subspace of the entire topological space available.
 * 
 * The colinda engine gets a RUN_ROBOT message from the elinda engine. Then it responds
 * with an ACTUATOR message to the physical simulator. This is handled by the flinda 
 * engine, and it responds rather than with a SENSOR message, with a TOPOLOGY_REQ message. 
 * The colinda engine responds with a TOPOLOGY message, and then the flinda engine will
 * send directly a fitness value to the elinda engine. 
 * 
 * As soon as a acknowledgement after reincarnation is implemented in the physical 
 * simulator, the elinda engine needs to be fooled during start-up in sending this same
 * ack.
 * 
 * The topology is interpreted is communicated as an array of characters. Only the absence
 * or existence of neurons on the grid cells is taken as input to judge diversity.
 */
#include <flinda.h>
#include <tcpipmsg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <linda/ptreaty.h>
#include <linda/tcpip.h>
#include <linda/tcpipbank.h>
#include <linda/log.h>
#include <linda/ptreaty.h>
#include <linda/bits.h>
#include <linda/abbey.h>
#include <linda/infocontainer.h>

static void *default_hostess(void *context);
static void *first_channel(void *context);
static void *add_channel(void *context);

static void *handle_topology(void *context);
static void *send_topology_request(void *context);
static void *finalize(void *context);

/**
 * Return default values to initialize the Elinda engine.
 */
void initFlinda() {
	flconf = malloc(sizeof(struct FlindaConfig));
	flconf->monk_count = 16;
	flconf->task_count = 32;
	flconf->boot = first_channel;
	flconf->topology_count = 10;
	flruntime = malloc(sizeof(struct FlindaRuntime));
	flruntime->eosim = malloc(sizeof(struct SyncThreads));
	flhistory = malloc(sizeof(struct FlindaHistory));
	flhistory->topologies = malloc(flconf->topology_count * sizeof(struct InfoArray*));
	uint8_t i;
	for (i=0; i < flconf->topology_count; i++) {
		flhistory->topologies[i] = NULL;
	}
	flhistory->topology_count = 0;
	ptreaty_init(flruntime->eosim);
	ptreaty_init_baton(flruntime->eosim);
	initMessages();
	initSockets();
}

/**
 * Starts Flinda by initializing the thread pool (abbey) and booting the TCP/IP connection.
 */
int startFlinda() {
	tprintf(LOG_VERBOSE, __func__, "Start abbey and boot m-bus");
	initialize_abbey(flconf->monk_count, flconf->task_count);
	tprintf(LOG_VERBOSE, __func__, "Boot!");
	dispatch_described_task(flconf->boot, NULL, "boot");
	return 0;
}

/**
 * Creates the first channel. It opens this channel to the mbus.
 */
static void *first_channel(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Create first channel");
	struct InfoChannel *ic = malloc(sizeof(struct InfoChannel));
	ic->type = 1;
	ic->host = malloc(sizeof(struct in_addr));
	ic->host->s_addr = INADDR_ANY;
	ic->port = tmconf->mbus_sym3d_port;
	ic->id = tmconf->mbus_id;
	tprintf(LOG_VERBOSE, __func__, "Dispatch add default channel task");
	dispatch_described_task(add_channel, (void*)ic, "add default channel");
	return NULL;
}

/**
 * Understands very general TCP/IP messages on a control-level. It adds TCP/IP channels
 * to the group of ingoing or the group of outgoing channels. By every function the same
 * switch-statement may be used, this means "control" and "data" channels are not strictly
 * separated. So, all incoming channels are handled in a uniform manner.
 */
static void *default_hostess(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Hostess inspects packet");
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;
	struct TcpipMessage *msg = pop(tcpSocket->inbox);

	if (msg == NULL) {
		tprintf(LOG_VERBOSE, __func__, "No message found");
		return NULL;
	}

	switch (msg->payload[0]) {
	case LINDA_TOPOLOGY_MSG: {
		struct InfoArray *infoa = malloc(sizeof(struct InfoArray));
		uint8_t header = 6;
		infoa->length = msg->size-header;
		infoa->values = calloc(infoa->length, sizeof(uint8_t));
		memcpy(infoa->values, &msg->payload[header], infoa->length);
		infoa->type = msg->payload[4]; //robotId
		dispatch_described_task(handle_topology, (void*)infoa, "handle topology");
//		free(infoa);
		freemsg(msg);
		break;
	}
	case LINDA_ACTUATOR_MSG: {
		tprintf(LOG_VERBOSE, __func__, "Actuator message received");
		struct InfoDefault *infod = malloc(sizeof(struct InfoDefault));
		infod->id = msg->payload[2];
		dispatch_described_task(send_topology_request, (void*)infod, "topology request");
		freemsg(msg);
		break;
	}
	case LINDA_POSITION_MSG: {
		tprintf(LOG_VERBOSE, __func__, "Not handled in this setting.");
		freemsg(msg);
		break;
	}
	case LINDA_END_ELINDA_MSG: {
		dispatch_described_task(finalize, NULL, "finalize");
		freemsg(msg);
		break;
	}
	default: {
		tprintf(LOG_WARNING, __func__, "Unrecognized message!");
		freemsg(msg);
	}
	}
	return NULL;
}

/**
 * The information sent to create a new channel is just enough to create a TcpipSocket.
 * Check beforehand if the channel not already exists by tcpipbank_get(ic->id) and do
 * not forget to call tcpipbank_add afterwards or the new channel won't be registered.
 * If everything is handled, a new tcpip_start task can be dispatched and messages
 * will be processed over this new channel.
 */
struct TcpipSocket* ic2sock(struct InfoChannel *ic) {
	struct TcpipSocket *lsock = tcpip_get(ic->type);
	lsock->port_nr = ic->port;
	if (ic->type == 0) lsock->serv_addr.sin_addr = *ic->host;
	else lsock->cli_addr.sin_addr = *ic->host;
	lsock->callbackIn = default_hostess; //coordination should be exogenous
	return lsock;
}

/**
 * Opens a new TCP/IP connection. The context is given by the information send over the
 * control channel. This routine adds a new socket to the double linked list of sockets,
 * if it does not already exist.
 */
static void *add_channel(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Add channel");
	struct InfoChannel *ic = (struct InfoChannel*)context;
	if (tcpipbank_get(ic->id) != NULL) {
		char text[128]; sprintf(text, "Channel with id %i already exists.", ic->id);
		tprintf(LOG_WARNING, __func__, text);
		return NULL;
	}
	struct TcpipSocket *lsock = ic2sock(ic);
	tcpipbank_add(lsock, ic->id);
	dispatch_described_task(tcpip_start, (void*)lsock, "start tcp/ip");
	free(ic);
	return NULL;
}

/**
 * 
 */
static void *send_topology_request(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Topology request will be sent");
	struct InfoDefault *infod = (struct InfoDefault*)context;
	uint8_t robotId = infod->id; 
	struct TcpipMessage *msg = createTopologyRequestMessage(robotId);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		freemsg(msg);
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	free(infod);
	return NULL;
}

uint8_t compare(struct InfoArray *a1, struct InfoArray *a2) {
	uint8_t i, result = 0;
	for (i = 0; i < a1->length; i++) {
		if (a1->values[i] != a2->values[i]) result++; 
	}
	return result;
}

uint8_t lower_fitness(uint8_t given_fitness) {
	uint8_t i;
	for (i = 0; i <	flhistory->topology_count; i++) {
		if (flhistory->topologies[i]->type < given_fitness)
			return 1;
	}
	return 0;
}

uint8_t lowest_fitness() {
	if (flhistory->topology_count == 0) {
		tprintf(LOG_ERR, __func__, "Don't call this routine if there are no topologies");
		return 0;
	}
	uint8_t i, fitness = flhistory->topologies[0]->type, fitnessId = 0;
	for (i = 1; i <	flhistory->topology_count; i++) {
		if (flhistory->topologies[i]->type < fitness) {
			fitness = flhistory->topologies[i]->type;
			fitnessId = i;
		}
	}
	return fitnessId; 
}

/**
 * 
 */
static void *handle_topology(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Handle topology");
	struct InfoArray *infoa = (struct InfoArray*)context;
	uint8_t robotId = infoa->type;

	uint8_t fitness = 0, i, fitness_delta, equal = 0;
	tprintf(LOG_VERBOSE, __func__, "Iterate existing topologies");
	for (i = 0; i <	flhistory->topology_count; i++) {
		fitness_delta = compare(flhistory->topologies[i], infoa);
		if (!fitness_delta) equal = 1;
		if (fitness_delta > 25) fitness_delta = 25;
		fitness += fitness_delta;
	}

	if (!flhistory->topology_count) {
		tprintf(LOG_VERBOSE, __func__, "Add first topology");
		flhistory->topologies[0] = infoa;
		flhistory->topologies[0]->type = fitness;
		flhistory->topology_count++;
	} else if (!equal) {
		tprintf(LOG_VERBOSE, __func__, "Same topology, be idle");
		free(infoa->values);
		free(infoa);
	} else if (flhistory->topology_count < flconf->topology_count) {
		tprintf(LOG_VERBOSE, __func__, "Add next topology");
		flhistory->topologies[flhistory->topology_count] = infoa;
		flhistory->topologies[flhistory->topology_count]->type = fitness;
		char text[128];
		sprintf(text, "Added topology number %i", flhistory->topology_count); 
		tprintf(LOG_VERBOSE, __func__, text);
		flhistory->topology_count++;
	} else if (!lower_fitness(fitness)) {
		tprintf(LOG_VERBOSE, __func__, "Add topology with lowest fitness");
		uint8_t replaceId = lowest_fitness();
		free(flhistory->topologies[replaceId]);
		flhistory->topologies[replaceId] = infoa;
		flhistory->topologies[replaceId]->type = fitness;
	} else {
		tprintf(LOG_VERBOSE, __func__, "No higher fitness");
		free(infoa->values);
		free(infoa);
	}

	struct TcpipMessage *msg = createFitnessMessage(robotId, fitness);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		freemsg(msg);
		return NULL;
	}

	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

static void *finalize(void *context) {
	tprintf(LOG_INFO, __func__, "Finalize!");
	ptreaty_make_m_run(flruntime->eosim);
	return NULL;
}

/**
 * The elinda engine starts evolution itself. It assumes that the m-bus is already
 * running. It generates some initial processes via the m-bus and waits conditionally
 * till an end-of-simulation message arrives over TCP/IP.
 */
int main() {
	openlog ("flinda", LOG_CONS, LOG_LOCAL0);
	initLog(LOG_NOTICE);
	pthread_t this = pthread_self();
	ptreaty_add_thread(&this, "Main");
	tprintf(LOG_NOTICE, __func__, "Start Flinda");
	initFlinda();
	startFlinda();

	tprintf(LOG_INFO, __func__, "Wait for Elinda engine");

	ptreaty_hoist_flag(flruntime->eosim);
	ptreaty_wait(flruntime->eosim);
	tprintf(LOG_INFO, __func__, "Simulation end");
	ptreaty_free(flruntime->eosim); 
	closelog();
	return 0;
}

