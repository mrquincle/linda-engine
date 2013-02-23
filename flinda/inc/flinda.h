#ifndef FLINDA_H_
#define FLINDA_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <linda/ptreaty.h>
#include <linda/infocontainer.h>

#include <inttypes.h>
	
struct FlindaRuntime {
	struct SyncThreads *eosim;
//	struct SyncThreads *eoboot;
};

struct FlindaConfig {
	uint8_t monk_count;
	uint8_t task_count;
	void *(*boot)(void*);
	uint8_t topology_count;
};

struct FlindaHistory {
	struct InfoArray **topologies;
	uint8_t topology_count;
};

struct FlindaConfig *flconf;

struct FlindaRuntime *flruntime;

struct FlindaHistory *flhistory;

#ifdef __cplusplus
}
#endif 

#endif /*FLINDA_H_*/
