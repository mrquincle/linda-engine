/**
 * @file genome.h
 * @brief Genome Specification
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
 * device parameters. 
 *
 * @todo The genome and its representation is in a virgin state. This has to be implemented still. 
 */

#ifndef GENOME_H_
#define GENOME_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdint.h>

	struct Product;

#ifdef MODULE_GRID
#include <grid.h>
#endif
	
	//struct Genome;
	
#ifdef MODULE_EVOLUTION
#include <evolution.h>
#endif
	
#ifdef MODULE_PHENOTYPE
#include <phenotype.h>
#endif

#define Codon uint8_t

	struct ProductId {
		uint8_t id[3];
	};

	struct Product {
		uint8_t id[3];
		uint8_t concentration;
		uint8_t new_concentration;
		struct Product *next;
	};

	/**
	 * Contains only an array of characters, the default content of the Genome struct. This is
	 * supposed to be allocated in one block with calloc, so individual codons can be retrieved
	 * by content[i].
	 */
	struct GenomeContainer {
		Codon *content;
	};

	/**
	 * A Genome is an unstructured 1-dimensional sequence of coding and non-coding regions, hence not just 
	 * a linked list of genes. The Genome is not an array, but a linked list (of ribosomes), so it can be
	 * expanded. It's a waste of space to have each codon linked to the next codon. Hence, a design-time
	 * array is the only possibility, because there is no calloc functionality in FreeRTOS. There is an
	 * indirection between Gene and CodonGene so that a CodonGene can be pointed to an address / codon in 
	 * the genome. A Gene can also point to the next Gene. Without this indirection the linked list
	 * field on a Gene would end up in the genome.
	 */
	struct Genome {
		struct GenomeContainer;
#ifdef MODULE_EVOLUTION
		struct EvolutionContainer;
#endif
#ifdef MODULE_PHENOTYPE
		struct PhenotypeContainer phenotype;
#endif
	};

	/**
	 * The configuration stores the amount of regulating transcription factors, and other parameters
	 * that define how genetic bit sequences have to be mapped to phenotypical symbols (which are in
	 * this case gene products / transcription factors). 
	 * 
	 * The maximum amount of genes is GenomeSize / 8. On average the amount of genes is a little 
	 * less than GenomeSize / 16.
	 */
	struct GenomeConfig {
		uint8_t regulatingFactors;
		uint8_t phenotypicFactors;
//		uint16_t genomeSize; 
	};

	struct Gene;
	
	union CodonGene;

	union CodonGene {
		struct {
			Codon content[8];
		};
		struct {
			Codon DeviceToken;		
			Codon ProductIn;		
			Codon ProductOut;		
			Codon LocationOut_X;	
			Codon LocationOut_Y;	
			Codon conc_inc;
			Codon conc_low;
			Codon conc_high;
		};
	};

	struct Gene {
		union CodonGene *codons;
		struct Gene *next;
	};

	struct ExtractedGenome {
		struct Gene *genes;
		uint16_t gene_count;
	};

	/**
	 * Three parameters are updated outside of this file, so the entire scheduling, the interaction between
	 * the genes, etc. is upto the consumer side.
	 */
	struct Gene *g;

	/**
	 * The Genome is labelled "dna" and is filled on the robot by listening to one of it's input ports,
	 * or in the simulator, by using dna of previous generations and applying mutational operators.  
	 */
	struct Genome *dna;

	/**
	 * The extracted genome is a subset of genes on the "dna", where a valid gene is denoted by a
	 * certain marker or id.
	 */
	struct ExtractedGenome *eg;

	struct GenomeConfig *gconf;

	void configGenome();

	void freeGenome();
	
	void precalculateChangeConcentration(struct Product *p, int8_t amount);
	
	void changeConcentration(struct Product *p, int8_t amount);
	
	void updateConcentration();

#ifdef WITH_TEST
	void extractGenes(uint16_t genomeSize);
#endif
	
	void freeGenes();
	
	int16_t stepGeneExtraction(uint16_t buffer_size);
	
	void initGeneExtraction();
	
	void transcribeGenes();

	struct Product *getProduct(struct ProductId *id);
	
	void receiveNewGenome();
	
#ifdef WITH_CONSOLE
void printGenes();
uint16_t printGenesToStr(char *str, uint16_t length);
void printGenesOfProduct(uint8_t productId);
void printGenesPerProductDistribution();
#endif
	
#ifdef WITH_TIME
	void generateGenome();
#else

#endif

#ifdef __cplusplus
}

#endif

#endif /*GENOME_H_*/
