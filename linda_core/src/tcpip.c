/**
 * @file tcpip.c
 *
 * Sets up a socket with an application. It can be used subsequently by a tcpipController or
 * tcpipComponent to communicate values with this application. It can be used for example
 * for a controller to have an external application mapping sensor values to actuator values,
 * or it can be used for a component to send performance/fitness information as response to
 * orders about robot placements in the arena.
 *
 * The current implementation uses an underlying threadpool which is called an abbey. This is
 * most apparent in the syntax of the functions: void *function_name(void *context). Such a
 * function is executed asynchronously by dispatchTask(some_function, some_context). There are
 * yet no control structures that define partial ordered sets (posets) on tasks. All task
 * graphs are implicitly given by the context pointers.
 *
 * The syntax of the messages send over the TCP/IP connection is not defined over here, but
 * the responsibility of individual controllers and components.
 *
 * Check the TCP/IP packets by e.g.: sudo tcpdump -X -i lo portrange 3333-41000
 *
 * @date_created    Jan 28, 2009
 * @date_modified   Apr 14, 2009
 * @author          Anne C. van Rossum
 * @project         Replicator, ALwEN, CHAP
 * @company         Almende B.V.
 * @license         open-source
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#include <tcpip.h>
#include <bits.h>
#include <ptreaty.h>
#include <log.h>
#include <abbey.h>

/************************************************************************************************
 *                      Defines
 ***********************************************************************************************/

#define BACKLOG				1
#define VERBOSE

/************************************************************************************************
 *                      Function declarations
 ***********************************************************************************************/

void *tcpip_start_client(void *context);
void *tcpip_start_server(void *context);

void sprintmsg(struct TcpipMessage *msg, char* text);

/************************************************************************************************
 *                      Function implementations
 ************************************************************************************************
 *
 *  For mailboxes
 *
 ***********************************************************************************************/

/**
 * Frees the message and its content. Be aware that the return pointer is not a NULL pointer in
 * the C99 standard. So, if used to check on like "if(msg == NULL)" make sure, "msg = NULL;" is
 * called after a call to this routine freemsg(msg).
 */
void freemsg(struct TcpipMessage *m) {
	if (m == NULL) return;
	free(m->payload);
	free(m);
}

/**
 * Adds a message to the mailbox. It will be added as the newest (last) item and it's next
 * item will be NULL (it is not a linked list).
 */
void push(struct TcpipMailbox *M, struct TcpipMessage *m) {
	pthread_mutex_lock(M->lock);
	if (M->first == NULL) {
		//		tprintf(LOG_VERBOSE, __func__, "Put as first");
		M->first = m;
	} else {
		//		if (M->last == NULL) tprintf(LOG_ERR, __func__, "Last should never be NULL");
		//assumption: if M->first != NULL then M->last != NULL
		M->last->next = m;
	}
	M->last = m;
	m->next = NULL;
	pthread_mutex_unlock(M->lock);
}

/**
 * Does advance to the next message, but does not deallocate the previous message. The caller
 * should take care of deallocate the previous message. The retrieved message does not
 * refer anymore to the next message, this is already set to NULL.
 */
struct TcpipMessage *advance(struct TcpipMailbox *M) {
	pthread_mutex_lock(M->lock);
	if (M->first == NULL) {
		pthread_mutex_unlock(M->lock);
		return NULL;
	}
	struct TcpipMessage *m = M->first->next;
	M->first->next = NULL;
	M->first = m;
	pthread_mutex_unlock(M->lock);
	return m;
}

/**
 * Pops the message, doesn't free anything.
 */
struct TcpipMessage *pop(struct TcpipMailbox *M) {
	pthread_mutex_lock(M->lock);
	if (M->first == NULL) {
		pthread_mutex_unlock(M->lock);
		return NULL;
	}
	struct TcpipMessage *m = M->first;
	M->first = M->first->next;
	m->next = NULL;
	pthread_mutex_unlock(M->lock);
	return m;
}

/**
 * A message is moved from the head of the source to the bottom of the destination mailbox. It
 * could call advance and push, but then the move cannot be executed as an atomic action. During
 * the move process it is not possible to add or remove items from either mailbox.
 */
