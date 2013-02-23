/**
 * This file can be include by the epuck.cc file as well as by the actual code running on the
 * robot. This file doesn't know about the message formats as defined in the SymbricatorRTOS,
 * but it operates directly upon an array of unsigned chars. This module does know the
 * AER format.
 */
#include <lindaconfig.h>
#include <sensorimotor.h>
#include <aer.h>
#include <stdint.h>
#include <time.h>  //TODO: check

#ifdef WITH_CONSOLE
#include <stdio.h>
#include <linda/log.h>
#include <colinda.h>
#endif

#ifdef WITH_SYMBRICATOR
#include "portable.h"
#endif

#include <stdlib.h>
#include <embryogeny.h>
#include <bits.h>
#include <topology.h>
#include <neuron.h>
#include <genome.h>

#ifdef WITH_GNUPLOT
#include <testPlayerStageHelper.h>
#endif

time_t *prev_cycle = NULL;
uint8_t timeResolutionResolved = 0;
time_t *time_resolution = NULL;
uint8_t running_state = 0;

/**
 * A routine might be called several times, the time in between those calls might
 * be an indication for the load a routine may exert on the system. When the amount
 * of data or the resolution of the data has a relation with the frequency of the
 * call to a routine, it might be desired that it is kept constant the same for a
 * long time. A fully dynamic adjustment of the caller's output depending on the
 * local phase between calls, might impose too large demands on the receiving party
 * of the data. Hence, over here a boolean is set and the second(!) in-between time
 * between two function calls is used to set the so-called resolution.
 *
 * This function depends on state, first, second, third call:
 *   1. Returns NULL;
 *   2. Returns first indication of time_resolution that can be used by caller;
 *   3. Returns adapted time_resolution figure to be used by caller forever;
 *
 * In the subsequent phase caller's of this function might adapt their output on
 * the system, for example by adding more spikes, and subsequently inspect the
 * result in terms of time between routine calls.
 */
time_t *setTimeResolution() {
	if (timeResolutionResolved > 1)
		return time_resolution;
	time_t now = time(NULL);

	if (prev_cycle == NULL) {
		prev_cycle = (time_t*) lindaMalloc(sizeof(time_t));
		time_resolution = (time_t*) lindaMalloc(sizeof(time_t));
		*prev_cycle = now;
		return NULL;
	}

	*time_resolution = now - *prev_cycle;
	timeResolutionResolved++;
	*prev_cycle = now;
	return time_resolution;
}


/**
 * Adds an AER item to the buffer. The buffer is considered full if the head pointer
 * is pointing just one slot before the tail pointer (in a circular way). When the
 * buffer is full, the item is not added to the list and the function returns false.
 * On addition, the head pointer is updated to the next slot in the buffer.
 */
bool pushAER(struct AERBuffer *aerbuffer, union AER *aertuple) {
	uint8_t head_next = (aerbuffer->head + 1) % MAX_AER_TUPLES;
	if (head_next == aerbuffer->tail) return false; //buffer = full
	aerbuffer->aer[aerbuffer->head] = *aertuple;
	aerbuffer->head = head_next;
	return true;
}

/**
 * This routine is a copy of pushAER, but assumes that there is already an AER item
 * allocated on the next open slot in the AERBuffer. In that case only the given
 * data have to be assigned to that AER tuple, which saves allocation and deallocation
 * calls each time.
 */
uint8_t pushAER_xyt(struct AERBuffer *aerbuffer, uint8_t x, uint8_t y, uint16_t time) {
	uint8_t head_next = (aerbuffer->head + 1) % MAX_AER_TUPLES;
	if (head_next == aerbuffer->tail) return false; //buffer = full
	aerbuffer->aer[aerbuffer->head].coordinate.x = x;
	aerbuffer->aer[aerbuffer->head].coordinate.y = y;
	aerbuffer->aer[aerbuffer->head].event = time;
	aerbuffer->head = head_next;
	return 1;
}

void initAER(struct AERBuffer *aerbuffer) {
	uint8_t i;
	aerbuffer->head = aerbuffer->tail = 0;
	for (i = 0; i < MAX_AER_TUPLES-1; i++) {
		aerbuffer->aer[i].data = 0;
	}
}

/**
 * The buffer is considered empty when the tail and head pointer point to the same
 * slot in the buffer. On empty this routine returns 1.
 */
