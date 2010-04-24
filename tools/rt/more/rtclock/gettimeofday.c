/*
 * gettimeofday accuracy benchmarking tool
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sched.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <limits.h>

#define LOOPS 50000

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define NSEC_PER_SEC 1000000000

static inline long long calcdiff_ns(struct timespec t1, struct timespec t2)
{
	long long diff;
	diff = NSEC_PER_SEC * (long long)((int) t1.tv_sec - (int) t2.tv_sec);
	diff += ((int) t1.tv_nsec - (int) t2.tv_nsec);
	return diff;
}

static struct timeval ts, tr, diff;
static double max = 0, min = ~0;//ULLONG_MAX;

main()
{
	struct sched_param schedp;
	int i;
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		handle_error("mlockall");

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = 98;
	sched_setscheduler(0, SCHED_RR, &schedp);

	for (i = 0; i < LOOPS; i ++) {
		gettimeofday(&ts, NULL);
		gettimeofday(&tr, NULL);
		timersub(&tr, &ts, &diff);
		if (diff.tv_usec > max)
			max = diff.tv_usec;
		if (diff.tv_usec < min)
			min = diff.tv_usec;
		printf("%d\n", diff.tv_usec);
		usleep(0);
	}
	printf("Max: %f Min: %f Curr: %f\n", max, min, diff.tv_usec);

	munlockall();
	_exit(0);
}
