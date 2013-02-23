/**
 * @file log.h
 * @brief Print/log functionality, thread-aware.
 * @author Anne C. van Rossum
 */

#ifndef LOG_H_
#define LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <syslog.h>
#include <pthread.h>
	
//#define	LOG_EMERG	0	/* system is unusable */
//#define	LOG_ALERT	1	/* action must be taken immediately */
//#define	LOG_CRIT	2	/* critical conditions */
//#define	LOG_ERR		3	/* error conditions */
//#define	LOG_WARNING	4	/* warning conditions */
//#define	LOG_NOTICE	5	/* normal but significant condition */
//#define	LOG_INFO	6	/* informational */
//#define	LOG_DEBUG	7	/* debug-level messages */

#define LOG_VERBOSE  8
#define LOG_VV       9
#define LOG_VVV     10
#define LOG_VVVV    11
#define LOG_VVVVV   12
#define LOG_VVVVVV  14
#define LOG_VVVVVVV 15
#define LOG_BLABLA  16

struct LogConf {
	uint8_t levelOfVerbosity;
	char *name;
	uint8_t printName;
	pthread_mutex_t *printAtomic;
};

void initLog(uint8_t verbosity);

uint8_t isPrinted(uint8_t verbosity);

void ntprintf(uint8_t verbosity, const char *function, char *msg);

void tprintf(uint8_t verbosity, const char *function, char *msg);

void btprintf(uint8_t verbosity, const char *function, char *msg);

void printfBinary(uint8_t x, uint8_t newLine);

void setVerbosity(uint8_t verbosity);

struct LogConf *logconf;

#ifdef __cplusplus
}
#endif


#endif /*LOG_H_*/
