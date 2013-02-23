/**
 * @file evolution.h
 * @brief Evolution configuration file (population size, genome)
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

#ifndef EVOLUTION_H_
#define EVOLUTION_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>
	
/**
 * The population size is the amount of robots in one generation. This is different from the
 * amount of robots that is tested in one trial. There can be way less robots simulated at one
 * particular moment in time than there are actually robots in the population.
 */
struct EvolutionConfig {
	uint8_t population_size;
};

struct EvolutionConfig *econf;

void initEvolution();

void startEvolution();

void stepEvolution();

#ifdef __cplusplus
}
#endif 

#endif /*EVOLUTION_H_*/
