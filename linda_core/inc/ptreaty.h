/**
 * @file ptreaty.h
 * @brief The ptreaty library as an extension on the POSIX pthread library.
 * @author Anne C. van Rossum
 */

#ifndef ptreaty_H_
#define ptreaty_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <pthread.h>
	
struct SyncThreads {
	pthread_mutex_t *request;
	pthread_mutex_t *baton;
	pthread_cond_t *signal;
	pthread_cond_t *ack;
	uint8_t predicate;
	uint8_t flags;
};

void ptreaty_init(struct SyncThreads *st);

void ptreaty_free(struct SyncThreads *st);

void ptreaty_add_thread(pthread_t *thread, const char *name);

const char* ptreaty_get_thread_name(pthread_t *thread);

void ptreaty_hoist_flag(struct SyncThreads *st);

uint8_t ptreaty_flag_hoisted(struct SyncThreads *st);

void ptreaty_wait_to_continue(struct SyncThreads *st);

void ptreaty_lower_flag(struct SyncThreads *st);

void ptreaty_make_m_continue(struct SyncThreads *st);

void ptreaty_thread_started(struct SyncThreads *st);

void ptreaty_if_thread_started(struct SyncThreads *st);

void ptreaty_create_threads_start(struct SyncThreads *st);

void ptreaty_create_threads_finish(struct SyncThreads *st);

void ptreaty_init_baton(struct SyncThreads *st);

void ptreaty_make_m_run(struct SyncThreads *st);

void ptreaty_make_m_run_once(struct SyncThreads *st);

void ptreaty_make_m_run_nx(struct SyncThreads *st);

void initWaitbyCondAckbyCond(struct SyncThreads *st);

void ptreaty_wait(struct SyncThreads *st);

void ptreaty_has_run(struct SyncThreads *st);

void ptreaty_make_m_stop(struct SyncThreads *st);

void ptreaty_make_m_just_run(struct SyncThreads *st);

uint8_t ptreaty_stop(struct SyncThreads *st);

void ptreaty_should_be_first(struct SyncThreads *st);

void ptreaty_should_be_later(struct SyncThreads *st);

#ifdef __cplusplus
}
#endif

#endif /*ptreaty_H_*/
