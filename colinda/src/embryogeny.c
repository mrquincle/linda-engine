/**
 * @file embryogeny.c
 * @brief Embryogeny as a gene regulatory network, according to Bongard.
 * @author Anne C. van Rossum
 */

#include <lindaconfig.h>
#include <stdlib.h>
#include <grid.h>
#include <topology.h>
#include <embryogeny.h>
#include <neuron.h>
#include <genome.h>

#include <bits.h>

#ifdef WITH_SYMBRICATOR
#include "portable.h"
#endif

#ifdef WITH_CONSOLE
#include <linda/log.h>
#include <stdio.h>
#endif

/****************************************************************************************************
 *  		Declarations
 ***************************************************************************************************/

/**
 * The following list is a series of routines that are all morphological operations that can be
 * executed by phenotypical products in the GRN. Those are not accessible by other units, they
 * can only execute those instructions by calling applyMorphologicalChange using a char as
 * index.
 */

void splitSparse();
void splitFull();
void moveNeuronNorth();
void moveNeuronWest();
void moveNeuronSouth();
void moveNeuronEast();
void moveSynapseNorth();
void moveSynapseWest();
void moveSynapseSouth();
void moveSynapseEast();
void incrementWeight();
void decrementWeight();
void removeSynapse();
void removeNeuron();
void nextSynapse();
void changeType ();
void changeSign();
void changeTopologicalType();

#ifdef WITH_PRINT_DISTRIBUTION
void initPrintDistribution();
uint16_t *distribution;
#endif

/****************************************************************************************************
 *  		Implementations
 ***************************************************************************************************/

/**
 * The initialization process writes default values into the embryogeny structure. It
 * initializes also the grid, so, when testing the grid separately be aware that the
 * grid has to be initialized manually.
 * 
 * In the default setting, init_embryology is called from developNeuralNetwork() in the
 * sensorimotor file. From there, free_embryology() is evoked after a new network has
 * to be developped.
 */
void init_embryology() {
	e = lindaMalloc(sizeof(struct Embryogeny));
	e->default_weight = 6;
	e->default_delay = 1;
	nn = lindaMalloc(sizeof(struct NN));

	configGrid();
	initGrid();

#ifdef WITH_PRINT_DISTRIBUTION
	initPrintDistribution();
#endif
}

/**
 * If the embryology constructs are deallocated before the gridcell, it is possible to
 * use the cellular instructions to free the memory, like removeNeuron. This might not
 * seem to be a big deal, but a neuron is connected to other neurons by synapses etc.
 * So it is quite easy if this is reused for deallocation purposes. 
 * 
 * The grid is allocated by init_embryology, for which reason it is also logical to
 * deallocate it in free_embryology. 
 */
void free_embryology() {
	gc = s->gridcells;
	do {
		if (gc->neuron != NULL) {
			removeNeuron();
		}
		gc = gc->next;
	} while (gc != s->gridcells);

#ifdef WITH_PRINT_DISTRIBUTION
	free(distribution);
#endif

	freeGrid();	
	free(nn);
	free(e);
}

/**
 * Creates an initial neural network on position [2,2] and [4,4], the first one is a sensor neuron,
 * the latter an actuator neuron. There is one synapse between those two neurons, from the sensor
 * neuron to the actuator neuron. The current neuron pointer used for embryogeny starts at position
 * [2,2].
 */
