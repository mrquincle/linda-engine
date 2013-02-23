/**
 * @file
 */
#include <lindaconfig.h>
#include <bits.h>
#include <topology.h>
#include <neuron.h>
#include <stdlib.h>

#ifdef WITH_SYMBRICATOR
#include "portable.h"
#endif

#ifdef WITH_CONSOLE
#include <stdio.h>
#include <linda/log.h>
#include <stdio.h>
#endif

/**RAISED(ln->history->spike_bitseq, lp->synapse->delay)
 * The STDP rule described in Izhikevich article "Polychronization: Computation with Spikes"
 * is implemented by using an array with the exponential values for all t precalculated (the
 * interspike time t can not be larger than the maximum delay, so it's a small array). The tau
 * parameters and A parameters as in the article are used:
 * - LTP = 0.10 * exp(-t/20) with t in ms
 * - LTD = 0.12 * exp(+t/20) with t in ms
 *
 * The first item is when there is an interval of 0. So, that value is conflicting in both
 * sequences. When spikes are really at the same time, no weight change occurs.
 * @remark A float (6 significant decimals) is used.
 */
const float LTD[16] = {-0.120000, -0.114148, -0.108580, -0.103285, -0.098248, -0.093456, -0.088898, -0.084563,
		-0.080438, -0.076515, -0.072784, -0.069234, -0.065857, -0.062645, -0.059590, -0.056684};
/**
 * @see LTP
 */
const float LTP[16] = {0.100000, 0.095123, 0.090484, 0.086071, 0.081873, 0.077880, 0.074082, 0.070469,
		0.067032, 0.063763, 0.060653, 0.057695, 0.054881, 0.052205, 0.049659, 0.047237};



/***********************************************************************************************
 *
 * @name spike_architecture
 * Routines to get spikes, adapt weights, and propagate the spikes through the network. They
 * operate on the global accessible nn struct.
 *
 ***********************************************************************************************/

/** @{ */

/**
 * Iterate through all the neurons and store their spikes on the wrapped neuron structure which
 * is aware of its synapses. The history trace on each neuron is also updated by one time step.
 */
void getSpikes() {
	//struct Neuron *
	n = nn->neurons;
	while (n != NULL) {
		//if ((n->type & TOPOLOGY_MASK) != INPUT_NEURON) {
		ADVANCE(n->history->spike_bitseq);
		if (fired()) {
			RAISE(n->history->spike_bitseq, 1);
		}
		//}
		n = n->next;
	}
}

/**
 * There are two cases: a presynaptic spike may be first, or a postsynaptic spike may come first.
 * To be able to forget patterns again, learning should not only strengthening synapses, but it
 * should also be possible to weaken them again. LTP stands for long-term potentiation, LTD for
 * long-term depression. In the former case, the weight increases, in the latter case, the weight
 * decreases. The weights are clipped to 10.0 and -10.0.
 */
/**
 * Freek's note#1: In calculating the interspike distance between a pre and post synaptic neuron,
 * the delay between the neurons is not accounted for. One should subtract from the spike time of
 * the post synaptic neuron both the spike time of the pre synaptic neuron and the delay of the
 * synapse.
 * Freek's note#2: This function currently is unfit for calling every ms, as it will cause learning
 * multiple times for a single pre- and post synaptic spike pair. As deleting spikes from the history
 * after using them to learn is undesirable (spike may still be important to other neurons) the most
 * plausible option is checking for every neuron whether it is connected to synapses that are eligible
 * for learning.
 * A synapse is eligible for learning when:
 *		a) the post synaptic neuron spikes
 * 	b) a spike of the pre synaptic neuron reaches the post synaptic neuron i.e. "delay" ms after
 *			the pre synaptic neurons spikes
 * suggestion for function that implements both adaptWeights and propagateSpikes:
void updateSynapses() {
   struct Synapse *s = nn->synapses;																	// pointer to iterate over
 	while(s != NULL) {																						// loop over all synapses
 		if(RAISED(s->pre_neuron->history->spike_bitseq, s->delay)) {							// if a pre synaptic spike reaches the post-synatpic neuron
 			s->weight += LTD[FIRST(s->post_neuron->history->spike_bitseq)];					// apply LTD with the most recent post-synaptic spike
 			s->post_neuron->I += (lp->synapse->weight / 3.0);										// increase the post-synaptic neuron's I
 		}
 		if(RAISED(s->post_neuron->history->spike_bitseq, 1)) {									// if a post-synatpic spike occurs
 			s->weight += LTP[FIRST(s->pre_neuron->history->spike_bitseq >> s->delay)];		// apply LTP with the most recent pre-synaptic spike that has arrived at the post-synaptic neuron, so occured at least "delay" ms ago
 		}
 	s = s->next;																								// update iterator
 	}
 }
 */
