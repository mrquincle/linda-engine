/************************************************************************/
/*! \file
 *
 * \ingroup AbbeyCore
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>  //for variadic functions
#include <string.h>
#include <abbey.h>
#include <ptreaty.h>

//! Task slot is ready to be filled with new task
#define TS_READY    0
//! Task slot is being filled with new task
#define TS_CREATING 1
//! Task (slot) is open to be handled by a monk
#define TS_OPEN     2
//! Task (slot) is being handled by a monk
#define TS_BUSY     3

//! The debug flag that prints more or less information
#define DEBUG_ABBEY 0
//! Boolean true=1 for readability
#define true 1
//! Task description parameter is a char array of fixed length
#define MAX_TASK_DESCRIPTION_LEN 64

/**
 * Pointer Algorithmitic Reminder...
 * A function pointer is a pointer. One that points to the address of a
 * function. The monk does not know which function it will get to handle,
 * so that's where function pointers become handy. The extra dots add
 * extra voodoo. In "void *(*foo)(int *);" the key is to read inside-out,
 * *foo points to a function that returns a void pointer and takes a
 * pointer to an int. The void pointer is not the carte blanch in pointer
 * land. The thing it refers to has an undetermined length and undetermined
 * "syntax". There ain't such things like big-endian voids. ;-) Information
 * in other variables can be used to cast back to the appropriate pointer
 * type.a
 * @see http://www.cplusplus.com/doc/tutorial/pointers.html
 */

/**
 * The task has a state, a function pointer and a context pointer. The latter
 * is used on the later evocation moment to be passed as argument to the
 * function.
 */
typedef struct {
	//! The state field flags if a task slot is occupied, a monk is busy, etc.
	int state;
	//! A pointer to a function to be executed.
	void *(*func)(void *);
	//! A void pointer to the arguments of the to be executed function.
	void *context;
	//! Description of the task.
	char description[MAX_TASK_DESCRIPTION_LEN];
} Task;

/**
 * This abbey uses threads from the pThread library. The pointer pthread_t
 * points to the first thread in the thread pool. Its size is determined once
 * depending on the number of monks in the abbey_init method. The task pointer
 * points to a bunch of tasks to be performed.
 */
static pthread_t *threadId;
static Task *task;
static int nofMonks, nofTasks;
static pthread_mutex_t abbeyMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  abbeyCond  = PTHREAD_COND_INITIALIZER;
static int taskBufferContent = 4, *taskBuffer = &taskBufferContent;
static int dedicatedTaskBuffer = 2;
static int amountOfMonksBusy = 0;

static void *increaseTaskBuffer(void* context);
static void *monk(void *arg);

#if DEBUG_ABBEY > 0
void print_tasks() {
	int i;
	printf("Tasks:\n");
	for(i = dedicatedTaskBuffer; i < nofTasks; i++) {
		printf(" %i: %s\n", i, task[i].description);
	}
}
#endif

/*! \brief
 * In setTaskState a monk wants to change the state of a task. It may
 * have executed it, it may be busy with it, whatever. To prevent that
 * multiple monks start on the same task, there is a mutex within this
 * method. Although a monk may have choosen to do this task, it may
 * encounter a lock, because another monk is busy with it. The mutex is
 * "abbey broad". This means that it is not possible that a monk starts
 * with a task during this period in testAndSetTask.
 */
static int set_task_state(int taskId, int state) {
	pthread_mutex_lock(&abbeyMutex);
#if DEBUG_ABBEY > 0
	printf("Abbey: Set task %d to state %d.\n", taskId, state);
#endif
	task[taskId].state = state;
	pthread_cond_broadcast(&abbeyCond);
	pthread_mutex_unlock(&abbeyMutex);
	return 0;
}

/*! \brief
 * The testAndSetTask routine is called from two different places. From
 * the routines *monk and dispatchTask. *monk sets a task state from
 * open to busy. The boolean mayFail is set to true, so failure in the
 * sense of not finding a task in an open state, does result in a
 * conditional wait, not in an exception. dispatchTask sets a task state
 * from ready to creating, and calls afterwards setTaskState to set it
 * to open. Between that it assigns the context, func and description
 * attributes of the task.
 *
 * @remark There is a habit to use while(-1) instead of while(1), that is
 * less naturally read as while(true).
 */