void start_embryology() {
	//neurons
	np = nn->neurons = lindaMalloc(sizeof(struct Neuron));
	np->next = NULL; np->ports_in = NULL; np->ports_out = NULL;
	np->history = lindaMalloc(sizeof(struct SpikeHistory));
	np->next = lindaMalloc(sizeof(struct Neuron));
	np->next->next = NULL; np->next->ports_in = NULL; np->next->ports_out = NULL;
	np->next->history = lindaMalloc(sizeof(struct SpikeHistory));
	(np->gridcell = getGridCell(1,1))->neuron = np;
	(np->next->gridcell = getGridCell(3,3))->neuron = np->next;

	//synapse
	struct Synapse *lsp = lindaMalloc(sizeof(struct Synapse));
	lsp->pre_neuron = np;
	lsp->post_neuron = np->next;
	lsp->weight = e->default_weight;
	lsp->delay = e->default_delay;

	//ports
	np->ports_out = lindaMalloc(sizeof(struct Port));
	np->ports_out->next = NULL;
	np->next->ports_in = lindaMalloc(sizeof(struct Port));
	np->next->ports_in->next = NULL;
	np->next->ports_in->synapse = np->ports_out->synapse = lsp;
	np->current_port = np->ports_out;
	np->next->current_port = np->next->ports_in;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Created np->ports_out on [%i,%i]",
			np->gridcell->position->x, np->gridcell->position->y);
	tprintf(LOG_DEBUG, __func__, text);
	sprintf(text, "Created np->ports_in on [%i,%i]",
			np->next->gridcell->position->x, np->next->gridcell->position->y);
	tprintf(LOG_DEBUG, __func__, text);
#endif

	testSynapsePortMapping();

	//types
	np->type = NEURONSIGN_EXCITATORY | INPUT_NEURON;
	np->next->type = NEURONSIGN_EXCITATORY | OUTPUT_NEURON;
	n = np;
	init_neuron();
#ifdef WITH_GUI
	visualizeCell(n->gridcell->position->x, n->gridcell->position->y, n->type);
#endif
	n = np->next;
	init_neuron();
#ifdef WITH_GUI
	visualizeCell(n->gridcell->position->x, n->gridcell->position->y, n->type);
#endif
	n = np;


}

#ifdef WITH_PRINT_DISTRIBUTION

void initPrintDistribution() {
	if (gconf == NULL) {
		tprintf(LOG_ERR, __func__, "No gconf struct initialized!");
		return;
	}
	distribution = calloc(gconf->phenotypicFactors, sizeof(uint16_t));
}

void printDistribution(uint8_t verbosity) {
	char *text;
	text = malloc(gconf->phenotypicFactors * 5 + 128);
	//	char text[128*5]; 
	sprintf(text, "Distribution: ");
	uint8_t i;
	for (i = 0; i < gconf->phenotypicFactors; i++) {
		sprintf(text, "%s%03i", text, distribution[i]);
		if (i != gconf->phenotypicFactors - 1) {
			sprintf(text, "%s, ", text);
		}
	}
	sprintf(text, "%s", text);
	tprintf(verbosity, __func__, text);
}

#endif

/**
 * This routine is called by an iterator through the 2D environment, as soon as the retrieved gene
 * products exceed a threshold a morphological law is applied. Bongard et al. apply a rule when the
 * concentration exceeds 0.8. See grid.c:applyEmbryogenesis.
 */
void applyMorphologicalChange(uint8_t index) {	
#ifdef WITH_PRINT_DISTRIBUTION
	if (!distribution[index]) {
		char text[64]; sprintf(text, "First time operation %i", index);
		tprintf(LOG_VERBOSE, __func__, text);
	}
	distribution[index]++;
#else
#endif
	switch (index)
	{
	case 0: changeType(); break;
	case 1: changeSign(); break;
	case 2: changeTopologicalType(); break;
	case 3: incrementWeight(); break;
	case 4: decrementWeight(); break;
	case 5: nextSynapse(); break;
	case 6: splitSparse(); break;
	case 7: splitFull(); break;
	case 8: moveNeuronNorth(); break;
	case 9: moveNeuronSouth(); break;
	case 10: moveNeuronEast(); break;
	case 11: moveNeuronWest(); break;
	case 12: removeSynapse(); break;
	case 13: removeNeuron(); break;
	default: { ; }
	}
#ifdef WITH_CONSOLE
	uint8_t errorvalue = 0;
	if (distribution[index] < 5) {
		errorvalue += testNeurons();
		errorvalue += testNeuronGrid();
		errorvalue += testSynapseExistence();
		errorvalue += testSynapsePortMapping();
	}
	if (errorvalue) {
		printNeurons(LOG_ALERT);
		char textA[64]; sprintf(textA, "Error at operation %i with the %ith execution", 
				index, distribution[index]);
		tprintf(LOG_ALERT, __func__, textA);
		errorvalue = 0;
	}
	//	printNeurons(LOG_DEBUG);
	//	char textA[64]; sprintf(textA, "Operation %i", index);
	//			tprintf(LOG_DEBUG, __func__, textA);
#endif
}

