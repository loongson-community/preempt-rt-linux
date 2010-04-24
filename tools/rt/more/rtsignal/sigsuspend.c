#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define LOOPS 10000

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

#define SIG_TEST (SIGRTMIN + 1)

static struct timespec ts, tr;
static double diff;
void catchit();

main()
{
	pid_t child;
	struct sched_param schedp;
	sigset_t newmask, oldmask;
	struct sigaction action;
	int i;
	
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		handle_error("mlockall");

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = 99;
	sched_setscheduler(0, SCHED_FIFO, &schedp);

	sigemptyset(&newmask);
	sigaddset(&newmask, SIG_TEST);
	sigprocmask(SIG_BLOCK, &newmask, &oldmask);

	action.sa_flags = 0;
	action.sa_handler = catchit;
	if (sigaction(SIG_TEST, &action, NULL) == -1)
		handle_error("sigusr: sigaction");

	if ((child = fork()) == 0) {	/*Child */
		pid_t parent;
		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
			handle_error("mlockall");

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 99;
		sched_setscheduler(0, SCHED_FIFO, &schedp);

		parent = getppid();
		kill(parent, SIG_TEST);
		for (i = 0; i < LOOPS; i ++) {
			sigsuspend(&oldmask);
			kill(parent, SIG_TEST);
		}
		
		sleep(2);
		munlockall();

		exit(0);	
	} else {		/* Parent */
		int stat;

		sigsuspend(&oldmask);
		for (i = 0; i < LOOPS; i ++) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			kill(child, SIG_TEST);
			sigsuspend(&oldmask);
			clock_gettime(CLOCK_MONOTONIC, &tr);
			diff = calcdiff_ns(tr, ts) / 2000;	/* ns -> us */
			printf("%f\n", diff);
		}

		sleep(4);
		child = wait(&stat);
		munlockall();
		_exit(0);
	}
}

void catchit(int signo)
{
	/* do something here */
}
