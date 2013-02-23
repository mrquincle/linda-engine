/**
 * @file tcpip.h
 * @brief Open TCP/IP sockets to/from this application.
 * @author Anne C. van Rossum
 */

#ifndef TCPIP_H_
#define TCPIP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <netinet/in.h>

/************************************************************************************************
 *                      Defines
 ***********************************************************************************************/

#define MAX_MAILBOX_SIZE	32
#define MAX_PACKET_SIZE		2048
#define TRUE				1
#define FALSE				0
#define TCP_CLIENT          4
#define TCP_SERVER			2
#define TCP_STOP_STREAM		1
#define TCP_IDLE			0

/************************************************************************************************
 *                      Data Structures
 ************************************************************************************************/

/**
 * The TcpipMessage struct contains a size and a payload field. The size refers to the entire
 * payload, also if the first items are command or identifier-like. And they are. This is needed
 * because a TCP/IP package may be of different size. The payload[0] value is considered to be
 * a command in tcpip.c and payload[1] the size of the rest of the command. Hence, the value of
 * size over here is 2 more than the value in payload[1].
 */
struct TcpipMessage {
	unsigned char size;
	unsigned char *payload;
	struct TcpipMessage *next;
};

struct TcpipMailbox {
	struct TcpipMessage *first;
	struct TcpipMessage *last;
	pthread_mutex_t *lock;
};

struct TcpipSocket {
	int port_nr;
	struct sockaddr_in serv_addr, cli_addr;
	int cli_sockfd, serv_sockfd, write_sockfd, read_sockfd;
	unsigned char status;
	struct TcpipMailbox *inbox, *outbox;
	pthread_t *tcpThread;
	void *(*callbackIn)(void*);
	void *(*callbackOut)(void*);
	void *(*callbackConnect)(void*);
	struct SyncThreads *sync;
	int trials;
};

struct InfoSockAndMsg {
	struct TcpipMessage *msg;
	struct TcpipSocket *sock;
};

/************************************************************************************************
 *                      Global variables
 ************************************************************************************************
 *
 * It is in principle possible to switch runtime from one socket to another by storing the
 * pointer to the tcpSocket variable and evoke tcpip_get() and tcpip_start_server() again.
 * It's the responsibility of the callee to switch to the former channel again. It is also
 * its responsibility to garantee that not the same port number is used.
 *
 ************************************************************************************************/

//#ifdef __cplusplus
//extern struct TcpipSocket *tcpSocket;
//#else
//struct TcpipSocket *tcpSocket;
//#endif

/************************************************************************************************
 *                      External Function Declarations (for sockets)
 ************************************************************************************************/

struct TcpipSocket *tcpip_get(unsigned char server);

void *tcpip_start(void* context);

void *tcpip_retrieve_packets(void* context);

void *tcpip_send_packets(void* context); 

void tcpip_close_all(struct TcpipSocket *tcpSocket);

/************************************************************************************************
 *                      External Function Declarations (for mailboxes)
 ************************************************************************************************/

void freemsg(struct TcpipMessage *m);

void push (struct TcpipMailbox *M, struct TcpipMessage *m);

int count(struct TcpipMailbox *M);

struct TcpipMessage *advance(struct TcpipMailbox *M);

struct TcpipMessage *pop(struct TcpipMailbox *M);

void move(struct TcpipMailbox *Msrc, struct TcpipMailbox *Mdest);

void sprintmsg(struct TcpipMessage *msg, char* text);

void tprintmsg(struct TcpipMessage *msg, uint8_t verbosity);

#ifdef __cplusplus
}
#endif

#endif /*TCPIP_H_*/