uint8_t isEmptyAER(struct AERBuffer *aerbuffer) {
	return (aerbuffer->tail == aerbuffer->head);
}

/**
 * Empties the buffer by popping tuples.
 */
void doEmptyAER(struct AERBuffer *aerbuffer) {
	union AER *aer = popAER(aerbuffer);
	while (aer != NULL) {
		aer = popAER(aerbuffer);
	}
}

/**
 * The buffer is considered full when the tail pointer finds the head pointer in the
 * next slot. This routine uses MAX_AER_TUPLES to know the size of the circular
 * AERBuffer.
 */
uint8_t isFullAER(struct AERBuffer *aerbuffer) {
	return (((aerbuffer->head + 1) % MAX_AER_TUPLES) == aerbuffer->tail);
}

/**
 * Returns an AER item from the buffer and update the tail pointer. It doesn't
 * allocate or deallocate anything. Just a pointer to the AER tuple is returned,
 * no copy is made.
 */
union AER *popAER(struct AERBuffer *aerbuffer) {
	if (aerbuffer->tail == aerbuffer->head) return NULL;
	union AER *result = &aerbuffer->aer[aerbuffer->tail];
	aerbuffer->tail = (aerbuffer->tail + 1) % MAX_AER_TUPLES;
	return result;
}

/**
 * Just empties the buffer by initializing all the pointers to 0.
 */
void emptyAERBuffer(struct AERBuffer *aerbuffer) {
	aerbuffer->tail = aerbuffer->head = 0;
}

/**
 * Counts the amount of spikes with coordinates x and y in the given AERBuffer. In the
 * buffer all items are iterated from the tail to the head pointer, even if the head
 * pointer has wrapped around the buffer. The buffer is considered empty if the tail
 * and head pointer point to the same item.
 */
uint8_t count_spikes(struct AERBuffer *b, uint8_t x, uint8_t y) {
	uint8_t i, amount = 0;
	if (b->head > b->tail) {
		for (i=b->tail; i < b->head; i++) {
			if ((b->aer[i].coordinate.x == x) && (b->aer[i].coordinate.y == y)) amount++;
		}
	} else if (b->head == b->tail) {

	} else {
		for (i=b->tail; i < MAX_AER_TUPLES; i++) {
			if ((b->aer[i].coordinate.x == x) && (b->aer[i].coordinate.y == y)) amount++;
		}
		for (i=0; i < b->head; i++) {
			if ((b->aer[i].coordinate.x == x) && (b->aer[i].coordinate.y == y)) amount++;
		}
	}
	return amount;
}

/**
 * Only one input sensor array might generate lots of AER tuples. This routine thus might
 * be called several times, to generate all spikes. This routine generateSpikes is called
 * with an additional "counter" parameter which is decremented till zero. When the counter
 * reaches zero no spikes are generated anymore. It might be the time to retrieve another
 * sensor value array from the simulator.
 *
 * The function returns:
 *   0. when nothing is executed because the time resolution is not set yet.
 * 	 1. when all spikes are generated successfully and added to the buffer.
 *   2. when the buffer is full.
 *   3. on error
 */
uint8_t generateSpikes(uint8_t *input, uint8_t inputbuf_size, struct AERBuffer *aerbuffer) {
	uint8_t result = 1; uint8_t i, j, spikecnt; time_t *resolution; time_t now;

	//printf("Start settr\n");
	resolution = setTimeResolution();
	if (resolution == NULL) return 0;

	now = time(NULL);
	*resolution /= 20;

	if (inputbuf_size < 3) {
#ifdef WITH_CONSOLE
		char msg[64];
		sprintf(msg, "Not enough input values (%i < 3)", inputbuf_size);
		tprintf(LOG_ALERT, __func__, msg);
#endif
		return 3;
	}
	//for (i=0; i < 8; i++) {
	for (i=0; i < 3; i+=2) {
		switch(input[i]) {
		case  0 ...  4: spikecnt = 10; break;
		case  5 ...  9: spikecnt = 9; break;
		case 10 ... 14: spikecnt = 8; break;
		case 15 ... 19: spikecnt = 7; break;
		case 20 ... 29: spikecnt = 6; break;
		case 30 ... 39: spikecnt = 5; break;
		case 40 ... 49: spikecnt = 4; break;
		case 50 ... 59: spikecnt = 3; break;
		case 60 ... 69: spikecnt = 2; break;
		case 70 ... 79: spikecnt = 0; break;
		default: spikecnt = 0;
		}
		for (j=0; j < spikecnt; j++) {
			result = result && pushAER_xyt(aerbuffer, i % 5, i / 5,
					(uint16_t)(now + (*resolution)*j));
		}
		if (!result) return 2;
	}
	return result;
}