void move(struct TcpipMailbox *Msrc, struct TcpipMailbox *Mdest) {
	pthread_mutex_lock(Msrc->lock);
	pthread_mutex_lock(Mdest->lock);

	if (Msrc->first == NULL) {
		tprintf(LOG_WARNING, __func__, "No message in source mailbox");
		pthread_mutex_unlock(Mdest->lock);
		pthread_mutex_unlock(Msrc->lock);
		return;
	}
	struct TcpipMessage *m = Msrc->first;
	if (Mdest->first == NULL) {
		Mdest->first = m;
	} else {
		Mdest->last->next = m;
	}
	Mdest->last = m;

	m = Msrc->first->next;
	Msrc->first->next = NULL;
	Msrc->first = m;

	pthread_mutex_unlock(Mdest->lock);
	pthread_mutex_unlock(Msrc->lock);
}

/**
 * Counts the amount of messages in a mailbox.
 */
int count(struct TcpipMailbox *M) {
	pthread_mutex_lock(M->lock);
	int result = 0;
	struct TcpipMessage *m = M->first;
	while (m != NULL) {
		result++;
		m = m->next;
	}
	pthread_mutex_unlock(M->lock);
	return result;
}

/************************************************************************************************
 *                      Function implementations
 ************************************************************************************************
 *
 *  For connections
 *
 ***********************************************************************************************/

/**
 * An error handler that reaps the zombies.
 */
void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * Setting the tcpip connection with a default values. After calling this initialization
 * routine, the returned TcpipSocket struct can be used to overwrite the default values.
 * Such as the port number or the server/client mode.
 */
struct TcpipSocket *tcpip_get(unsigned char server) {
	tprintf(LOG_VERBOSE, __func__, "Return default TCP/IP Connection");
	struct TcpipSocket *tcpSocket = malloc(sizeof(struct TcpipSocket));
	tcpSocket->port_nr = 3333;
	tcpSocket->status = 0;
	if (server) RAISE(tcpSocket->status, TCP_SERVER);
	else RAISE(tcpSocket->status, TCP_CLIENT);

	tcpSocket->inbox = malloc(sizeof(struct TcpipMailbox));
	tcpSocket->outbox = malloc(sizeof(struct TcpipMailbox));
	tcpSocket->inbox->lock = malloc(sizeof(pthread_mutex_t));
	tcpSocket->outbox->lock = malloc(sizeof(pthread_mutex_t));
	tcpSocket->inbox->first = NULL;
	tcpSocket->inbox->last = NULL;
	tcpSocket->outbox->first = NULL;
	tcpSocket->outbox->last = NULL;
	pthread_mutex_init (tcpSocket->inbox->lock, NULL);
	pthread_mutex_init (tcpSocket->outbox->lock, NULL);
	tcpSocket->tcpThread = malloc(sizeof(pthread_t));
	tcpSocket->sync = malloc(sizeof(struct SyncThreads));
	ptreaty_init(tcpSocket->sync);
	tcpSocket->callbackIn = NULL;
	tcpSocket->callbackOut = NULL;
	tcpSocket->callbackConnect = NULL;
	tcpSocket->trials = 3;
	
	tprintf(LOG_VERBOSE, __func__, "TCP/IP Connection initialized");
	return tcpSocket;
}

/**
 * Starts tcp/ip, and is the same as tcpip_start, but using this routine, the user
 * doesn't need to know how tasks are dispatched.
 */
void tcpip_run(struct TcpipSocket *sock) {
	dispatch_described_task(tcpip_start, (void*)sock, "start client or server");
}

/**
 * Starts a stream on a TCP/IP socket. The tcpSocket struct should be initialized before,
 * which can be done with default values using tcpip_get(). The routine calls a task
 * that starts a TCP server or client, depending on tcpSocket->status. Over there a
 * server/client mode bit exists. It can be set by executing tcpip_get(SERVER) or
 * tcpip_get(CLIENT).
 */
void *tcpip_start(void* context) {
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;

	if (CLEARED(tcpSocket->status, TCP_SERVER)) {
		dispatch_described_task(tcpip_start_client, context, "start client");
	} else {
		dispatch_described_task(tcpip_start_server, context, "start server");
	}
	return NULL;
}

