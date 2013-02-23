/**
 * @file fitness.h
 * @brief Storage of fitness figures for the entire simulated population
 * @author Anne C. van Rossum
 */

#ifndef FITNESS_H_
#define FITNESS_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>
#include <pthread.h>
	
struct AgentFitnessContainer {
	uint8_t fitness;
	uint8_t fitness_level;
};

struct FitnessConfig {
	uint8_t survival_percentage; //0 till 100
};

struct FitnessConfig *fconf;

void initFitnessModule();

void addFitness(uint8_t id, uint8_t fitness);

#ifdef __cplusplus
}
#endif 

#endif /*FITNESS_H_*/
