/**
 * @file genome.c
 * @brief Generation and extraction of genomes
 * @author Anne C. van Rossum
 */
#include <inttypes.h>
#include <genome.h>
#include <lindaconfig.h>

#include <grid.h>

#ifdef WITH_CONSOLE
#include <stdio.h>
#include <linda/log.h>
#include <string.h>
#endif

/**
 * Only floating point values for Ribosome's are used, they are truncated to 2D locations for
 * production locations and to gene product indices. It is assumed that a robot can grow on 6 dockable
 * sides and that motor neurons can be attached to wheels left/right and sensor neurons can be attached
 * to sensors all around.
 * A "device token" is formed from a triple of float values. It is needed so that other genes can
 * regulate it using that token as reference (indirectly via gene products). The "gene products" trigger
 * a rule when they reach a concentration threshold as in table 1 in Bongard's paper. This moves neurons
 * around, splits a neuron, etc.
 * I don't really belief in point mutations, so a "device token" is not tailored to a broad range of
 * floating point values, so that it can be found by point mutations from a random sequence. But gene
 * duplications etc. are of course possible.
 */

/**
 * A gene contains the following ribosomes:
 *   [DeviceToken, Flags, ProductIn, ProductOut, LocationOut, Concentrations].
 * Each of those are triples of floating point values, except for Flags which is a boolean.
 */

/***********************************************************************************************
 *
 * @name genome_general
 * The genome and operations on the genome. Mutation operations are defined elsewhere.
 *
 ***********************************************************************************************/

/**
 * The configGenome allocate configuration parameters and configures the grid. So, it sets the
 * global variables "gconf" and "s".
 *
 * The genome does not have a initGenome routine, but a generateGenome or a receiveGenome routine,
 * depending if the code is running on a robot or in a simulator. Those routines set the global
 * variable "dna" (not this function). The order of generateGenome/receiveGenome and this function
 * does not matter.
 */
void configGenome() {
	gconf = lindaMalloc(sizeof(struct GenomeConfig));
	gconf->regulatingFactors = 11;
	gconf->phenotypicFactors = 14;
}

/**
 * Deallocates the memory space for the grid and the genome configuration structure. The
 * dna structure (type struct Genome) is not freed. The grid is not freed either, that is done
 * in free_embryology(). This routine is by default called from developNeuralNetwork() amongst
 * all the other deallocation routines. Most modules are also initialized over there. 
 */
void freeGenome() {
	free(gconf);
}

/**
 * Deallocates all the genes that are extracted. It is assumed that the genes are having a
 * copy of the genome string, and hence the codons are deallocated too. 
 */
void freeGenes() {
	struct Gene *lg, *lnext;
	if (eg == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "No extracted genes struct!");
#endif
		return;
	}
	if (eg->genes == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "No extracted genes!");
#endif
		return;
	}
	
	lg = eg->genes;
	while (lg != NULL) {
		lnext = lg->next;
		free(lg->codons);
		free(lg);
		lg = lnext;
	}
	eg->gene_count = 0;
}

#ifdef WITH_TIME

/**
 * The genome generation occurs offboard, hence the random routines can use the random generator
 * that is aboard every operating system and the calloc system call.
 */
void generateGenome() {
	dna = lindaMalloc(sizeof(struct Genome));
	dna->content = lindaCalloc(gconf->genomeSize, sizeof(Codon));
	srand(time(NULL));
	uint16_t i;
	for (i = 0; i < gconf->genomeSize; i++) {
		dna->content[i] = rand();
	}
}
#else

/**
 * When a new genome is received, it is only necessary to create the dna structure itself,
 * the content will be provided by different means. The developer can choose to allocate
 * an entire array for the full-fledged genome content, or the developer can be more careful
 * and only set the content pointer to an incoming part of the genome. Extract the genes
 * from this small part and then receive a next part. This is similar toe flashing only 
 * small parts of a binary instead of the entire binary at one. 
 */
void receiveNewGenome() {
	dna = lindaMalloc(sizeof(struct Genome));
}

#endif

void printCodonGene(union CodonGene *codon, uint8_t verbosity) {
	char textV[128]; sprintf(textV, "%i: [%i->%i], @[%i,%i], +%i {%i-%i}", 
			codon->DeviceToken,
			codon->ProductIn,
			codon->ProductOut,
			codon->LocationOut_X,
			codon->LocationOut_Y,
			codon->conc_inc,
			codon->conc_low,
			codon->conc_high);
	tprintf(verbosity, __func__, textV);
}