#ifdef WITH_CONSOLE
/**
 * Prints the neural network;
 */
void printNetwork() {
	printf("Prints the neural network\n");
	struct Neuron *ln = nn->neurons; uint8_t i = 0;
	while (ln != NULL) {
		printf("Position neuron %d: [%d,%d]\n", i,
				ln->gridcell->position->x, ln->gridcell->position->y);
		i++;
		ln = ln->next;
	}
	printf("\n");
}
#endif

struct Synapse* existConnection(struct Neuron *src, struct Neuron *target) {
	struct Port *lp = src->ports_out;
	while (lp != NULL) {
		if (lp->synapse->post_neuron == target) {
			return lp->synapse;
		}
		lp = lp->next;
	}
	return NULL;
}

#ifdef WITH_CONSOLE

void printConnections() {
	struct Neuron *ln_src, *ln_tar;
	uint8_t x_src,y_src,x_tar,y_tar;
	printf("Conn:  ");
	for (y_src=0; y_src < s->rows; y_src++) {
		for (x_src=0;x_src < s->columns; x_src++) {
			printf("%d-%d ", x_src, y_src);
		}
	}
	printf("\n       ");
	for (y_src=0; y_src < s->columns * s->rows; y_src++) printf("----");
	printf("\n");

	for (y_src=0; y_src < s->rows; y_src++) {
		for (x_src=0;x_src < s->columns; x_src++) {
			printf(" %d-%d  |", x_src, y_src);

			ln_src = getGridCell(x_src,y_src)->neuron;

			if (ln_src != NULL) {
				for (y_tar=0; y_tar < s->rows; y_tar++) {
					for (x_tar=0;x_tar < s->columns; x_tar++) {
						ln_tar = getGridCell(x_tar,y_tar)->neuron;
						struct Synapse *ls = existConnection(ln_src, ln_tar);
						if (ls != NULL) {
							printf("%1.1f ", ls->weight);
						} else {
							printf("    ");
						}
					}
				}
			}
			printf("\n");
		}
	}
}
#endif

#ifdef WITH_CONSOLE

void printCurrents() {
	printf("Prints the input currents of neurons in the neural network\n");
	struct Neuron *ln = nn->neurons;
	while (ln != NULL) {
		printf("Current neuron [%d,%d]: %f\n",
				ln->gridcell->position->x, ln->gridcell->position->y,
				ln->I);
		ln = ln->next;
	}
	printf("\n");
}
#endif

/**
 * This development of a neural network starts with the configuration of the genome. It
 * overwrite the settings of the amount of regulating and phenotypic factors. On the robot
 * it may be decided to send the actual topology instead of a genome to the robot. In that
 * case this function can be skipped and it's better to create the grid directly via other
 * means.
 *
 * The routine expects the genes to be already extracted, and starts transcribing genes
 * (scaling the different values to proper ranges). After the genes are transcribed, it
 * starts the embryogenetic process. For t=0...1000 concentrations are tested and updated
 * and cellular instructions executed if appropriate. After this process, the grid is
 * filled with the neural network and might be printed.
 */
void developNeuralNetwork() {
	if (gconf != NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_VERBOSE, __func__, "Deallocated everything");
#endif
		freeGenome();
		free_embryology();
#ifdef WITH_CONSOLE
		tprintf(LOG_VERBOSE, __func__, "Everything deallocated");
#endif
	}
	
	configGenome();
	init_embryology();
		
//	gconf->regulatingFactors = 4;
//	gconf->phenotypicFactors = 4;
	transcribeGenes();

#ifdef WITH_CONSOLE
	if (eg->gene_count < 10) {
		tprintf(LOG_VERBOSE, __func__, "Print interpreted/transcribed genes");
		//	printf("Legend: [DeviceToken ProductIn ProductOut GridX GridY conc_inc conc_low conc_high]");
		char textA[1024];
		printGenesToStr(textA, 1024);
		btprintf(LOG_VERBOSE, __func__, textA);
		tprintf(LOG_VERBOSE, __func__, "Init embryology");
	} else {
		tprintf(LOG_VERBOSE, __func__, "Genome too big (>10 genes) to be printed");
	}
