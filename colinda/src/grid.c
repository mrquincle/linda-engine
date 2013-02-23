//#include <stdlib.h>
#include <lindaconfig.h>
#include <genome.h>
#include <topology.h>
#include <neuron.h>

#ifdef WITH_SYMBRICATOR
#include "portable.h"
#endif

#ifdef WITH_CONSOLE
#include <linda/log.h>
#include <stdio.h>

#ifdef WITH_GNUPLOT
#include <linda/gnuplot_i.h>
#endif

#ifdef WITH_GUI
#include <colinda.h>
#include <infocontainer.h>
#include <linda/abbey.h>
#include <unistd.h> //for sleep
#endif

#endif

/****************************************************************************************************
 *  		Declarations
 ***************************************************************************************************/

void updateConcentrations();
void decayConcentrations();
void diffuseConcentrations();
void copyConcentrationsToNew();
void avgConcentrationsToCurrent();

/****************************************************************************************************
 *  		Implementations
 ***************************************************************************************************/

#ifdef WITH_GUI
/**
 * Visualizes one cell by sending a task to the Visualization engine. A GUI that is
 * able to visualize different types of neuron in a 2D grid. 
 */
void visualizeCell(uint8_t x, uint8_t y, uint8_t value) {
	struct InfoArray *infoa = malloc(sizeof(struct InfoArray));
	infoa->values = calloc(4, sizeof(uint8_t));
	infoa->values[0] = x;
	infoa->values[1] = y;
	infoa->values[2] = value;// & 0x0F;
	infoa->values[3] = value;// & TOPOLOGY_MASK; //(value > 0);
	infoa->length = 4;
	dispatch_described_task(visualize, (void*)infoa, "visualize");
	sleep(1);
}

/**
 * Visualizes all cells of the entire grid. To slow down the visualization engine it is 
 * possible to include a "sleep()" call over here. 
 */
void visualizeCells() {
	struct GridCell *lgc = s->gridcells;
	do {
		if (lgc->neuron != NULL) {
			visualizeCell(lgc->position->x, lgc->position->y, lgc->neuron->type);
		} else {
			visualizeCell(lgc->position->x, lgc->position->y, 0);
		}
		lgc = lgc->next;
	} while (lgc != s->gridcells);
}
#endif

/**
 * Retrieve a gridcell using 2D coordinates.
 */
struct GridCell *getGridCell(uint8_t x, uint8_t y) {
	uint8_t counter = x + y * s->columns;
	struct GridCell *lgc = s->gridcells;
	while ((lgc != NULL) && (counter != 0)) {
		counter--;
		lgc = lgc->next;
	}
#ifdef WITH_CONSOLE
	if (lgc == NULL)
		tprintf(LOG_ALERT, __func__, "GridCell not found!");
#endif
	return lgc;
}

struct GridCell *getGridCellByIndex(uint8_t i) {
	struct GridCell *lgc = s->gridcells;
	while ((lgc != NULL) && (i != 0)) {
		i--;
		lgc = lgc->next;
	}
	return lgc;
}

/**
 * Go through the grid cells and diffuse gene concentrations to neighbouring grid cells. And decay
 * all gene product concentrations everywhere by a small amount.
 */
void updateGrid() {
	updateConcentrations();
	decayConcentrations();
	copyConcentrationsToNew();
	//	printf("\n");
	//#ifdef WITH_CONSOLE
	//	tprintf(LOG_NOTICE, __func__, "Before diffusion:");
	//#endif
	//	printAllConcentrations();
	//#ifdef WITH_CONSOLE
	//	tprintf(LOG_NOTICE, __func__, "Before diffusion (new conc):");
	//#endif
	//	printAllConcentrationUpdates();
	diffuseConcentrations();
	//#ifdef WITH_CONSOLE
	//	tprintf(LOG_NOTICE, __func__, "After diffusion:");
	//#endif
	//	printAllConcentrations();
	//#ifdef WITH_CONSOLE
	//	tprintf(LOG_NOTICE, __func__, "Diffusion grid:");
	//#endif
	//	printAllConcentrationUpdates();
	avgConcentrationsToCurrent();
	//#ifdef WITH_CONSOLE
	//	tprintf(LOG_NOTICE, __func__, "After averaging:");
	//#endif
	//	printAllConcentrations();
}