#ifdef WITH_TEST
/**
 * In the Genome there are regions that code for genes and regions that do not. Not each time the
 * entire genome should be iterated to search for genes, but only after mutations have occured.
 * This routine generates a linked list of genes. Hence, on the actual robot, with offboard
 * evolution, this might only need to be done once. Or even offboard, sending over only the
 * extracted genes to the robot.
 */
void extractGenes(uint16_t genomeSize) {
	g = NULL; //should already be NULL if there are never genes extracted before
	uint16_t i = 0;
	eg = lindaMalloc(sizeof(struct ExtractedGenome));
	eg->genes = NULL;
	do {
		if (!(dna->content[i] % 10)) { //found a gene!
			if (g == NULL) {
				eg->genes = (g = lindaMalloc(sizeof(struct Gene)));
			} else {
				g = (g->next = lindaMalloc(sizeof(struct Gene)));
			}
#ifdef WITH_CONSOLE
			tprintf(LOG_VVVV, __func__, "New gene");
#endif
//			g->codons = (union CodonGene*)&dna->content[i];
			g->codons = malloc(sizeof(union CodonGene));
			uint8_t j; for (j = 0; j < 8; j++) g->codons->content[j] = dna->content[i+j];
			g->next = NULL;
			i+=7;
		}
		i++;
	} while (i < genomeSize-7);
}
#endif

/**
 * On a resource constrained defined it doesn't make sense to first store the entire genome
 * in its raw format and only then start extracting it. This can be done on the fly when
 * parts of the genome are received. The disadvantage of this is that new memory has to be
 * allocated for each gene, instead of just pointing to each gene in the raw genome structure.
 *
 * There should be taken care of the ends of the genomes,so a gene that is send in two parts
 * is properly recognized. For that reason the function returns the remaining 1 till 7 items
 * that yet have to be processed, and copies it to the start of the buffer.
 */
int16_t stepGeneExtraction(uint16_t buffer_size) {
	char text[64]; sprintf(text, "Gene extraction from buffer with size %i", buffer_size);
	tprintf(LOG_VV, __func__, text);
	uint16_t i = 0; int16_t j;
	do {
		if (!(dna->content[i] % 10)) { //found a gene!
			if (g == NULL) {
				eg->genes = (g = lindaMalloc(sizeof(struct Gene)));
			} else {
				g = (g->next = lindaMalloc(sizeof(struct Gene)));
			}
			char text[64]; sprintf(text, "New gene at position %i", i);
			tprintf(LOG_VVV, __func__, text);
			g->codons = malloc(sizeof(union CodonGene));
			for (j = 0; j < 8; j++) {
				g->codons->content[j] = dna->content[i+j];
			}
			g->next = NULL;
			printCodonGene(g->codons, LOG_VVV);
			i+=7;
		}
		i++;
	} while (i < buffer_size-8);

	tprintf(LOG_VVV, __func__, "Copy last to first");
	//copy last values of buffer to the start of the buffer
	j = 0;
	do {
		dna->content[j] = dna->content[i];
		i++; j++;
	} while (i < buffer_size-1);
	return j;
}

/**
 * Should be called before stepGeneExtraction. It only allocates the extracted genome struct
 * and sets the linked list of to be extracted genes to null.
 */
void initGeneExtraction() {
	g = NULL;
	eg = lindaMalloc(sizeof(struct ExtractedGenome));
	eg->genes = NULL;
	eg->gene_count = 0;
}

uint8_t normalize(uint8_t value, uint8_t bins) {
	return ((uint16_t)(value+1) * bins) / (256 + 1);
}


/**
 * The genes are in their "raw" format when they are just extracted from the genome. The
 * translation or transcription to actual values, or the interpretation of codons to symbols
 * is done in this function. So, this function establishes quite directly the evolvability
 * of the system itself. The "start" codon that distinguishes coding from non-coding parts
 * of the dna is reused over here to retrieve a device token that might code for different
 * types of operations. For now it is disregarded and everything codes for neural cellular
 * operations. The ProductIn should be a regulatingFactor id ranging from [phF...phF+regF],
 * and the ProductOut should be a random id from [0...phF+regF]. The locations are normalized
 * to grid sizes, the concentrations to 0...100% values and the concentration delta to a
 * 0...20% value.
 *
 * If the ProductIn is the same as the ProductOut the gene is removed from the pool.
 */
