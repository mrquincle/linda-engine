#ifndef ELINDA_H_
#define ELINDA_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <inttypes.h>
	
#define ELINDA_SIMSTATE_TODO		0x00
#define ELINDA_SIMSTATE_CURRENT		0x01
#define ELINDA_SIMSTATE_DONE		0x10

#define ELINDA_PROCSTATE_DEFAULT	0x00
#define ELINDA_PROCSTATE_STARTING	0x01
#define ELINDA_PROCSTATE_RUNNING	0x10
	
/**
 * Defines hooks end of simulation (eosim) and and of booting procedure (eoboot).
 */
struct ElindaRuntime {
	struct SyncThreads *eosim;
//	struct SyncThreads *eoboot;
};
	
struct ElindaConfig {
	uint8_t simulation_size;
	uint8_t monk_count;
	uint8_t task_count;
	uint8_t generation_count;
	uint8_t generation_id;
	void *(*boot)(void*);
};

/**
 * The robots that have been simulated, the ones that are currently running and the ones
 * that have to be simulated still.
 */
struct AgentElindaContainer {
	uint8_t simulation_state;
	uint8_t process_state;
};

struct ElindaConfig *elconf;

struct ElindaRuntime *elruntime;

#ifdef __cplusplus
}
#endif 

#endif /*ELINDA_H_*/