/**
 * The configGrid routine only needs to be called once. It allocates memory for a 2D space and
 * defines the amount of cells on it and sets decay and diffusion parameters.
 */
void configGrid() {
	s = lindaMalloc(sizeof(struct Space));
	s->rows = 5;
	s->columns = 5;
	s->decay_step = 1;
	s->diffuse_ratio = 8; //should be 4 or more
	s->concentration_threshold = 75;
	s->concentration_default = 20;
}

/**
 * Deallocates a gridcell. This means position information, as well as deallocating
 * the linked list of connections (which should not be circular), and a linked list
 * of gene products.
 */
void freeGridCell(struct GridCell *lgc) {
	struct Product *lp, *lpnext;
	struct GridConnection *lgcc, *lgccnext;
	free(lgc->position);
	lgcc = lgc->connections;
	if (lgcc == NULL) goto free_gridcell_products;
	lgccnext = lgcc->next;
	while (lgccnext != NULL) {
		free(lgcc);
		lgcc = lgccnext;
		lgccnext = lgcc->next;
	}
	free(lgcc);

free_gridcell_products:
	lp = lgc->products;
	if (lp == NULL) goto free_gridcell;
	lpnext = lp->next;
	while (lpnext != NULL) {
		free(lp);
		lp = lpnext;
		lpnext = lp->next;
	}
	free(lp);
	
free_gridcell:
	free(lgc);
}

/**
 * The gridcells are stored in a circular linked list, so care has to be taken
 * in deallocating all cells appropriately. Hence, iterating over rows and columns
 * is the easiest way to free the space.
 */
void freeGrid() {
	struct GridCell *lgc = s->gridcells, *lgcnext;
	if (lgc == NULL) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "No cells!");
#endif
		goto free_space;
	}
	lgcnext = lgc->next; uint8_t i;
	for (i=1; i<(s->rows * s->columns); i++) {
		lgc = lgcnext;
		lgcnext = lgc->next;
		freeGridCell(lgc);
	}
	if (lgcnext != s->gridcells) {
#ifdef WITH_CONSOLE
		tprintf(LOG_ALERT, __func__, "No proper circular list of cells!");
#endif
		goto free_space;
	}
	freeGridCell(s->gridcells);
free_space:
	free(s);
}

/**
 * Allocate space for all grid elements, using the configuration parameters set before in configGrid.
 * The cells are stored in a circular linked list. The positions are set to proper [x,y] coordinates.
 */
void initGrid() {
	uint8_t i;
	s->gridcells = lindaMalloc(sizeof(struct GridCell));
	struct GridCell *lgc = s->gridcells;
	lgc->neuron = NULL;
	lgc->position = lindaMalloc(sizeof(struct Position));
	lgc->position->x = 0; lgc->position->y = 0;
	for (i=1; i<(s->rows * s->columns); i++) {
		lgc->next = lindaMalloc(sizeof(struct GridCell));
		lgc = lgc->next;
		lgc->neuron = NULL;
		lgc->position = lindaMalloc(sizeof(struct Position));
		lgc->position->x = i % s->columns; lgc->position->y = i / s->columns;
	}
	lgc->next = s->gridcells;

	lgc = s->gridcells; struct GridConnection *lgcc = NULL;
	for (i=0; i<(s->rows * s->columns); i++) {
		if (((i + 1) % s->columns)) {
			//									printf("Add east: %i\n", i);
			lgc->connections = lindaMalloc(sizeof(struct GridConnection));
			lgcc = lgc->connections;
			lgcc->from = lgc;
			lgcc->to = getGridCellByIndex(i + 1);
		}
		if ((i % s->columns)) {
			//									printf("Add west: %i\n", i);
			if (lgcc == NULL) {
				lgc->connections = lindaMalloc(sizeof(struct GridConnection));
				lgcc = lgc->connections;
			} else {
				lgcc->next = lindaMalloc(sizeof(struct GridConnection));
				lgcc = lgcc->next;
			}
			lgcc->from = lgc;
			lgcc->to = getGridCellByIndex(i - 1);
		}
		if (!(i < s->columns)) {
			//									printf("Add north: %i\n", i);
			if (lgcc == NULL) {
				lgc->connections = lindaMalloc(sizeof(struct GridConnection));
				lgcc = lgc->connections;
			} else {
				lgcc->next = lindaMalloc(sizeof(struct GridConnection));
				lgcc = lgcc->next;
			}
			lgcc->from = lgc;
			lgcc->to = getGridCellByIndex(i - s->columns);
		}
		if (!(i >= (s->rows - 1) * s->columns)) {
			//						printf("Add south: %i\n", i);
			if (lgcc == NULL) {
				lgc->connections = lindaMalloc(sizeof(struct GridConnection));
				lgcc = lgc->connections;
			} else {
				lgcc->next = lindaMalloc(sizeof(struct GridConnection));
				lgcc = lgcc->next;
			}
			lgcc->from = lgc;
			lgcc->to = getGridCellByIndex(i + s->columns);
		}
		lgc = lgc->next;
		lgcc = NULL;
	}
}

