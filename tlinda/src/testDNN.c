/**
 * @file testDNN.c
 * @brief Test file for an NN developed through embryogeny.  
 * @author Anne C. van Rossum

 * After the evolutionary phase, in which the end product is a series of extracted genes,
 * the embryogeny takes over and transforms this series of genes to a topological network
 * of - currently - neurons. This file tests if actual cellular instructions might lead
 * to different topologies and can be seen as the successor of testEmbryogeny.c.
 * 
 * A test genome is used (a random genome which can be written to a file and reused multiple
 * times). Basically, what this test unit does, is check the results of the applyEmbryogenesis 
 * routine. So, instead of the testEmbryogeny unit, it sees upon the type of cellular 
 * instructions that are generated. 
 * 
 * @todo Automation of checking if a diversity of genomes does result in a diversity in
 * cellular instructions. For now this needs to be checked manually and worse, diversity in
 * cellular instructions doesn't seem to occur yet. The distribution of cellular instructions
 * seems to be skewed to certain operations.
 */

#define TEST_DNN

#ifdef TEST_DNN

#include <lindaconfig.h>
#include <genome.h>
#include <genomes.h>
#include <stdio.h>
#include <grid.h>
#include <embryogeny.h>
#include <topology.h>

#include <linda/log.h>
#include <linda/ptreaty.h>
#include <linda/abbey.h>
#include <linda/gnuplot_i.h>

/**
 * To overwrite the existing genome file (say genome.text) enable the OVERWRITE_GENOME macro. 
 */
#define OVERWRITE_GENOME

/**
 * The test procedure is to use random genomes. If there is a problem, copy the genome.text
 * file and run this genome all the time (undefining OVERWRITE_GENOME) and then check certain
 * specific sequences of instructions.
 */
//#define TEST_SEPARATE_INSTRUCTIONS

/**
 * This routine stores the genome in a text-file. It is not human-readable, because every
 * byte is written as an ASCII symbol. It looks really random. :-)
 */
void storeGenome() {
	uint16_t i;
	FILE* f1 = fopen ("genome.text", "wt");
	for (i = 0; i < gsconf->genomeSize; i++) {
		//fprintf(f1, "%i\n", dna->content[i]);
		fputc(dna->content[i], f1);
		//		write(f1, line, strlen(line));
	}
	fclose(f1);
}

/**
 * This routine reads the genome from a text-file that is stored previously by using
 * storeGenome. It is expected to be of the size defined in gconf. There are no checks on the
 * file itself.
 */
void readGenome() {
	uint16_t i = 0; uint8_t value; FILE* f1;
	if((f1 = fopen("genome.text", "r"))==NULL) {
		printf("Cannot open file.\n");
		exit(1);
	}
	dna = malloc(sizeof(struct Genome));
	dna->content = calloc(gsconf->genomeSize, sizeof(Codon));

	for(i = 0; i < gsconf->genomeSize; i++) {
		value = fgetc(f1);
		//@todo		if(value == EOF) break;
		dna->content[i] = value;
	}
	fclose(f1);

}

/**
 * Tests one cycle, this means that a new cycle should only be started if the data structures
 * are properly deallocated. Hence, this is a test for that...
 */