void adaptWeights() {
	struct Neuron *ln = nn->neurons;
	struct Port *lp;
	int16_t interspike_distance;

	while (ln != NULL) {
		lp = ln->ports_out;
		while (lp != NULL) {
			interspike_distance = FIRST(lp->synapse->post_neuron->history->spike_bitseq) -
			FIRST(ln->history->spike_bitseq);
			if (interspike_distance > 0) { //pre-post order
				lp->synapse->weight += LTP[interspike_distance];
				if (lp->synapse->weight > 10.0) lp->synapse->weight = 10.0;
			} else if (interspike_distance < 0) { //post-pre order
				lp->synapse->weight += LTD[-interspike_distance];
				if (lp->synapse->weight < -10.0) lp->synapse->weight = -10.0;
			} //else: spikes are exactly at the same moment (respecting the delay line)
			lp = lp->next;
		}
		ln = ln->next;
	}
}

/**
 * Spikes propagation doesn't seem that difficult, but each synapse might have a different delay
 * towards its postsynaptic neuron (because of different chemical properties of neurotransmitters
 * in the synaptic cleft).
 *
 * The function iterates through all the synapses, considers its historical spike distribution, of
 * which now not the last item is important, but the item that is "delay" time slots ago. If a
 * spike occured at that moment, the input I variable of the postsynaptic neuron is incremented
 * with an amount proportional to the synaptic weight. The postsynaptic neuron is different each
 * time, so all spikes have to be propagated, and only then the input of a neuron can be used
 * for the next processing stage.
 */
void propagateSpikes() {
	struct Neuron *ln = nn->neurons;
	struct Port *lp;
	while (ln != NULL) {
		lp = ln->ports_out;
		while (lp != NULL) {
			if (RAISED(ln->history->spike_bitseq, lp->synapse->delay)) {
#ifdef WITH_CONSOLE
				tprintf(LOG_VVV, __func__, "Raise");
#endif
				lp->synapse->post_neuron->I += (lp->synapse->weight / 3.0);
			}
			lp = lp->next;
		}
		ln = ln->next;
	}
}

/**
 * For every non-input neuron update is called once with the accumulated current figure
 * previously calculated in propagateSpikes. In the case of a neuron with 8 simultaneously
 * spiking input neurons, this figure might become the summation of all weights, say
 * 8*6 = 48 mA.
 */
void updateNeurons() {
	//struct Neuron *
	n = nn->neurons;
	while (n != NULL) {
		if ((n->type & TOPOLOGY_MASK) != INPUT_NEURON) {
			//printf("[%d,%d] ", n->gridcell->position->x, n->gridcell->position->y);
			update(n->I);
			n->I = 0;
		}
		n = n->next;
	}
}

/** @} */

/***********************************************************************************************
 *
 * @name developmental_architecture
 * Routines to add neurons, duplicate synapses, etc.
 *
 ***********************************************************************************************/

/** @{ */

/**
 * There is only one synapse per port. By duplicating ports of one neuron on another it is 
 * important not only to copy ports, but also the synapses. And not just from the source
 * neuron to the target neuron, but also from the neurons connected to the source neuron
 * by those synapses. So, for a neuron that has 4 incoming synapses, and hence 4 incoming
 * ports, there will be in total 12 memory allocations. The 4 synapses, the 4 incoming ports
 * and the 4 outgoing ports on the "collateral" neurons. This is quite hard to keep track of
 * manually, so that's why addSynapse is used 4 times. 
 * 
 * This routine does only copy to a target that does not contain ports yet. To copy ports
 * to a neuron that has already incoming synapses, please, implement a new function.
 */
void copyPortsIn(struct Neuron *src, struct Neuron *target) {
	struct Port *lpsrc = src->ports_in, *lptarget = target->ports_in;
	struct Synapse *lstarget = NULL; struct Neuron *other;

	if (lpsrc == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_VV, __func__, "No ports \"in\" to copy...");
#endif
		return;
	}
	if (lptarget != NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "This method only copies to empty target...");