/***********************************************************************************************
 *
 * @name concentration_operations
 * Routines to update the concentration of gene products in a cell, diffuse it through the grid
 * and decay gene products by a constant rate.
 *
 ***********************************************************************************************/

/** @{ */

/**
 * After all genes are extracted, the concentrations of products have to be initalized, not
 * only for the cells in which gene products are disseminated according to genetic information,
 * but also for the other cells in which gene products can appear by diffusion.
 */
void initConcentrations() {
	gc = s->gridcells;
	if (gc == NULL) return;
	do {
		uint8_t i;
		struct Product *lpprev = gc->products;
		for (i = 0; i < gconf->phenotypicFactors; i++) {
			//#ifdef WITH_CONSOLE
			//	tprintf(LOG_INFO, __func__, "Create gene product");
			//#endif
			struct Product *lp = lindaMalloc(sizeof(struct Product));
			lp->id[0] = i;
			lp->concentration = s->concentration_default;
			lp->next = NULL;
			if (i == 0)
				gc->products = lp;
			else {
				lpprev->next = lp;
			}
			lpprev = lp;
			lp = lp->next;
		}
		for (i = gconf->phenotypicFactors; i < gconf->phenotypicFactors + gconf->regulatingFactors; i++) {
			struct Product *lp = lindaMalloc(sizeof(struct Product));
			lp->id[0] = i;
			lp->concentration = s->concentration_default;
			lp->next = NULL;
			if (i == 0)
				gc->products = lp;
			else {
				lpprev->next = lp;
			}
			lpprev = lp;
			lp = lp->next;
		}
		gc = gc->next;
	} while (gc != s->gridcells);
}

/**
 * In every gene there is a spot - it is called P4 in Bongard's paper in figure 3 - that codes for
 * the location (grid cell) in which a gene product might be disseminated.
 * Complexity O(g*a), with g the amount of genes, and a the amount of activated / non-empty cells.
 * If only a few cells are coded for by the genes, not all the cells have to be iterated.
 */
void updateConcentrations() {
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "New update iteration");
#endif
	g = eg->genes;
	while (g != NULL) {
		gc = getGridCell(g->codons->LocationOut_X, g->codons->LocationOut_Y);
#ifdef WITH_CONSOLE
		char text[64]; sprintf(text, "@[%i,%i]",
				g->codons->LocationOut_X, g->codons->LocationOut_Y);
		tprintf(LOG_VVV, __func__, text);
#endif
		updateConcentration();
		g = g->next;
	}
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Concentrations updated");
#endif
}

/**
 * Decays the concentrations in each cell in the grid. Complexity O(c*p), with c the amount of
 * cells, p the amount of products. The decay is constant, independent on the concentration.
 */
void decayConcentrations() {
	//#ifdef WITH_CONSOLE
	//	//	tprintf(LOG_INFO, __func__, "Start");
	//#endif
	//	gc = s->gridcells;
	//	do {
	//		struct Product *lp = gc->products;
	//		while (lp != NULL) {
	//			changeConcentration(lp, -s->decay_step);
	//			lp = lp->next;
	//		}
	//		gc = gc->next;
	//	} while (gc != s->gridcells);
}