void newcycle() {
	tprintf(LOG_VERBOSE, __func__, "Extract genes");
	extractGenes(gsconf->genomeSize);

	tprintf(LOG_VERBOSE, __func__, "Transcribe genes");
	configGrid();
	transcribeGenes();

	init_embryology();
	start_embryology();
	initConcentrations();

	tprintf(LOG_NOTICE, __func__, "Initial grid layout");
	printGrid();
	printf("\n");

	//	printGenesOfProduct(0);
	printGenesPerProductDistribution();

#ifdef TEST_SEPARATE_INSTRUCTIONS 
	gc = getGridCell(1,1);
	tprintf(LOG_NOTICE, __func__, "Print for change");
	printNeurons(LOG_DEBUG);
	tprintf(LOG_NOTICE, __func__, "Split full");
	applyMorphologicalChange(11);
	printNeurons(LOG_NOTICE);
	tprintf(LOG_NOTICE, __func__, "And now remove!");
	applyMorphologicalChange(17);
	printNeurons(LOG_NOTICE);
	tprintf(LOG_NOTICE, __func__, "And split/copy sparse");
	gc = getGridCell(2,1);
	applyMorphologicalChange(10);
	//	printNeurons(LOG_NOTICE);
	//	tprintf(LOG_NOTICE, __func__, "And split/copy full");
	//	gc = getGridCell(2,1);
	//	applyMorphologicalChange(11);
	printNeurons(LOG_NOTICE);
	tprintf(LOG_NOTICE, __func__, "And remove");
	applyMorphologicalChange(17);
	printNeurons(LOG_NOTICE);
	tprintf(LOG_NOTICE, __func__, "And remove again");
	gc = getGridCell(3,1);
	applyMorphologicalChange(17);
	printNeurons(LOG_NOTICE);
#else
	//	setVerbosity(LOG_NOTICE);
	tprintf(LOG_NOTICE, __func__, "Run GRN");
	uint16_t t = 0;
	do {
		updateGrid();
		//		tprintf(LOG_VVV, __func__, "Print concentrations");
		//		printAllConcentrationsMultiplePerRow();

		tprintf(LOG_VVV, __func__, "Apply morphological changes");
		applyEmbryogenesis();

		tprintf(LOG_VVV, __func__, "Draws figures (in file)");
		//		drawAllConcentrations(t);

		//		tprintf(LOG_VVV, __func__, "Draws figure for product 3 (in file)");
		//		drawConcentrations(3, t);
		t++;
		//		if (t == 140) setVerbosity(LOG_VERBOSE);

		if (!(t % 100)) {
			printDistribution(LOG_VERBOSE);
		}
	} while(t < 1000);
	if (!isPrinted(LOG_VVV)) printf("\n");

#endif

	tprintf(LOG_NOTICE, __func__, "Final grid layout");
	printGrid();
	printf("\n");
}

/**
 * This is stuff that is not necessarily deallocated in the colinda engine, so it's separated
 * from the freecycle() routine.
 */
void initcycle() {
	if (dna != NULL) {
		tprintf(LOG_VERBOSE, __func__, "Deallocate");
		free(dna->content);
		free(dna);
		receiveNewGenome();
	}

	tprintf(LOG_VERBOSE, __func__, "Initialize genome.h");
	receiveNewGenome();
	configGenome();

	tprintf(LOG_VERBOSE, __func__, "Set # of factors");
	gconf->regulatingFactors = 11; //20
	gconf->phenotypicFactors = 14; //23;

#ifdef OVERWRITE_GENOME
	tprintf(LOG_VERBOSE, __func__, "Generate genome");
	struct RawGenome *rawdna = generateGenome();
	tprintf(LOG_VERBOSE, __func__, "Copy to colinda genome");
	dna->content = rawdna->content;
	//	generateGenome();
	free(rawdna);
	storeGenome();
#else
	tprintf(LOG_VERBOSE, __func__, "Read genome from file");
	readGenome();
#endif
}

void freecycle() {
	tprintf(LOG_VERBOSE, __func__, "Free genes");
	freeGenes();
	tprintf(LOG_VERBOSE, __func__, "Genes freed");
}

/**
 * The main function of this test unit. It configures the genome and updates subsequently 
 * some genome configuration parameters. Then a random genome is generated or read from a 
 * file. The genes are extracted and transcribed / interpreted / normalized. The GRN is 
 * initialized by init routines for the embryology and grid (concentration) modules. For
 * testing purposes the grid is visualized, gene distribution across products printed and
 * for every iteration t, the concentration of products is visualized in .png files. At
 * the end of all iterations, the produced neural network topology is printed and the final
 * values of all gene product concentrations.
 */
int main() {
	openlog ("tlinda", LOG_CONS, LOG_LOCAL0);
	initLog(LOG_VERBOSE);
	pthread_t this = pthread_self();
	ptreaty_add_thread(&this, "Main");
	tprintf(LOG_NOTICE, __func__, "Start Tlinda - Test DNN");

	tprintf(LOG_INFO, __func__, "Initialize genomes.h");
	initGenomes();
	gsconf->genomeSize = 20000;
	dna = NULL;
	initGeneExtraction();
		
	uint8_t i;
	for (i = 0; i < 10; i++) {
		initcycle();
		newcycle();
		freecycle();
	}

	tprintf(LOG_NOTICE, __func__, "End DNN test");

	return 0;
}

#endif  //TEST_DNN
