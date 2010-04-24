/*
 * Real time signal benchmarking tool
 */

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sched.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>

#define VERBOSE

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

int main(int argc, char *argv[])
{
	struct sched_param schedp;
	int i, sig, sigq_max, numsigs = 0, stat;
	int SIG = SIGRTMIN;
	sigset_t set, pend;
	siginfo_t info;
	pid_t child;

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		handle_error("mlockall");

	sigq_max = sysconf(_SC_SIGQUEUE_MAX);
	sigemptyset(&set);
	sigaddset(&set, SIG);
	sigprocmask(SIG_SETMASK, &set, NULL);

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = 99;
	sched_setscheduler(0, SCHED_FIFO, &schedp);

	if ((child = fork()) == 0) {	/* child */
		struct timespec ts, tr;
		double diff, max = 0, min = ~0/*ULLONG_MAX*/, avg, total = 0;
		pid_t parent = getppid();
	
		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
			handle_error("mlockall");
		
		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 99;
		sched_setscheduler(0, SCHED_FIFO, &schedp);
	
		sleep(1);	
		sig = sigwaitinfo(&set, &info);
		if (sig < 0)
			handle_error("sigusr: sigwaitinfo");
		for (i = 0; i < sigq_max; i++) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			if (sigqueue(parent, SIG, (const union sigval)0) < 0)
				handle_error("sigqueue");
			sig = sigwaitinfo(&set, &info);
			if (sig < 0)
				handle_error("sigusr: sigwaitinfo");
			clock_gettime(CLOCK_MONOTONIC, &tr);
			numsigs ++;

			diff = calcdiff_ns(tr, ts) / 2000;	/* ns -> us */
#ifndef VERBOSE
			total += diff;
			if (diff > max)
				max = diff;
			if (diff < min)
				min = diff;
			avg = total / numsigs;
			printf("Max: %f Min: %f Avg: %f Curr: %f\n", max, min, avg, diff);
#else
			printf("%f\n", diff);
#endif
		}
		sleep(1);
		munlockall();
		exit(0);
	}
	sigpending(&pend);
	if (sigqueue(child, SIG, (const union sigval)0) < 0)
		handle_error("sigqueue");
	for (i = 0; i < sigq_max; i++) {
		sig = sigwaitinfo(&set, &info);
		if (sig < 0)
			handle_error("sigwait");
		numsigs++;
		if (sigqueue(child, sig, (const union sigval)info.si_int) < 0)
			handle_error("sigqueue");
	}
	
	sleep(4);
	child = wait(&stat);
	munlockall();

	return 0;
}