/**
 * The diffusion rate is half the rate of the decay rate in Bongard's paper. So, this means that
 * this function contains an iterator that makes it execute on half the rate of the decay routine.
 * The concentration of the gene product in question doesn't have an influence on the diffusion
 * amounts. However, the concentration at the source location decrements of course.
 *
 * Complexity O(c*p*l) with c the amount of cells, p the amount of products and l the amount of
 * links between cells or in other words the amount of neighbours.
 *
 * @todo There is also no inter-unit diffusion yet, which should be implemented through inter-robot
 * links. The instruction "gc = lc->to" which switches the current active grid cell to the target
 * cell of the connection and the subsequent command "precalculateChangeConcentration" should in that case be
 * interpreted as a inter-module command involving communication. That's easy to do, in "gc" there
 * should be a "local" versus "remote" field and "precalculateChangeConcentration" should check that field
 * and relay the command if needed (put it on its "task output queue").
 */
void diffuseConcentrations() {
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "New diffusion iteration");
#endif
	struct GridCell *lgc = s->gridcells; int8_t diffuse_amount;
	do {
		struct Product *lp = lgc->products;

		while (lp != NULL) {
			struct GridConnection *lc = lgc->connections;
			diffuse_amount = 0;
			while (lc != NULL) {
				if (lp->concentration > s->diffuse_ratio) {
					gc = lc->to;
#ifdef WITH_CONSOLE
					char text[100];
					sprintf(text, "Change concentration of %i @[%i,%i] with %i. Caused by %i @[%i,%i].",
							lp->id[0],	gc->position->x, gc->position->y, lp->concentration / s->diffuse_ratio,
							lp->concentration, lgc->position->x, lgc->position->y);
					tprintf(LOG_VVVV, __func__, text);
#endif
					struct Product *ltop = getProduct((struct ProductId*)lp->id);
					precalculateChangeConcentration(ltop, lp->concentration / s->diffuse_ratio);
					diffuse_amount += (lp->concentration / s->diffuse_ratio);
				}
				lc = lc->next;
			}
			gc = lgc;
			changeConcentration(lp, -diffuse_amount);
			//			changeConcentration(lp, -diffuse_amount / 2);
			lp = lp->next;
		}
		lgc = lgc->next;
	} while (lgc != s->gridcells);
}

void copyConcentrationsToNew() {
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Copy concentration values");
#endif
	struct GridCell *lgc = s->gridcells;
	do {
		struct Product *lp = lgc->products;
		while (lp != NULL) {
			lp->new_concentration = lp->concentration;
			lp = lp->next;
		}
		lgc = lgc->next;
	} while (lgc != s->gridcells);
#ifdef WITH_CONSOLE
	tprintf(LOG_VVV, __func__, "Concentrations copied");
#endif
}

void avgConcentrationsToCurrent() {
	struct GridCell *lgc = s->gridcells;
	do {
		struct Product *lp = lgc->products;
		while (lp != NULL) {
			lp->concentration = ((uint16_t)lp->new_concentration + lp->concentration) / 2;
			lp = lp->next;
		}
		lgc = lgc->next;
	} while (lgc != s->gridcells);
}


/**
 * The concentrations in the grid cells, when exceeding a concentration incur morphological
 * changes, such as duplicating a neuron from one grid cell to another. Hence, for every
 * gridcell this is executed by calling a routine. Only the lower set of products are
 * phenotypic factors, the top set are regulating factors. Only the phenotypic factors code
 * for morphological changes.
 */
void applyEmbryogenesis() {
	gc = s->gridcells;
	do {
		if (gc->neuron != NULL) {
			struct Product *lp = gc->products;
			while (lp != NULL) {
				if (lp->id[0] < gconf->phenotypicFactors) {
					if (lp->concentration >= s->concentration_threshold) {
						//check if neuron is still there: can be moved by morphological change
						if (gc->neuron != NULL) {
							np = gc->neuron;
#ifdef WITH_CONSOLE
							char text[64];
							sprintf(text, "Apply operation %i in cell [%i,%i]",
									lp->id[0], gc->position->x, gc->position->y);
							tprintf(LOG_VVV, __func__, text);
#endif
							applyMorphologicalChange(lp->id[0]);
						}
					}
				}
				lp = lp->next;
			}
		}
		gc = gc->next;
	} while (gc != s->gridcells);
}

