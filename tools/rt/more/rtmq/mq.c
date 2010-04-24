/*
 * mq.c -- testing the peformance of message queue
 *
 * Usage:
 *  $ ./mq <send-mq> <receive-mq> <loops> <size> 
 *
 * 如果中断很频繁，这中测试方法会有问题！！！！可能在接收到数据以后被中断很久！！！
 *
 *  o loops is the loops used to send and receive.
 *  o size is the size of the message, the range is from 0 to 8192
 */

#include <pthread.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
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

#define SIG_TEST (SIGRTMIN + 1)

void catchit(int signo)
{
}

int main(int argc, char *argv[])
{
	struct sched_param schedp;
	struct mq_attr attr;
	mqd_t mqdes1, mqdes2;
	ssize_t nr;
	void *buf;
	int loops;
	int size;
	int stat;
	pid_t pid, child;
	sigset_t newmask, oldmask;
	struct sigaction action;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s <send-mq> <receive-mq> <loops> <size>\n"
				"$ ./mq /send /receive 5000 5\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		exit(EXIT_FAILURE);

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = 99;
	sched_setscheduler(0, SCHED_FIFO, &schedp);

	loops = atoi(argv[3]);
	size = atoi(argv[4]);

	sigemptyset(&newmask);
	sigaddset(&newmask, SIG_TEST);
	sigprocmask(SIG_BLOCK, &newmask, &oldmask);

	action.sa_flags = 0;
	action.sa_handler = catchit;
	if (sigaction(SIG_TEST, &action, NULL) == -1)
		handle_error("sigusr: sigaction");

	mqdes1 = mq_open(argv[1], O_CREAT | O_RDWR, 0666, NULL);
	if (mqdes1 == (mqd_t) -1)
		handle_error("mq_open");

	mqdes2 = mq_open(argv[2], O_CREAT | O_RDWR, 0666, NULL);
	if (mqdes2 == (mqd_t) -1)
		handle_error("mq_open");

	if ((child = fork()) == 0) {
		struct timespec ts, tr;
		double diff, max = 0, min = ~0/*ULLONG_MAX*/, sum = 0, avg = 0;
		char *msg;
		int count = loops;

		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
			exit(EXIT_FAILURE);

		memset(&schedp, 0, sizeof(schedp));
		schedp.sched_priority = 99;
		sched_setscheduler(0, SCHED_FIFO, &schedp);
		msg = malloc(size + 1);
		if (msg == NULL)
			handle_error("malloc");
		memset(msg, '\0', size+1);
		memset(msg, 'A', size);

		buf = malloc(size + 1);
		if (buf == NULL)
			handle_error("malloc");
		
		pid = getpid();
		sigsuspend(&oldmask);
		while (loops) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			/* Send */
			if (mq_send(mqdes1, msg, strlen(msg), 99) < 0)
				handle_error("mq_send");
			if (mq_getattr(mqdes1, &attr) == -1)
				handle_error("mq_getattr");
			/* Receive */
			nr = mq_receive(mqdes2, buf, attr.mq_msgsize, NULL);
			if (nr == -1)
				handle_error("mq_receive");
			clock_gettime(CLOCK_MONOTONIC, &tr);
			diff = calcdiff_ns(tr, ts);
#ifdef VERBOSE
			printf("%f\n", diff / 2000);
#else
			if (diff > max)
				max = diff;
			if (diff < min)
				min = diff;
			sum += diff;
#endif
			loops --;
		}
//		avg = sum / count;
//		printf("Max: %f Min: %f Avg: %f Curr: %f\n", max, min, avg, diff);
//		printf("%d %f\n", size, max / 2000);
		munlockall();
		free(msg);
		free(buf);
		exit(0);
	}

	pid = getpid();
	buf = malloc(size + 1);
	if (buf == NULL)
		handle_error("malloc");

	/* Determine max. msg size; allocate buffer to receive msg */
	kill(child, SIG_TEST);
	while (loops) {
		/* Receive */
		if (mq_getattr(mqdes1, &attr) == -1)
			handle_error("mq_getattr");
		nr = mq_receive(mqdes1, buf, attr.mq_msgsize, NULL);
		if (nr == -1)
			handle_error("mq_receive");
		/* Send */
		if (mq_send(mqdes2, buf, attr.mq_msgsize, 99) < 0)
			handle_error("mq_send");
		loops --;
	}

	child = wait(&stat);

	munlockall();
	free(buf);
	mq_close(mqdes1);
	mq_close(mqdes2);
	
	/* Destroy the message queue */
	mq_unlink(argv[1]);
	mq_unlink(argv[2]);

	exit(0);
}
