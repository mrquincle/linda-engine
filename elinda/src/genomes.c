/**
 * @file genome.c
 * @brief Generation and extraction of genomes
 * @author Anne C. van Rossum
 */
#include <inttypes.h>
#include <genomes.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include <linda/log.h>

/**
 * In the colinda engine the genome is also initialized. Be careful in testing cases and
 * make sure that for example the size of the genome over here, does not exceed the size
 * of the genome in the Colinda engines. Moreover, the initGenome() procedure sets only
 * an indicated genome size. This is allowed to be overwritten by other files. This is
 * for example done in the elinda::main() routine after the call to initEvolution() which
 * does call this initialization method, initGenomes().
 */
void initGenomes() {
	gsconf = malloc(sizeof(struct GenomesConfig));
	gsconf->genomeSize = 5000;
	srand(time(NULL));
}

/**
 * This routine prints a summary of the genome. Currently only the first and last series
 * of codons. Later on statistical information might be added.
 */
void printGenomeSummary(struct RawGenome *g, uint8_t verbosity) {
	tprintf(verbosity, __func__, "Genome start");
	uint16_t i; char textV[128];

	for (i = 0; i < 30; i+=10) {
		sprintf(textV, "[%i, %i, %i, %i, %i, %i, %i, %i, %i, %i]", 
				g->content[i+0], g->content[i+1], g->content[i+2], g->content[i+3], g->content[i+4], 
				g->content[i+5], g->content[i+6], g->content[i+7], g->content[i+8], g->content[i+9]); 
		tprintf(verbosity, __func__, textV);
	}

	tprintf(verbosity, __func__, "Genome end");
	for (i = gsconf->genomeSize - 30; i < gsconf->genomeSize; i+=10) {
		sprintf(textV, "[%i, %i, %i, %i, %i, %i, %i, %i, %i, %i]", 
				g->content[i+0], g->content[i+1], g->content[i+2], g->content[i+3], g->content[i+4], 
				g->content[i+5], g->content[i+6], g->content[i+7], g->content[i+8], g->content[i+9]); 
		tprintf(verbosity, __func__, textV);
	}	
}

/**
 * The random genome can be used as a seed. Subsequent genomes should be formed by manipulating
 * the genomes of previous generations, or be load from a file.
 */
struct RawGenome *generateRandomGenome() {
	struct RawGenome *ldna = malloc(sizeof(struct RawGenome));
	ldna->content = malloc(gsconf->genomeSize * sizeof(Codon));
	uint16_t i;
	for (i = 0; i < gsconf->genomeSize; i++) { 
		ldna->content[i] = rand();
	}
	return ldna;	
}

/**
 * Can be used for testing purposes. It just contains an incrementing range of values from
 * 0 till 249 in the case of a genome size of 500 elements. Each value occurs twice, like: 
 * [0 0 1 1 .... 248 248 249 249]. 
 * It's easy to see, using this test genome, if the entire genome is communicated between the
 * processes.
 */
struct RawGenome *generateTestGenome() {
	struct RawGenome *ldna = malloc(sizeof(struct RawGenome));
	ldna->content = calloc(gsconf->genomeSize, sizeof(Codon));
	uint16_t i;
	for (i = 0; i < gsconf->genomeSize; i++) { 
		ldna->content[i] = i / 2;
	}
	return ldna;
}

/**
 * A genome is generated and needs to be added to the agent, if it is gonna to be used. It
 * is also possible to use generateGenomeFor(uint8_t id).  
 */
struct RawGenome *generateGenome() {
	//	return generateTestGenome();
	return generateRandomGenome();
}

/**
 * Assumes both structs are already allocated, as well as their content array of Codons,
 * which is both assumed to be the size of gsconf->genomeSize.
 */
void copyGenome(struct RawGenome *src, struct RawGenome *target) {
	uint16_t i;
	for (i = 0; i < gsconf->genomeSize; i++) { 
		target->content[i] = src->content[i];
	}		
}

/**
 * Prints the entire dna content, decorated with indices for each element. There
 * are by default 16 items on a line.
 */
void printGenome(struct RawGenome *genome) {
	uint16_t i, line_size = 16;
	for (i = 0; i < gsconf->genomeSize; i++) {
		if (!(i % line_size)) printf("\n%3i: ", i);
		printf("%3i", genome->content[i]);
		if (((i + 1) % line_size) && (i+1 != gsconf->genomeSize)) printf(", ");
	}
}
