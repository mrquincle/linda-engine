#include <tcpipmsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <flinda.h>

#include <linda/tcpip.h>
#include <linda/log.h>

void initMessages() {
	tmconf = malloc(sizeof(struct TcpipMessageConfig));
	tmconf->mbus_elinda_port = 3333;
	tmconf->mbus_sym3d_port = 4444;
	tmconf->elinda_id = 255;
	tmconf->mbus_id = 254;
	tmconf->sym3d_id = 253;
}

struct TcpipMessage *createTopologyRequestMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 4;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_TOPOLOGY_REQ;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = tmconf->sym3d_id;
	lm->payload[3] = robotId;
	return lm;	
}

struct TcpipMessage *createFitnessMessage(uint8_t robotId, uint8_t fitvalue) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 6;
	lm->payload = calloc(lm->size, sizeof(uint8_t));
	lm->payload[0] = LINDA_FITNESS_MSG;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = tmconf->sym3d_id;
	lm->payload[3] = tmconf->elinda_id;
	lm->payload[4] = robotId;
	lm->payload[5] = fitvalue;
	return lm;	
}

/**
 * The TCP/IP messages start with a command, then an indication of the size. The rest is left
 * over to protocols build upon this. The position message contains after this a robot id
 * and 3 position parameters for the x,y and z dimensions. The robot id is mapped to one of the
 * robots actually running in the simulator (by a modulus operation).
 */
struct TcpipMessage *createPositionMessage(uint8_t robotId, int16_t x, int16_t y, int16_t z) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 11;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_POSITION_MSG;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = tmconf->elinda_id;
	lm->payload[3] = tmconf->sym3d_id;
	lm->payload[4] = robotId;// % elconf->simulation_size;
	lm->payload[5] = (char)x >> 8;
	lm->payload[6] = (char) x;
	lm->payload[7] = (char)y >> 8;
	lm->payload[8] = (char) y;
	lm->payload[9] = (char)z >> 8;
	lm->payload[10] = (char) z;

	if (lm->payload[5] == 0xFF) lm->payload[5] = 0;
	if (lm->payload[7] == 0xFF) lm->payload[7] = 0;
	if (lm->payload[9] == 0xFF) lm->payload[9] = 0;
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
	char* name = (char*)&lm->payload[2]; //check
//	sprintf(name, "../../colinda/Debug/colinda %i", robotId);
	sprintf(name, "colinda %i", robotId);
//	sprintf(name, "./example");
	lm->size = strlen(name) + 2;
	lm->payload[1] = lm->size - 2;
	return lm;
}

/**
 * Creates a new channel in the m-bus to a Colinda instance.
 */
struct TcpipMessage *createConnectColindaMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 10;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_NEW_CHANNEL;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = 1; //server
	lm->payload[3] = (uint8_t) (INADDR_ANY >> 24);
	lm->payload[4] = (uint8_t) (INADDR_ANY >> 16);
	lm->payload[5] = (uint8_t) (INADDR_ANY >> 8);
	lm->payload[6] = (uint8_t) (INADDR_ANY);
	lm->payload[7] = (uint8_t) ((tmconf->mbus_elinda_port + 2 + robotId) >> 8);
	lm->payload[8] = (uint8_t) (tmconf->mbus_elinda_port + 2 + robotId);
	lm->payload[9] = robotId;
	return lm;
}

/**
 * creates a new channel in the m-bus to the Symbricator3D simulator.
 */
struct TcpipMessage *createConnectSym3DMessage() {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 10;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_NEW_CHANNEL;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = 0; //client
	lm->payload[3] = (uint8_t) (INADDR_ANY >> 24);
	lm->payload[4] = (uint8_t) (INADDR_ANY >> 16);
	lm->payload[5] = (uint8_t) (INADDR_ANY >> 8);
	lm->payload[6] = (uint8_t) (INADDR_ANY);
	lm->payload[7] = (uint8_t) (tmconf->mbus_sym3d_port >> 8);
	lm->payload[8] = (uint8_t) (tmconf->mbus_sym3d_port);
	lm->payload[9] = tmconf->sym3d_id;
	return lm;
}

/**
 * Message that will be sent to the Colinda controller from the Elinda engine.
 */
struct TcpipMessage *createRunRobotMessage(uint8_t robotId) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = 4;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_RUNROBOT_MSG;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = tmconf->elinda_id;
	lm->payload[3] = robotId;
	return lm;
}