/**
 * Starts a stream on a TCP/IP socket. The tcpSocket struct should be initialized before,
 * which can be done with default values using tcpip_get(). The client is running as a
 * task, and hence in a separate thread and when it is finished it starts the task that
 * receives messages, namely tcpip_retrieve_packets. This routine does not to be called
 * by the application. The application sets the tcpSocket->status server/client mode
 * bit, e.g. by executing tcpip_get(CLIENT) and calls tcpip_start() which calls the
 * proper server versus client routine.
 */
void *tcpip_start_client(void *context) {
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;
	tprintf(LOG_VERBOSE, __func__, "TCP/IP start in client mode");
	tcpSocket->serv_addr.sin_family = AF_INET;
	tcpSocket->serv_addr.sin_port = htons(tcpSocket->port_nr);
	//@todo Check: tcpSocket->serv_addr.sin_addr.s_addr = INADDR_ANY;
	if ((tcpSocket->serv_sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) == -1) {
		tprintf(LOG_WARNING, __func__, "Setting up socket failed\n");
		return NULL;
	}
	connect(tcpSocket->serv_sockfd,(struct sockaddr*)&tcpSocket->serv_addr,
			sizeof(tcpSocket->serv_addr));
	tprintf(LOG_VERBOSE, __func__, "Client sets up a socket");
	tprintf(LOG_BLABLA, __func__,
			"Messages can be send, but disappear in the void if no server is available");

	tcpSocket->write_sockfd = tcpSocket->serv_sockfd;
	tcpSocket->read_sockfd = tcpSocket->serv_sockfd;

	dispatch_described_task(tcpip_retrieve_packets, context, "retrieve packets");
	if (tcpSocket->callbackConnect != NULL)
		dispatch_described_task(tcpSocket->callbackConnect, context, "client started");
	return NULL;
}

/**
 * Starts a stream on a TCP/IP socket. The tcpSocket struct should be initialized before,
 * which can be done with default values using tcpip_get(). The server is running as a
 * task, and hence in a separate thread and when it is finished it starts the task that
 * receives messages, namely tcpip_retrieve_packets. This routine does not to be called
 * by the application. The application sets the tcpSocket->status server/client mode
 * bit, e.g. by executing tcpip_get(SERVER) and calls tcpip_start() which calls the
 * proper server versus client routine.
 */
void *tcpip_start_server(void* context) {
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;
	tprintf(LOG_VERBOSE, __func__, "TCP/IP start in server mode");
	//	RAISE(tcpSocket->status, TCP_SERVER);
	if ((tcpSocket->serv_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		tprintf(LOG_ERR, __func__, "At socket(SOCK_STREAM) there was an error...");
		exit(1);
	}

	int yes=1;
	if (setsockopt(tcpSocket->serv_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
			sizeof(int)) == -1) {
		tprintf(LOG_ERR, __func__, "At setsockopt(sockfd) there was an error...");
		exit(1);
	}

	tcpSocket->serv_addr.sin_family = AF_INET;
	tcpSocket->serv_addr.sin_port = htons(tcpSocket->port_nr);
	//@todo check incoming address:
	tcpSocket->serv_addr.sin_addr.s_addr = INADDR_ANY;
	memset(tcpSocket->serv_addr.sin_zero, '\0', sizeof tcpSocket->serv_addr.sin_zero);

	if (bind(tcpSocket->serv_sockfd, (struct sockaddr *)&tcpSocket->serv_addr,
			sizeof (tcpSocket->serv_addr)) == -1) {
		tprintf(LOG_ERR, __func__, "At bind(sockfd) there was an error...");
		exit(1);
	}

	if (listen(tcpSocket->serv_sockfd, BACKLOG) == -1) {
		tprintf(LOG_ERR, __func__, "At listen(sockfd) there was an error...");
		exit(1);
	}

	struct sigaction sa;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		tprintf(LOG_ERR, __func__, "At sigaction(address) there was an error...");
		exit(1);
	}

	tprintf(LOG_VERBOSE, __func__, "Waiting for client to connect...");
	unsigned int sin_size = sizeof(tcpSocket->cli_addr);
	if ((tcpSocket->cli_sockfd = accept(tcpSocket->serv_sockfd,
			(struct sockaddr *)&tcpSocket->cli_addr, &sin_size)) == -1) {
		tprintf(LOG_ERR, __func__, "At accept(sockfd) there was an error...");
	}
	char text[64];
	sprintf(text, "Connected to client %s", inet_ntoa(tcpSocket->cli_addr.sin_addr));
	tprintf(LOG_VERBOSE, __func__, text);

	tcpSocket->write_sockfd = tcpSocket->cli_sockfd;
	tcpSocket->read_sockfd = tcpSocket->cli_sockfd;

	dispatch_described_task(tcpip_retrieve_packets, (void*)tcpSocket, "retrieve packets");
	if (tcpSocket->callbackConnect != NULL)
		dispatch_described_task(tcpSocket->callbackConnect, context, "server started");
	return NULL;
}

