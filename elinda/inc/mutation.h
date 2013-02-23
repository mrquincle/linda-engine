/**
 * @file mutation.h
 * @brief Mutation engine.
 * @author Anne C. van Rossum
 */

#ifndef MUTATION_H_
#define MUTATION_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>
	
struct MutationConfig {
	uint8_t mutation_count;
};

struct MutationConfig *mconf;

void configMutation();

void applyMutations(uint8_t id);

#ifdef __cplusplus
}
#endif 

#endif /*MUTATION_H_*/
