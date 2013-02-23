/**
 * @file poseta.c
 *
 * A wrapper around the abbey that allows for tasks that call back to a certain function. The
 * term poseta comes from POSET (partial ordered sets) and from a(bbey). Normal abbey tasks are
 * limited entities that can enforce a subsequent task to be executed only by explicitly calling
 * it. In other words, there is no possibility for exogeneous coordination. For example, the
 * execution of an emailAdminTask always after a receiveMessageTask, even without the latter
 * knowing that this needs to be done. Of course this can also be solved by adding emailAdminTask
 * as a parameter. However, this becomes cumbersome when a task evokes an entire sequences of
 * daughter tasks in which only the last one needs to evoke this callback task. This became
 * obvious in the implementation of TCP/IP routines.
 *
 * Another type of task, called a TupleTask is defined. In some cases it might be necessary not
 * to give up execution of control. Subsequent tasks are then executed by the same monk.
 *
 * @date_created    May 14, 2009
 * @date_modified   May 14, 2009
 * @author          Anne C. van Rossum
 * @project         Replicator
 * @company         Almende B.V.
 * @license         open-source, GNU Lesser General Public License
 */

#include <ptreaty.h>
#include <abbey.h>
#include <stdlib.h>
#include <poseta.h>
#include <inttypes.h>
#include <string.h>
#include <log.h>
#include <stdio.h>

/***********************************************************************************************
 *
 * @name poseta_structs
 * Wrapper around the abbey
 *
 ***********************************************************************************************/

/**
 * Just two tasks wrapped in one. This makes sure that the same monk is executing
 * the task if the method/task tuple_task() is dispatched.
 */
struct TupleTask {
	//! A pointer to a function to be executed.
	void *(*func0)(void *);
	//! A pointer to the arguments of the to be executed function.
	void *context0;
	//! A pointer to the function to be executed afterwards.
	void *(*func1)(void *);
	//! A pointer to the arguments of the post-function
	void *context1;
};

/**
 * The same as a TupleTask, but now with the second routine specified as a condition
 * in the form of a "treaty". Those "treaties" use several mutexes and conditionals
 * from a SyncThread struct, to implement synchronization primitives, like signals
 * and handshakes. The order field defines if first the treaty is checked and then
 * the function is executed (order=1), or the other way around (order=0).
 */
struct PosetaTask {
	//! A pointer to a function to be executed.
	void *(*func)(void *);
	//! A pointer to the arguments of the to be executed function.
	void *context;
	//! A pointer to the treaty
	void (*treaty)(struct SyncThreads *);
	//! A pointer to the arguments of the treaty
	struct SyncThreads *st;
	//! Order
	uint8_t order;
};

/**
 * Multiple conditions can not be registered on one task. For that you would need to define
 * multiple tasks.
 * @todo This needs to be rewritten. Multiple conditions are not straightforward to
 * implement. Most logical is not to call exec(context) each time a condition is met, but
 * to check a bunch of prerequisites/conditions, only then execute a function and end with
 * a series of post-conditions. Perhaps it is not necessary to really define multiple
 * conditions on a routine, but at least every function should be able to have a precondition
 * as well as a postcondition.
 */
struct Condition {
	//! A condition is indexed with void function pointers.
	void *(*name)(void *);
	//! A condition index defines the type of condition.
	uint8_t condition_index;
	//! The function to be executed. It's most probably a poseta_task routine.
	void *(*exec)(void *);
	//! The context to the to be executed function. Most probably a PosetaTask.
	void *context;
	//! The conditions are a linked list.
	struct Condition *next;
};

// doesn't work just by having condition blocks, see the to do above
//struct ConditionBlock {
//	struct Condition *cond;
//	uint8_t count;
//	struct ConditionBlock *next;
//};

/**
 * Configuration parameters. Contains the head of the linked list of conditions.
 */
struct PosetaConfig {
	struct Condition *cond;
};

//! Global variable to store the configuration
struct PosetaConfig *posconf;


/***********************************************************************************************
 * Function declarations
 ***********************************************************************************************/

/**
 * Returns the condition with the given name from the linked list.
 */
struct Condition *getCondition(void *(*name)(void*));

/**
 * Adds a condition to the linked list.
 */
void addCondition(struct Condition *cond);

/***********************************************************************************************
 *
 * @name poseta_routines
 *
 * Condition management routines
 *
 ***********************************************************************************************/

/**
 * Allocates memory for the configuration struct and initializes the list pointer.
 */
void initPoseta() {
	posconf = malloc(sizeof(struct PosetaConfig));
	posconf->cond = NULL;
}

/**
 * Adds a condition to the linked list. It is checked if the condition with this name
 * already exists, and in that case an error is thrown and the condition is not added.
 */
void addCondition(struct Condition *cond) {
	//first condition ever
	struct Condition *lc = posconf->cond;
	if (lc == NULL) {
		tprintf(LOG_VERBOSE, __func__, "Add first condition");
		lc = posconf->cond = cond;
		lc->next = NULL;
		return;
	}
	if (getCondition(cond->name) != NULL) {
		tprintf(LOG_ALERT, __func__, "Condition exists already");
		return;
	}
	tprintf(LOG_VERBOSE, __func__, "Success");
	cond->next = lc;
	posconf->cond = cond;
}

/**
 * Retrieves a condition from the linked list. If the condition is not found a NULL pointer
 * is returned.
 */
