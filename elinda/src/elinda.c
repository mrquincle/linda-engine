/**
 * @file elinda.c
 *
 * The Elinda engine is an evolutionary engine that is part of the Linda engine. It provides
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

#include <linda/abbey.h>
#include <linda/tcpip.h>
#include <linda/tcpipbank.h>
#include <linda/log.h>
#include <linda/ptreaty.h>
#include <linda/bits.h>
#include <linda/poseta.h>
#include <linda/infocontainer.h>

#include <tcpipmsg.h>
#include <evolution.h>
#include <genomes.h>
#include <fitness.h>
#include <agent.h>
#include <mutation.h>

static void *default_hostess(void *context);
static void *first_channel(void *context);
static void *add_channel(void *context);

static void *connect_to_3dsim(void *context);
static void *generate(void *context);
static void *inseminate(void *context);
static void *reincarnate(void *context);
static void *handle_fitness(void *context);
static void *simulate_next_group(void *context);
static void *simulate_next_generation(void *context);
static void *generate_all(void *context);
static void *init0(void *context);
static void *run_robot(void *context);
static void *finalize(void *context);
static void *tcpip_started(void *context);
static void *tcpip_started_callback(void *context);

void connectTasksInLinda();

/**
 * Return default values to initialize the Elinda engine.
 */
void initElinda() {
	elconf = malloc(sizeof(struct ElindaConfig));
	elconf->monk_count = 8;
	elconf->task_count = 16;
	elconf->simulation_size = 2;
	elconf->generation_count = 8;
	elconf->generation_id = 0;
	elconf->boot = first_channel;
	elruntime = malloc(sizeof(struct ElindaRuntime));
	elruntime->eosim = malloc(sizeof(struct SyncThreads));
	ptreaty_init(elruntime->eosim);
	initMessages();
	initSockets();
	initPoseta();
	connectTasksInLinda();
}

/**
 * Set up task dependencies. The Colinda processes and the channels towards them are only created when the boot task (by default
 * the first_channel task) is executed. On the same way are robots reincarnated in the 3D simulator, only when the connection with
 * the simulator is setup.
 */
void connectTasksInLinda() {
	tprintf(LOG_VERBOSE, __func__, "Connect tasks in Linda");
	poseta_func1_if_func0(tcpip_started, init0);
	poseta_func1_if_func0(elconf->boot, generate_all);
	poseta_func1_if_func0(connect_to_3dsim, reincarnate);
}

/**
 * Starts Elinda by initializing the thread pool (abbey) and booting the TCP/IP connection.
 */
int startElinda() {
	tprintf(LOG_VERBOSE, __func__, "Start abbey and boot!");
	initialize_abbey(elconf->monk_count, elconf->task_count);
	dispatch_poseta_task(elconf->boot, NULL, "boot");
	return 0;
}

/**
 * Creates the first channel. In the elinda engine there will no additional channels
 * needed if an m-bus component is used. All commands enter over this one channel and
 * are sent over it. This concerns fitness, reincarnation and genome data. The default
 * channel opens on port 3334.
 */
