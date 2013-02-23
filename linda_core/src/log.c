/**
 * @file log.c
 * @brief Prints depending on verbosity. Thread aware.   
 * @author Anne C. van Rossum
 */

#include <stdio.h>
#include <log.h>
#include <pthread.h>
#include <ptreaty.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

/**
 * This routine has to be called before logging can start. It uses the pthread library,
 * sorry for that. 
 */
void initLog(uint8_t verbosity) {
	logconf = malloc(sizeof(struct LogConf));
	logconf->levelOfVerbosity = verbosity;
	logconf->printName = 0;
	logconf->printAtomic = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init (logconf->printAtomic, NULL);
}

/**
 * This routine can be used to wrap around large amount of print commands, so that the
 * verbosity level is not checked individually at each command.
 */
uint8_t isPrinted(uint8_t verbosity) {
	return (!(verbosity > logconf->levelOfVerbosity));
}

/**
 * On encoutering an error condition the verbosity can be increased for example.
 */
void setVerbosity(uint8_t verbosity) {
	logconf->levelOfVerbosity = verbosity;
}

/*
 * Prints the verbosity level, the function in which it occurs (given parameter __func__)
 * and the message. Does not print thread information, use tprintf for that.
 * 
 * @remark For the dot tool, HIDE_UNDOC_RELATIONS can not be set per individual method. I
 * don't like this function to show up in every graph, so I excluded it from doxygen 
 * commentary by using only one *. 
 */
void ntprintf(uint8_t verbosity, const char *function, char *msg) {
	if (verbosity > logconf->levelOfVerbosity) return;
	printf("VERBOSITY %i: [%s] %s\n", verbosity, function, msg);

	syslog(LOG_MAKEPRI(LOG_LOCAL0, LOG_INFO), 
			"VERBOSITY %i: [%s] %s\n", verbosity, function, msg);
}

#define RESET		0
#define BRIGHT 		1
#define DIM		2
#define UNDERLINE 	3
#define BLINK		4
#define REVERSE		7
#define HIDDEN		8

#define BLACK 		0
#define RED		1
#define GREEN		2
#define YELLOW		3
#define BLUE		4
#define MAGENTA		5
#define CYAN		6
#define	WHITE		7

void textcolor(int attr, int fg, int bg) {
	char command[13];

	/* Command is the control command to the terminal */
	sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
	printf("%s", command);
}

/*
 * Prints the verbosity level, the function in which it occurs (given parameter __func__),
 * the thread name and the message.
 */
void tprintf(uint8_t verbosity, const char *function, char *msg) {
	if (verbosity > logconf->levelOfVerbosity) return;
	pthread_t this = pthread_self();
	const char *thread = ptreaty_get_thread_name(&this);
	uint8_t color = 7;

	uint8_t text_style = 0;
	switch (verbosity) {
	case 0: case 1:
		color = 1; text_style = 7;
		break;
	case 2: case 3: 
		color = 1; text_style = 1;
		break;
	case 4: case 5: case 6: case 7: case 8: case 9: 
		color = verbosity - 2; text_style = 1;
		break;
	case 10: case 11: case 12: case 13: case 14: case 15: case 16:
		color = verbosity - 8;
		break;
	default: 
		color = 0;
	}

	textcolor(text_style, color, BLACK);
	if (logconf->printName) {
		printf("[%s(%s) | %s] %s\n", function, logconf->name, thread, msg);
	} else {
		printf("[%s | %s] %s\n", function, thread, msg);
	}
	textcolor(RESET, WHITE, BLACK);
	
	// The syslog function is a possible cancellation point, so threading errors might
	// very well appear right here... syslog doesn't understand verbosities > 7
	if (verbosity > 7) verbosity = 7;
	syslog(LOG_MAKEPRI(LOG_LOCAL0, verbosity), "[%s | %s] %s\n", function, thread, msg);
	fflush(NULL);
}

void btprintf(uint8_t verbosity, const char *function, char *msg) {
	pthread_mutex_lock(logconf->printAtomic);
	char * pch;
	pch = strtok (msg,"\n");
	while (pch!=NULL) {
		tprintf(verbosity, function, pch);
		pch= strtok(NULL,"\n");
	}
	fflush(NULL);
	pthread_mutex_unlock(logconf->printAtomic);
}

/**
 * Prints a character in its binary form. The parameter newLine determines if the function
 * will print a new line or not.
 */
void printfBinary(uint8_t x, uint8_t newLine) {
	uint8_t i;
	for(i=7; i>0; i--)
		printf("%d", (x >> i) & 1);
	printf("%d", x & 1);
	if(newLine)
		printf("\n");
}