#endif

	start_embryology();
	initConcentrations();

#ifdef WITH_CONSOLE
	//	printGrid();
	tprintf(LOG_DEBUG, __func__, "Run GRN");
#endif

	uint16_t t = 0;
	do {
		updateGrid();
#ifdef WITH_CONSOLE
		if (t == 0)
		tprintf(LOG_VERBOSE, __func__, "Apply morphological changes");
#endif
		applyEmbryogenesis();
#ifdef WITH_CONSOLE
		if (t == 0)
		tprintf(LOG_VERBOSE, __func__, "First cycle passed");
#endif
#ifdef WITH_GUI
//		if (!(t % 100)) visualizeCells();
#endif
		t++;
	} while(t < 1000);

#ifdef WITH_GUI
	visualizeCells();
#endif

#ifdef WITH_CONSOLE
	char text[128]; sprintf(text, "The resulting topology for robot %i", clconf->id);
	tprintf(LOG_DEBUG, __func__, text);
	char text1[1024]; 
	printGridToStr(text1);
	btprintf(LOG_DEBUG, __func__, text1);
#endif
}

/**
 * This routine calls all the Linda Engine API functions like propagateSpikes, calculateWeights,
 * etc. It might be called again from the SymbricatorRTOS or from the (extended) hardware
 * simulator.
 *
 * Currently, the runNeuralNetwork routine is divided into several stages. This makes it
 * possible to visualize the network on a state-transition. The result value of this
 * routine will indicate the next stage. With a return value of 0, everything is handled
 * and a new input buffer with AER tuples is expected and inspected.
 */
uint8_t runNeuralNetwork(struct AERBuffer *in, struct AERBuffer *out) {
	union AER *aer;

	switch(running_state) {
	case 0:
		aer = popAER(in);
		while (aer != NULL) {
			struct GridCell *lgc = getGridCell(aer->coordinate.x, aer->coordinate.y);
			if (lgc != NULL) {
				if (lgc->neuron != NULL) {
					n = lgc->neuron;
					ADVANCE(n->history->spike_bitseq);
					RAISE(n->history->spike_bitseq, 1);
				}
			}
			aer = popAER(in);
		}
#ifdef WITH_CONSOLE
		tprintf(LOG_VVV, __func__, "Propagate spikes");
#endif
		propagateSpikes();
		break;

	case 1:
#ifdef WITH_CONSOLE
		tprintf(LOG_VVV, __func__, "Update neurons");
#endif
		updateNeurons();
		getSpikes();

#ifdef WITH_CONSOLE
		//printCurrents();
		tprintf(LOG_VVV, __func__, "Push aer tuples");
#endif
		//read output neurons
		struct GridCell *lgc = s->gridcells;
		uint8_t size = s->columns * s->rows, i = 0;
		while ((lgc != NULL) & (i < size)) {
			if (lgc->neuron != NULL) {
				if ((lgc->neuron->type & TOPOLOGY_MASK) == OUTPUT_NEURON) {
					n = lgc->neuron;
					if (RAISED(n->history->spike_bitseq, 1)) {
						pushAER_xyt(out, n->gridcell->position->x,
								n->gridcell->position->y, 0);
					}
				}
			}
			lgc = lgc->next;
			i++;
		}

		break;
	}
	running_state++;
	running_state %= 2;
	return running_state;
}

/**
 * The output in this case is just velocities for the two wheels. It gets from the union AER buffer
 * exactly spike_cnt values and adapts the velocity according to the amount of spikes by
 * the two output neurons. It expects the neuron coordinates to be [3,3] and [5,3]. The spike_cnt
 * variables is set to 0 by this function itself.
 */
void interpretSpikes(struct AERBuffer *b, int16_t *output) {
	output[1] = 20 * (count_spikes(b, 3, 3)) - 20 * count_spikes(b, 1, 3) + 10;
	output[0] = 20 * (count_spikes(b, 4, 3)) - 20 * count_spikes(b, 2, 3) + 10;


	//empty buffer
	doEmptyAER(b);
}
