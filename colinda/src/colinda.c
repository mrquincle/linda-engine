/**
 * @file colinda.c
 *
 * The Colinda engine is an evolutionary engine that is part of the Linda engine. It provides
 * reincarnation information to the Symbricator3D simulator and genetic information to several
 * colinda engines. It does send this information over one TCP/IP channel which can be
 * demultiplexed by an m-bus component. See the following diagram, with 1.) the Symbricator3D
 * simulator, 2.) the Elinda binary. 3.) the Colinda binaries.
 *                _____
 *               |     |
 *               |  2  |
 *       _____  /|_____|
 *      |     |/    |
 *      |  1  |     |
 *      |_____|\  __|__
 *              \|     |
 *               |  3  | Nx
 *               |_____|
 *
 * The Elinda engine uses the tcpip component which uses a threadpool, called an abbey. This
 * functionality can be reused for other purposes to run tasks in parallel at the Elinda
 * level. The tasks that can be distinguished in the Elinda engine are:
 *   # default_hostess: receiving messages and starting tasks
 *
 * @date_created    Mar 16, 2009
 * @date_modified   May 06, 2009
 * @author          Anne C. van Rossum
 * @project         Replicator
 * @company         Almende B.V.
 * @license         open-source, GNU Lesser General Public License
 */

#include <unistd.h>
#include <wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <string.h>

#include <linda/abbey.h>
#include <linda/infocontainer.h>
#include <linda/tcpip.h>
#include <linda/tcpipbank.h>
#include <linda/log.h>
#include <linda/ptreaty.h>
#include <linda/bits.h>

#include <tcpipmsg.h>
#include <genome.h>
#include <colinda.h>
#include <sensorimotor.h>
#include <grid.h>
#include <neuron.h>

static void *default_hostess(void *context);
static void *first_channel(void *context);
static void *add_channel(void *context);

static void *alive(void *context);

static void *glue_genome(void *context);
static void *start_development(void *context);
static void *handle_sensor_data(void *context);
static void *start_robot(void *context);
static void *genome_part_ack(void *context);
static void *send_topology(void *context);

#ifdef WITH_GUI	
static void *init_connection_to_gui(void *context);
static void *start_gui(void *context);
#endif

/**
 * Return default values to initialize the Colinda engine.
 */
void initColinda() {
	clconf = malloc(sizeof(struct ColindaConfig));
	clconf->monk_count = 16;
	clconf->task_count = 32;
	clconf->boot = first_channel;
	clconf->dna_buffer_ptr = 0;
	clconf->dna_part_ptr = 0;
	clruntime = malloc(sizeof(struct ColindaRuntime));
	dna = NULL;
	initMessages();
	initSockets();
	initGeneExtraction();		
}

/**
 * Starts Colinda by initializing the thread pool (abbey) and booting the TCP/IP connection.
 */
int startColinda() {
	tprintf(LOG_VERBOSE, __func__, "Start abbey and boot m-bus");
	initialize_abbey(clconf->monk_count, clconf->task_count);
	dispatch_described_task(clconf->boot, NULL, "boot");
	return 0;
}

/**
 * Creates the first and only channel. By default it is assumed that the mbus and the elinda
 * engine communicate on the mbus_elinda line (which is by default port 3333), the mbus and
 * the Symbricator3D simulator on the next port (3334) and the colinda engines start at port
 * number (3335), depending on their id (given by the start of the process). The colinda
 * engines are slaves to the mbus master.
 */
