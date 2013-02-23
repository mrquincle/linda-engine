/**
 * @file embryogeny.h
 * @brief The Developmental Engine
 * @author Anne C. van Rossum
 * 
 * The Gene Regulatory Network has to apply all kind of changes to the network of neurons on the
 * defined grid. This module knows how to remove and add neurons, move them, split (duplicate)
 * them, etc. It is like table 1 "Phenotypic effect of neural development gene products" in the
 * article "Evolving Complete Agents using Artificial Ontogeny" by Bongard and Pfeifer. For 
 * differences, see the descriptions at the functions.
 * 
 * The developing paradigm described by Bongard et al. is taken as inspiration(!) for a developmental
 * model. Bongard uses only 6 grid cells in which multiple neurons can exist. This introduces an
 * artificial constraint of 100 neurons for the overall grid, where one diffusion site might contain 
 * much more neurons than another site. In a 2D environment the neurons compete for space and the 
 * amount of neurons is inherently limited to the amount of cells in the grid. So, in this 
 * implementation at a certain grid cell or "diffusion site" only one neuron might exist. The grid 
 * is therefore bigger then the 6 cells of Bongard.  The current implementation has 5x5 grid cells, 
 * is hence 2D.   
 */

#ifndef EMBRYOGENY_H_
#define EMBRYOGENY_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>

	/**
	 * What kind of changes to the NN do we want to implement? Neurons may be duplicated to nearby
	 * positions, or they might be randomly removed, or they may have some lifetime, or they may
	 * grow into certain directions with higher concentrations. How to write it such that for example
	 * columnar architectures may emerge?
	 */
	struct Embryogeny {
		void *start;
		uint8_t default_delay;
		float default_weight;
	};

	/**
	 * In the embryogeny process it is necessary to keep track of synapses on a neuron in a cell, so
	 * that cellular instructions may be defined on a synapse level. Such as remove synapse, or add
	 * synapse. That is why there is a current_port pointer.
	 */
	struct EmbryogenyContainer {
		struct Port *current_port;
	};

	struct Embryogeny *e;

	void init_embryology();
	
	void start_embryology();

	void free_embryology();
	
	void applyMorphologicalChange(uint8_t index);

#ifdef WITH_PRINT_DISTRIBUTION
	void printDistribution(uint8_t verbosity);
#endif
	
	/**
	 * Iterate all the cells and apply the cellular encoding operations. For that we need to pointers,
	 * one that navigates the neuron space, one that navigates the synapse space. 
	 */
	struct Neuron *np;

#ifdef WITH_TEST
	uint16_t countNeurons();

	uint8_t testNeurons();
	uint8_t testNeuronGrid();
	uint8_t testSynapseExistence();
	uint8_t testSynapsePortMapping();
#endif
	
#ifdef __cplusplus
}

#endif

#endif /*EMBRYOGENY_H_*/
