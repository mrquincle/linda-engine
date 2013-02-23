/**
 * @file genome.h
 * @brief RawGenome Specification
 * @author Anne C. van Rossum
 * 
 * How is the content of the genome specified? And how is the expression of the genome implemented?
 * Over here the genome is implemented as in "Evolving Complete Agents using Artificial Ontogeny"
 * by Bongard and Pfeifer. It is not really obvious from the paper how exactly the genome is 
 * constructed, for that reason also parts from "Analog Genetic Encoding for the Evolution of Circuits 
 * and Networks" by Mattiussi and Floreano are used. The genetic representation used by them allows
 * for non-coding parts in the genome, which IMHO lead to a more structured form of mutational
 * preprocessing. However, Mattiussi and Floreano don't use a regulatory network in which genes 
 * activate other genes, but the genes directly encode for (in this case) the network topology and
 * device parameters. Only the start codon idea of Mattiussi and Floreano has been used, the patent
 * (with the device and ports idea) is not violated. 
 */

#ifndef GENOMES_H_
#define GENOMES_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>

/**
 * A codon is an unsigned char.
 */
#define Codon uint8_t

	/**
	 * The RawGenome is a series of Codons, just an array of characters. It is supposed to be 
	 * allocated in one block with calloc, so individual codons can be retrieved by content[i]. 
	 */
	struct RawGenome {
		Codon *content;
	};
	
	/**
	 * Contains the genome. 
	 */
	struct AgentGenomeContainer {
		struct RawGenome *genome;
	};

	/**
	 * The configuration settings for the Elinda engine only contains the size of the genome,
	 * maximum size is a million pairs. How many genes that corresponds to, depends on the amount
	 * of non-coding DNA and the average amount of codons one gene occupies.
	 */
	struct GenomesConfig {
		uint16_t genomeSize; 
	};
	
	/**
	 * A genome is generated and needs to be added to the agent. 
	 */
	struct RawGenome *generateGenome();
	
	/**
	 * A genome is generated for the agent with the given id. And it will be put in the 
	 * AgentGenomeContainer in the Agent struct.  
	 */
	void generateGenomeFor(uint8_t id);

	/**
	 * Initializes the default configuration.
	 */
	void initGenomes();
	
	/**
	 * Copy the content of a source genome to the target data structure.
	 */
	void copyGenome(struct RawGenome *src, struct RawGenome *target);
	
	/**
	 * Print head and tail, and perhaps some statistical information about a genome.
	 */
	void printGenomeSummary(struct RawGenome *g, uint8_t verbosity);
	
	/**
	 * Prints the entire genome.
	 */
	void printGenome();
	
	/**
	 * The configuration for the genome. Contains in the offline version only the size
	 * of the genome.
	 */
	struct GenomesConfig *gsconf;
	
#ifdef __cplusplus
}

#endif

#endif /*GENOMES_H_*/
