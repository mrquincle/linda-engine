/**
 * @file agent.h
 * @brief Definition of an agent (simulation entity)
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

#ifndef AGENT_H_
#define AGENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/****************************************************************************************************
 *  		Declarations
 ***************************************************************************************************/

#define MODULE_FITNESS
#define MODULE_GENOME
#define MODULE_ELINDA
	
struct Agent;

/****************************************************************************************************
 *  		Modules
 ***************************************************************************************************/

/**
 * The fitness module adds a fitness container.
 */

#ifdef MODULE_FITNESS
#include <fitness.h>
#endif
#ifdef MODULE_GENOME
#include <genomes.h>
#endif
#ifdef MODULE_ELINDA
#include <elinda.h>
#endif

/****************************************************************************************************
 *  		Definitions
 ***************************************************************************************************/

struct AgentBasicContainer {
//	struct Agent *next;
	uint8_t id;
};

/**
 * An agent contains several containers. The basic container makes it a linked list and gives it
 * an identifier. There are optional containers, in this case to store a genome and a fitness
 * value. 
 */
struct Agent {
	struct AgentBasicContainer;
#ifdef MODULE_FITNESS
	struct AgentFitnessContainer;
#endif
#ifdef MODULE_GENOME
	struct AgentGenomeContainer;
#endif
#ifdef MODULE_ELINDA
	struct AgentElindaContainer elinda;
#endif
};

/****************************************************************************************************
 *  		Agent pointers
 ***************************************************************************************************/

/**
 * An array of agents.
 */
struct Agent *aa;

/****************************************************************************************************
 *  		Methods
 ***************************************************************************************************/

struct Agent *getAgent(uint8_t id);

void printAgentStates();

void initAgents();

void clearSimulationState();

struct Agent *getAgentToBeSimulated();

uint8_t allAgentsSimulated();

#ifdef __cplusplus
}

#endif


#endif /*AGENT_H_*/