/***********************************************************************************************
 *
 * @name cellular_operations
 * Routines to develop a neural network.
 *
 ***********************************************************************************************/

/** @{ */


/*
 * For the first implementation a split event just causes a neuron to create a duplicate in the next
 * grid cell (in the linked list). If that cell is already occupied, don't do anything. The outgoing
 * synapses are moved to the daughter neuron. This means that the source neuron id has to be updated
 * for all outgoing synapses. Moreover, one synapse is added from the mother to the daughter. Default
 * weights and delays are added to this new synapse.
 */
void splitSparse() {
	struct GridCell *newgc = np->gridcell->next;
	if (newgc->neuron != NULL) return; //next grid cell already occupied
	if (!newgc->position->x) return; //don't warp around grid

#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Apply split operation on cell [%i,%i]",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	//create new neuron and link reciprocally to grid
	struct Neuron *ln = lindaMalloc(sizeof(struct Neuron));
	ln->next = NULL; ln->ports_in = NULL; ln->ports_out = NULL;
	ln->history = lindaMalloc(sizeof(struct SpikeHistory));
	np->gridcell->next->neuron = ln;
	ln->gridcell = np->gridcell->next;

	//copy neuron properties and initialize new neuron
	ln->type = np->type;
	n = ln;
	init_neuron();

	//move synapses and create new link between neurons
	moveOutgoingSynapses(np, ln);

	struct Synapse *ls = addSynapse(np, ln);
	ls->delay = e->default_delay;
	ls->weight = e->default_weight;

	//update current ports
	ln->current_port = ln->ports_in;
	np->current_port = np->ports_out;

#ifdef WITH_GUI
	visualizeCell(n->gridcell->position->x, n->gridcell->position->y, n->type);   
#endif

	//jump back to neuron at neuron pointer
	n = np;

	//add to linked list of neurons
	struct Neuron *lnext = np->next;
	np->next = ln;
	ln->next = lnext;
#ifdef WITH_TEST
	uint8_t ncount = countNeurons();
	char textA[128]; sprintf(textA, "Neuron added. Now total amount %i.", ncount);
	tprintf(LOG_VV, __func__, textA);
#endif
}

/**
 * This split variation in Bongard's article copies input as well as output synapses to the new neuron
 * and adds reciprocal synapses between them. First, this was simplified to only copying output
 * synapses, because a linked list of "incoming synapses" on each neuron is not necessary in that case,
 * however, such a list is useful again for removal of a neuron. So, the implementation is faithful to
 * Bongard.
 */
void splitFull() {
	struct GridCell *newgc = np->gridcell->next;
	if (newgc->neuron != NULL) return; //next grid cell already occupied
	if (!newgc->position->x) return; //don't warp around grid

#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Apply copy operation on cell [%i,%i]",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	//duplicate neuron and add reciprocally to grid
	struct Neuron *ln = duplicateNeuron(np);
	np->gridcell->next->neuron = ln;
	ln->gridcell = np->gridcell->next;

	//update current port
	ln->current_port = ln->ports_in;

	//DEBUG: disable
	//	struct Synapse *ls = addSynapse(np, ln);
	//	ls->delay = e->default_delay;
	//	ls->weight = e->default_weight;
	//
	//	ls = addSynapse(ln, np);
	//	ls->delay = e->default_delay;
	//	ls->weight = e->default_weight;

	//add to linked list of neurons
	struct Neuron *lnext = np->next;
	np->next = ln;
	ln->next = lnext;
#ifdef WITH_TEST
	printNeuron(ln, LOG_VV);
	uint8_t ncount = countNeurons();
	char textA[128]; sprintf(textA, "Neuron added. Now total amount %i.", ncount);
	tprintf(LOG_VV, __func__, textA);
#endif
}

/*
 * As splitFull, but without creating synapses between the old and new neuron. It can be used
 * to create isolated neurons, without the need to delete new created synapses all the time.
 * Isolated neurons can be used to build a topographic map.
 */