static void *first_channel(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Create first channel");
	struct InfoChannel *ic = malloc(sizeof(struct InfoChannel));
	ic->type = 0;
	ic->host = malloc(sizeof(struct in_addr));
	ic->host->s_addr = INADDR_ANY;
	ic->port = tmconf->mbus_elinda_port + 2 + clconf->id;
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
	tprintf(LOG_VV, __func__, "Hostess inspects packet");
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;
	struct TcpipMessage *msg = pop(tcpSocket->inbox);

	if (msg == NULL) {
		tprintf(LOG_VERBOSE, __func__, "No message found");
		return NULL;
	}

	switch (msg->payload[0]) {
	case LINDA_SENSOR_MSG: {
		struct InfoArray *infoa = malloc(sizeof(struct InfoArray));
		uint8_t header = 6;
		infoa->length = msg->size-header;
		infoa->values = calloc(infoa->length, sizeof(uint8_t));
		memcpy(infoa->values, &msg->payload[header], infoa->length);
		infoa->type = msg->payload[5];
		dispatch_described_task(handle_sensor_data, (void*)infoa, "sensor data");
		freemsg(msg);
		break;
	}
	case LINDA_GENOME_MSG: {
		tprintf(LOG_VVV, __func__, "Gets genome msg");
		struct InfoSockAndMsg *sam = malloc(sizeof(struct InfoSockAndMsg));
		sam->msg = msg;
		sam->sock = tcpSocket;
		dispatch_described_task(glue_genome, (void*)sam, "glue genome");
		break;
	}
	case LINDA_TOPOLOGY_REQ: {
		tprintf(LOG_VVV, __func__, "Topology request");
		dispatch_described_task(send_topology, NULL, "glue genome");
		freemsg(msg);
		break;
	}
	case LINDA_RUNROBOT_MSG: {
		tprintf(LOG_VVV, __func__, "Gets run robot msg");
		dispatch_described_task(start_robot, NULL, "run robot");
		freemsg(msg);
		break;
	}
	case LINDA_END_ELINDA_MSG: {
		if (ptreaty_flag_hoisted(clruntime->sync))
			ptreaty_make_m_run(clruntime->sync);
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
	lsock->callbackIn = default_hostess; //coordination should be exogeneous
#ifdef WITH_GUI	
	lsock->callbackConnect = init_connection_to_gui;
#else
	lsock->callbackConnect = alive;
#endif
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
	return NULL;
}

#ifdef WITH_GUI
/**
 * Adds a connection to the GUI.
 */
static void *init_connection_to_gui(void *context) {
	tprintf(LOG_INFO, __func__, "Create a channel to the GUI");
	struct TcpipMessage *msgA = createConnectGUIMessage(clconf->id);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msgA);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	dispatch_described_task(start_gui, NULL, "start GUI");
	return NULL;
}

/**
 * This task waits a while before it actually starts to run the process that should
 * make use of the channel setup before in init_connection_to_gui.
 */
static void *start_gui(void *context) {
	sleep(1);
	tprintf(LOG_INFO, __func__, "Start GUI");
	struct TcpipMessage *msgA = createRunGUIMessage(clconf->id);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msgA);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	dispatch_described_task(alive, NULL, "Send alive signal");
	return NULL;
}
#endif

/**
 * Sends an acknowledgement to the process that started the Colinda engine, so that it
 * can now continue in sending messages over TCP/IP. It is like an I-am-alife signal
 * and should be sent to the Elinda engine.
 */
