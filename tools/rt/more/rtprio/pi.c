/*
 * http://standards.ieee.org/reading/ieee/interp/1003.1-2001/1003.1-2001-index.html
 */

/* compile: gcc -Wall -D__USE_UNIX98  pi.c -o pi -lpthread -lrt */
#include <unistd.h> 	/* _POSIX_THREAD_PRIO_INHERIT */
#include <stdio.h>	/* printf */
#include <stdlib.h>	/* EXIT_FAILURE */
#include <string.h>	/* strerror */
#include <error.h>	/* error */
#include <linux/sched.h>/* SCHED_FIFO */
#include <pthread.h>

#define __USE_UNIX_98

#define _GNU_SOURCE
#if defined(_POSIX_THREAD_PRIO_INHERIT) && _POSIX_THREAD_PRIO_INHERIT != -1
#define HAVE_PI_MUTEX 1
#else
#define HAVE_PI_MUTEX 0
#endif

#if HAVE_PI_MUTEX == 0
#error "Can't run this test without PI Muxtex support"
#endif

/* cut and past from pthread.h - no idea whats going on ...*/
/* Mutex protocols.  */
enum
{
  PTHREAD_PRIO_NONE,
  PTHREAD_PRIO_INHERIT,
  PTHREAD_PRIO_PROTECT
};


int main(int argc,char ** argv){
	int status;
	int policy = SCHED_FIFO;
	int priority = 0;
	pthread_mutexattr_t mutex_attr;
	pthread_mutex_t mymutex;
	pthread_t t_hi,t_low;

        status = pthread_mutexattr_init(&mutex_attr);
        if (status) {
                fprintf(stderr,"mutex attr init: %s\n", strerror(status));
                return EXIT_FAILURE;
        }

        status = pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_INHERIT);
        if (status) {
                fprintf(stderr,"mutex attr policy: %s\n", strerror(status));
                return EXIT_FAILURE;
	}

        status = pthread_mutex_init(&mymutex, &mutex_attr);
        if (status) {
                fprintf(stderr,"mutex init: %s\n", strerror(status));
                return EXIT_FAILURE;
        }

	/* the low prio task can safely lock this now !) */
	pthread_mutex_lock(&mymutex);
	pthread_mutex_unlock(&mymutex);

	return 0;
}
