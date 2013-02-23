/**
 * @file lindaconfig.h
 * @brief Overall configuration parameters 
 * @author Anne C. van Rossum
 */

#ifndef LINDACONFIG_H_
#define LINDACONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>

#ifndef BOOL
#define BOOL 		uint8_t
#endif

/**
 * The definitions for the different modules will be defined in Project Properties in Eclipse
 * or have to be added on the command line as DMODULE_TOPOLOGY. The defines are still here for
 * reference purposes. 
 */
#define MODULE_TOPOLOGY
#define MODULE_GRID 
//#define MODULE_EVOLUTION
//#define MODULE_FITNESS
//#define MODULE_PHENOTYPE
#define MODULE_EMBRYOGENY
	
//#define WITH_SYMBRICATOR       0
#define WITH_CONSOLE      		1
#define WITH_GNUPLOT	  		1
//#define WITH_TIME              1
#define WITH_GUI 				0
#define WITH_TEST				1
#define WITH_PRINT_DISTRIBUTION  1
	
//#if WITH_CONSOLE == 0
//#undef WITH_CONSOLE
//#endif
//
//#if WITH_GNUPLOT == 0
//#undef WITH_GNUPLOT
//#endif
//	
//#if WITH_SYMBRICATOR == 0
//#undef WITH_SYMBRICATOR
//#endif
//
//#if WITH_TIME == 0
//#undef WITH_TIME	
//#endif
//
#if WITH_TEST == 0
#undef WITH_TIME	
#endif

#if WITH_GUI == 0
#undef WITH_GUI
#endif
	
#if WITH_PRINT_DISTRIBUTION == 0
#undef WITH_PRINT_DISTRIBUTION
#endif
	
#ifdef __cplusplus
}

#endif

#ifdef WITH_TIME
#include <time.h>
#endif

#ifdef WITH_SYMBRICATOR
#define lindaMalloc pvPortMalloc
#define lindaCalloc #warning 
#else
#include <stdlib.h>
#define lindaMalloc malloc
#define lindaCalloc calloc
#endif

#endif /*LINDACONFIG_H_*/
