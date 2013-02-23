#ifndef TCPIPBANK_H_
#define TCPIPBANK_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <tcpip.h>
#include <inttypes.h>

struct TcpipSocket_LI;

struct TcpipSocket_LI {
	struct TcpipSocket_LI* next;
	struct TcpipSocket *item;
	uint8_t id;
};

struct Sockets {
	struct TcpipSocket_LI* first;
	struct TcpipSocket_LI* last;
};

struct Sockets sockets;

void initSockets();

//struct TcpipSocket* tcpipbank_create();

void tcpipbank_add(struct TcpipSocket *sock, uint8_t id);

void tcpipbank_del(uint8_t id);

struct TcpipSocket* tcpipbank_get(uint8_t id);

#ifdef __cplusplus
}
#endif 

#endif /*TCPIPBANK_H_*/
