/**
 * @file topology.h
 * @brief Topology of Neurons and Synapses
 * @author Anne C. van Rossum
 * 
 * The chosen topology is a "double linked network" where connections between elements are stored
 * in "both" directions. A connection (synapse) does have a source and a target part, as well as
 * an element (neuron) a set of incoming as outgoing edges. An element can not refer to two sets 
 * of connections because they are stored as a linked list, where one connection only refers to one
 * next connection. Hence ports are used as indirection.
 * 
 * If this implementation is too big, a dynamic array of synapses might be used, which removes the 
 * need for the port abstraction. It will be slower in e.g. moving synapses, faster in creating
 * neurons. Referencing through an iterator entity through outgoing synapses or through port entities 
 * makes however no difference.
 * 
 * The topology forms connections between individual neurons. The dynamics of a neuron is
 * defined independent of the topology. 
 * 
 * -# The topology is explicitly stored, not implicit by topological pointer operations jumping
 * from one neuron in a known memory construct to another. An explicit coding by synapses is
 * slower, but more flexible w.r.t. interfacing and dynamics in adding and removing synapses 
 * and neurons at runtime.
 * -# The time structure is not rolled out, but maintains implicit by coding delays on the 
 * synapses. This means that changing a delay is only one operation on one synapse, not a 
 * recalculation of a large part of the timed network. 
 * -# The output neurons map to evokations of actual software modules. So, a turn a wheel 
 * command, etc. This module expects to be able to drive incremental functions, where each
 * spike pushes the actuator in a certain direction. And where multiple neurons can cooperate
 * to be able to drive the actuator in opposite directions. The actuator should work on a
 * slower temporal scale. Not every spike needs to drive the actuator in a one-upon-one 
 * fashion. For smooth movements: a spike-velocity or spike-acceleration coding might be
 * appropriate.   
 * 
 * @todo Remove the Port abstraction. 
 * -# An outgoing synapse has to link to the next outgoing synapse
 * -# An incoming synapse has to link to the next incoming synapse
 * -# A synapse has to link to the source and target neuron
 * 
 * @todo Think about an event-based simulation. Is it possible with Izhikevich neurons? They
 * need an "I" input each time step. If there is no incoming spike event, will I be 0 or will
 * I be random?  
 */
 
#ifndef LINDA_TOPOLOGY_H_
#define LINDA_TOPOLOGY_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdint.h>

#ifndef NULL
#define NULL 0
#endif

/**
 * To set a neuron as an output neuron, clear flags using the TOPOLOGY_MASK and
 * set subsequently the OUTPUT_NEURON bits by a bit-wise OR operation:
 *   neuron->type = (neuron->type & ~TOPOLOGY_MASK) | OUTPUT_NEURON;
 */	
#define TOPOLOGY_SHIFT     1
#define TOPOLOGY_MASK   0x06			// 0000 0110
	
#define OUTPUT_NEURON	0x02			// 0000 0010
#define HIDDEN_NEURON	0x04			// 0000 0100
#define INPUT_NEURON	0x06			// 0000 0110

	struct Port;
	struct Synapse;
	struct SpikeHistory;
	struct NN;

	/**
	 * The synapse contains a reference to the post synaptic neuron. It also contains a delay which
	 * make it possible to have multiple polychronous groups, even more than the amount of neurons, like
	 * Izhikevich describes. The synapse is made explicit, so adding and removing synapses or neurons
	 * at runtime is no problem. The synapse contains both the source and the target neuron, and a 
	 * neuron also contains its incoming synapses, so it is possible to navigate backwards through the 
	 * network and e.g. remove the incoming synapses with the removal of a neuron. 
	 */
	struct Synapse {
		struct Neuron *pre_neuron;  //pre_synaptic neuron
		struct Neuron *post_neuron; //post_synaptic neuron
		uint8_t delay;
		float weight; 
//		struct Synapse *nextOnNeuron;
	};

	/**
	 * A linked list of ports, each linked to a synapse.
	 */
	struct Port {
		struct Synapse *synapse;
		struct Port *next;
	};

	/**
	 * How the spike history is implemented has consequences for:
	 * -# How the mapping from the spike activity on an output to a symbolic routine is done;
	 * -# What kind of learning is possible.
	 * 
	 * This implementation just stores flags in a 16-bit sequence. Getting a spike from the history
	 * is just by using the RAISED macro. Moving the history backwards is also very simple by using
	 * the ADVANCE macro (see the bits.h header).  
	 */
	struct SpikeHistory {
		uint16_t spike_bitseq;
	};


	/**
	 * The neurons as in neuron.c do not know other neurons, nor their connections to other neurons.
	 * Hence, Neuron_I contains an iterator that goes through all the neurons in the neural network.
	 * 
	 * Secondly, it contains per neuron a set of outgoing synapses, which each contains a reference
	 * to the postsynaptic neuron.  
	 * 
	 * Thirdly, the dataflow (or spike flow) across the topology has to be scheduled. The calculations 
	 * of the new membrane parameters should be done in one bunch, and ONLY THEN should the output of 
	 * all firing neurons be transferred to all target neurons. This means that the output of all 
	 * neurons should be stored temporarily somewhere. This is done in SpikeHistory. The second use of
	 * the spike history is for the cause of learning by STDP which needs interspike timings.
	 * 
	 * Fourthly, the combined input to this neuron over the incoming synapses is stored in the parameter
	 * I. It is subsequently used to calculate if a neuron spikes the next round.
	 * 
	 * Fifthly, a method can be attached to for example output neurons. A firing event will cause it to
	 * execute this function. Execution is delayed till all neurons have fired. 
	 */
	struct TopologyContainer {
		struct Neuron *next;
		struct Port *ports_in;
		struct Port *ports_out;		
		struct SpikeHistory *history;
		float I;
		void *method; 
	};

	/**
	 * The neural network contains a linked list of neurons. It also contains a circular linked list for
	 * synapses. The lastSynapse variable makes it possible to add a synapse easily.
	 */
	struct NN {
		struct Neuron *neurons; 
//		struct Synapse *synapses;
//		struct Synapse *lastSynapse;
	};

	struct NN *nn;

	void getSpikes();
	void adaptWeights();
	void propagateSpikes();
	void updateNeurons();
	
	struct Neuron *duplicateNeuron(struct Neuron *src);
	void moveOutgoingSynapses(struct Neuron *src, struct Neuron *target);
	struct Synapse *addSynapse(struct Neuron *src, struct Neuron *target);
	void portSynapse(struct Neuron *src, struct Neuron *target, struct Port *port);
	
	void portCurrentSynapse(struct Neuron *target);
	
	uint8_t getPortContext(struct Neuron *neuron, struct Port *port);
	struct Port *getOpposite(struct Neuron *neuron, struct Port *port, uint8_t flags);
	struct Port *getPreviousPort(struct Neuron *neuron, struct Port *port);
	
	void next_topological_type();
	
#ifdef WITH_CONSOLE
	void printNeuron(struct Neuron *neuron, uint8_t verbosity); 
	void printNeurons(uint8_t verbosity);
#endif
	
#ifdef __cplusplus
}

#endif

#endif /*LINDA_TOPOLOGY_H_*/
