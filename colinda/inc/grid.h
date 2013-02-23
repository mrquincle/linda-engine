/**
 * @file grid.h
 * @brief A 2D Spatial Layout of the NN
 * @author Anne C. van Rossum
 * 
 * The topology of the neurons and sequence of firing is not yet an embodied network. To have
 * this, neurons should be positioned in a spatial area. In this case a 2D squared grid is 
 * implemented. The 2D environment introduces a possibility to have information flow on a 
 * grid level instead of on a neuron level only. The implementation of diffusion of so-called
 * gene products is also implemented over here.
 * 
 * The embryogeny might work on this 2D representation of a neural network. Where cell
 * division and cell-to-cell etc. obey certain physical (spatial) laws. 
 * 
 * @todo The gene products are yet not part of the equation.  
 */

#ifndef GRID_H_
#define GRID_H_

#ifdef __cplusplus
extern "C" {
#endif 
	
struct GridCell;
struct GridConnection;

struct Position {
	uint8_t x; uint8_t y;
};

struct GridContainer {
	struct GridCell *gridcell;
};

/**
 * To be able to quickly iterate through the cells in a grid, they are organized in a linked list
 * besides being organized by connections between the cells.
 */
struct GridCell {
	struct Product *products;
	struct GridConnection *connections;
	struct GridCell *next;
	struct Neuron *neuron;
	struct Position *position;
};

/**
 * Almost same form as synapse in topology.h, might be useful to use same abstraction. The only
 * thing that will be different in that case is that the Synapse struct will contain an additional
 * pointer to a {weight, delay} tuple. However, perhaps a "label" pointer would be useful for
 * a GridConnection some time too. For example to implement "temporal" diffusion.
 */
struct GridConnection {
	struct GridCell *from;
	struct GridCell *to;
	struct GridConnection *next;
};

/**
 * Space is subdivided into a grid of cells, even though this might be tempting to define as
 * a double array, a linked list is used, so that it is easier to migrate to another tessalation
 * of the space, to a 3D space, to implement dynamics in the amount of grid cells, or to adjust
 * granularity with respect to resources at designtime or runtime. 
 */
struct Space {
	struct GridCell *gridcells;
	uint8_t rows;
	uint8_t columns;
	uint8_t decay_step; //Bongard 0.005 on 1.0 scale = 1/200, so on a 255 scale: decay of 1
	uint8_t diffuse_ratio;
	uint8_t concentration_threshold;
	uint8_t concentration_default;
};

struct GridCell *gc; 

struct Space *s;

struct GridCell *getGridCell(uint8_t x, uint8_t y);

void configGrid();

void initGrid();

void freeGrid();

void initConcentrations();

void updateGrid();

void applyEmbryogenesis();

#ifdef WITH_CONSOLE
void printGridToStr();
void printGrid();
void printAllConcentrationsMultiplePerRow();
void printAllConcentrations();
void printAllConcentrationUpdates();
void drawGrid();
void drawConcentrations(uint8_t product_id, uint16_t fileIndex);
void drawAllConcentrations(uint16_t fileIndex);

#ifdef WITH_GUI
void visualizeCell(uint8_t x, uint8_t y, uint8_t value);
void visualizeCells();
#endif

#endif

#ifdef __cplusplus
}

#endif

#endif /*GRID_H_*/
