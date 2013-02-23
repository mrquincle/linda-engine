/**
 * Operates upon a genome and applies default mutation operators.
 */

#include <mutation.h>
#include <genomes.h>
#include <time.h>
#include <stdlib.h>
#include <agent.h>
#include <stdio.h>

#include <linda/bits.h>
#include <linda/log.h>

void configMutation() {
	mconf = malloc(sizeof(struct MutationConfig));
	mconf->mutation_count = 100;
}

/**
 * Applies a series of mutations, for now just point mutations.
 */
void applyMutations(uint8_t id) {
	tprintf(LOG_VERBOSE, __func__, "Mutate genome");
	struct Agent *la = getAgent(id);
	srand(time(NULL)); uint8_t i;
	for (i = 0; i < mconf->mutation_count; i++) {
		uint8_t position = rand() % gsconf->genomeSize;
		uint8_t bit = rand() % 8;
//		printf("Switch genome at position %i from %i", position, 
//				la->genome->content[position]);
		TOGGLE(la->genome->content[position], bit);
//		printf(" to %i\n", la->genome->content[position]);
	}
}
