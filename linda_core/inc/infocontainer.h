#ifndef INFOCONTAINER_H_
#define INFOCONTAINER_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/**
 * The default info container contains a type, an id and a value.
 */
struct InfoDefault {
	uint8_t type;
	uint8_t id;
	uint8_t value;
};

struct InfoProcess {
	char* name;
};

struct InfoChannel {
	uint8_t type;
	struct in_addr *host;
	int port;
	uint8_t id;
};

struct InfoArray {
	uint8_t type;
	uint8_t* values;
	uint8_t length;
};


#ifdef __cplusplus
}
#endif

#endif /*INFOCONTAINER_H_*/
