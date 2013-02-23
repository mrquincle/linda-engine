/**
 * @file ptreaty.c
 * @brief The ptreaty library, extension of pthread library.
 * @author Anne C. van Rossum
 *
 * The ptreaty library can be seen as an extension of the pthread library. It is a
 * portable elaboration that should make it easier to work with threads.
 *
 * In the simulator a so-called "baton" is used, the object in a relay race. It
 * makes sure that there is always some thread doing something. And that never all
 * threads are stalled.
 *
 * For some background on threads, see the bottom of this file.
 */

#include <pthread.h>
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <ptreaty.h>
#include <log.h>
#include <stdlib.h>
#include <bits.h>

/***********************************************************************************************
 *
 * @name ptreaty
 * Framework to make it easier to work with POSIX threads.
 *
 ***********************************************************************************************/

/************************************************************************************************
 *                      Data Structures
 ************************************************************************************************/

struct ThreadName;

struct ThreadName {
	pthread_t *thread;
	char name[64];
	struct ThreadName *next;
};

/************************************************************************************************
 *                      Global variables
 ************************************************************************************************/
struct ThreadName *threadnames;

/***********************************************************************************************
 *
 * @name ptreaty_names
 * Names for threads. Is also used by the logging facility.
 *
 ***********************************************************************************************/

/**
 * Add a thread and a name to a linked list. There is no error checking upon adding threads
 * multiple times.
 */
void ptreaty_add_thread(pthread_t *thread, const char *name) {
	struct ThreadName *ltn;
	if (threadnames == NULL) {
		threadnames = malloc(sizeof(struct ThreadName));
		ltn = threadnames;
	} else {
		ltn = threadnames;
		while (ltn->next != NULL) {
			ltn = ltn->next;
		}
		ltn->next = malloc(sizeof(struct ThreadName));
		ltn = ltn->next;
	}
	ltn->thread = thread;
	sprintf(ltn->name, "%s", name);
	ltn->next = NULL;
}

/**
 * Returns the name of the given thread, if it is added before by ptreaty_add_thread. If
 * it can not be found "Unknown thread" is returned as name.
 */
const char* ptreaty_get_thread_name(pthread_t *thread) {
	struct ThreadName *ltn;
	ltn = threadnames;
	while (ltn != NULL) {
		if (pthread_equal(*ltn->thread, *thread)) break;
		ltn = ltn->next;
	}
	if (ltn == NULL) return "Unknown thread";
	return ltn->name;
}

/***********************************************************************************************
 *
 * @name init_or_free
 * Initializes or frees the content of the SyncThread object.
 *
 ***********************************************************************************************/

void ptreaty_init(struct SyncThreads *st)  {
	st->baton = malloc(sizeof(pthread_mutex_t));
	st->request = malloc(sizeof(pthread_mutex_t));
	st->ack = malloc(sizeof(pthread_cond_t));
	st->signal = malloc(sizeof(pthread_cond_t));
	pthread_mutex_init (st->baton, NULL);
	pthread_mutex_init (st->request, NULL);
	pthread_cond_init (st->ack, NULL);
	pthread_cond_init (st->signal, NULL);
	st->flags = 0;
	st->predicate = 0;
}

/**
 * The content of the SyncThread structure is deallocated.
 */
void ptreaty_free(struct SyncThreads *st) {
	pthread_mutex_destroy(st->baton);
	pthread_mutex_destroy(st->request);
	pthread_cond_destroy(st->ack);
	pthread_cond_destroy(st->signal);
	free(st->baton);
	free(st->request);
	free(st->ack);
	free(st->signal);
}

/***********************************************************************************************
 *
 * @name ptreaty_create_threads
 * Create threads and make sure that they are started.
 *
 ***********************************************************************************************/

/**
 * Execute this routine before ptreaty_if_thread_started. It locks the baton.
 */
void ptreaty_create_threads_start(struct SyncThreads *st) {
	pthread_mutex_lock(st->baton);
}

/**
 * Execute this routine after thread creation, and before ptreaty_create_threads_finish. It waits for
 * a "thread created" signal and gets the baton back.
 */
void ptreaty_if_thread_started(struct SyncThreads *st) {
	pthread_cond_wait(st->signal, st->baton);
}

/**
 * Execute this routine after ptreaty_if_thread_started to release the baton to the application.
 */
void ptreaty_create_threads_finish(struct SyncThreads *st) {
	pthread_mutex_unlock(st->baton);
}

/**
 * Execute this routine first in the new created thread. It initialize the baton. And sets
 * the predicate to 0, which means that there are no signals arrived for whatever waiting
 * routine that might be implemented later.
 */
void ptreaty_init_baton(struct SyncThreads *st) {
	tprintf(LOG_VV, __func__, "lock baton");
	pthread_mutex_lock(st->baton);
	tprintf(LOG_VV, __func__, "baton locked");
	st->predicate = 0;
}

/**
 * Execute this after ptreaty_init_baton in the just created thread. It sends a "thread created"
 * signal. Be sure to return the baton in the new created thread by either a call to ptreaty_wait_to_continue
 * or to ptreaty_return_baton.
 */