void splitIsolated() {
	struct GridCell *newgc = np->gridcell->next;
	if (newgc->neuron != NULL) return; //next grid cell already occupied
	if (!newgc->position->x) return; //don't warp around grid

#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Apply isolated copy operation on cell [%i,%i]",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif

	struct Neuron *ln = duplicateNeuron(np);
	ln->next = np->next;
	np->next = ln;

	np->gridcell->next->neuron = ln;
	ln->gridcell = np->gridcell->next;
	//	check this
	//	ln->current_port = ln->ports_in;

	//add to linked list of neurons
	struct Neuron *lnext = np->next;
	np->next = ln;
	ln->next = lnext;

}

/**
 * Originally this was a "move back" command, but that doesn't make a lot of sense in a 2D world.
 * A neuron can't be forced over an edge of the grid. It won't be removed, nor be driven to the
 * other side as in a toroidal world. If there is a neuron there already, a neuron won't move
 * either.
 */
void moveNeuronNorth() {
	struct GridCell *oldgc = np->gridcell;
	int8_t y = oldgc->position->y - 1;
	if (y < 0) return;
	int8_t x = oldgc->position->x;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron != NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move neuron on cell [%i,%i] north",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	lgc->neuron = np;
	oldgc->neuron = NULL;
	np->gridcell = lgc;
}

void moveNeuronWest() {
	struct GridCell *oldgc = np->gridcell;
	int8_t x = oldgc->position->x - 1;
	if (x < 0) return;
	int8_t y = oldgc->position->y;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron != NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move neuron on cell [%i,%i] west",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	lgc->neuron = np;
	oldgc->neuron = NULL;
	np->gridcell = lgc;
}

void moveNeuronSouth() {
	struct GridCell *oldgc = np->gridcell;
	int8_t y = oldgc->position->y + 1;
	if (y >= s->columns) return;
	int8_t x = oldgc->position->x;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron != NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move neuron on cell [%i,%i] south",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	lgc->neuron = np;
	oldgc->neuron = NULL;
	np->gridcell = lgc;
}

void moveNeuronEast() {
	struct GridCell *oldgc = np->gridcell;
	int8_t x = oldgc->position->x + 1;
	if (x >= s->rows) return;
	int8_t y = oldgc->position->y;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron != NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move neuron on cell [%i,%i] east",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	lgc->neuron = np;
	oldgc->neuron = NULL;
	np->gridcell = lgc;
}

/**
 * Moves the current synapse to the neuron in the north, that is, if there is any neuron over there.
 */
void moveSynapseNorth() {
	if (np->current_port == NULL) return;
	struct GridCell *oldgc = np->gridcell;
	int8_t y = oldgc->position->y - 1;
	if (y < 0) return;
	int8_t x = oldgc->position->x;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move synapse on cell [%i,%i] north",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	//	portSynapse(np, lgc->neuron, np->current_port);
	portCurrentSynapse(lgc->neuron);
}

void moveSynapseWest() {
	if (np->current_port == NULL) return;
	struct GridCell *oldgc = np->gridcell;
	int8_t x = oldgc->position->x - 1;
	if (x < 0) return;
	int8_t y = oldgc->position->y;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move synapse on cell [%i,%i] west",
			gc->position->x, gc->position->y);
	tprintf(LOG_VERBOSE, __func__, text);
#endif
	portCurrentSynapse(lgc->neuron);
}

void moveSynapseSouth() {
	if (np->current_port == NULL) return;
	struct GridCell *oldgc = np->gridcell;
	int8_t y = oldgc->position->y + 1;
	if (y >= s->columns) return;
	int8_t x = oldgc->position->x;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move synapse on cell [%i,%i] south",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	//	portSynapse(np, lgc->neuron, np->current_port);
	portCurrentSynapse(lgc->neuron);
}

