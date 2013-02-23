/**
 * Neural filters are grids filled with neurons that have a predefined wiring pattern
 * and perform a computational function. The outer plexiform layer is the first layer
 * of synapses in the retina. It are synapses between the light receptors (cods and 
 * cones), the horizontal cells and the bipolar cells. See "Virtual Retina: A Biological
 * Retina Model and Simulator, with Contrast Gain Control" by Wohrer and Kornprobst 
 * (2008). The authors abstract the layers of cells as a spatial continuum (no discrete 
 * cells) in differential equations. The neural filters over here do not abstract from
 * individual neurons to be able to swap neuron types for different ones and test the
 * consequences of this in an evolutionary paradigm.
 * 
 * Wysoski et al. uses SpikeNet to implement on-off center-surround filters, let I quote 
 * from "Adaptive Learning Procedure for a Network of Spiking Neurons" (2006): "The neural 
 * network is composed of 3 layers of integrate-and-fire neurons. The neurons have a latency 
 * of firing that depends upon the order of spikes received. Each neuron acts as a coincidence 
 * detection unit, [...]" For me it's not clear if over here the same neurons are meant, or 
 * neurons in subsequent layers. If neurons have a latency of firing they might correspond 
 * with neurons of type "I" in Izhikevich table that exhibit "spike latency". If neurons are 
 * doing coincidence detection, they correspond with neurons of type "L" in Izhikevich table 
 * "integration and coincidence detection". This table can be found in "Which Model to use for 
 * Cortical Spiking Neurons?". However, perhaps those functions can be combined, moreover, those 
 * are subcortical neurons that might not be part of this paper by Izhikevich. The term "Rank 
 * Order Coding" originates from Thorpe et al. in "Spike-Based Strategies for Rapid Processing". 
 * Probably Wysoski got his inspiration from Thorpe, I have to check that.
 * 
 * The above is my inspiration to create a map with type "I" neurons first. The file neuron.c
 * implements this type of neuron. A filter of such neurons is created using the grid.c file,
 * there is only no developmental dimension. Subsequently a map with type "L" neurons is 
 * created. Afterwards, the result of on-center off-surround filters is easy to detect. It 
 * should show outputs like in "Towards Bridging the Gap between Biological and Computational 
 * Image Segmentation" by Kokkinos et al. in section "3.2 The Boundary Contour System". It's
 * not clear for me if on-center off-surround filters can be created by "I" and "L" type
 * neurons alone, but that's a good starting point. :-)  
 */
#include <embryogeny.h>

/*void start_filter_latency_map() {
	//neurons
	np = nn->neurons = lindaMalloc(sizeof(struct Neuron));
	np->history = lindaMalloc(sizeof(struct SpikeHistory));
	(np->gridcell = getGridCell(0,0))->neuron = np;
	
	//types
	np->type = EXCITATORY | INPUT_NEURON;
	n = np;
	init_neuron();
}

void create_filter_latency_map() {
	init_embryology();
	init_filter_latency_map();
	for (i = 0; i < s->rows * s->columns - 1; i++) {
		splitIsolated();
	}
}*/