void ptreaty_thread_started(struct SyncThreads *st) {
	pthread_cond_signal(st->signal);
}

/**
 * Can be used to return the baton directly, for example after ptreaty_thread_started when a
 * routine does have no need to communicate via signals with the application anymore.
 */
void ptreaty_return_baton(struct SyncThreads *st) {
	pthread_mutex_unlock(st->baton);
}


/***********************************************************************************************
 *
 * @name ptreaty_handshakes
 * Certain protocols, or treaties.
 *
 ***********************************************************************************************/

/**
 * The ptreaty_wait routine can be used to wait till some party indicates that it may
 * continue. It returns the baton to the application. The interested party is sure of
 * a waiting thread to hear the "signal" command if this routine is surrounded by
 * ptreaty_hoist_flag and ptreaty_lower_flag routines.
 */
void ptreaty_wait(struct SyncThreads *st) {
	while (st->predicate == 0) {
		tprintf(LOG_VV, __func__, "Wait for signal");
		pthread_cond_wait(st->signal, st->baton);
	}
	tprintf(LOG_VV, __func__, "Signal came");
	st->predicate--;
}

/**
 * This routine can be used with ptreaty_wait to implement a handshake. It has to be called
 * before ptreaty_wait.
 */
void ptreaty_hoist_flag(struct SyncThreads *st) {
	tprintf(LOG_VV, __func__, "Wait for lock");
	pthread_mutex_lock(st->request);
	tprintf(LOG_VV, __func__, "Obtained lock");
}

/**
 * This routine can be used with ptreaty_wait_to_continue to implement a handshake. It has to be called
 * after ptreaty_wait.
 */
void ptreaty_lower_flag(struct SyncThreads *st) {
	tprintf(LOG_VV, __func__, "Release lock");
	pthread_mutex_unlock(st->request);
}

/**
 * Additional routine that can test if flag is hoisted.
 */
uint8_t ptreaty_flag_hoisted(struct SyncThreads *st) {
	return (pthread_mutex_trylock(st->request) == EBUSY);
}

/**
 * This routine makes the waiting thread - using ptreaty_wait - run. It does not garantee
 * that that thread will run immediately. It does neither garantee that the thread will
 * run before a second call to ptreaty_make_m_continue comes.
 *
 * To ensure that the waiting thread will at least run, use ptreaty_make_m_run_once. And on
 * the waiting thread side call ptreaty_has_run.
 *
 * This routine assumes the baton is not yet obtained, if the baton is already there, use
 * ptreaty_make_m_just_run instead.
 */
void ptreaty_make_m_run(struct SyncThreads *st) {
	if (ptreaty_flag_hoisted(st)) {
		tprintf(LOG_VV, __func__, "Lock baton");
		pthread_mutex_lock(st->baton);
		tprintf(LOG_VV, __func__, "Make 'm run");
		st->predicate++;
		pthread_cond_signal(st->signal);
		tprintf(LOG_VV, __func__, "Unlock baton");
		pthread_mutex_unlock(st->baton);
	} else {
		pthread_mutex_unlock(st->request);
	}
}

/**
 * Works only if there is one thread using the ptreaty_should_be_later routine. If there
 * are multiple guys waiting, we are in trouble.
 */
void ptreaty_should_be_first(struct SyncThreads *st) {
	tprintf(LOG_VERBOSE, __func__, "Lock request");
	pthread_mutex_lock(st->request);
	tprintf(LOG_VERBOSE, __func__, "Signal");
	pthread_cond_broadcast(st->signal);
	tprintf(LOG_VERBOSE, __func__, "Unlock");
	if (pthread_mutex_trylock(st->baton) == EBUSY) {
		pthread_mutex_unlock(st->request);
		return;
	}
	pthread_mutex_unlock(st->baton);
}

void ptreaty_should_be_later(struct SyncThreads *st) {
	if (pthread_mutex_trylock(st->request) == EBUSY) {
		//busy, means it is locked somewhere and somehow, so it's safe to continue
	} else {
		pthread_mutex_lock(st->baton);
		tprintf(LOG_DEBUG, __func__, "Wait for first routine");
		pthread_cond_wait(st->signal, st->request);
//		pthread_mutex_lock(st->request);
		tprintf(LOG_VERBOSE, __func__, "Execution continues");
	}
}

/**
 * This routine assumes the baton is already there. If the ptreaty_wait routine is neatly
 * surrounded by the ptreaty_hoist_flag and ptreaty_lower_flag it doesn't make sense to
 * check for ptreaty_flag_hoisted, because it will be hoisted of course.
 */
void ptreaty_make_m_just_run(struct SyncThreads *st) {
	st->predicate++;
	pthread_cond_signal(st->signal);
}

/**
 * This routine is like ptreaty_make_m_continue, but ensures that the waiting thread will at least
 * run once. On the waiting thread side it needs ptreaty_has_run. It does not assume that
 * the baton is already obtained and will block till it gets it. As with ptreaty_make_m_continue,
 * make sure blocking is no problem. Or use ptreaty_make_m_just_run or ptreaty_make_m_run_nx
 * instead.
 */
