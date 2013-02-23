/**
 * @file tcpip_helper.h
 * @brief Create domain-specific messages.
 * @author Anne C. van Rossum
 */

#ifndef TCPIP_HELPER_H_
#define TCPIP_HELPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linda/tcpip.h>

#define MBUS_ADD_CHANNEL  30
#define MBUS_NEW_PROCESS  50

#define LINDA_POSITION_MSG		10
#define LINDA_ORIENTATION_MSG	11
#define LINDA_FITNESS_MSG		12
#define LINDA_SENSOR_MSG		13
#define LINDA_ACTUATOR_MSG		14
#define LINDA_GENOME_MSG		15
#define LINDA_GENOME_ACK		16
#define LINDA_RUNROBOT_MSG		17
#define LINDA_GENOME_PART_ACK	18
	
#define LINDA_NEW_CHANNEL		MBUS_ADD_CHANNEL

#define LINDA_NEW_PROCESS_MSG   MBUS_NEW_PROCESS
#define LINDA_NEW_PROCESS_ACK   51

#define LINDA_END_ELINDA_MSG	70

struct TcpipMessageConfig {
	int mbus_elinda_port;
	int mbus_sym3d_port;
//	int mbus_gui_port;
	uint8_t mbus_id;
	uint8_t elinda_id;
	uint8_t sym3d_id;
//	uint8_t gui_id;
};

void initMessages();

struct TcpipMessage *createConnectGUIMessage();

struct TcpipMessage *createPositionMessage(
		uint8_t robotId, int16_t x, int16_t y, int16_t z);

struct TcpipMessage *createActuatorMessage(
		uint8_t robotId, int16_t* output);

struct TcpipMessage *createConnectColindaMessage(uint8_t robotId);

struct TcpipMessage *createRunColindaMessage(uint8_t robotId);

struct TcpipMessage *createGenomeMessage(
		uint8_t robotId, uint8_t *pdna, uint8_t partId);

struct TcpipMessage *createConnectSym3DMessage();

struct TcpipMessage *createRunRobotMessage(uint8_t robotId);

struct TcpipMessageConfig *tmconf;

#ifdef __cplusplus
}
#endif

#endif /*TCPIP_HELPER_H_*/
