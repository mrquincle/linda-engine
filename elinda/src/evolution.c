/**
 * @file evolution.c
 * @brief Implementation of the evolutionary engine
 *  
 * The evolutionary engine within the Linda Engine contains a population of genomes and applies
 * mutation operators to transform those genomes into adapted version that can be tested later on
 * in a fitness context.
 * 
 * @date_created    Mar 16, 2009 
 * @date_modified   May 06, 2009
 * @author          Anne C. van Rossum
 * @project         Replicator
 * @company         Almende B.V.
 * @license         open-source, GNU Lesser General Public License
 */

#include <genomes.h>
#include <evolution.h>
#include <mutation.h>
#include <stdlib.h>
#include <agent.h>
#include <stdio.h>
#include <fitness.h>

#include <linda/log.h>

/**
 * The population size is set over here. It might seem ad-hoc but in the simulator configuration 
 * the amount of trials is set, and the amount of individuals in each trial. Live with it. :-)
 * It sets the global variable "econf". 
 */
void configEvolution() {
	econf = malloc(sizeof(struct EvolutionConfig));
	econf->population_size = 4;
}

/**
 * Initializes necessary data structures. Change the configuration parameters, such as the 
 * population size, if necessary, afterwards. After adjusting the parameters, generate the 
 * genomes by calling startEvolution().
 * This should only be done if in the meantime, the agents are allocated. 
 */
void initEvolution() {
	configEvolution();
	initGenomes();
}

/**
 * Starts evolution by generating a series of genomes. They are coupled to agents and the agents
 * should be allocated beforehand and amount to the configured population_size over here.
 */
void startEvolution() {
	initFitnessModule();
	char text[64]; sprintf(text, "Generate %i genomes", econf->population_size);
	tprintf(LOG_INFO, __func__, text); 
	uint8_t i;
	for (i = 0; i < econf->population_size; i++) {
		aa[i].genome = generateGenome();
		printGenomeSummary(aa[i].genome, LOG_NOTICE);
	}
}

/**
 * Comparison function used by the qsort algorithm in applySelection. The function returns
 * a positive value if the value of a0 is LESS than the value of a1.
 */
int compare_agent_fitness (const void *a0, const void *a1) {
  const struct Agent* aa0 = (const struct Agent*) a0;
  const struct Agent* aa1 = (const struct Agent*) a1;
  return (aa0->fitness < aa1->fitness) - (aa0->fitness > aa1->fitness);
}

/**
 * Sort the agents on fitness and replace non-fit genomes by the fittest ones.
 */
void applySelection() {
	if (fconf == NULL) {
		tprintf(LOG_ERR, __func__, "Fitness module not initialized!"); return;	
	}
	
	qsort(aa, econf->population_size, sizeof(struct Agent), compare_agent_fitness);
	tprintf(LOG_INFO, __func__, "Get survivors");
	uint8_t i; char text[128];
	uint8_t population_survivors = (econf->population_size * fconf->survival_percentage) / 100;
	sprintf(text, "There are %i survivors", population_survivors);
	tprintf(LOG_NOTICE, __func__, text);
	for (i = 0; i < econf->population_size; i++) {
		char text2[128]; 
		sprintf(text2, "Fitness of %i is %i", i, aa[i].fitness);
		tprintf(LOG_NOTICE, __func__, text2); 
	}
	for (i = population_survivors; i < econf->population_size; i++) {
		uint8_t ancestor = rand() % population_survivors;
		copyGenome(aa[ancestor].genome, aa[i].genome);
	}
	
	//reorganize indices
	for (i = 0; i < econf->population_size; i++) {
		aa[i].id = i;
	}

	tprintf(LOG_INFO, __func__, "Agents procreated");
}

/**
 * Apply natural selection given the set of fitness values. Multiply the ones that come through
 * that natural selection filter. And step through all the genomes that remain with a mutation 
 * operator. 
 */
void stepEvolution() {
	tprintf(LOG_INFO, __func__, "Step evolution");
	uint8_t i;
	applySelection();
	for (i = 0; i < econf->population_size; i++) {
		applyMutations(i);
	}
}