void ptreaty_make_m_run_once(struct SyncThreads *st) {
	tprintf(LOG_VV, __func__, "Lock baton");
	uint8_t unlock = (pthread_mutex_lock(st->baton) != EDEADLK);
	if (!unlock)
		tprintf(LOG_WARNING, __func__, "Tries to lock owned mutex");
	st->predicate++;
	if (st->predicate >= 2)
		tprintf(LOG_ALERT, __func__, "Predicate value should be 1");
	pthread_cond_signal(st->signal);
	if (st->predicate != 0) {
		pthread_cond_wait(st->ack, st->baton);
	}
	if (unlock) pthread_mutex_unlock(st->baton);
}

/**
 * This routine goes together with ptreaty_make_m_run_once.
 */
void ptreaty_has_run(struct SyncThreads *st) {
	pthread_cond_signal(st->ack);
}

/**
 * This routine is a wrapper around ptreaty_make_m_run_once, that enables the signaling
 * thread to signal multiple times, without blocking. The signaling thread only blocks
 * when the waiting thread is in the waiting condition. It is the responsibility of the
 * waiting thread to acknowledge this routine before getting blocked for something else.
 */
void ptreaty_make_m_run_nx(struct SyncThreads *st) {
	if (ptreaty_flag_hoisted(st)) {
		ptreaty_make_m_run_once(st);
	} else {
		ptreaty_make_m_just_run(st);
	}
}

/**
 * This routine is the same as ptreaty_wait, but uses another signal. So
 * ptreaty_wait and ptreaty_wait_to_continue can be used in parallel.
 *
 * @todo Use a conditional array, and an additional parameter for the functions rather
 * than misusing "ack" for it.
 */
void ptreaty_wait_to_continue(struct SyncThreads *st) {
	tprintf(LOG_VERBOSE, __func__, "Wait for ack signal");
	pthread_cond_wait(st->ack, st->baton);
	tprintf(LOG_VERBOSE, __func__, "Received ack signal");
}

/**
 * Goes together with ptreaty_wait_to_continue. It can not signal threads waiting by the
 * ptreaty_wait function.
 */
void ptreaty_make_m_continue(struct SyncThreads *st) {
	if (ptreaty_flag_hoisted(st)) {
		tprintf(LOG_VERBOSE, __func__, "Lock baton");
		pthread_mutex_lock(st->baton);
		tprintf(LOG_VERBOSE, __func__, "Make 'm continue");
		pthread_cond_signal(st->ack);
		tprintf(LOG_VERBOSE, __func__, "Unlock baton");
		pthread_mutex_unlock(st->baton);
	} else {
		pthread_mutex_unlock(st->request);
	}
}

/***********************************************************************************************
 *
 * @name ptreaty_finalization
 * Ending threads.
 *
 ***********************************************************************************************/

/**
 * The end of the application, probably the baton is not there yet, so it needs first to
 * be obtained. It goes together with ptreaty_stop.
 */
void ptreaty_make_m_stop(struct SyncThreads *st) {
	RAISE(st->flags, 1);
	pthread_mutex_lock(st->baton);
	st->predicate++;
	pthread_cond_signal(st->signal);
	pthread_mutex_unlock(st->baton);
}

/**
 * Goes together with ptreaty_make_m_stop. Releases the baton.
 */
uint8_t ptreaty_stop(struct SyncThreads *st) {
	if (RAISED(st->flags, 1)) {
		pthread_mutex_unlock(st->baton);
		return 1;
	}
	return 0;
}

/**
 * Some background on threads might be handy. :-) The online source at
 * http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html explains a lot if
 * you never encountered threads or this specific implementation, pthreads, before.
 * Nothing to be ashamed of by the way.
 *
 * For example it says that each thread does have its own set of registers, stack for
 * local variables and return addresses and stack pointer. From this it is possible
 * to deduce that multiple threads can call a function and that they all get their
 * own context. They can't ruin each others local variables.
 *
 * Global variables however can be ruined. One thread can set a global variable to
 * a certain value, while the other expect it to be remain the same. At the site
 * http://www.cs.cf.ac.uk/Dave/C/node29.html you can find another text in which is
 * described how thread-specific data keys can store global variables.
 *
 * In the current implementation a global variable stays global and thread transitions
 * are controlled. So, no thread-specific data keys are used. This is only possible
 * by a global mutex for the simulator, the baton: "sim->state". Only when this mutex
 * is obtained it is possible to change the global variable "se", which stands for the
 * current simulated entity. And before the mutex is released again, the party that
 * changed "se" has the responsibility to set "se" to the old value.
 *
 * There might be pros and cons for both methods. For example it is not clear if the
 * thread-specific data keys can be accessed by a kind of general thread. It would
 * need a reimplementation of the postman inbox that sends TCP/IP messages to the
 * individual "se" inboxes in the current implementation.
 */
