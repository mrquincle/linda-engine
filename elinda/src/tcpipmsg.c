#include <tcpipmsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <genomes.h>
#include <elinda.h>

#include <linda/log.h>
#include <linda/tcpip.h>

void initMessages() {
	tmconf = malloc(sizeof(struct TcpipMessageConfig));
	tmconf->mbus_elinda_port = 3333;
	tmconf->mbus_sym3d_port = 4444;
//	tmconf->mbus_gui_port = 3000;
	tmconf->elinda_id = 255;
	tmconf->mbus_id = 254;
	tmconf->sym3d_id = 253;
//	tmconf->gui_id = 200;
}

/**
 * The TCP/IP messages start with a command, then an indication of the size. The rest is left
 * over to protocols build upon this. The position message contains after this a robot id
 * and 3 position parameters for the x,y and z dimensions. The robot id is mapped to one of the
 * robots actually running in the simulator (by a modulus operation).
 */
struct TcpipMessage *createPositionMessage(uint8_t robotId, int16_t x, int16_t y, int16_t z) {
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->payload = calloc(MAX_PACKET_SIZE-1, sizeof(unsigned char));
	lm->size = 11;
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
	char* name = (char*)&lm->payload[2];
	sprintf(name, "colinda %i", robotId);
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
 * Creates a new channel in the m-bus to the Symbricator3D simulator.
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
 * The genome message might be cut in several pieces before sending it as
 * separate tcp/ip messages. Especially if later on, not TCP, but UDP is used.
 * Hence partId ranges from 0 till what is needed to sent the message across. If
 * the genome is sent, its value is set to -1.
 */
struct TcpipMessage *createGenomeMessage(uint8_t robotId, uint8_t *pdna, uint8_t partId) {
	tprintf(LOG_VV, __func__, "Next genome part");
	uint8_t header = 6;
	struct TcpipMessage *lm = malloc(sizeof(struct TcpipMessage));
	lm->size = MAX_PACKET_SIZE;
	lm->payload = calloc(lm->size, sizeof(unsigned char));
	lm->payload[0] = LINDA_GENOME_MSG;
	lm->payload[1] = lm->size - 2;
	lm->payload[2] = tmconf->elinda_id;
	lm->payload[3] = robotId;
	lm->payload[4] = partId;
	lm->payload[5] = gsconf->genomeSize / (uint16_t)(lm->size - header);
	if ((lm->payload[5] * (lm->size - header)) < gsconf->genomeSize) lm->payload[5]++;
	uint16_t i, j = (lm->size - header) * partId;
	if (j >= gsconf->genomeSize) {
		free(lm);
		return NULL;
	}
	for (i = 0; i < lm->size - header; i++) {
		if ((i+j) >= gsconf->genomeSize) {
			lm->size = i + header;
			lm->payload[1] = lm->size - 2;
			char text[128]; 
			sprintf(text, "Created %i parts of size %i (= %i) for total genome of size %i",
					lm->payload[5], MAX_PACKET_SIZE - header, 
					lm->payload[5] * (MAX_PACKET_SIZE - header), 
					gsconf->genomeSize);
			tprintf(LOG_VERBOSE, __func__, text);
			sprintf(text, "This last part %i (without header) is of size %i", lm->payload[4], lm->size-header);
			tprintf(LOG_VERBOSE, __func__, text);
			return lm;
		}
		lm->payload[header+i] = pdna[i+j];
		//@todo Check if everything has been sent
	}
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