/**
 * This method starts listening to a socket, each time a command is received it is placed in the
 * inbox and the thread continues to listen for messages on the socket. The size of each command
 * is predefined and has to be adjusted over here if the protocol changes. There is no
 * end-of-command construction. Sending random bytes at times, or sending more or less bytes
 * than expected will corrupt this listener.
 *
 * @todo reformulate in task format, should call itself recursively, put write_sockfd in
 * tcpSocket struct and "calculate" only once beforehand
 */
void* tcpip_retrieve_packets(void* context) {
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;
	char text[128];
	sprintf(text, "Listen for packets on fd %i", tcpSocket->read_sockfd);
	tprintf(LOG_VV, __func__, text);
	unsigned char command, size, i;
	int nofbytes;
	unsigned char result[MAX_PACKET_SIZE-1];
	struct TcpipMessage *msg;

	nofbytes = recv(tcpSocket->read_sockfd, &command, sizeof(unsigned char), 0);
	char errormsg[64];
	switch(nofbytes) {
	case -1: 
		sprintf(errormsg, "Error with error code %i!", errno);
		tprintf(LOG_ERR, __func__, errormsg);
		if (errno != 107) return NULL;
		tcpSocket->trials--;
		if (!tcpSocket->trials) {
			tprintf(LOG_CRIT, __func__, "Can not get a connection!");
			return NULL;
		}
		sleep(3);
		tprintf(LOG_WARNING, __func__, "Try again in 3 seconds!");
		close(tcpSocket->cli_sockfd);
		close(tcpSocket->serv_sockfd);
		dispatch_described_task(tcpip_start, context, "restart tcp/ip");
		return NULL;
	case 0:
		tprintf(LOG_WARNING, __func__, "Other side disconnected, restart!");
		close(tcpSocket->cli_sockfd);
		close(tcpSocket->serv_sockfd);
		dispatch_described_task(tcpip_start, context, "restart tcp/ip");
		return NULL;
	default:
		;
	}
	
	sprintf(text, "Command packet received... %i", command);
	tprintf(LOG_VVVV, __func__, text);
	if (nofbytes <= 0) goto loop;
	nofbytes = recv(tcpSocket->read_sockfd, &size, sizeof(unsigned char), 0);
	sprintf(text, "Size packet received... %i", size);
	tprintf(LOG_VVVV, __func__, text);
	if (nofbytes <= 0) goto loop;
	if (size == 0) goto loop;
	nofbytes = recv(tcpSocket->read_sockfd, result, size, 0);
	sprintf(text, "The rest of packet received... %i", nofbytes);
	tprintf(LOG_VVVV, __func__, text);

	msg = malloc(sizeof(struct TcpipMessage));
	msg->payload = calloc(MAX_PACKET_SIZE-1, sizeof(unsigned char));
	msg->size = size+2;

	msg->payload[0] = command;
	msg->payload[1] = size;
	for (i=0;i<nofbytes;i++) {
		msg->payload[i+2] = result[i];
	}
	tprintmsg(msg, LOG_VVV);
	//	tcpSocket->messageCount++;
	push(tcpSocket->inbox, msg);

	//not nice, this construct
	if (tcpSocket->callbackIn != NULL)
		dispatch_task(tcpSocket->callbackIn, context);

	loop:
	//	if (tcpSocket->messageCount > 20) return NULL;

	dispatch_described_task(tcpip_retrieve_packets, context, "retrieve packets");
	return NULL;
}