/** @} */

/***********************************************************************************************
 *
 * @name grid_printing
 * The printing routines for the grid.
 *
 ***********************************************************************************************/

#ifdef WITH_CONSOLE

void printConcentrations(uint8_t product_id);


void printAllConcentrations() {
	uint8_t i;
	//	printf("Concentrations of all gene products in grid format\n\n");
	for (i = 0; i < gconf->phenotypicFactors + gconf->regulatingFactors; i++) {
		printf("Gene product %i\n", i);
		printConcentrations(i);
		printf("\n");
	}
}

void printConcentrationsPerRow(uint8_t product_id, uint8_t row_id);

void printAllConcentrationsMultiplePerRow() {
	uint8_t i, j;
	//	printf("Concentrations of all gene products in grid format\n\n");
	for (i = 0; i < gconf->phenotypicFactors + gconf->regulatingFactors; i+=5) {
		for (j = 0; j < s->rows; j++) {
			printConcentrationsPerRow(i, j);
			printConcentrationsPerRow(i+1, j);
			printConcentrationsPerRow(i+2, j);
			printConcentrationsPerRow(i+3, j);
			printConcentrationsPerRow(i+4, j);
			printf("\n");
		}
		printf("\n");
	}
}


void printConcentrationsPerRow(uint8_t product_id, uint8_t row_id) {
	struct GridCell *lgc = getGridCell(0, row_id);
	uint8_t cell_id = row_id * s->columns;
	do {
		struct Product *lp = lgc->products;
		while (lp != NULL) {
			if (lp->id[0] == product_id) break;
			lp = lp->next;
		}
		if (lp != NULL) {
			printf("%3i ", lp->concentration);
		} else {
			printf("    ");
		}
		cell_id++;
		if (!(cell_id % 5)) {
			printf("  ");
			break;
		}
		lgc = lgc->next;
	} while (lgc != s->gridcells);
}

void printConcentrations(uint8_t product_id) {
	struct GridCell *lgc = s->gridcells; uint8_t cell_id = 0;
	do {
		struct Product *lp = lgc->products;
		while (lp != NULL) {
			if (lp->id[0] == product_id) break;
			lp = lp->next;
		}
		if (lp != NULL) {
			printf("%3i ", lp->concentration);
		} else {
			printf("FFF ");
		}
		cell_id++;
		if (!(cell_id % 5)) printf("\n");
		lgc = lgc->next;
	} while (lgc != s->gridcells);
}

void printConcentrationUpdates(uint8_t product_id);

void printAllConcentrationUpdates() {
	uint8_t i;
	//		printf("Concentrations of all gene products in grid format\n\n");
	for (i = 0; i < gconf->phenotypicFactors + gconf->regulatingFactors; i++) {
		printf("Gene product %i\n", i);
		printConcentrationUpdates(i);
		printf("\n");
	}
}

void printConcentrationUpdates(uint8_t product_id) {
	struct GridCell *lgc = s->gridcells; uint8_t cell_id = 0;
	do {
		struct Product *lp = lgc->products;
		while (lp != NULL) {
			if (lp->id[0] == product_id) break;
			lp = lp->next;
		}
		if (lp != NULL) {
			printf("%3i ", lp->new_concentration);
		} else {
			printf("FFF ");
		}
		cell_id++;
		if (!(cell_id % 5)) printf("\n");
		lgc = lgc->next;
	} while (lgc != s->gridcells);
}

void printGrid2() {
	struct Neuron *ln;
	printf("Grid:  ");
	uint8_t i = 0;
	for (i=0; i < s->columns; i++) printf("%d  ", i);
	printf("\n      ");
	for (i=0; i < s->columns; i++) printf("---");
	printf("\n");
	struct GridCell *lgc = s->gridcells;
	i = 0;
	do {
		ln = lgc->neuron;
		if (!(i % 5)) printf("   %d |", i % 5);
		if (ln != NULL) {
			if ((ln->type & TOPOLOGY_MASK) == OUTPUT_NEURON)
				printf(" O ");
			else if ((ln->type & TOPOLOGY_MASK) == INPUT_NEURON)
				printf(" I ");
			else
				printf(" X ");
		} else {
			printf("   ");
		}
		if ((i % 5) == 4) printf("\n");
		lgc = lgc->next; i++;
	} while (lgc != s->gridcells);
	printf("\n");	
}

