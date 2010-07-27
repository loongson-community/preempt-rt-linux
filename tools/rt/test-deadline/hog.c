#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>
#include <linux/unistd.h>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include "edf_syscall.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Ugly, but .... */
#define gettid() syscall(__NR_gettid)
#define sigev_notify_thread_id _sigev_un._tid

extern int clock_nanosleep(clockid_t __clock_id, int __flags,
			   __const struct timespec *__req,
			   struct timespec *__rem);

#define USEC_PER_SEC		1000000
#define NSEC_PER_SEC		1000000000

#define TIMER_RELTIME		0

/* Struct to transfer parameters to the thread */
struct thread_param {
	pthread_t thread_id;
	char *name;
	struct timespec wcet;
	struct timespec deadline;
	struct timespec period;
	int cpu;
	int clock;
	struct thread_param *next;
};

static int shutdown;

static inline void tsnorm(struct timespec *ts)
{
	while (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_nsec -= NSEC_PER_SEC;
		ts->tv_sec++;
	}
}

/*
 * timer thread
 */
void *timerthread(void *param)
{
	struct thread_param *par = param;
	struct sched_param_ex sched_edfp;
	unsigned long mask;
	int policy;
	int nrun=0;
	int ret, pid;

	policy = SCHED_EDF;

	memset(&sched_edfp, 0, sizeof(sched_edfp));

	sched_edfp.sched_priority = 0;
	sched_edfp.sched_deadline.tv_sec = par->deadline.tv_sec;
	sched_edfp.sched_deadline.tv_nsec = par->deadline.tv_nsec;
	sched_edfp.sched_runtime.tv_sec = par->wcet.tv_sec;
	sched_edfp.sched_runtime.tv_nsec = par->wcet.tv_nsec;
	sched_edfp.sched_period.tv_sec = par->period.tv_sec;
	sched_edfp.sched_period.tv_nsec = par->period.tv_nsec;
	mask = 0x1 << par->cpu;

	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret)
		exit(-1);

	ret = sched_setscheduler_ex(0, policy, sizeof(sched_edfp), &sched_edfp);
	if (ret)
		exit(-1);

	/* Get current time */
	pid = gettid();

	fprintf(stderr, "task %d: START\n", (int)gettid());
	while (!shutdown) {
		volatile int i;

		for (i = 0; i < 1000; i++)
			asm("nop; nop; nop; nop;");
		printf("task %d: running cycle %d\n", pid, nrun);
		nrun++;
	}

	/* switch to normal */
	sched_edfp.sched_priority = 0;
	sched_setscheduler_ex(0, SCHED_OTHER, sizeof(sched_edfp), &sched_edfp);

	return NULL;
}

static inline void time_to_timespec(struct timespec *t, int value)
{	
	t->tv_sec = value / USEC_PER_SEC;
	t->tv_nsec = (value % USEC_PER_SEC) * 1000;
}

int read_training_set(char *cfile, struct thread_param **par, int *ncount)
{
	FILE *fd;
	char name[50];
	struct thread_param *temp;
	struct thread_param *temphead = NULL;
	int wcet, deadline, period, cpu;
	
	*ncount = 0;

	fd = fopen(cfile, "r");
	if (fd < 0) {
		fprintf(stderr,"Unable to open configuration file\n");
		return -1;
	}
	
	while(!feof(fd)) {
		int n = fscanf(fd, "%50s\t\%d\t\%d\t%d\t%d\n", name,
			       &wcet, &deadline, &period, &cpu);
		if (n == 5) {
			/* read all the parameter */
			fprintf(stderr, "name: %s, deadline %d, wcet %d," \
				       "period %d, cpu %d\n", name, deadline, \
				       wcet, period, cpu);
			temp = (struct thread_param *)
			       malloc(sizeof(struct thread_param));
			temp->name = strdup(name);

			time_to_timespec(&temp->deadline, deadline);
			time_to_timespec(&temp->wcet, wcet);
			time_to_timespec(&temp->period, period);
			temp->cpu = cpu;
			ncount++;

			if (temphead == NULL) {
				temphead = temp;
				*par = temphead;
			} else {
				temphead->next = temp;
				temphead = temphead->next;
			}
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	sigset_t sigset;
	struct thread_param *par;
	struct thread_param *head;
	char *config_file;
	int ret = -1, ncount = 0;
	int signum = SIGALRM;

	if (argc < 2) {
		fprintf(stderr, "configuration file missing\n");
		exit(-1);
	}
	
	config_file = argv[1];
	
	read_training_set(config_file, &par, &ncount);

	sigemptyset(&sigset);
	sigaddset(&sigset, signum);
	sigprocmask (SIG_BLOCK, &sigset, NULL);
	/*
	signal(SIGINT, sighand);
	signal(SIGTERM, sighand);
	*/

	for (head = par; head != NULL; head = head->next) {
		head->clock = CLOCK_MONOTONIC;
		pthread_create(&head->thread_id, NULL,
			       timerthread, head);
	}

	for (head = par; head != NULL; head = head->next)
		ret = pthread_join(head->thread_id, NULL);
	
	exit(ret);
}
