/**
 * The testGenome suite:
 * 
 *  calls generateGenome() and prints it out. 
 */

//#define TEST_GENOME

#ifdef TEST_GENOME

#define WITH_CONSOLE

#define WITH_TEST

#include <stdio.h>
#include <genomes.h>
#include <genome.h>
#include <syslog.h>
#include <pthread.h>

#include <grid.h>

#include <linda/log.h>
#include <linda/ptreaty.h>
#include <linda/abbey.h>

/**
 * The default value for amount of transcription factors is 43, the amount of factors that cause
 * phenotypic transformation is 23. The grid size is by default 5 by 5. Replace those numbers by
 * your own, if you don't use the default ones. Concentrations range from 0.00 to 1.00, so from
 * 0 till 100 (not to 100-1). 
 */
uint16_t overwriteGenome(uint8_t test) {
	uint16_t i;
	char text[64]; 
	sprintf(text, "Test %i", test);
	tprintf(LOG_INFO, __func__, text);
	switch(test) {
	case 0:
		//test if genes are not mixed up when there are two markers in one gene 
		//should result in 1 gene, with values [0, 43-1, 23-1, 5-1, 5-1, 100, 100, 0]. 
		for (i = 0; i < gsconf->genomeSize; i++) {
			dna->content[i] = 0xFF;
			if (i == 0) dna->content[i] = 0;
			if (i == 7) dna->content[i] = 0;
		}
		return 1;
	case 1:
		//should result in 2 genes, with values [0, 43-1, 23-1, 5-1, 5-1, 100, 100, 100].
		for (i = 0; i < gsconf->genomeSize; i++) {
			dna->content[i] = 0xFF;
			if (i == 0) dna->content[i] = 0;
			if (i == 8) dna->content[i] = 0;
		}
		return 2;
	case 2:
		//should result in gconf->genomeSize/8 genes, with values [0, 23, 0, 0, 0, 0, 0, 0].
		for (i = 0; i < gsconf->genomeSize; i++) {
			dna->content[i] = 0x0;
		}
		return ((uint16_t)gsconf->genomeSize) / 8;
	case 3:
		//test if data beyond genome is not read as if it were a gene
		//should result in 0 genes
		for (i = 0; i < gsconf->genomeSize; i++) {
			dna->content[i] = 0xFF;
			if (i == gsconf->genomeSize - 7) dna->content[i] = 0;
		}
		return 0;
	case 4:
		//test if data right at the end of genome is properly read as a gene
		//should result in 1 gene 
		for (i = 0; i < gsconf->genomeSize; i++) {
			dna->content[i] = 0xFF;
			if (i == gsconf->genomeSize - 8) dna->content[i] = 0;
		}
		return 1;
	}	
	return 0xFF;
}

uint16_t countGenes() {
	g = eg->genes; uint16_t result = 0; 
	while (g != NULL) {
		result++;
		g = g->next;
	}
	return result;
}

void *random_task(void *context) {
	uint8_t i;
	for (i = 0; i < 1000; i++) { ; }
	return NULL;
}

int main() {
	openlog ("tlinda", LOG_CONS, LOG_LOCAL0);
	initLog(LOG_INFO);
	pthread_t this = pthread_self();
	ptreaty_add_thread(&this, "Main");
	tprintf(LOG_NOTICE, __func__, "Start Tlinda - Test Genome");

	tprintf(LOG_INFO, __func__, "Initialize genomes.h");
	initGenomes();
	gsconf->genomeSize = 100;
		
	tprintf(LOG_VERBOSE, __func__, "Initialize genome.h");
	receiveNewGenome();
	configGenome();

	tprintf(LOG_VERBOSE, __func__, "Generate genome");
	struct RawGenome *rawdna = generateGenome();
	
	tprintf(LOG_VERBOSE, __func__, "Copy to colinda genome");
	dna->content = rawdna->content;

	tprintf(LOG_VERBOSE, __func__, "Print genome in sets of chars");
	printGenome(dna);

	printf("\n\nA gene occupies a total of 8 characters. The amount of chars in total is %i.\n", gsconf->genomeSize);
	printf("The amount of genes is defined later on by the extraction mechanism.\n");

	tprintf(LOG_VERBOSE, __func__, "Actual gene extraction");
	extractGenes(gsconf->genomeSize);
	printGenes();

	tprintf(LOG_VERBOSE, __func__, "Transcribe genes");
	configGrid();
	transcribeGenes();

	printf("\n\n== The gene mapped to actual symbol values ==\n");
	printGenes();
	
	printf("\n\n== Some automated tests ==\n\n");

	uint16_t geneCountA, geneCountB, i;
	uint8_t nofTests = 5; 
	for (i = 0; i < nofTests; i++) {
		geneCountA = overwriteGenome(i);
		char text[64];
		sprintf(text, "Expected result %i", geneCountA);
		tprintf(LOG_DEBUG, __func__, text);
		extractGenes(gsconf->genomeSize);
		tprintf(LOG_DEBUG, __func__, "Genes extracted");
		geneCountB = countGenes();
		char result[128];
		sprintf(result, "Test [%i]:", i);
		if (geneCountA != geneCountB) {
			sprintf(result, "%s Error!", result);
			tprintf(LOG_INFO, __func__, result);
			char errmsg[128]; sprintf(errmsg, "Gene count differs: %i vs %i genes found!!\n", geneCountA, geneCountB);
			tprintf(LOG_ERR, __func__, errmsg); 
			printf("The genome:");
			printGenome(dna);
			printf("\nThe (wrong) extraction of genes");
			printGenes();
			printf("\n");
		} else {
			sprintf(result, "%s Correct!", result);
			tprintf(LOG_INFO, __func__, result);
		}
	}

	printf("\n== End of tests ==\n");

	printf("\n\n");
	return 0;

}

#endif