void moveSynapseEast() {
	if (np->current_port == NULL) return;
	struct GridCell *oldgc = np->gridcell;
	int8_t x = oldgc->position->x + 1;
	if (x >= s->rows) return;
	int8_t y = oldgc->position->y;
	struct GridCell *lgc = getGridCell(x,y);
	if (lgc->neuron == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move synapse on cell [%i,%i] east",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	//	portSynapse(np, lgc->neuron, np->current_port);
	portCurrentSynapse(lgc->neuron);
}

/**
 * Goes to the next synapse (on the current neuron). The embryogeny hence does need to store a
 * "current" synapse pointer per neuron.
 */
void nextSynapse() {
	if (np->current_port == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Move to next synapse on cell [%i,%i]",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif

	if (np->current_port->next != NULL) {
		np->current_port = np->current_port->next;
		return;
	}

	uint8_t flags = getPortContext(np, np->current_port);
	if (RAISED(flags, 1)) {
		if (np->ports_out != NULL) {
			np->current_port = np->ports_out;
		} else {
			np->current_port = np->ports_in;
		}
	} else {
		if (np->ports_in != NULL) {
			np->current_port = np->ports_in;
		} else {
			np->current_port = np->ports_out;
		}
	}
}


/**
 * Change the weight of the synapse by incrementing it with 10 (on 255). The resolution is coarse on
 * purpose. Real learning should be done online, not by evolutionary means. It is not possible to go
 * beyond 255.
 */
void incrementWeight() {
	if (np->current_port == NULL) return;
	//is float sp->w += (10 % (0xFF - sp->w));
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Increment weight of current synapse on neuron @[%i,%i]",
			gc->position->x, gc->position->y);
	tprintf(LOG_VVV, __func__, text);
#endif
	struct Synapse *ls = np->current_port->synapse;
	ls->weight += 1.0; //-= (10 % sp->w);
	if (ls->weight > 10.0) {
		ls->weight = 10.0;
	}
}

/**
 * Change the weight of the synapse by decrementing it with 1. It is not possible to go beyond
 * certain ranges.
 */
void decrementWeight() {
	if (np->current_port == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Decrement weight of current synapse on neuron @[%i,%i]",
			gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	struct Synapse *ls = np->current_port->synapse;
	ls->weight -= 1.0; //-= (10 % sp->w);
	if (ls->weight < -10.0) {
		ls->weight = -10.0;
	}
}

uint8_t somecounter = 0;

/**
 * This routine called by removeSynapse. It uses the 
 */
void removeCurrentSynapse() {
	struct Port *lp = np->current_port, *lnew = NULL;
	if (lp == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ERR, __func__, "No current port!");
#endif
		return;
	}
	struct Synapse *ls = lp->synapse;
	if (ls == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ERR, __func__, "Port has no synapse. Error before!");
#endif
		//	return;
	}

#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Update port list on this side");
#endif
	struct Port *lprev = getPreviousPort(np, lp);
	uint8_t flags = getPortContext(np, lp);
	if (lprev != NULL) {
		lnew = (lprev->next = lp->next);
	} else {
		if (RAISED(flags, 1)) {
			lnew = (np->ports_in = lp->next);
		} else {
			lnew = (np->ports_out = lp->next);
		}
	}

	np->current_port = lnew; //might be NULL
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Update port list on opposite side");
#endif
	struct Port *lpother = getOpposite(np, lp, flags);

	if (lpother == NULL) {
#ifdef WITH_CONSOLE
		if (isPrinted(LOG_VV)) {
			char textA[128];
			sprintf(textA, "Command getOpposite(np, lp, %i)", flags);
			tprintf(LOG_VERBOSE, __func__, textA);
			uint8_t ncount = countNeurons();
			sprintf(textA, "Amount of neurons: %i.", ncount);
			tprintf(LOG_VERBOSE, __func__, textA);
			printNeuron(np, LOG_EMERG);
			tprintf(LOG_EMERG, __func__, "Should never occur!");
		}
#endif
		return;
	}
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Update port list on opposite side");
#endif

	struct Neuron *lnother;
	if (RAISED(flags, 1)) {
#ifdef WITH_CONSOLE
		tprintf(LOG_VV, __func__, "Other side (while this one is an in-port)");
#endif
		lnother = ls->pre_neuron;
		printNeuron(lnother, LOG_VV);
		struct Port *lprevother = getPreviousPort(lnother, lpother);
		if (lprevother != NULL) {
			lprevother->next = lpother->next;
		} else {
			lnother->ports_out = lpother->next;
		}
	} else {
#ifdef WITH_CONSOLE
		tprintf(LOG_VV, __func__, "Other side (while this one is an out-port)");
#endif
		lnother = ls->post_neuron;
		struct Port *lprevother = getPreviousPort(lnother, lpother);
		if (lprevother != NULL) {
			lprevother->next = lpother->next;
		} else {
			tprintf(LOG_VV, __func__, "There is no previous port");
			//			if (lp->next)
			lnother->ports_in = lpother->next;
		}
	}

	if (lnother->current_port == lpother) { 
#ifdef WITH_CONSOLE
		if (lpother->next == NULL)
			tprintf(LOG_VV, __func__, "Current port on other side becomes NULL");
		printNeuron(lnother, LOG_VV);
#endif
		lnother->current_port = lpother->next;
		printNeuron(lnother, LOG_VVVV);
	}
	free(lpother);
	free(lp);
	free(ls);
}

