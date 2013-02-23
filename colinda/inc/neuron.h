/**
 * @file neuron.h
 * @brief Definition of a Neuron
 * @author Anne C. van Rossum
 * 
 * The most basic module in the Linda Engine. It contains macro's that turn on or off additional
 * functionality like 2D grid awareness.
 * 
 * The neuron and the routines working on a neuron are stored in a separate file that does
 * not know anything about other neurons. The topology that connects neurons together is 
 * the place where connections are stored, see topology.h. 
 * 
 * All the routines operate on a global variable, named "n", so the routines themselves do not 
 * have a neuron struct as parameter. A simulator may swap this pointer to another neuron and 
 * this file acts if nothing has changed. Adding a parameter to all routines would introduce 
 * overhead.
 * 
 * @remark The definition of the struct "Neuron" is steering the implementation. There is no 
 * possibility anymore to have an automated means to allocate memory for the neural network as a 
 * whole, in which the individual neuron might be embedded in a larger chunk, which for example 
 * implicitly encodes connectivity by introducing pointer arithmetics. Neural fields in which
 * differential equations describe a lot of neurons at once, are ruled out by this implementation.
 * This is done on purpose: abstractions higher than on individual neuron level, makes it very
 * hard to implement a embryogenic dimension. 
 */

#ifndef NEURON_H_
#define NEURON_H_

#ifdef __cplusplus
extern "C" {
#endif 
	
#include <stdint.h>
#include <lindaconfig.h>
	
/****************************************************************************************************
 *  		Declarations 
 ***************************************************************************************************/

/**
 * The neuron sign can be positive and negative. In neuroscientific parliance, excitatory versus
 * inhibitory. The first bit is used.
 */
#define NEURONSIGN_SHIFT                   0
#define NEURONSIGN_MASK                 0x01
	
#define NEURONSIGN_EXCITATORY           0x01
#define NEURONSIGN_INHIBITORY           0x00
	
/**
 * The neuron types from 0000.0XXX till 1001.1XXX (with X meaning "don't cares") are defined. 
 * For more types use the range 1010.0XXX (0xA0) till 1111.1XXX (0xF8), which is enough for
 * twelve additional types.
 */
#define NEURONTYPE_SHIFT                   3
#define NEURONTYPE_MASK                 0xF8
	
#define NEURONTYPE_TONIC_SPIKING      	0x00
#define NEURONTYPE_PHASIC_SPIKING     	0x08
#define NEURONTYPE_TONIC_BURSTING     	0x10
#define NEURONTYPE_PHASIC_BURSTING		0x18
#define NEURONTYPE_MIXED_MODE			0x20
#define NEURONTYPE_SPIKE_FREQ_ADAPT		0x28
#define NEURONTYPE_CLASS1_EXC			0x30
#define NEURONTYPE_CLASS2_EXC			0x38
#define NEURONTYPE_SPIKE_LATENCY		0x40
#define NEURONTYPE_SUBTHRESHOLD_OSC		0x48
#define NEURONTYPE_RESONATOR			0x50
#define NEURONTYPE_INTEGRATOR         	0x58
#define NEURONTYPE_REBOUND_SPIKE		0x60
#define NEURONTYPE_REBOUND_BURST		0x68
#define NEURONTYPE_THRESH_VARIABILITY	0x70
#define NEURONTYPE_BISTABILITY			0x78
#define NEURONTYPE_DAP					0x80
#define NEURONTYPE_ACCOMODATION			0x88
#define NEURONTYPE_INHIB_IND_SPIKING	0x90
#define NEURONTYPE_INHIB_IND_BURSTING	0x98
	
/****************************************************************************************************
 *  		Modules 
 ***************************************************************************************************/

/**
 * If modules are turned on, header files have to be included. The macro acrobatics to do so is done
 * over here. 
 */
#ifdef MODULE_TOPOLOGY
#include <topology.h>
#endif
#ifdef MODULE_GRID
#include <grid.h>
#endif
#ifdef MODULE_EMBRYOGENY
#include <embryogeny.h>
#endif
	
/****************************************************************************************************
 *  		Definitions 
 ***************************************************************************************************/

/**
 * The properties of an Izhikevich neuron in a human-readable and programmer-readable format. ;-)
 */
union IzhikevichContainer {
	struct {
		float membrane_potential;				//v
		float membrane_recovery;				//u
		float membrane_recovery_timescale;		//a
		float membrane_recovery_sensitivity;	//b is set to +20.0 mV by default
		float membrane_potential_reset;			//c is set to -65.0 mV by default
		float membrane_recovery_reset;			//d
		uint8_t type;							//bit 0: EXCITATORY or INHIBITORY (mask 0x01)
												//bit 1-2: INPUT/OUTPUT/HIDDEN (mask 0x06)
												//bit 3-7: Izhikevich types (mask 0xF8) 
	};
	struct {
		float v; float u; float a; float b; float c; float d; //uint8_t type;
	};
};

/**
 * The implementation of the neuron data structure is glassbox-based. A glassbox-based solution defines
 * the data structure with information obtained from "higher-level" header files. The functionality and
 * parameters of those higher levels are maintained separately, but disabling it, requires disabling
 * macros on this lowest level. 
 * A blackbox-based solution would have "higher-level" neurons refer to "lower-level" neurons, but it is
 * not possible to reuse for example an iterator on a lower level. And for each transition to a lower
 * level a pointer to that lower level has to be followed. 
 * A hook-based solution would anticipate upon higher levels by reserving space to pointers for property
 * sets or containers on higher levels. With this level of indirection it is still possible to use
 * iterators of lower levels. It however introduces additional typecasting.
 * If containers are added from a certain file, be aware that this "neuron.h." file should not be linked 
 * from that file, because that introduces circular dependencies that the gcc compiler doesn't like. So,
 * "topology.h" does not need to include "neuron.h", even if it refers to this Neuron struct. 
 */
struct Neuron {
	union IzhikevichContainer; 
#ifdef MODULE_TOPOLOGY
	struct TopologyContainer;
#endif
#ifdef MODULE_GRID
	struct GridContainer;
#endif
#ifdef MODULE_EMBRYOGENY
	struct EmbryogenyContainer;
#endif
};

/****************************************************************************************************
 *  		Entity pointers 
 ***************************************************************************************************/

struct Neuron *n;

/****************************************************************************************************
 *  		Methods 
 ***************************************************************************************************/

BOOL fired();
void update(float I);
void init_neuron();

void next_type();
void next_sign();

#ifdef __cplusplus
}

#endif

#endif /*NEURON_H_*/