void printGridToStr(char *string) {
	struct Neuron *ln;
	uint8_t x,y;
	sprintf(string, "Grid:  ");
	for (y=0; y < s->columns; y++) sprintf(string, "%s%d  ", string, y);
	sprintf(string, "%s\n      ", string);
	for (y=0; y < s->columns; y++) sprintf(string, "%s---", string);
	sprintf(string, "%s\n", string);
	for (y=0; y < s->rows; y++) {
		sprintf(string, "%s   %d |", string, y);
		for (x=0;x < s->columns; x++) {
			ln = getGridCell(x,y)->neuron;
			if (ln != NULL) {
				if ((ln->type & TOPOLOGY_MASK) == OUTPUT_NEURON)
					sprintf(string, "%s O ", string);
				else if ((ln->type & TOPOLOGY_MASK) == INPUT_NEURON)
					sprintf(string, "%s I ", string);
				else
					sprintf(string, "%s X ", string);
			} else {
				sprintf(string, "%s   ", string);
			}
		}
		sprintf(string, "%s\n", string);
	}
	sprintf(string, "%s\n", string);

//	printGrid2();
}

void printGrid() {
	struct Neuron *ln;
	uint8_t x,y;
	printf("Grid:  ");
	for (y=0; y < s->columns; y++) printf("%d  ", y);
	printf("\n      ");
	for (y=0; y < s->columns; y++) printf("---");
	printf("\n");
	for (y=0; y < s->rows; y++) {
		printf("   %d |", y);
		for (x=0;x < s->columns; x++) {
			ln = getGridCell(x,y)->neuron;
			if (ln != NULL) {
				if ((ln->type & TOPOLOGY_MASK) == OUTPUT_NEURON)
					printf(" O ");
				else if ((ln->type & TOPOLOGY_MASK) == INPUT_NEURON)
					printf(" I ");
				else
					printf(" X ");
			} else {
				printf("   ");
			}
		}
		printf("\n");
	}
	printf("\n");
}

#ifdef WITH_GNUPLOT

/**
 */
void drawGrid() {
	gnuplot_ctrl *h1;
	h1 = gnuplot_init();
	gnuplot_setstyle(h1, "points");
	uint8_t x,y,z = 0;
	double *x_axis = (double*) calloc(s->columns, sizeof(double));
	double *y_axis = (double*) calloc(s->rows, sizeof(double));
	double *z_axis = (double*) calloc(s->columns * s->rows, sizeof(double));
	for (x=0; x < s->columns; x++) x_axis[x] = x;
	for (y=0; y < s->rows; y++) y_axis[y] = y;
	for (x=0; x < s->columns; x++) {
		for (y=0; y < s->rows; y++) {
			z = x*s->columns + y;
			struct Neuron *ln = getGridCell(x,y)->neuron;
			if (ln != NULL) {
				if ((ln->type & TOPOLOGY_MASK) == OUTPUT_NEURON)
					z_axis[z] = 10;
				else if ((ln->type & TOPOLOGY_MASK) == INPUT_NEURON)
					z_axis[z] = -5;
				else
					z_axis[z] = 5;
			} else {
				z_axis[z] = 0;
			}
		}
	}
	gnuplot_splot(h1,x_axis,y_axis,z_axis,s->columns * s->rows,"Grid");

	//needs to be freed
}

void drawAgainConcentrations(uint8_t product_id, uint16_t fileIndex, gnuplot_ctrl *handle);