struct Condition *getCondition(void *(*name)(void*)) {
	struct Condition *lc = posconf->cond;
	if (lc == NULL) {
		tprintf(LOG_WARNING, __func__, "No conditions at all...");
		return NULL;
	}
	uint8_t i = 0;
	do {
		//strncmp doesnt work on function pointers of course
		if(*lc->name == *name) return lc;
		lc = lc->next;
		i++;
	} while (lc != NULL);
//	char text[64]; sprintf(text, "Condition not found in list of %i items", i);
//	tprintf(LOG_VERBOSE, __func__, text);
	return NULL;
}

/***********************************************************************************************
 *
 * @name poseta_tasks
 *
 * Tasks in a POSET context.
 *
 ***********************************************************************************************/

/**
 * Expects context to be a TupleTask. Casts it like that and executes the task in the order
 * func0 and then func1. It is guaranteed to be executed by the same monk and in this order.
 */
void *tuple_task(void *context) {
	struct TupleTask *tt = (struct TupleTask*)context;
	tt->func0(tt->context0);
	tt->func1(tt->context1);
	return NULL;
}

/**
 * Expects context to be a PosetaTask. Casts it like that and executes the task in the order
 * specified by the order parameter in the PosetaTask struct.
 */
void *poseta_task(void *context) {
	struct PosetaTask *pt = (struct PosetaTask*)context;
	if (!pt->order) {
		pt->func(pt->context);
		pt->treaty(pt->st);
	} else {
		pt->treaty(pt->st);
		pt->func(pt->context);
	}
	return NULL;
}

/***********************************************************************************************
 *
 * @name poseta_functions
 *
 * Functions that can be called and enforce certain constraints on the tasks to be executed.
 *
 ***********************************************************************************************/

/**
 * Executes func1 only when func0 has been executed before. This only indicates the
 * dependency. func1 is not executed automatically. The execution of the functions
 * needs to be done via the dispatch_poseta_task routines, and not via the normal abbey
 * routines like dispatch_described_task or dispatch_task. Only the poseta variant
 * knows how to set the "trap" and catch it later again.
 *
 * This routine reformulates the condition into a function that is called first when the
 * dispatch_poseta_task routine is executed.
 */
void poseta_func1_if_func0(void *(*func0)(void *), void *(*func1)(void *)) {
	//register func0, execute func(hoist, func0) when encountered
	struct PosetaTask *pt0 = malloc(sizeof(struct PosetaTask));
	pt0->func = func0;
	pt0->treaty = ptreaty_should_be_first;
	pt0->st = malloc(sizeof(struct SyncThreads));
	pt0->order = 0;
	ptreaty_init(pt0->st);
	struct Condition *cond0 = malloc(sizeof(struct Condition));
	cond0->condition_index = 0;
	cond0->exec = poseta_task;
	cond0->context = (void*)pt0;
	cond0->name = func0;
	addCondition(cond0);

	//register func1, execute func(hoisted, func1) when encountered
	struct PosetaTask *pt1 = malloc(sizeof(struct PosetaTask));
	pt1->func = func1;
	pt1->treaty = ptreaty_should_be_later;
	pt1->st = pt0->st;
	pt1->order = 1;
	struct Condition *cond1 = malloc(sizeof(struct Condition));
	cond1->condition_index = 1;
	cond1->exec = poseta_task;
	cond1->context = (void*)pt1;
	cond1->name = func1;
	addCondition(cond1);
}

/**
 * Executes always func1 after detection of func0 being executed. This not only indicates
 * a dependency, but it also automatically executes func1 when func0 is dispatched. So,
 * what is the context of this second function? Mmm... that is not known... So, this
 * does work only with a second function without context, or a first function providing
 * direct context for the second function.
 */
void poseta_func0_then_func1(void *(*func0)(void *), void *(*func1)(void *)) {
	//register func0, execute func(func0,func1) when encountered
}

/**
 * Dispatches a task. This just works as dispatch_described_task or dispatch_task, but
 * now checks eventual conditions set beforehand. It is unnecessary to call this version
 * if not a routine like poseta_func1_if_func0, or another such a routine is called
 * before. The context wouldn't be set correctly in that case, and the function falls
 * back on just execution of that task.
 */
int dispatch_poseta_task(void *(*func)(void *), void *context, char *taskDesc) {
	tprintf(LOG_VERBOSE, __func__, taskDesc);
	struct Condition *lc = getCondition(func);
	if (lc == NULL) {
		char text[128]; sprintf(text, "Task \"%s\" is not registered before!", taskDesc);
		tprintf(LOG_WARNING, __func__, text);
		goto fallback;
	}
	switch (lc->condition_index) {
	case 0: case 1:
		((struct PosetaTask*)lc->context)->context = context;
		break;
	default:
		;
	}

fallback:
	return dispatch_described_task(lc->exec, lc->context, taskDesc);
}

/**
 * Executes two functions. C doesn't allow for functional compositions, but this looks a
 * little bit like that... Both functions are executed by a monk, and the READY flag is set
 * only when both are finished, and doesn't come down temporarily. The functions are
 * guaranteed to be executed by the same monk.
 */
int dispatch_tuple_task(void *(*func0)(void *), void *context0,
		void *(*func1)(void*), void *context1, char *taskDesc) {
	struct TupleTask *tt = malloc(sizeof(struct TupleTask));
	tt->func0 = func0;
	tt->context0 = context0;
	tt->func1 = func1;
	tt->context1 = context1;
	return dispatch_described_task(tuple_task, (void*)tt, taskDesc);
}