static int find_task_and_change_state(int state, int newState, int mayFail) {
	int i;
	pthread_mutex_lock(&abbeyMutex);
	while(true) {
		for(i = dedicatedTaskBuffer; i < nofTasks; i++) {
			if(task[i].state == state) {
#if DEBUG_ABBEY > 0
				printf("Abbey: Set task %d from state %d to state %d.\n", i, state, newState);
#endif
				task[i].state = newState;
				pthread_cond_broadcast(&abbeyCond);
				pthread_mutex_unlock(&abbeyMutex);
				return i;
			}
		}
		if(mayFail || !dedicatedTaskBuffer)
			pthread_cond_wait(&abbeyCond, &abbeyMutex);
		else {
			printf("Abbey Error: Find_task_and_change_state(): Failed to find a ");
			printf("task in the state %d for new state %d.\n", state, newState);
			printf("This probably means that tasks are input in a faster rate than ");
			printf("the monks can process.\n");

#if DEBUG_ABBEY > 0
			print_tasks();
#endif
			//of course no exception, but counter measures
			dedicatedTaskBuffer = 0;
//			pthread_mutex_unlock(&abbeyMutex);
			//dispatch_task(*increaseTaskBuffer, taskBuffer);
			increaseTaskBuffer(taskBuffer);
			pthread_mutex_unlock(&abbeyMutex);
			//printf("Dedicated task did run.\n\n");

		}
	}
}

/*! \brief Task buffer increase
 *
 * Increases the task buffer with a certain amount, the void pointer should be
 * castable to an int pointer.
 */
static void *increaseTaskBuffer(void* context) {
	int amount = *(int*) context;
	printf("The task buffer is increased from %d to %d\n", nofTasks, nofTasks + amount);
	Task *newtask = (Task *) realloc(task, sizeof(Task) * (nofTasks + amount));
	if (newtask == NULL) {
		printf("Couldn't increase task buffer...\n");
		return NULL;
	}
	task = newtask;
	nofTasks += amount;
	dedicatedTaskBuffer = 2;
	printf("Buffer is increased now...\n");
	return NULL;
}

/**
 * Adds one monk to the abbey.
 * @todo: The reallocation procedure is incorrect apparently; error: 
 *    *** glibc detected *** colinda: realloc(): invalid next size: 0x08c14308 ***
 *    The amount of monks is increased from 16 to 17
 */
static void *addMonk(void* context) {
	printf("The amount of monks is increased from %d to %d\n", nofMonks, nofMonks+1);
	pthread_t *newthreadId = (pthread_t *) 
			realloc(threadId, (sizeof(pthread_t) * (nofMonks+1)));
	if (newthreadId == NULL) {
		printf("Couldn't add monk to memory...\n");
		return NULL;
	}
	threadId = newthreadId;
	nofMonks++;
	if(pthread_create(&threadId[nofMonks], NULL, monk, NULL) != 0) {
		printf("Abbey Error: Failed to create thread!\n");
		return NULL;
	}
	char name[64];
	sprintf(name, "Monk %i", nofMonks);
	ptreaty_add_thread(&threadId[nofMonks], name);
#if DEBUG_ABBEY > 1
	printf("Abbey: Create thread 0x%lx (number %d).\n", threadId[nofMonks], nofMonks);
#endif

	printf("Monk has been added.\n");
	dedicatedTaskBuffer = 2;
	return NULL;
}

/*! \brief Monk as stateful thread
 *
 * The monk iterates the tasks to find one that is open, sets it state flag to
 * busy and calls func(context). Afterwards it sets the task state to ready.
 * Job finished.
 */