/**
 * Remove synapse on the current neuron.
 *
 * Complexity O(S) with S the amount of synapses. This is because no double linked list is used.
 */
void removeSynapse() {
	struct Port *lp = np->current_port;
	if (lp == NULL) return;
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Remove synapse @[%i,%i]", gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	removeCurrentSynapse();
}

/**
 * Remove the current neuron and all its ingoing and outgoing synapses. Also of course the ingoing
 * ones, because they will be dangling in nothing.
 */
void removeNeuron() {
	//	printNeuron(np);
#ifdef WITH_CONSOLE
	char text[64];
	sprintf(text, "Remove neuron @[%i,%i]", gc->position->x, gc->position->y);
	tprintf(LOG_VV, __func__, text);
#endif
	struct Port *lpnext;
	np->current_port = np->ports_in;
	while (np->current_port != NULL) {
		lpnext = np->current_port->next;
		removeCurrentSynapse();
		np->current_port = lpnext;
	}

	np->current_port = np->ports_out;
	while (np->current_port != NULL) {
		lpnext = np->current_port->next;
		removeCurrentSynapse();
		np->current_port = lpnext;
	}

	if (np->history != NULL)
		free(np->history);

#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Remove neuron from list");
#endif
	struct Neuron *ln = np->next;
	struct Neuron *lnprev = nn->neurons;
	if (lnprev == np) {
		nn->neurons = ln;
		lnprev = NULL;
	} else {
		while (lnprev != NULL) {
			if (lnprev->next == np) {
#ifdef WITH_CONSOLE
				tprintf(LOG_VVVVV, __func__, "Found previous neuron");
#endif			
				break;
			}
			lnprev = lnprev->next;
			if (lnprev->next == lnprev) {
#ifdef WITH_CONSOLE
				tprintf(LOG_ALERT, __func__, "Should not occur. Circular!");
#endif			
				lnprev = NULL; break;
			}
		}
	}

	if (lnprev != NULL) {
		lnprev->next = ln;
	} 

	//remove gridcell reference
	np->gridcell->neuron = NULL;

	//free memory
	free(np);

	//update to next neuron, if there is any
	np = ln;

#ifdef WITH_TEST
	uint8_t ncount = countNeurons();
	char textA[128]; sprintf(textA, "Neuron removed. Now %i neurons left.", ncount);
	tprintf(LOG_VV, __func__, textA);
#endif
}

void changeType () {
#ifdef WITH_CONSOLE
	//	tprintf(LOG_VERBOSE, __func__, "Next type");
#endif
	n = np;
	next_type();
}

void changeSign() {
	n = np;
	next_sign();
}

void changeTopologicalType() {
	n = np;
	next_topological_type();
}

/** @} */

#ifdef WITH_TEST

/**
 * Counts the amount of neurons. For testing purposes.
 */
uint16_t countNeurons() {
	struct Neuron *lnp = nn->neurons; uint16_t i = 0;
	while (lnp != NULL) {
		i++;
		lnp = lnp->next;
	}
	return i;
}

