/**
 * A bank of TCP/IP connections. A series of connections have to be kept online all the time.
 * It needs also be dynamically adjusted. This means that everything that belongs to a certain
 * connection is stored in a TcpipSocket struct, nothing in local variables. If every monk
 * does have its own socket to handle, that struct can be stored in thread-specific data keys.
 * The compiler is called with the "-qtls" flag to obtain this thread-local storage possibility.
 * 
 * @see http://people.redhat.com/drepper/tls.pdf
 */

#include <tcpipbank.h>
#include <stdlib.h>

/**
 * This routine is called by a separate thread. It initializes a TcpipSocket struct, which 
 * will be thread specific (using the __thread keyword). The bucket brigade from incoming
 * to outgoing mailboxes needs to cross thread boundaries.  
 */
void initSockets() {
	sockets.first = sockets.last = NULL;
}

void tcpipbank_add(struct TcpipSocket *sock, uint8_t id) {
	struct TcpipSocket_LI* lisock = sockets.last;
	if (lisock == NULL) {
		sockets.first = sockets.last = lisock = malloc(sizeof(struct TcpipSocket_LI));
	} else {
		sockets.last = sockets.last->next = lisock = malloc(sizeof(struct TcpipSocket_LI));
	}
	lisock->id = id;
	lisock->item = sock;
	lisock->next = NULL;
}

void tcpipbank_del(uint8_t id) {
	struct TcpipSocket_LI* lsock = sockets.first;

	while (lsock != NULL) {
		if (lsock->id == id) {
			//do a lot beforehand
			free (lsock->item);
		}
		lsock = lsock->next;
	}
}

struct TcpipSocket* tcpipbank_get(uint8_t id) {
	struct TcpipSocket_LI* lsock = sockets.first;

	while (lsock != NULL) {
		if (lsock->id == id) {
			return lsock->item;
		}
		lsock = lsock->next;
	}
	return NULL;
}