#endif				
		return;
	}

	while (lpsrc != NULL) {
		other = lpsrc->synapse->pre_neuron;
		lstarget = addSynapse(other, target);
		lstarget->weight = lpsrc->synapse->weight;
		lstarget->delay = lpsrc->synapse->delay;
		lpsrc = lpsrc->next;
	}
#ifdef WITH_CONSOLE
	tprintf(LOG_VV, __func__, "Port copy finished");
#endif
}

/**
 */
void copyPortsOut(struct Neuron *src, struct Neuron *target) {
	struct Port *lpsrc = src->ports_out, *lptarget = target->ports_out;
	struct Synapse *lstarget = NULL; struct Neuron *other;

	if (lpsrc == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_VV, __func__, "No ports \"out\" to copy...");
#endif		
		return;
	}
	if (lptarget != NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "This method only copies to empty target...");
#endif				
		return;
	}

	while (lpsrc != NULL) {
		other = lpsrc->synapse->post_neuron;
		lstarget = addSynapse(target, other);
		lstarget->weight = lpsrc->synapse->weight;
		lstarget->delay = lpsrc->synapse->delay;
		lpsrc = lpsrc->next;
	}
#ifdef WITH_CONSOLE
	tprintf(LOG_VV, __func__, "Port copy finished");
#endif
}

/**
 * Copy ports from the source neuron to the target neuron. The bits in the given context field
 * define if it concerns "in" or "out" ports. A bit hoisted at 1 means "in", if not raised,
 * it means "out". It doesn't make sense to copy "in" ports to "out" ports on another neuron.
 * 
 * @todo (?) doesn't work if target already has ports.
 */
void copyPorts(struct Neuron *src, struct Neuron *target, uint8_t context) {
	if (RAISED(context, 1)) {
		copyPortsIn(src, target);
	} else {
		copyPortsOut(src, target);
	}
	return;
}

/**
 * Creates a new neuron instance duplicating also the synapses going to and leading from that
 * neuron. If they were just moved, weights can't be adjusted per synapse of course.
 */
struct Neuron *duplicateNeuron(struct Neuron *src) {
	struct Neuron *ln = lindaMalloc(sizeof(struct Neuron));
	ln->next = NULL; ln->ports_in = NULL; ln->ports_out = NULL;
	ln->history = lindaMalloc(sizeof(struct SpikeHistory));
	ln->type = src->type;
	n = ln;
	init_neuron();
	uint8_t context = 0;
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Start copying ports");
#endif
	copyPorts(src, ln, context);
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Ports out copied");
#endif
	context = 2;
	//	RAISE(context, 1);
	copyPorts(src, ln, context);
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Ports in copied");
#endif
	return ln;
}

/**
 * On moving synapses just moving a pointer is sufficient and subsequently updating the source
 * fields in each synapse. It removes the entire ports_out structure from the source neuron,
 * allocate it again, if necessary.
 */
void moveOutgoingSynapses(struct Neuron *src, struct Neuron *target) {
	target->ports_out = src->ports_out;
	src->ports_out = NULL;
	struct Port *lp = target->ports_out;
	while (lp != NULL) {
		lp->synapse->pre_neuron = target;
		//		printNeuron(lp->synapse->post_neuron);
		lp = lp->next;
	}
}

/**
 * Adds a synapse, but weights and delay isn't initialized yet. The routine adds the synapse
 * automatically to the neural network "synapses" linked list.  @TODO NOT!
 */
struct Synapse *addSynapse(struct Neuron *src, struct Neuron *target) {
	//create synapse
	struct Synapse *ls = lindaMalloc(sizeof(struct Synapse));
	ls->pre_neuron = src;
	ls->post_neuron = target;

	//create source port, add to port list
	struct Port *lp = lindaMalloc(sizeof(struct Port));
	lp->synapse = ls;
	lp->next = src->ports_out;
	src->ports_out = lp;

	//create target port, add to port list
	lp = lindaMalloc(sizeof(struct Port));
	lp->synapse = ls;
	lp->next = target->ports_in;
	target->ports_in = lp;

	return ls;
}

/**
 * Returns the previous "in" port. Ports are stored in a single linked list, so complexity of this
 * retrieval function is O(n) with n the amounts of ports. Making it a double linked list
 * would expand memory space with S(2*n) bytes, with n the overall amount of synapses in the
 * neural network. Reverse search is very rare (only for embryogeny purposes) so the trade-off
 * is easy in this case.
 */
