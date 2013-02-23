
#include <fitness.h>
#include <agent.h>
#include <evolution.h>
#include <inttypes.h>
#include <stdlib.h>

#include <linda/log.h>

void initFitnessModule() {
	fconf = malloc(sizeof(struct FitnessConfig));
	fconf->survival_percentage = 50;
}

void addFitness(uint8_t id, uint8_t fitness) {
	struct Agent *la = getAgent(id);
	if (la == NULL) return;
	la->fitness = fitness;
}

