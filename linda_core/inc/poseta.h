/**
 * @file poseta.h
 *
 * A wrapper around the abbey that allows for tasks that call back to a certain function. The
 * term poseta comes from POSET (partial ordered sets) and from a(bbey).
 *
 * @date_created    May 14, 2009
 * @date_modified   May 14, 2009
 * @author          Anne C. van Rossum
 * @project         Replicator
 * @company         Almende B.V.
 * @license         open-source, GNU Lesser General Public License
 */

#ifndef POSETA_H_
#define POSETA_H_

#ifdef __cplusplus
extern "C" {
#endif

void initPoseta();

void poseta_func1_if_func0(void *(*func0)(void *), void *(*func1)(void *));

int dispatch_poseta_task(void *(*func)(void *), void *context, char *taskDesc);

/**
 * Executes two functions at once.
 */
int dispatch_tuple_task(void *(*func0)(void *), void *context0,
						void *(*func1)(void*), void *context1, char *taskDesc);

#ifdef __cplusplus
}
#endif

#endif /*POSETA_H_*/