void transcribeGenes() {
	g = eg->genes; struct Gene *lgprev = NULL;
	
	if (gconf == NULL) {
		tprintf(LOG_EMERG, __func__, "Struct gconf not initialized!");
	} else if (s == NULL) {
		tprintf(LOG_EMERG, __func__, "Struct s not initialized!");
		tprintf(LOG_EMERG, __func__, "If initEvolution is not called, remember to manually call configGrid.");
	}
	
	while (g != NULL) {
		if (g->codons == NULL) {
#ifdef WITH_CONSOLE
			tprintf(LOG_ALERT, __func__, "Wrong format for genes");
#endif
			g = NULL;
			break;
		}
		
		g->codons->DeviceToken /= 10; //now DeviceToken's range from 0..25
		g->codons->ProductIn = normalize(g->codons->ProductIn, gconf->regulatingFactors) + gconf->phenotypicFactors;
		g->codons->ProductOut = normalize(g->codons->ProductOut, gconf->regulatingFactors + gconf->phenotypicFactors);
		g->codons->LocationOut_X = normalize(g->codons->LocationOut_X, s->columns);
		g->codons->LocationOut_Y = normalize(g->codons->LocationOut_Y, s->rows);
		g->codons->conc_inc = normalize(g->codons->conc_inc, 11) + 10; //from 10-20
		g->codons->conc_low = normalize(g->codons->conc_low, 101);
		g->codons->conc_high = normalize(g->codons->conc_high, 101);
#ifdef WITH_CONSOLE
		printCodonGene(g->codons, LOG_VVV);
#endif

		//remove gene if self-enforcing
		if (g->codons->ProductIn == g->codons->ProductOut) {
			if (lgprev == NULL) eg->genes = g->next;
			else lgprev->next = g->next;
		} else {
			eg->gene_count++;
		}
		lgprev = g;
		g = g->next;
	}
}

/**
 * From a series of products that a grid cell hosts, the product with the given id is returned.
 */
struct Product *getProduct(struct ProductId *id) {
	struct Product *p = gc->products;
	while (p != NULL) {
		if (p->id[0] == id->id[0])
			//			if (p->id[1] == id->id[1])
			//				if (p->id[2] == id->id[2])
			return p;
		p = p->next;
	}
	return NULL;
}


/***********************************************************************************************
 *
 * @name gene2cell
 * The genome works upon multiple grid cells, but over here only it operates only on one grid
 * cell.
 *
 ***********************************************************************************************/

/**
 * Stores the change in concentration of a gene product in a grid cell. The amount might be
 * positive or negative. Precede this calculation by calling precalculateChangeConcentration
 * with "p->concentration".
 */
void precalculateChangeConcentration(struct Product *p, int8_t amount) {
	if (p == NULL) return; //add product?
	int16_t sum = (int16_t)p->new_concentration + amount;
	if (sum > 100) p->new_concentration = 100;
	else if (sum < 0) p->new_concentration = 0;
	else p->new_concentration = (uint8_t)sum;
}

/**
 * Changes the concentration of a gene product in a grid cell. The amount might be positive or
 * negative. The borders of the type of p->concentration are respected (0 - 100).
 */
void changeConcentration(struct Product *p, int8_t amount) {
	if (p == NULL) return; //add product?
	int16_t sum = (int16_t)p->concentration + amount;
	if (sum > 100) p->concentration = 100;
	else if (sum < 0) p->concentration = 0;
	else p->concentration = (uint8_t)sum;
}

/**
 * Current gene is given by global parameter "g", the current grid cell by "gc". The concentration in
 * the grid cell is updated using the getProduct function on the arguments given by the gene parameter.
 * This way this function does not need to know if it is living in e.g. a 5x5 squared 2D grid. It
 * does neither need to know how to iterate through the genes.
 */
void updateConcentration() {
	struct Product *p_in = getProduct((struct ProductId*)&g->codons->ProductIn);
	struct Product *p_out = getProduct((struct ProductId*)&g->codons->ProductOut);

#ifdef WITH_CONSOLE
	if (p_in == NULL) {
		tprintf(LOG_ALERT, __func__, "ProductIn not found!");
		printCodonGene(g->codons, LOG_ALERT);
		tprintf(LOG_ALERT, __func__, "This might be because not all genes are interpreted");
		tprintf(LOG_ALERT, __func__, "That might happen if parts of the genome arrive after development has started");
	}
	if (p_out == NULL) {
		tprintf(LOG_ALERT, __func__, "ProductOut not found!");
		tprintf(LOG_ALERT, __func__, "See error about ProductIn message, comes never alone...");
	}
#endif

#ifdef WITH_CONSOLE
	char text[64]; sprintf(text, "%i ? E [%i ... %i]",
			p_in->concentration, g->codons->conc_low, g->codons->conc_high);
	tprintf(LOG_VVVV, __func__, text);
#endif

	if (g->codons->conc_low < g->codons->conc_high) {
		if ((p_in->concentration > g->codons->conc_low) && (p_in->concentration < g->codons->conc_high)) {
			changeConcentration(p_out, g->codons->conc_inc);
			//			tprintf(LOG_VVVV, __func__, "Plus");
		} else if ((p_in->concentration > 0) && (p_in->concentration < 10)) {
			changeConcentration(p_out, -g->codons->conc_inc);
			//			tprintf(LOG_VVVV, __func__, "Minus");
		}
	} else {
		if ((p_in->concentration > g->codons->conc_high) && (p_in->concentration < g->codons->conc_low)) {
			changeConcentration(p_out, -g->codons->conc_inc);
			//			tprintf(LOG_VVVV, __func__, "Minus");
		} else if ((p_in->concentration > 0) && (p_in->concentration < 10)) {
			changeConcentration(p_out, g->codons->conc_inc);
			//#ifdef WITH_CONSOLE
			//			char text[64]; sprintf(text, "New concentration %i (d[%i]) @[%i,%i]",
			//					p_out->concentration, g->codons->conc_inc, gc->position->x, gc->position->y);
			//			tprintf(LOG_VVVV, __func__, text);
			//#endif
		}
	}
}

