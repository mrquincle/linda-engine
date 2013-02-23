/**
 * @file neuron.c
 * @brief The different implementations of neurons
 * @author Anne C. van Rossum
 */

#include <lindaconfig.h>

#include <neuron.h>
#include <bits.h>

#ifdef WITH_CONSOLE
#include <stdio.h> //only for printf
#include <linda/log.h>
#endif

/**
 * Check the parameters at http://vesicle.nsi.edu/users/izhikevich/publications/figure1.m
 * To see the graphs, use testNeuron, however, adapt the time scale and the input each time.
 */
void init_neuron() {
	switch (n->type & NEURONTYPE_MASK) {
	case NEURONTYPE_TONIC_SPIKING:
		n->a = +0.02; n->b = +0.20; n->c = -65.0; n->d = +6.00;
		n->v = -70.0; n->u = n->v * n->b;
		break;
	case NEURONTYPE_PHASIC_SPIKING:
		n->a = +0.02; n->b = +0.25; n->c = -65.0; n->d = +6.00;
		n->v = -64.0; n->u = n->v * n->b;
		break;
	case NEURONTYPE_INTEGRATOR:
		n->a = +0.02; n->b = -0.10; n->c = -55.0; n->d = +6.00;
		n->v = -60.0; n->u = n->v * n->b;
		break;
	default:
		n->v = -64.0;
		n->u = n->v * 0.2;
		n->b = +0.25;
		n->c = -65.0;
		if (n->type & NEURONSIGN_MASK) {
			n->a = +0.02; //should be randomized
			n->d = +6.00;
		} else {
			n->a = +0.10;
			n->d = +2.00;
		}
	}
}

/**
 * The potentials and other (membrane) parameters are updated, subsequently those values are
 * checked against a certain threshold and it is decided if the neuron fires or not. When a
 * neuron fires it does not iterate through its connections, because they are not known to 
 * it. It just returns a "fired" state.
 * First it has to be known which neurons spiked, if simultaneous arrival of spikes from
 * neurons earlier and later in the iteration is important, first all spikes have to be 
 * retrieved. If delays are also important, those spikes have to be stored and also the time
 * they will be travelling to the next neuron. However, that is the responsibility of the
 * network, not of this file.   
 */
BOOL fired() {
	BOOL result = 0;
	if (n->type & NEURONSIGN_MASK) {
		//set I
	}
	if (n->v >= 30.0) {
		n->v = n->c;
		n->u += n->d;
		//if (n->u > 100) n->u = -65.0/4; //weird behaviour, unstable with current I input
		result = 1;
	} 
	return result;
}

/**
 * Besides checking for the firing condition, the internal variables have to be updated. The
 * neuron does not know anything about the others, so the sum over all its inputs is provided
 * as an argument: input.
 */
void update(float I) {
	//printf("Parameters: %f, %f becomes with input I=%f: ", n->v, n->u, I);
	float euler_step = 0.5;
	switch (n->type & NEURONTYPE_MASK) {
	case NEURONTYPE_INTEGRATOR:
		euler_step = 0.25; uint8_t euler = 4;
		do {
			n->v += euler_step * ((0.04 * n->v + 4.1) * n->v + 108.0 - n->u + I);
			euler--;
		} while (euler > 0);
		break;
	default:
		n->v += euler_step * ((0.04 * n->v + 5.0) * n->v + 140.0 - n->u + I);
		n->v += euler_step * ((0.04 * n->v + 5.0) * n->v + 140.0 - n->u + I);
	}
	n->u += n->a * (n->b * n->v - n->u);
}

void next_type() {
	uint8_t neurontype = (n->type & NEURONTYPE_MASK) + (0x01 < NEURONTYPE_SHIFT);
	neurontype %= NEURONTYPE_INHIB_IND_BURSTING;
	n->type = n->type & ~NEURONTYPE_MASK;
	n->type = n->type | neurontype;
}

void next_sign() {
	TOGGLE(n->type, NEURONSIGN_MASK);
}