void drawAllConcentrations(uint16_t fileIndex) {
	gnuplot_ctrl *handle = gnuplot_init();

	//	gnuplot_cmd(handle, "set size square");
	//	gnuplot_setstyle(handle,"points");
	gnuplot_setstyle(handle,"lines");
	gnuplot_cmd(handle, "set dgrid3d");
	gnuplot_cmd(handle, "set cntrparam levels 10");
	gnuplot_cmd(handle, "set parametric");
	gnuplot_cmd(handle, "set terminal png");
	gnuplot_cmd(handle, "set contour base");
	gnuplot_cmd(handle, "set zrange [0:100]");
	gnuplot_cmd(handle, "set view 60,30");

	gnuplot_cmd(handle, "unset ztic");
	gnuplot_cmd(handle, "set noxtic");
	gnuplot_cmd(handle, "set noytic");
	gnuplot_cmd(handle, "set nokey");
	//	gnuplot_cmd(handle, "set rmargin 5");
	gnuplot_cmd(handle, "set lmargin 10");
	//	gnuplot_cmd(handle, "set tmargin 30");
	//	gnuplot_cmd(handle, "set bmargin 30");

	char filename[64];
	sprintf(filename, "set output 'figures/figure_%03i.png'", fileIndex);
	gnuplot_cmd(handle, filename);

	gnuplot_cmd(handle, "set size 1,1");
	gnuplot_cmd(handle, "set origin 0,0");

	char text[64];
	uint8_t columns = 4, rows = (gconf->phenotypicFactors + gconf->regulatingFactors + columns-1) / columns;
	uint8_t i = 0;
	sprintf(text, "set multiplot layout %i,%i rowsfirst scale 1.8,2.0", rows+1,columns);

	//	tprintf(LOG_VERBOSE, __func__, text);
	gnuplot_cmd(handle, text);
	while (i < gconf->phenotypicFactors + gconf->regulatingFactors) {
		drawAgainConcentrations(i, fileIndex, handle);
		i++;
	}
	gnuplot_cmd(handle, "unset multiplot");
	//	sleep(2);
	gnuplot_close(handle);
}

int8_t file_exist(char *filename) {
	return ( access( filename, F_OK ) != -1 );
}

void drawConcentrations(uint8_t product_id, uint16_t fileIndex) {
	char filename[128];
	sprintf(filename, "figures");
	if (!file_exist(filename)) {
		tprintf(LOG_ERR, __func__, "Directory does not exist!");
		return;
	}
	gnuplot_ctrl *handle = gnuplot_init();
	gnuplot_setstyle(handle,"lines");
	gnuplot_cmd(handle, "set dgrid3d");
	gnuplot_cmd(handle, "set cntrparam levels 10");
	gnuplot_cmd(handle, "set parametric");
	gnuplot_cmd(handle, "set terminal png");
	gnuplot_cmd(handle, "set contour base");
	gnuplot_cmd(handle, "set zrange [0:100]");
	gnuplot_cmd(handle, "set view 60,30");

	sprintf(filename, "set output 'figures/figure_%03i.png'", fileIndex);
	gnuplot_cmd(handle, filename);
	drawAgainConcentrations(product_id, fileIndex, handle);
	gnuplot_close(handle);
}

void drawAgainConcentrations(uint8_t product_id, uint16_t fileIndex, gnuplot_ctrl *handle) {
	uint8_t n = s->columns * s->rows;
	uint8_t x = 0, y = 0, i = 0;
	double *x_axis = (double*) calloc(n, sizeof(double));
	double *y_axis = (double*) calloc(n, sizeof(double));
	double *z_axis = (double*) calloc(n, sizeof(double));

	struct GridCell *lgc = s->gridcells;
	do {
		struct Product *lp = lgc->products;
		while (lp != NULL) {
			if (lp->id[0] == product_id) break;
			lp = lp->next;
		}
		x_axis[i] = x;
		y_axis[i] = y;
		if (lp != NULL) {
			z_axis[i] = lp->concentration;
		} else {
			z_axis[i] = 0;
		}
		lgc = lgc->next;
		i++; x++;
		if (!(i % 5)) { y++; x = 0; }
	} while (lgc != s->gridcells);
	gnuplot_splot(handle, x_axis, y_axis, z_axis, n, "%");
	free(x_axis);
	free(y_axis);
	free(z_axis);
	//	gnuplot_contour_plot(handle,x_axis,y_axis,z_axis,5,5,"Concentrations for product 0");
}

#endif

#endif
