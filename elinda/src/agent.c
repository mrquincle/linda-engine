/**
 * @file agent.c
 * @brief Routines using agents
 *  
 * The Linda engine uses an agent abstraction for the robots. Each robots might also contain
 * several agents, but they are not part of this Elinda engine, but rather of the individual
 * controllers, the Colinda engines.
 * 
 * @date_created    Mar 16, 2009 
 * @date_modified   May 04, 2009
 * @author          Anne C. van Rossum
 * @project         Replicator
 * @company         Almende B.V.
 * @license         open-source, GNU Lesser General Public License
 */

#include <agent.h>
#include <evolution.h>
#include <stdlib.h>
#include <elinda.h>
#include <stdio.h>

#include <linda/log.h>

/***********************************************************************************************
 *
 * @name simulation_framework
 * 
 * Framework to define a series of simulated agents.
 *
 ***********************************************************************************************/

/**
 * 
 */
void initAgents() {
	if (econf == NULL) return;
	if (econf->population_size == 0) return;
	tprintf(LOG_VERBOSE, __func__, "Initialize agents");

	aa = calloc(econf->population_size, sizeof(struct Agent));
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		aa[i].elinda.simulation_state = ELINDA_SIMSTATE_TODO;
		aa[i].elinda.process_state = ELINDA_PROCSTATE_DEFAULT;
		aa[i].id = i;
		aa[i].fitness_level = 0;
		aa[i].fitness = 0;
	}
	clearSimulationState();
}

void clearSimulationState() {
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		aa[i].elinda.simulation_state = ELINDA_SIMSTATE_TODO;
	}
//	printAgentStates();
}

struct Agent *getAgentToBeSimulated() {
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		if (aa[i].elinda.simulation_state == ELINDA_SIMSTATE_TODO) {
			aa[i].elinda.simulation_state = ELINDA_SIMSTATE_CURRENT;
			char text[64]; sprintf(text, "Return agent %i at index %i", aa[i].id, i);
			tprintf(LOG_INFO, __func__, text);
			return &aa[i];
		}
	}
	tprintf(LOG_WARNING, __func__, "No agents to be simulated!");
	return NULL;
}

/**
 * Returns 0 if other agents are running, returns 1 if no other agents are running anymore
 * and there are some agents in a "todo" state, return 2 if all agents have been run.
 */
uint8_t allAgentsSimulated() {
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		if (aa[i].elinda.simulation_state == ELINDA_SIMSTATE_CURRENT) {
			tprintf(LOG_INFO, __func__, "Some agents are still running...");
			return 0;
		}
	}
	for (i = 0; i < econf->population_size; i++) {
		if (aa[i].elinda.simulation_state == ELINDA_SIMSTATE_TODO) {
			tprintf(LOG_INFO, __func__, "Some agents have to be run...");
			return 1;
		}
	}
	tprintf(LOG_INFO, __func__, "All agents did run...");
	return 2;
}

void printAgentStates() {
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		switch(aa[i].elinda.simulation_state) {
		case ELINDA_SIMSTATE_CURRENT: 
			printf("Agent %i: CURRENTLY RUNNING\n", i); break;
		case ELINDA_SIMSTATE_TODO: 
			printf("Agent %i: TODO\n", i); break;
		case ELINDA_SIMSTATE_DONE: 
			printf("Agent %i: DID ALREADY RUN\n", i); break;
		default: 
			printf("Agent %i: UKNOWN\n", i); 
		}
	}
}

struct Agent *getAgent(uint8_t id) {
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		if (aa[i].id == id) {
			return &aa[i];
		}
	}
	char text[64]; sprintf(text, "Agent %i does not exist!", id);
	tprintf(LOG_WARNING, __func__, text);
	return NULL;
}
