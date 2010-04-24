#include <stdio.h>
#include <fcntl.h>
#include <sched.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#define NSEC_PER_SEC 1000000000ULL

static void set_prio(int prio)
{
	struct sched_param schedp;

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = prio;
	sched_setscheduler(0, SCHED_FIFO, &schedp);
}

int main(int argc, char **argv)
{
	int fd, i, j, nloop, zero = 0;
	int *ptr;
	long long iloop = 80000000;

	if (argc != 2) {
		fprintf(stderr, "usage: test <#loops>\n");
		exit(1);
	}

	nloop = atoi(argv[1]);

	setbuf(stdout, NULL);

	printf("Please set the prio of the processes:\n");
	printf("$ chrt -f -p <prio> <pid>\n");
	if (fork() == 0) {
		set_prio(95);
		printf("95 start\n");
		sleep(1);
		printf("95 ready\n");
		for (i = 0; i < nloop; i++) {
			/* do nothing */
			printf("95 execute,\n");
			j += i;	
			printf("95 sleep,\n");
			usleep(70);
		}
		printf("middle prio: 95, pid = %d exit\n", getpid());
		exit(0);
	}
	if (fork() == 0) {
		set_prio(97);
		printf("97 start\n");
		sleep(1);
		printf("97 ready\n");
		for (i = 0; i < nloop; i++) {
			/* do nothing */
			printf("97 execute,\n");
			j += i;	
			printf("97 sleep,\n");
			usleep(90);
		}
		printf("middle prio: 97, pid = %d exit\n", getpid());
		exit(0);
	}
	if (fork() == 0) {
		set_prio(99);
		printf("99 start\n", getpid());
		sleep(0);
		printf("99 ready\n", getpid());
		/* all of this three processes sleep 5 seconds to ensure their prio have been changed. */
		for (i = 0; i < nloop; i++) {
			printf("99 execute,\n");
			/* do nothing */
			j += i;
			printf("99 sleep,\n");
			usleep(50);
		}
		printf("high prio: 99, pid = %d exit\n", getpid());
		exit(0);
	}
	set_prio(90);
	sleep(0);
	printf("90 start\n", getpid());
	for (i = 0; i < nloop; i++) {
		printf("90 execute,\n");
		/* do nothing */
		j += i;
		printf("90 sleep,\n");
		usleep(100);
	}
	printf("low prio: 90, pid = %d exit\n", getpid());
	

	return 0;
}