/**
 * Each time something is pushed in an outbox, call tcpip_send to assure delivery.
 */
void tcpip_send(struct TcpipSocket *sock) {
	dispatch_described_task(tcpip_send_packets, (void*)sock, "send packets");
}

/**
 * This routine sends immediately a message over a TCP/IP socket by the system call
 * "send". To ensure asynchronous execution, this function can be executed as a task
 * in a thread pool. It can send a bunch of messages that are waiting in the outbox
 * of the current tcpSocket struct. It will not execute itself in the end.
 *
 * @todo there might be a periodic task that executes this task regularly
 */
void *tcpip_send_packets(void* context) {
	tprintf(LOG_VV, __func__, "Send TCP/IP packets...");
	struct TcpipSocket *tcpSocket = (struct TcpipSocket*)context;
	struct TcpipMessage *msg;
	int retval;
	msg = pop(tcpSocket->outbox);
	if (msg == NULL) {
		tprintf(LOG_WARNING, __func__, "Nothing to send");
	}
	tprintmsg(msg, LOG_VVV);
	tprintf(LOG_VVVV, __func__, "Send now!");
	retval = send(tcpSocket->write_sockfd, msg->payload, msg->size, 0);
	if (retval == -1) {
		char errormsg[64]; sprintf(errormsg, "Error with error code %i!", errno);
		tprintf(LOG_ERR, __func__, errormsg);
		return NULL;
	} else if (retval == 0) {
		tprintf(LOG_WARNING, __func__, "Other side disconnected, restart!");
		return NULL;
	}
	tprintf(LOG_VVVV, __func__, "Free msg");
	freemsg(msg);
	if (tcpSocket->callbackOut != NULL) {
		tprintf(LOG_VERBOSE, __func__, "Callback");
		dispatch_described_task(tcpSocket->callbackOut, context, "tcp/ip callback");
	}
	return NULL;
}

/**
 * Closes sockets.
 */
void tcpip_close_all(struct TcpipSocket *tcpSocket) {
	close(tcpSocket->cli_sockfd);
	close(tcpSocket->serv_sockfd);
}

/************************************************************************************************
 *                      Function implementations
 ************************************************************************************************
 *
 *  For logging and printing
 *
 ***********************************************************************************************/

/**
 * Print TcpipMessage to char array.
 */
void sprintmsg(struct TcpipMessage *msg, char* text) {
	//	printf("%s: size = %i\n", __func__, msg->size);
	sprintf(text, "%s[", text);
	int i;
	for (i=0;i<msg->size;i++) {
		//sprintf(text, "%s%3i", text, msg->payload[i]);
		sprintf(text, "%s%i", text, msg->payload[i]);
		if (i != msg->size-1) sprintf(text, "%s,", text);
	}
	sprintf(text, "%s]", text);
}

/**
 * Print message to stdout
 */
void tprintmsg(struct TcpipMessage *msg, uint8_t verbosity) {
	if (isPrinted(verbosity)) {
		//		printf("%s obtain lock\n", __func__);
		pthread_mutex_lock(logconf->printAtomic);
		char text[msg->size*5+128];
		sprintf(text, "Message ");
		sprintmsg(msg, text);
		sprintf(text, "%s (size %i)", text, msg->size);
		tprintf(LOG_VERBOSE, __func__, text);
		pthread_mutex_unlock(logconf->printAtomic);
	}
}

/**
 * Create some message
 */
struct TcpipMessage *templateMsg() {
	struct TcpipMessage *msg = malloc(sizeof (struct TcpipMessage *));
	msg->payload = calloc(MAX_PACKET_SIZE-1, sizeof(unsigned char));
	msg->size = 5;
	int i;
	for (i=0;i<msg->size;i++) {
		msg->payload[i] = i * 10;
	}
	msg->payload[2] = msg->size - 2;
	return msg;
}
