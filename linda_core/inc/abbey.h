/************************************************************************/

/*!
 * \defgroup AbbeyCore Core functionality for the abbey concept
 */

/*! \file
 * 
 * \brief An abbey is a very liberal version of a thread pool.
 * 
 * \class Abbey
 * 
 * \ingroup AbbeyCore
 * 
 * \brief The abbey introduced
 * 
 * The abbey is a thread pool where threads live in the role of monks.
 * The difference is in their power. Monks are agent-oriented variants
 * of threads. Threads do not know how often they are running, they have
 * no meta-information about their execution. Threads are all similar
 * while different types of threads may be tailored to different types 
 * of tasks and hardware environments. Thus introducing delegation from
 * one thread to another, and introducing competition between threads
 * more or less better in certain tasks. Threads are not autonomous, but
 * scheduled by a third party, a scheduler. While autonomous threads
 * may indicate when it is cheap to swap threads (cheap context-switch)
 * or appoint themselves which thread to run next. A thread is not 
 * persistent. A thread that does have state can store all kind of 
 * additional information that may or may not be important in certain
 * execution contexts. Last but not least, a thread is static. A thread
 * that is dynamic, an adaptable thread, may have learning capabilities.
 * 
 * The monk is appointed to become such an environment aware, diverse,
 * competing, autonomous, statefull, adaptive thread.  
 * 
 * \author   Peet van Tooren, Stephan Kroon, Anne van Rossum
 ***********************************************************************/


/*! \mainpage
 * \section Intro Introduction
 * The abbey is a new approach of tackling concurrency, where concepts of
 * escalation and delegation play a big role. And, in the background, 
 * concepts like locality, decentralization, etc.  
 * 
 * \section QuickLinks Quick Links
 * - \ref License
 *  - License agreement
 * - \ref Abbey
 *  - The Abbey  
 * - \ref ToDoList
 *  - To Do List
 * 
 * \section ToDoList
 * Several abbeys should be distributed over multiple machines 
 */
 
#ifndef abbey_h
#define abbey_h

#ifdef __cplusplus
extern "C" {
#endif 

/*!
* \page License
* 
* The software developed within the Common Hybrid Agent Platform environment
* may be used by everybody. It's origin lies in the following company:
* <ul>
*   <li>Almende B.V.
* </ul>
*  
* No warranties involved.   
*                                                                                                  
* Published under the GNU Lesser General Public License                                                                                  
*
******************************************************************************/

int initialize_abbey(int monkCount, int taskBuffer);

/**
 * There are three ways differing in the parameters used to
 * aid dispatching. 
 */
int dispatch_described_task(void *(*func)(void *), 
  void *context, char *taskDesc);
int dispatch_task(void *(*func)(void *), 
  void *context);
int dispatch_vararray_task(void *(*func)(void *), ...);

#ifdef __cplusplus
}
#endif 

#endif // abbey_h
