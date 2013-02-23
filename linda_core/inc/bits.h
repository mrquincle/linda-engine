/**
 * @file bits.h
 * @brief Basic bit-wise operations.
 * @author Anne C. van Rossum
 *
 * Some basic bit-wise operations. Currently using uint8_t integers as underlying
 * format. Meant for microcontroller usage, hence that small.
 */

#ifndef BITS_H_
#define BITS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/**
 * Doesn't check if bit size is small enough, like under 8 or under 16.
 */

#define RAISE(bitseq, bit) ((bitseq) |= (1 << (bit)))
#define CLEAR(bitseq, bit) ((bitseq) &= ~(1 << (bit)))

/**
 * @todo Check all uses, because it might be not realized that it starts with 0..7
 */
#define RAISED(bitseq, bit) ((bitseq) & (1 << (bit)))
	

#define CLEARED(bitseq, bit) (!((bitseq) & (1 << (bit))))

#define ADVANCE(bitseq) (bitseq <<= 1)

#define TOGGLE(bitseq, bit) ((bitseq) ^= 1 << (bit))

uint8_t FIRST(uint8_t bitseq);

uint8_t RANDOM();

#ifdef __cplusplus
}

#endif

#endif /*BITS_H_*/