#ifdef WITH_CONSOLE

/***********************************************************************************************
 *
 * @name gene_printing
 * The printing routines for genes and genomes.
 *
 ***********************************************************************************************/

/**
 * Prints the genes from the "eg" global variable. They have to be extracted before.
 * By default two genes are printed on one line, each with 8 items, called codons.
 * Each codon is the size of a char. The genes are indexed. Compare with printGenome
 * to check if the extraction of genes was appropriate.
 */
void printGenes() {
	g = eg->genes;
	uint16_t i = 0, j;
	while (g != NULL) {
		if (!(i % 2)) printf("\n%3i: ", i);
		printf("[");
		for (j = 0; j < 8; j++) {
			printf("%3i", g->codons->content[j]);
			if (j != 7) printf(", ");
			else printf("] ");
		}
		g = g->next;
		i++;
	}
	printf("\n");
}

const char genome_print_columns = 1; 

/**
 * Prints the genes to a string, however, the string is probably from a fixed size, 
 * so that's why a second parameter is added. Before this value is reached, taking into
 * account that [xxx, ..., xxx] (size 8*5+1) can follow, the routine quits. It returns
 * the number of the next gene that might be printed. If everything is printed, it returns
 * the total number of genes.
 */
uint16_t printGenesToStr(char *str, uint16_t length) {
	g = eg->genes;
	uint16_t i = 0, j;
	while (g != NULL) {
		if (!(i % genome_print_columns)) sprintf(str, "%s\n%3i: ", str, i);
		if (strlen(str) > (length - (8*5+1))) {
			sprintf(str, "%s\n", str);
			return i;
		}
		sprintf(str, "%s[", str);
		for (j = 0; j < 8; j++) {
			sprintf(str, "%s%3i", str, g->codons->content[j]);
			if (j != 7) sprintf(str, "%s, ", str);
			else sprintf(str, "%s] ", str);
		}
		g = g->next;
		i++;
	}
	sprintf(str, "%s\n", str);
	return i;
}

/**
 * The amount of genes per gene product is equally distributed if the genome is big enough.
 * The amount of "inhibiting" and "exciting" gene products should have a ratio of 1:1 for
 * all products.
 */
void printGenesOfProduct(uint8_t productId) {
	g = eg->genes;
	uint16_t i = 0, j;
	while (g != NULL) {
		if (g->codons->ProductOut == productId) {
			if (!(i % 2)) printf("\n%3i: ", i);
			printf("[");
			for (j = 0; j < 8; j++) {
				printf("%3i", g->codons->content[j]);
				if (j != 7) printf(", ");
				else printf("] ");
			}
			i++;
		}
		g = g->next;
	}
	printf("\n");
}

/**
 * Prints the amount of genes dedicated to a certain gene product.
 */
void printGenesPerProductDistribution() {
	uint8_t arr_size = gconf->phenotypicFactors + gconf->regulatingFactors;
	uint8_t *dist = lindaCalloc(arr_size, sizeof(uint8_t)), j;
	tprintf(LOG_VERBOSE, __func__, "Print genes per product distribution");
	for (j = 0; j < arr_size; j++) {
		g = eg->genes;
		while (g != NULL) {
			if (g->codons->ProductOut == j)
				dist[j] = dist[j] + 1;
			g = g->next;
		}
	}
	printf("[");
	for (j = 0; j < arr_size; j++) {
		printf("%i", dist[j]);
		if (j < arr_size - 1) printf(", ");
	}
	printf("]\n");
	free(dist);
}


#endif

