
/**
 * @mainpage
 * 
 * The Linda Engine contains a spiking neural network with Izhikevich neurons and delay lines
 * between the neurons. The network is not godlike given, but is developed from a few cells 
 * by a Gene Regulatory Network, splitting, moving and wiring the cells till a fullfledged
 * network arises. 
 * 
 * The most basic file is inc/neuron.h where a neuron is defined and its properties can be 
 * changed by commenting or uncommenting macros like MODULE_TOPOLOGY or MODULE_GRID. This makes it
 * possible to have a "Neuron" datatype that can be very simple to very complex.   
 * 
 * @section conventions Conventions
 * - The most important design methodology: There are global variables used to make it possible for 
 * an iterator in another file to switch the current entities worked upon. This reduces the amount 
 * of function parameters considerably. So, instead of calling "update(neuron)", the parameter "n" 
 * will be set to for example the next neuron by "n = n->next" and just calling "update()".
 * - The names of those global variables are always as brief as possible. There is no "with" 
 * keyword in C, as in Pascal or even VB. Hence, a long name will all make all functions 
 * ridicuously verbose.
 * - The names of local variables will contain l is prefix, and also be as short as possible. So, 
 * a function might iterate through a list of neurons by using "ln = ln->next" and when real neuron
 * operators are called it sets "n = ln" and evokes for example "update()" like described above.     
 * - No "typedefs" are used, this means a little more typing of "union" and "struct" keywords. On a
 * change from union to struct, refactoring may take a while. However, the syntax with the keywords
 * is easier to highlight in most editors. And there are no lists of typedefs everywhere.
 * - The names of structs and unions are long. There might be an abbreviated version available by
 * using a "union" within the struct or union. So, it is possible to use "n->membrane_potential" as 
 * well as "n->v". 
 * - There are tagged, but anonymous structs used in other structs. The "struct Neuron" contains 
 * several containers, like the "union IzhikevichContainer" and the "struct TopologyContainer" which
 * each add different fields to the neuron struct. This makes it possible to write just 
 * "n->membrane_recovery" (part of the Izhikevich container) as well as "n->next" (part of the 
 * Topology container). To be able to do this, the compiler uses the "-fms-extensions" flag.
 * 
 * An example of a ack of the Simulator can be seen below. It shows the spikes to the 4 output n
 * neurons driving the two wheels on the robot back, forth, left and right.
 * 
 * @image html actuator_input.jpg 
 * 
 * 
 * 
 * In the case there are any questions, feel free to contact me:
 * 
 * anne DELETE_THIS AT almende.com
 * 
 * @author Anne C. van Rossum
 */


#ifndef MAINPAGE_H_
#define MAINPAGE_H_

#endif /*MAINPAGE_H_*/
