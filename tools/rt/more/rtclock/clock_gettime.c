/*
 * clock_gettime accuracy benchmarking tool
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <limits.h>

#define LOOPS 200000

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

static struct timespec ts, tr;
static double diff, max = 0, min = ~0; //ULLONG_MAX;

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
		clock_gettime(CLOCK_MONOTONIC, &ts);
		clock_gettime(CLOCK_MONOTONIC, &tr);
		diff = calcdiff_ns(tr, ts);
		if (diff > max)
			max = diff;
		if (diff < min)
			min = diff;
		printf("%f\n", diff / 1000);
		usleep(20);
	}
	printf("Max: %f Min: %f Curr: %f\n", max / 1000, min / 1000, diff / 1000);

	munlockall();
	_exit(0);
}