struct Port *getPreviousPort(struct Neuron *neuron, struct Port *port) {
	struct Port *lp = neuron->ports_in;
	if (lp == port) return NULL;
	if (lp != NULL) {
		while (lp->next != NULL) {
			if (lp->next == port) return lp;
			lp = lp->next;
		}
	}
	lp = neuron->ports_out;
	if (lp == port)	return NULL;
	if (lp != NULL) {
		while (lp->next != NULL) {
			if (lp->next == port) return lp;
			lp = lp->next;
		}
	}
	return NULL;
}

struct Port *getPreviousInPort(struct Neuron *neuron, struct Port *port) {
	struct Port *lp = neuron->ports_in;
	if (lp == port) return NULL;
	if (lp != NULL) {
		while (lp->next != NULL) {
			if (lp->next == port) return lp;
			lp = lp->next;
		}
	}
	return NULL;
}

struct Port *getPreviousOutPort(struct Neuron *neuron, struct Port *port) {
	struct Port *lp = neuron->ports_out;
	if (lp == port) return NULL;
	if (lp != NULL) {
		while (lp->next != NULL) {
			if (lp->next == port) return lp;
			lp = lp->next;
		}
	}
	return NULL;
}

/**
 * The flags tell the caller if the port was found in the in-ports or the out-ports list,
 * and additionally tells if a NULL value is caused because the port is the head of the
 * linked list. A bit at 1 set is "in", a bit at 2 set is "out", a bit at 3 set is "head".
 */
uint8_t getPortContext(struct Neuron *neuron, struct Port *port) {
	uint8_t flags = 0;
	struct Port *lp = neuron->ports_in;
	if (lp != NULL) {
		RAISE(flags, 1);
		if (lp == port) { RAISE(flags, 3); return flags; }
		do {
			lp = lp->next;
			if (lp == port) return flags;
		} while (lp != NULL);
	}
	lp = neuron->ports_out;
	if (lp != NULL) {
		flags = 0; RAISE(flags, 2);
		if (lp == port) { RAISE(flags, 3); return flags; }
		do {
			lp = lp->next;
			if (lp == port) return flags;
		} while (lp != NULL);
		flags = 0;
	}
	return flags;
}

/**
 * The flags variable says if the given port is ingoing (bit at 1 is set) or outgoing (bit is
 * not set). The returned port is of course the opposite.
 */
struct Port *getOpposite(struct Neuron *neuron, struct Port *port, uint8_t flags) {
	struct Neuron *lnother = NULL;
	if (RAISED(flags, 1)) {
		if (port->synapse == NULL) {
			tprintf(LOG_ALERT, __func__, "No synapse attached to port (in)!");
		}
		lnother = port->synapse->pre_neuron;
		struct Port *lp = lnother->ports_out;
		if (lp == NULL) {
			tprintf(LOG_ALERT, __func__, "No ports out at all at other side!");
		}
		while (lp != NULL) {
			if (lp->synapse->post_neuron == neuron) return lp;
			lp = lp->next;
		}
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "No synapse connects to port out of the other neuron!");
#endif
		return NULL;
	} else {
		if (port->synapse == NULL) {
			tprintf(LOG_ALERT, __func__, "No synapse attached to port (out)!");
		}
		lnother = port->synapse->post_neuron;
		struct Port *lp = lnother->ports_in;
		if (lp == NULL) {
			tprintf(LOG_ALERT, __func__, "No ports in at all at other side!");
		}
		while (lp != NULL) {
			if (lp->synapse->pre_neuron == neuron) return lp;
			lp = lp->next;
		}
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "No synapse connects to port in of the other neuron!");
#endif
		return NULL;
	}
}

/**
 * Porting a synapse by moving the given "port" struct from the source neuron to the target
 * neuron.
 */
void portSynapse(struct Neuron *src, struct Neuron *target, struct Port *port) {
	uint8_t flags = getPortContext(src, port);
	struct Port *lprev = getPreviousPort(src, port);
	if (RAISED(flags, 3)) {
		if (RAISED(flags, 1)) src->ports_in = port->next;
		else src->ports_out = port->next;
	} else if (lprev == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "Port has no predecessor.");
#endif
	} else {
		lprev->next = port->next;
	}

	if (RAISED(flags, 1)) {
		port->next = target->ports_in;
		target->ports_in = port;
	} else if (RAISED(flags, 2)) {
		port->next = target->ports_out;
		target->ports_out = port;
	} else {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "Huh?");