uint8_t testNeuronGrid() {
	testNeurons();
	struct Neuron *lnp = nn->neurons;
	while (lnp != NULL) { 
		if (lnp->gridcell == NULL) {
			tprintf(LOG_ALERT, __func__, "No gridcell attached!!");
		} else if (lnp->gridcell->neuron != lnp) {
			char text[128]; 
			sprintf(text, "Neuron and gridcell [%i,%i] are not interlinked!", 
					lnp->gridcell->position->x, lnp->gridcell->position->y);
			tprintf(LOG_ALERT, __func__, text);
			return 1;
		}
		lnp = lnp->next;
	}
	return 0;
}

/**
 * Test if the list of neurons is not by accident circular, which might lead to infinite loops.
 */
uint8_t testNeurons() {
	struct Neuron *lnp = nn->neurons;
	if (lnp == NULL) return 0;
	do {
		lnp = lnp->next;
		if (lnp == nn->neurons) {
			tprintf(LOG_ALERT, __func__, "Neurons form a circular list: Danger of infinite loop!");
			return 1;
		}
	} while (lnp != NULL);
	return 0;
}

/**
 * Iterates all neurons, and if they have ports connected to them, it is checked if the port
 * is actually attached to a synapse.
 */
uint8_t testSynapseExistence() {
	struct Neuron *lnp = nn->neurons; 
	while (lnp != NULL) {
		struct Port *lpp = lnp->ports_in;
		while (lpp != NULL) {
			if (lpp->synapse == NULL) {
				tprintf(LOG_ALERT, __func__, "Port in without synapse!");
				if (lnp->gridcell == NULL) 
					tprintf(LOG_ALERT, __func__, "No gridcell attached!!");
				char text[128]; 
				sprintf(text, "This is at neuron at gridcell [%i,%i]", 
						lnp->gridcell->position->x, lnp->gridcell->position->y);
				return 1;
			}
			lpp = lpp->next;
		}
		lpp = lnp->ports_out;
		while (lpp != NULL) {
			if (lpp->synapse == NULL) {
				tprintf(LOG_ALERT, __func__, "Port out without synapse!");
				if (lnp->gridcell == NULL) 
					tprintf(LOG_ALERT, __func__, "No gridcell attached!!");
				char text[128]; 
				sprintf(text, "This is at neuron at gridcell [%i,%i]", 
						lnp->gridcell->position->x, lnp->gridcell->position->y);
				return 1;
			}
			lpp = lpp->next;
		}
		lnp = lnp->next;
	}
	return 0;
}

/**
 * Tests not just the synapse, but also if it connects ports out to ports in on another
 * neuron. 
 */
uint8_t testSynapsePortMapping() {
	struct Neuron *lnp = nn->neurons; 
	while (lnp != NULL) {
		struct Port *lpp = lnp->ports_in;
		struct Port *test;
		while (lpp != NULL) {
			test = getOpposite(lnp, lpp, 2);
			if (test == NULL) {
				char text[64]; 
				sprintf(text, "Of neuron [%i,%i]", 
						lnp->gridcell->position->x, lnp->gridcell->position->y);
				tprintf(LOG_ALERT, __func__, text);
				return 1;
			}
			lpp = lpp->next;
		}
		lpp = lnp->ports_out;
		while (lpp != NULL) {
			test = getOpposite(lnp, lpp, 0);
			if (test == NULL) {
				char text[64]; 
				sprintf(text, "Of neuron [%i,%i]", 
						lnp->gridcell->position->x, lnp->gridcell->position->y);
				tprintf(LOG_ALERT, __func__, text);
				return 1;
			}
			lpp = lpp->next;
		}
		lnp = lnp->next;
	}
	return 0;
}
/*
void testNeuronPortsReverse() {
	struct Neuron *lnp = nn->neurons; 
	while (lnp != NULL) {
		struct Port *lpp = lnp->ports_in;
		struct Port *test;
		if (lpp == NULL) goto testNPR2;
		while (lpp->next != NULL) lpp = lpp->next; //find the last port


testNPR2:
		lpp = lnp->ports_out;



		lnp = lnp->next;
	}
	return 0;
}
}*/

#endif