static void *monk(void *arg) {
	int taskId;

	while(true) {
		taskId = find_task_and_change_state(TS_OPEN, TS_BUSY, 1);

		amountOfMonksBusy++;
#if DEBUG_ABBEY > 0
		printf("Abbey: Monk 0x%lx starts to work on task %d.\n", pthread_self(), taskId);
#endif

#if DEBUG_ABBEY > 0
		if(task[taskId].description[0] != '\0')
			printf("Abbey: Executing Task: %s\n", task[taskId].description);
#endif

		//the real work! :-)
		task[taskId].func(task[taskId].context);

		set_task_state(taskId, TS_READY);


#if DEBUG_ABBEY > 0
		printf("Amount of monks busy is %d, total is %d\n", amountOfMonksBusy, nofMonks);
#endif
		pthread_mutex_lock(&abbeyMutex);
		if (amountOfMonksBusy >= nofMonks) {
			dedicatedTaskBuffer = 0;
//			dispatch_task(*addMonk, NULL);
			addMonk(NULL); //can not be dispatched
		}
		pthread_mutex_unlock(&abbeyMutex);
		
#if DEBUG_ABBEY > 0
		printf("Abbey: Monk 0x%lx is free again.\n", pthread_self());
#endif
		amountOfMonksBusy--;


	}
	return NULL;
}

/*! \brief Allocation of buffer of tasks and pool of monks.
 *
 * The initialization concerns the allocations of a set of tasks, where
 * calloc allocates such a block which individual sizes of Task. Similarly
 * a set of threads is created. Each thread that is created gets a
 * reference to a monk routine. So, it are a bunch of the same (monk)
 * routines that are eternally executed in parallel.
 */
int initialize_abbey(int monkCount, int taskBuffer) {
	unsigned int i;
	nofTasks = taskBuffer;
	nofMonks = monkCount;

	task = (Task *)calloc(nofTasks, sizeof(Task));
	threadId = (pthread_t *) calloc(nofMonks, sizeof(pthread_t));
#if DEBUG_ABBEY > 1
	printf("Abbey: Initialize abbey from thread: 0x%lx.\n", pthread_self());
#endif
	for(i = 0; i < nofMonks; i++) {
		if(pthread_create(&threadId[i], NULL, monk, NULL) != 0) {
			printf("Abbey Error: Failed to create thread!\n");
			return -1;
		}
		char name[64];
		sprintf(name, "Monk %i", i);
		ptreaty_add_thread(&threadId[i], name);

#if DEBUG_ABBEY > 1
		printf("Abbey: Create thread 0x%lx (number %d).\n", threadId[i], i);
#endif
	}
	return 0;
}

/*! \brief Dispatch a task to the task buffer.
 *
 * The dispatch routine can eat a description, nothing trendy about that
 * thing. It iterates the tasks for ready ones. When it finds one it
 * overwrites that task (that is of course executed before) with the new
 * func(context) thing to execute. The state is set to open, and a monk
 * may start it whenever pan happens.
 *
 * @remark The ugly names are of course because ANSI C does not allow
 * function polymorphism / overloading.
 */
int dispatch_described_task(
		void *(*func)(void *), void *context, char *taskDesc) {

	int taskId = find_task_and_change_state(TS_READY, TS_CREATING, 0);

#if DEBUG_ABBEY > 0
	printf("Abbey: Task %d is dispatched.\n", taskId);
#endif
	task[taskId].context = context;
	task[taskId].func = func;
	strncpy(task[taskId].description, taskDesc, MAX_TASK_DESCRIPTION_LEN-1);
	set_task_state(taskId, TS_OPEN);

	return 0;
}

/*! \brief Dispatch task
 *
 * The dispatch routine without a description is so thoroughly wrought, I
 * remain speechless. ;-)
 */
int dispatch_task(void *(*func)(void *), void *context) {
	return dispatch_described_task(func, context, "");
}

/*! \brief Dispatch task
 *
 * A variadic function variant. The vl variable is initialized by the call of
 * va_start(). I don't know if dispatchTaskV is actually used by something...
 *
 * @see http://www.codeproject.com/cpp/argfunctions.asp
 */
int dispatch_vararray_task(void *(*func)(void *), ...) {
	va_list vl;
	va_start(vl, func);
	return dispatch_described_task(func, vl, "VarArgD");
}
