/**
 * @file aer.h
 * @brief Address Event Representation [AER] management
 * @author Anne C. van Rossum
 *
 * The aer.h file includes the AER and AERBuffer definition. AER means Address Event Representation.
 * This representation is used extensively on so-called neuromorphic engineering chips, see for
 * example: "Aer building blocks for multi-layer multi-chip neuromorphic vision systems" by
 * Serrano-Gotarredona et al.
 */

#ifndef AER_H_
#define AER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
#include "core/inttypes.h"
#include "core/QueueHookable.h"
*/

#define MAX_AER_TUPLES 56
//#define MAX_AER_TUPLES 255

/**
 * A tuple of an address and an event. AER means Address Event Representation. The
 * event is a time stamp, the address an identifier.
 */
union AER {
	struct {
		union {
			struct {
				uint8_t x;
				uint8_t y;
			} coordinate;
			uint16_t address;
		};
		uint16_t event;
	};
	int32_t data;
};

struct AERBuffer {
	union AER aer[MAX_AER_TUPLES];
	uint8_t head;
	uint8_t tail;
};

#ifdef __cplusplus
}

#endif

#endif /*AER_H_*/