static void *first_channel(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Create first channel");
	struct InfoChannel *ic = malloc(sizeof(struct InfoChannel));
	ic->type = 0;
	ic->host = malloc(sizeof(struct in_addr));
	ic->host->s_addr = INADDR_ANY;
	ic->port = tmconf->mbus_elinda_port;
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
	struct TcpipMessage *msg = pop(tcpSocket->inbox);// (struct TcpipMessage*)context;

	if (msg == NULL) {
		tprintf(LOG_VERBOSE, __func__, "No message found");
		return NULL;
	}

	switch (msg->payload[0]) {
	case LINDA_NEW_PROCESS_ACK: {
		struct InfoDefault *infod = malloc(sizeof(struct InfoDefault));
		infod->id = msg->payload[2];
		infod->value = 0;
		struct Agent *la = getAgent(infod->id);
		if (la == NULL) break;
		la->elinda.process_state = ELINDA_PROCSTATE_RUNNING;
		dispatch_described_task(inseminate, (void*)infod, "inseminate");
		freemsg(msg);
		break;
	}
	case LINDA_GENOME_ACK: {
		struct InfoDefault *infod = malloc(sizeof(struct InfoDefault));
		infod->id = msg->payload[2];
		infod->value = msg->payload[3];
		dispatch_poseta_task(reincarnate, (void*)infod, "reincarnate");
		freemsg(msg);
		break;
	}
	case LINDA_GENOME_PART_ACK: {
		struct InfoDefault *infod = malloc(sizeof(struct InfoDefault));
		infod->id = msg->payload[2];
		infod->value = msg->payload[4] + 1;
		dispatch_described_task(inseminate, (void*)infod, "inseminate");
		freemsg(msg);
		break;
	}
	case LINDA_FITNESS_MSG: {
		struct InfoDefault *infod = malloc(sizeof(struct InfoDefault));
		tprintmsg(msg, LOG_VV);
		infod->id = msg->payload[4];
		infod->value = msg->payload[5]; //and [6]?
		struct Agent *la = getAgent(infod->id);
		if (la == NULL) break;
		//		RAISE(la->simulation_state, ELINDA_SIMSTATE_DONE);
		//		CLEAR(la->simulation_state, ELINDA_SIMSTATE_CURRENT);
		dispatch_described_task(handle_fitness, (void*)infod, "handle fitness");
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
	tprintf(LOG_VERBOSE, __func__, "Retrieve channel");
	struct TcpipSocket *lsock = tcpip_get(ic->type);
	lsock->port_nr = ic->port;
	if (ic->type == 0) lsock->serv_addr.sin_addr = *ic->host;
	else lsock->cli_addr.sin_addr = *ic->host;
	lsock->callbackIn = default_hostess; //coordination should be exogenous
	lsock->callbackConnect = tcpip_started_callback;
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

static void *tcpip_started_callback(void *context) {
	dispatch_poseta_task(tcpip_started, NULL, "Tcp/ip started");
	return NULL;
}

static void *tcpip_started(void *context) {
	tprintf(LOG_INFO, __func__, "Tcp/ip started");
	return NULL;
}

/**
 * Adds a connection to the Symbricator3D simulator.
 */
static void *connect_to_3dsim(void *context) {
	tprintf(LOG_INFO, __func__, "Create a channel to the Symbricator3D simulator");
	struct TcpipMessage *msgA = createConnectSym3DMessage();
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msgA);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

/**
 * Sends a command to the m-bus that will create a new Colinda controller. The process
 * will be started with the proper default channel that attaches it automatically to
 * the m-bus component. The first I-am-alife signal should be sent to the Elinda
 * engine. Only then a genome can be sent in the direction of the new Colinda engine.
 */
static void *generate(void *context) {
	uint8_t robotId = *(uint8_t*)context;
	char text[64]; sprintf(text, "To-be-simulated robot: %i", robotId);
	tprintf(LOG_INFO, __func__, text);
	getAgent(robotId)->elinda.process_state = ELINDA_PROCSTATE_STARTING;
	tprintf(LOG_VERBOSE, __func__, "Initialize a channel to the robot");
	struct TcpipMessage *msgA = createConnectColindaMessage(robotId);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		return NULL;
	}
	push(lsock_dest->outbox, msgA);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	tprintf(LOG_INFO, __func__, "Generate new colinda process");
	struct TcpipMessage *msgB = createRunColindaMessage(robotId);
	push(lsock_dest->outbox, msgB);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	return NULL;
}

/**
 * The identifier of the robot in the Symbricator3D simulator is different from the
 * identifier of the Colinda engine. The simulatedRobotId is the robotId modulus the
 * amount of simulated robots at once.
 */
static void *inseminate(void *context) {
	struct InfoDefault *infod = (struct InfoDefault*)context;
	uint8_t robotId = infod->id;
	uint8_t partId = infod->value;
	if (!(infod->value)) {
		char text[64]; sprintf(text, "Start insemination of %i", robotId);
		tprintf(LOG_INFO, __func__, text);
		getAgent(robotId)->elinda.simulation_state = ELINDA_SIMSTATE_CURRENT;
	} else {
		char text[64]; sprintf(text, "Continue insemination of %i (part %i)", robotId, partId);
		tprintf(LOG_VERBOSE, __func__, text);
	}

	tprintf(LOG_VERBOSE, __func__, "Get agent!");
	struct RawGenome *ldna = getAgent(robotId)->genome;
	if (ldna == NULL) {
		tprintf(LOG_WARNING, __func__, "No genome found!");
		free(infod);
		return NULL;
	}
	
	tprintf(LOG_VERBOSE, __func__, "Get socket");
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	struct TcpipMessage *msg;
	msg = createGenomeMessage(robotId, ldna->content, partId);
	if (msg == NULL) goto inseminate_finish;
	
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
	}
	tprintf(LOG_VVV, __func__, "Push");
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	
inseminate_finish:
	free(infod);
	return NULL;
}

/**
 * This routine gets the robot_id from the context parameter and sends reincarnation
 * commands. This can be (re)position as well as (re)orientation messages to the
 * simulator. This function can be called after the procreate command has been sent,
 * and the procreate_done message comes back. The procreate message contains the
 * genome for the corresponding Colinda controller.
 */
static void *reincarnate(void *context) {
	struct InfoDefault *infod = (struct InfoDefault*)context;
	uint8_t robotId = infod->id;
	struct TcpipMessage *msg = createPositionMessage(robotId, 
			(robotId % elconf->simulation_size) * -10, 0, 1);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	if (lsock_dest == NULL) {
		tprintf(LOG_WARNING, __func__, "Not initialized?");
		free(msg);
		dispatch_described_task(reincarnate, context, "try to reincarnate again");
		return NULL;
	}
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	dispatch_described_task(run_robot, context, "run robot");
	//infod is freed in run_robot
	return NULL;
}

/**
 * Runs the Colinda controller. On this command the Colinda engine sends the first
 * actuator command.
 * @todo This command should actually be sent only when the physical simulator tells
 * the Elinda engine that the reincarnation (repositioning etc.) has occured properly.
 * Just as in the real world this takes time namely...
 */
static void *run_robot(void *context) {
	tprintf(LOG_VERBOSE, __func__, "Run robot");
	struct InfoDefault *infod = (struct InfoDefault*)context;
	uint8_t robotId = infod->id;
	struct TcpipMessage *msg = createRunRobotMessage(robotId);
	struct TcpipSocket *lsock_dest = tcpipbank_get(tmconf->mbus_id);
	push(lsock_dest->outbox, msg);
	dispatch_described_task(tcpip_send_packets, (void*)lsock_dest, "send packets");
	free(infod);
	return NULL;
}

/**
 * This routine has two responsibilities. First it adds the fitness value to an array of
 * values. And if it concerns the last robot in a set of simulated robots, it needs to
 * start the cycle again starting with insemination of a new genome.
 */
static void *handle_fitness(void *context) {
	struct InfoDefault *infod = (struct InfoDefault*)context;
	char text[32]; sprintf(text, "Handle fitness for %i", infod->id);
	tprintf(LOG_WARNING, __func__, text);
	addFitness(infod->id, infod->value);

	//	printAgentStates();
	aa[infod->id].elinda.simulation_state = ELINDA_SIMSTATE_DONE;
	switch(allAgentsSimulated()) {
	case 1: {
		dispatch_described_task(simulate_next_group, NULL, "next group");
		break;
	} case 2: { 
		dispatch_described_task(simulate_next_generation, NULL, "next generation");
		break;
	} default:
		; // wait for subsequent fitness values
	}
	free(infod);
	return NULL;
}

/**
 * Simulates the next group of robots in the same generation. No new genomes are created, but
 * depending on the state of the agents that have to run, a new process might need to be
 * generated or the genome can be sent directly.
 */
static void *simulate_next_group(void *context) {
	tprintf(LOG_INFO, __func__, "Simulate next group");
	uint8_t i;
	for (i = 0; i < elconf->simulation_size; i++) {
		struct Agent *la = getAgentToBeSimulated();
		if (la == NULL) break;
		if (la->elinda.process_state == ELINDA_PROCSTATE_DEFAULT) {
			dispatch_described_task(generate, (void*)&la->id, "generate");
		} else {
			struct InfoDefault *infod = malloc(sizeof(struct InfoDefault));
			infod->id = la->id;
			infod->value = 0;
			dispatch_described_task(inseminate, (void*)infod, "inseminate");
		}
	}
	return NULL;
}

/**
 * Creates a new generation of robots. All genomes are adapted and after that the first group
 * of robots are simulated.
 */
static void *simulate_next_generation(void *context) {
	tprintf(LOG_INFO, __func__, "Simulate next generation");
	elconf->generation_id++;
	if (elconf->generation_count == elconf->generation_id) {
		dispatch_described_task(finalize, NULL, "finalize");
		return NULL;
	}
	stepEvolution();
	clearSimulationState();
	dispatch_described_task(simulate_next_group, NULL, "first group");
	return NULL;
}

/**
 * The generation procedure for all robots is wrapped in one task, so that the
 * dependency with the boot task (which should occur first) is easier to define.
 */
static void *generate_all(void *context) {
	tprintf(LOG_INFO, __func__, "Start");
	uint8_t i; struct Agent *la;
	for (i = 0; i < elconf->simulation_size; i++) {
		la = getAgentToBeSimulated();
		if (la == NULL) break;
		dispatch_described_task(generate, (void*)&la->id, "generate");
	}
	return NULL;
}

/**
 * Connect to 3D simulator and generate all colinda engines.
 */
static void *init0(void *context) {
	tprintf(LOG_INFO, __func__, "Initialize on level 0");
	dispatch_poseta_task(connect_to_3dsim, NULL, "connect to 3d simulator");
	dispatch_poseta_task(generate_all, NULL, "generate all");
	return NULL;
}


static void *finalize(void *context) {
	tprintf(LOG_NOTICE, __func__, "Finalize!");
	//@todo messages to attached processes!
	
	//@todo free everything
	ptreaty_make_m_run(elruntime->eosim);
	return NULL;
}

/**
 * The elinda engine starts evolution itself. It assumes that the m-bus is already
 * running. It generates some initial processes via the m-bus and waits conditionally
 * till an end-of-simulation message arrives over TCP/IP.
 */
int main() {
	openlog ("elinda", LOG_CONS, LOG_LOCAL0);
	initLog(LOG_INFO);
//	initLog(LOG_BLABLA);
	pthread_t this = pthread_self();
	ptreaty_add_thread(&this, "Main");
	tprintf(LOG_NOTICE, __func__, "Start Elinda");
	initElinda();
	startElinda();

	tprintf(LOG_INFO, __func__, "Init and start evolution cycle");
	configMutation();
	initEvolution();
	gsconf->genomeSize = 10000;
	initAgents();
	startEvolution();
	
	tprintf(LOG_INFO, __func__, "Init 0 task dispatched");
	dispatch_poseta_task(init0, NULL, "Init 0");

	ptreaty_init_baton(elruntime->eosim);
	ptreaty_hoist_flag(elruntime->eosim);
	ptreaty_wait(elruntime->eosim);
	tprintf(LOG_NOTICE, __func__, "Simulation end");
	ptreaty_free(elruntime->eosim); 
	closelog();
	return 0;
}

