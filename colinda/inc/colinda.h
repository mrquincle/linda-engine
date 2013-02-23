#ifndef COLINDA_H_
#define COLINDA_H_

#ifdef __cplusplus
extern "C" {
#endif 
	
#include <inttypes.h>
	
struct ColindaRuntime {
	struct SyncThreads *sync;
};
	
/**
 * The colinda engine does run on top of an abbey, that can handle a certain amount of parallel tasks
 * and starts with a predefined amount of threads. The first function to be executed can be used to
 * boot the system and if the colinda engine is compiled as a library this hook might be used to do
 * something in user space and use only a subset of the colinda functionality. The pointer to the dna 
 * buffer can be used to reuse the same buffer to load a genome that is actually much bigger. So, this
 * saves some space on the device. The id is used in the simulator mode to send messages to the Linda
 * engine. It might or might not be necessary in the real setting.
 */
struct ColindaConfig {
	uint8_t monk_count;
	uint8_t task_count;
	void *(*boot)(void*);
	
	int16_t dna_buffer_ptr;
	int16_t dna_part_ptr;
	uint8_t id;
};

struct ColindaConfig *clconf;

struct ColindaRuntime *clruntime;

/**
 * The only task that is available for other files.
 */
void *visualize(void *context);

#ifdef __cplusplus
}
#endif 

#endif /*COLINDA_H_*/
