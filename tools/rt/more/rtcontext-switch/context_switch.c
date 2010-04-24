/*
 * Context Switch benchmarking tool
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

#define SIG_TEST (SIGRTMIN + 1)

#define LOOPS 20000

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

void catchit(int signo)
{
	/* do something here */
}
main()
{
	double diff[LOOPS +  1], max = 0, min = ~0; //ULLONG_MAX;
	struct timespec ts, tr, interval;
	pid_t child, pid;
	struct sched_param schedp;
	sigset_t newmask, oldmask;
	struct sigaction action;
	int i;

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		handle_error("mlockall");
	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = 98;
	sched_setscheduler(0, SCHED_RR, &schedp);

	sigemptyset(&newmask);
	sigaddset(&newmask, SIG_TEST);
	sigprocmask(SIG_BLOCK, &newmask, &oldmask);

	action.sa_flags = 0;
	action.sa_handler = catchit;
	if (sigaction(SIG_TEST, &action, NULL) == -1)
		handle_error("sigusr: sigaction");



	interval.tv_sec = 0;
	interval.tv_nsec = 0;

	if ((child = fork()) == 0) {	/* Child */
		pid_t pid = getpid();
		pid_t parent = getppid();
	
		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
			handle_error("mlockall");

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 98;
		sched_setscheduler(0, SCHED_RR, &schedp);

		kill(parent, SIG_TEST);
		for (i = 0; i < LOOPS; i ++) {
			//printf("child,\n");
			nanosleep(&interval, NULL);	/* We must reschedule to the another process */
			//printf("child wait,\n");
		}

		sleep(2);
		munlockall();
		exit(0);	
	} else {		/* Parent */
		int stat;

		usleep(50);

		pid = getpid();

		sigsuspend(&oldmask);
		for (i = 0; i < LOOPS; i ++) {
			//printf("parent,\n");
			clock_gettime(CLOCK_MONOTONIC, &ts);
			nanosleep(&interval, NULL);	/* schedule */
			clock_gettime(CLOCK_MONOTONIC, &tr);
			diff[i] = calcdiff_ns(tr, ts);
			if (diff[i] > max)
				max = diff[i];
			if (diff[i] < min)
				min = diff[i];
		}
	//	clock_gettime(CLOCK_MONOTONIC, &tr);
	//	diff = calcdiff_ns(tr, ts) / LOOPS;
	//	printf("Max: %f Min: %f Curr: %f\n", max / 2000, min / 2000, diff[i-1] / 2000);
		
		for (i = 0; i < LOOPS; i ++) {
			printf("%f\n", diff[i] / 2000);
		}

		child = wait(&stat);
		munlockall();
		_exit(0);
	}
}
