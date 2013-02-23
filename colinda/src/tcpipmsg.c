
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <genome.h>
#include <tcpipmsg.h>
#include <string.h>

#include <linda/tcpip.h>

void initMessages() {
	tmconf = malloc(sizeof(struct TcpipMessageConfig));
	tmconf->mbus_elinda_port = 3333;
	tmconf->mbus_gui_port = 3000; //start of port range
	tmconf->elinda_id = 255;
	tmconf->mbus_id = 254;
	tmconf->sym3d_id = 253;
	tmconf->gui_id = 200; //start of port ids
}

#ifdef WITH_GUI
/**
 * Creates a GUI component for this robot. This GUI component starts to listen on 
 * port 3333. So, that connection should already have been set-up before.
 */
struct TcpipMessage *createRunGUIMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = MAX_PACKET_SIZE-1;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_NEW_PROCESS_MSG;
	char* name = (char*)&lm->payload[2];
	uint16_t gui_size = 270 + 10, screen_size = 1280;
	uint16_t xpos = screen_size - 2 * gui_size + (robotId % 2) * gui_size;
	uint16_t ypos = 30 + (robotId / 2) * gui_size; 
	sprintf(name, "lindaGUI localhost %i %i %i", tmconf->mbus_gui_port + robotId, xpos, ypos);
	lm->size = strlen(name) + 2;
	lm->payload[1] = lm->size - 2;
	return lm;
}

/**
 * Creates a new channel in the m-bus to the GUI.
 */
struct TcpipMessage *createConnectGUIMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 10;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_NEW_CHANNEL;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = MBUS_SERVER_CHANNEL; 
	lm->payload[3] = (uint8_t) (INADDR_ANY >> 24);
	lm->payload[4] = (uint8_t) (INADDR_ANY >> 16);
	lm->payload[5] = (uint8_t) (INADDR_ANY >> 8);
	lm->payload[6] = (uint8_t) (INADDR_ANY);
	lm->payload[7] = (uint8_t) ((tmconf->mbus_gui_port + robotId) >> 8);
	lm->payload[8] = (uint8_t) (tmconf->mbus_gui_port + robotId);
	lm->payload[9] = tmconf->gui_id + robotId;
	return lm;
}

/**
 * Creates individual visualization messages for the GUI that should be started up
 * and connected beforehand.
 */
struct TcpipMessage *createGUIColorMessage(uint8_t robotId, uint8_t *msg) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 9;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_SET_COLOR_VALUE;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = robotId;
	lm->payload[3] = tmconf->gui_id  + robotId;
	lm->payload[4] = 0;
	lm->payload[5] = msg[1];  //y (inverted by GUI)
	lm->payload[6] = msg[0];  //x
	lm->payload[7] = 0; //no visualization of ASCII token (letter or value)
	if ((msg[2] > 1) && (msg[2] < 9)) 
		lm->payload[7] = msg[2] + 65; //1..9 to ASCII becomes A..K, so no 48 but 65
	lm->payload[8] = msg[3] % 10;  //color
	if (lm->payload[7] == 0) lm->payload[8] = 0;
	return lm;
}
#endif

/**
 * Assumes two values in output array. The robot id is mapped to one of the robots actually running
 * in the simulator. The received sensor data has the same syntax, but the identifiers in 2 and 3
 * reversed and the sensor data instead of the actuator data starting from item 5 on.
 */
struct TcpipMessage *createActuatorMessage(uint8_t robotId, uint8_t actuatorId, 
		int16_t* output) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 8;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_ACTUATOR_MSG;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = robotId; //origin
	lm->payload[3] = tmconf->sym3d_id;
	lm->payload[4] = robotId;
	lm->payload[5] = actuatorId;
	lm->payload[6] = output[0];
	lm->payload[7] = output[1];
	return lm;
}

/**
 * Creates a topology message. This will be sent to the sym3d simulator.
 */
struct TcpipMessage *createTopologyMessage(uint8_t robotId, uint8_t* topology, uint8_t length) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 5+length;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_TOPOLOGY_MSG;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = robotId; //origin
	lm->payload[3] = tmconf->sym3d_id;
	lm->payload[4] = robotId;
	uint8_t i;
	for (i = 0; i < length; i++) {
		lm->payload[i+5] = topology[i];
	}
	return lm;
}

/**
 * Starts a new "colinda" process when sent to an m-bus. The robot id is given
 * as argument to that process. It is assumed that the colinda engine itself initiates
 * a tcp/ip connection. So, after some confirmation has been sent that the process
 * is actually started a new channel has to be created in the m-bus.
 */
struct TcpipMessage *createRunColindaMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = MAX_PACKET_SIZE-1;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_NEW_PROCESS_MSG;
	lm->payload[1] = lm->size - 2;
	char* name = (char*)&lm->payload[2]; //check
	sprintf(name, "./colinda id=%i", robotId);
	return lm;
}

/**
 * Sends an ack.
 */
struct TcpipMessage *createRunColindaAckMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 4;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_NEW_PROCESS_ACK;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = robotId;
	lm->payload[3] = tmconf->elinda_id;
	return lm;
}

/**
 * The genome, after it has been sent to a Colinda engine, will be developed into an actual
 * controller. When this is finished an acknowledgment message is sent back.
 */
struct TcpipMessage *createGenomeAck(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 4;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_GENOME_ACK;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = robotId;
	lm->payload[3] = tmconf->elinda_id;
	return lm;
}

/**
 * Each part of the genome is acknowledged.
 */
struct TcpipMessage *createGenomePartAck(uint8_t robotId, uint8_t partId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 5;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_GENOME_PART_ACK;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = robotId;
	lm->payload[3] = tmconf->elinda_id;
	lm->payload[4] = partId;
	return lm;
}