static void *alive(void *context) {
#ifdef WITH_GUI
	sleep(18);
#endif
	tprintf(LOG_INFO, __func__, "Alive signal!");
	struct TcpipMessage *msg = createRunColindaAckMessage(clconf->id);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

/**
 * Sends first actuator command
 */
static void *start_robot(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Start running the robot");
	int16_t output[2] = {0,0};
	struct TcpipMessage *msg = createActuatorMessage(clconf->id, 0, (int16_t*)&output);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

/**
 * 
 */
static void *send_topology(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Send topology");
	uint8_t topology_size = s->rows * s->columns;
	uint8_t topology[topology_size];

	struct Neuron *ln;
	uint8_t x,y;
	for (y=0; y < s->rows; y++) {
		for (x=0;x < s->columns; x++) {
			ln = getGridCell(x,y)->neuron;
			if (ln != NULL) {
				topology[x+y*s->columns] = ln->type;
			} else {
				topology[x+y*s->columns] = 0;
			}		
		}
	}

	struct TcpipMessage *msg = createTopologyMessage(clconf->id, (uint8_t*)&topology, topology_size);
	char text[msg->size*4+64];
	sprintf(text, "Topology message ");
	sprintmsg(msg, text);
	sprintf(text, "%s (size %i)", text, msg->size);
	tprintf(LOG_VV, __func__, text);

	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	tprintf(LOG_VV, __func__, "Topology msg created");
	return NULL;
}

/**
 * There will be genome messages entering until the last one has been received. Then
 * the developmental engine can operate on the genome and translate it to a controller.
 * For now that will be a spatial neural network. For later, that might become a
 * full-fledged multi-agent system. The socket is given as one of the parameter, because
 * the inbox needs to be reshuffled if the messages arrive in the wrong order.
 */
static void *glue_genome(void *context) {
	tprintf(LOG_VV, __func__, "Glue genome");
	struct InfoSockAndMsg *sam = (struct InfoSockAndMsg*)context;
	if (dna == NULL) {
		receiveNewGenome();
	}

	if (sam->msg->payload[4] == 0) {
		freeGenes();
		clconf->dna_buffer_ptr = 0;
		clconf->dna_part_ptr = 0;
	}

	uint8_t partId = sam->msg->payload[4]; 
	//put pieces in order again
	if (partId != clconf->dna_part_ptr) {
		char text2[128]; 
		sprintf(text2, "Wrong genome part (%i instead of %i) received!",
				partId, clconf->dna_part_ptr);
		tprintf(LOG_ERR, __func__, text2);
		return NULL;
	}

	uint8_t header = 6; int value = sam->msg->size - header;
	if (value > MAX_PACKET_SIZE-header) value = MAX_PACKET_SIZE-header;
	char text[64]; sprintf(text, "Part %i of %i. Size = %i",
			partId, sam->msg->payload[5], value);
	tprintf(LOG_VVV, __func__, text);

	dna->content = (Codon*)&sam->msg->payload[header];
	clconf->dna_buffer_ptr = stepGeneExtraction(value);
	clconf->dna_part_ptr++;

	dispatch_described_task(genome_part_ack, (void*)&partId, "genome ack");

	if (partId == sam->msg->payload[5]-1) {
		char text3[128]; 
		sprintf(text3, "Last part (%i of %i) received!", partId, sam->msg->payload[5]);
		tprintf(LOG_VERBOSE, __func__, text3);
		dispatch_described_task(start_development, NULL, "start development");
	}

	freemsg(sam->msg);
	return NULL;
}

/**
 * The genome is a quite important part of the information going to a robot, it's like
 * flashing its memory. Hence, every part of the genome is acknowledged. This is also
 * necessary because the underlying protocol does not preserve the order of the parts.
 */
static void *genome_part_ack(void *context) {
	char *partId = (char*)context;
	struct TcpipMessage *msg = createGenomePartAck(clconf->id, *partId);
	//	free(partId);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

/**
 * The genome has been received by the controller and can be developed now. Everything
 * concerned should be initialized and there we go! The end product is a neural network
 * that subsequently can be used for sensor-actuator mappings. After the network has been
 * developed (or matured), a message will be sent back that the Colinda is ready for the
 * operational phase and can start to receive messages from the Symbricator3D simulator.
 */
static void *start_development(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Develop controller");
	developNeuralNetwork();
	tprintf(LOG_VERBOSE, __func__, "Developmental ack");
	struct TcpipMessage *msg = createGenomeAck(clconf->id);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

/**
 * This routine handles a sensor message and sends an actuator message back.
 */
static void *handle_sensor_data(void *context) {
	struct InfoArray *infoa = (struct InfoArray*)context;
	struct AERBuffer *in = malloc(sizeof(struct AERBuffer));
	struct AERBuffer *out = malloc(sizeof(struct AERBuffer));
	initAER(in); initAER(out);
	tprintf(LOG_VV, __func__, "Generate incoming spikes");
	generateSpikes(infoa->values, infoa->length, in);
	do {
		//print network
		tprintf(LOG_VV, __func__, "Run network (again)");
		;
	} while (runNeuralNetwork(in, out));
	int16_t *output = calloc(2, sizeof(int16_t));
	tprintf(LOG_VV, __func__, "Interpret outgoing spikes");
	interpretSpikes(out, output);

	tprintf(LOG_VV, __func__, "Send the actuator commands");
	struct TcpipMessage *msg = createActuatorMessage(clconf->id, 0, output);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

#ifdef WITH_GUI
/**
 * Visualizes the occupation of a grid cell.
 */
void *visualize(void *context) {
	struct InfoArray *infoa = (struct InfoArray*)context;
	if (infoa->length != 4) {
		tprintf(LOG_WARNING, __func__, "Inproper visualization command");
	}
	struct TcpipMessage *msg = createGUIColorMessage(clconf->id, infoa->values);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	free(infoa);
	return NULL;
}
#endif

/**
 * The elinda engine starts evolution itself. It assumes that the m-bus is already
 * running. It generates some initial processes via the m-bus and waits conditionally
 * till an end-of-simulation message arrives over TCP/IP.
 */
int main(int argc, const char* argv[] ) {
	initColinda();
	if (argc == 2) {
		clconf->id = atoi( argv[1] );
	} else {
		tprintf(LOG_EMERG, __func__, "Should have a controller id argument");
	}
	char text[32]; 
	sprintf(text, "(id=%i) colinda", clconf->id);
	openlog (text, LOG_CONS, LOG_LOCAL0);
	//	initLog(LOG_DEBUG);
	initLog(LOG_VERBOSE);
	logconf->name = calloc(32, sizeof(char));
	sprintf(logconf->name, "robot:%i", clconf->id);
	logconf->printName = 1;
	pthread_t this = pthread_self();
	ptreaty_add_thread(&this, "Main");
	tprintf(LOG_NOTICE, __func__, "Start Colinda");

	tprintf(LOG_VERBOSE, __func__, "Start connection with mbus");
	startColinda();

	clruntime->sync = malloc(sizeof(struct SyncThreads));
	ptreaty_init(clruntime->sync);
	ptreaty_init_baton(clruntime->sync);

	tprintf(LOG_VERBOSE, __func__, "Setting up EOS trap");
	ptreaty_hoist_flag(clruntime->sync);
	tprintf(LOG_VERBOSE, __func__, "Wait for EOS");
	ptreaty_wait(clruntime->sync);

	closelog();

	return 0;
}