#endif
	}
	port->synapse->pre_neuron = src;
	port->synapse->post_neuron = target;
}

/**
 * Ports a synapse from the current port on the current neuron to the target neuron given
 * by the function parameter. 
 */
void portCurrentSynapse(struct Neuron *target) {
	
	//@todo Check if synapse on neuron becomes self-referential
	
	struct Port *lpnext = np->current_port->next, *lpnew = NULL;
	struct Port *lpprev = getPreviousPort(np, np->current_port);
	uint8_t flags = getPortContext(np, np->current_port);
	portSynapse(np, target, np->current_port);
	if (lpprev != NULL) {
		lpnew = (lpprev->next = lpnext);
	} else {
		if (RAISED(flags, 1)) {
			lpnew = (np->ports_in = lpnext);
		} else {
			lpnew = (np->ports_out = lpnext);
		}
	}
	np->current_port = lpnew; //may be NULL
}

/**
 * Makes the current neuron the next topological type. So, this routine switches the type from
 * input to hidden to output neuron.
 */
void next_topological_type() {
	uint8_t topological_type = (n->type & TOPOLOGY_MASK) + (0x01 < TOPOLOGY_SHIFT);
	topological_type %= INPUT_NEURON;
	if (topological_type == 0) topological_type += (0x01 < TOPOLOGY_SHIFT);
	n->type = n->type & ~TOPOLOGY_MASK;
	n->type = n->type | topological_type;
}

/** @} */

#ifdef WITH_CONSOLE

void printNeurons(uint8_t verbosity) {
	struct Neuron *ln = nn->neurons;
	while (ln != NULL) {
		printNeuron(ln, verbosity);
		ln = ln->next;
	}
}

void printNeuron(struct Neuron *neuron, uint8_t verbosity) {
	if (neuron == NULL) {
		tprintf(LOG_ALERT, __func__, "Who is gonna print a NULL pointer!?");
		return;
	}
	if (neuron->gridcell == NULL) {
		tprintf(LOG_ALERT, __func__, "Neuron is not linked to gridcell");
		return;
	}
	struct Position *lpos = neuron->gridcell->position;
	char text[256];
	sprintf(text, "Neuron at [%i,%i], in: ", lpos->x, lpos->y);
	struct Port *lp; uint8_t existing = 0;
	lp = neuron->ports_in;
	while (lp != NULL) {
		if (lp->synapse == NULL) goto print_neuron_no_synapse;
		if (lp->synapse->pre_neuron == NULL) goto print_neuron_no_neuron;
		if (lp->synapse->pre_neuron->gridcell == NULL) goto print_neuron_no_gridcell;
		lpos = lp->synapse->pre_neuron->gridcell->position;
		sprintf(text, "%s [%i,%i]", text, lpos->x, lpos->y);
		existing = 1;
		lp = lp->next;
	}
	if (!existing) sprintf(text, "%s ---- ", text);
	sprintf(text, "%s and out: ", text);
	lp = neuron->ports_out;
	existing = 0;
	while (lp != NULL) {
		if (lp->synapse == NULL) goto print_neuron_no_synapse;
		if (lp->synapse->post_neuron == NULL) goto print_neuron_no_neuron;
		if (lp->synapse->post_neuron->gridcell == NULL) goto print_neuron_no_gridcell;
		lpos = lp->synapse->post_neuron->gridcell->position;
		sprintf(text, "%s [%i,%i]", text, lpos->x, lpos->y);
		existing = 1;
		lp = lp->next;
	}
	if (!existing) sprintf(text, "%s ---- ", text);
	tprintf(verbosity, __func__, text);
	return;
	print_neuron_no_synapse:
	tprintf(LOG_ALERT, __func__, text);
	tprintf(LOG_ALERT, __func__, "Inproperly initialized, no synapse!");
	return;
	print_neuron_no_neuron:
	tprintf(LOG_ALERT, __func__, text);
	tprintf(LOG_ALERT, __func__, "Inproperly initialized, no neuron!");
	return;
	print_neuron_no_gridcell:
	tprintf(LOG_ALERT, __func__, text);
	tprintf(LOG_ALERT, __func__, "Inproperly initialized, no gridcell!");
	return;
}

#endif

