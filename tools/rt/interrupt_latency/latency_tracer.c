/*
 * latency-tracer.c
 *
 *   This is tool for tracing the interrupt, schedule and response latency.  We
 *   need to use it with the interrupt latency driver, which is written by "Wu
 *   Zhangjin" and is available from the rt/2.6.33/loongson branch of
 *   http://dev.lemote.com/code/rt4ls
 *
 *   This file based on the sendme.c written by Carsten Emde <C.Emde@osadl.org>
 *
 *  Copyright (C) 2010 Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation either version 2 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#define __USE_GNU
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <time.h>

#define _GNU_SOURCE
#include <utmpx.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#define VERSION "0.1"

#define USEC_PER_SEC		1000000

#define SIGTEST SIGHUP

enum {
	AFFINITY_UNSPECIFIED,
	AFFINITY_SPECIFIED,
	AFFINITY_USECURRENT
};
static int setaffinity = AFFINITY_UNSPECIFIED;

enum {
	INTERRUPT_LATENCY = 1,
	HANDLE_LATENCY,
	SCHEDULE_LATENCY,
	RESPONSE_LATENCY
};

static int affinity;
static int tracelimit;
static int priority;
static int shutdown;
static int verbose;
static int misslimit;
static int max_cycles;
static volatile struct timeval after;
static int interval = 1000;

static int kernvar(int mode, const char *name, char *value, size_t sizeofvalue)
{
	char filename[128];
	char *fileprefix = "/sys/kernel/debug/tracing/";
	int retval = 1;
	int path;

	strncpy(filename, fileprefix, sizeof(filename));
	strncat(filename, name, sizeof(filename) - strlen(fileprefix));
	path = open(filename, mode);
	if (path >= 0) {
		if (mode == O_RDONLY) {
			int got;
			got = read(path, value, sizeofvalue);
			if (got > 0) {
				retval = 0;
				value[got - 1] = '\0';
			}
		} else if (mode == O_WRONLY) {
			if (write(path, value, sizeofvalue) == sizeofvalue)
				retval = 0;
		}
		close(path);
	}
	return retval;
}

void signalhandler(int signo)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	after = tv;
	if (signo == SIGINT || signo == SIGTERM)
		shutdown = 1;
}

void stop_tracing(void)
{
	kernvar(O_WRONLY, "tracing_enabled", "0", 1);
}

static void display_help(void)
{
	puts("Usage: sendme");
	puts("Version: " VERSION);
	puts("Function: send a signal from driver to userspace");
	puts("Options:\n"
	     "-a [NUM] --affinity        pin to current processor\n"
	     "                           with NUM pin to processor NUM\n"
	     "-b USEC  --breaktrace=USEC send break trace command when latency > USEC\n"
	     "-i INTV  --interval=INTV   base interval of thread in us default=1000\n"
	     "-l LOOPS --loops=LOOPS     number of loops: default=0(endless)\n"
	     "-v TYPE  --verbose=TYPE    latency type: Interrupt=1,Handle=2,chedule=3,Response=4\n"
	     "-m MISS  --misses=MISS     number of the max events missed limited\n"
	     "-p PRIO  --prio=PRIO       priority\n");
	exit(1);
}

static void process_options(int argc, char *argv[])
{
	int error = 0;
	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);

	for (;;) {
		int option_index = 0;
		/** Options for getopt */
		static struct option long_options[] = {
			{"affinity", optional_argument, NULL, 'a'},
			{"breaktrace", required_argument, NULL, 'b'},
			{"interval", required_argument, NULL, 'i'},
			{"loops", required_argument, NULL, 'l'},
			{"priority", required_argument, NULL, 'p'},
			{"verbose", required_argument, NULL, 'v'},
			{"miss", required_argument, NULL, 'm'},
			{"help", no_argument, NULL, '?'},
			{NULL, 0, NULL, 0}
		};
		int c = getopt_long(argc, argv, "a::b:i:l:p:v:m:",
				    long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'a':
			if (optarg != NULL) {
				affinity = atoi(optarg);
				setaffinity = AFFINITY_SPECIFIED;
			} else if (optind < argc && atoi(argv[optind])) {
				affinity = atoi(argv[optind]);
				setaffinity = AFFINITY_SPECIFIED;
			} else
				setaffinity = AFFINITY_USECURRENT;
			break;
		case 'b':
			tracelimit = atoi(optarg);
			break;
		case 'i':
			interval = atoi(optarg);
			break;
		case 'l':
			max_cycles = atoi(optarg);
			break;
		case 'p':
			priority = atoi(optarg);
			break;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'm':
			misslimit = atoi(optarg);
			break;
		case '?':
			error = 1;
			break;
		}
	}

	if (setaffinity == AFFINITY_SPECIFIED) {
		if (affinity < 0)
			error = 1;
		if (affinity >= max_cpus) {
			fprintf(stderr,
				"ERROR: CPU #%d not found, only %d CPUs available\n",
				affinity, max_cpus);
			error = 1;
		}
	}

	if (priority < 0 || priority > 99)
		error = 1;

	if (error)
		display_help();
}

static int check_privs(void)
{
	int policy = sched_getscheduler(0);
	struct sched_param param;

	/* if we're already running a realtime scheduler
	 * then we *should* be able to change things later
	 */
	if (policy == SCHED_FIFO || policy == SCHED_RR)
		return 0;

	/* try to change to SCHED_FIFO */
	param.sched_priority = 1;
	if (sched_setscheduler(0, SCHED_FIFO, &param)) {
		fprintf(stderr, "Unable to change scheduling policy!\n");
		fprintf(stderr, "either run as root or join realtime group\n");
		return 1;
	}

	/* we're good; change back and return success */
	sched_setscheduler(0, policy, NULL);
	return 0;
}

int main(int argc, char *argv[])
{
	int path;
	cpu_set_t mask;
	int policy = SCHED_FIFO;
	struct sched_param schedp;
	struct flock fl;
	int retval = 0;

	if (check_privs())
		return 1;

	process_options(argc, argv);

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		perror("mlockall");
		return 1;
	}

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = priority;
	sched_setscheduler(0, policy, &schedp);

	if (setaffinity != AFFINITY_UNSPECIFIED) {
		CPU_ZERO(&mask);
		if (setaffinity == AFFINITY_USECURRENT)
			affinity = sched_getcpu();
		CPU_SET(affinity, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
			fprintf(stderr, "WARNING: Could not set CPU affinity "
				"to CPU #%d\n", affinity);
	}

	path = open("/dev/interrupt_latency", O_RDWR);
	if (path < 0) {
		fprintf(stderr, "ERROR: Could not access interrupt latency device, "
			"try 'modprobe interrupt_latency' or insmod interrupt_latency.ko\n");
		return 1;
	}
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	if (fcntl(path, F_SETLK, &fl) == -1) {
		fprintf(stderr, "ERRROR: interrupt latency device locked\n");
		retval = 1;
	} else {
		char timestamp[80];
		char sigtest[8];
		char response_latency[8] = "4";
		char enable_tracing[8] = "1";
		char disable_tracing[8] = "0";
		char interval_string[8];
		struct timeval interrupttime, handletime, exittime, schedtime, diff;
		unsigned int diffno = 0, old_total, total = 0, maxmiss, diffmiss, vdiff;
		unsigned int mindiff1 = UINT_MAX, maxdiff1 = 0, diff1;
		unsigned int mindiff2 = UINT_MAX, maxdiff2 = 0, diff2;
		unsigned int mindiff3 = UINT_MAX, maxdiff3 = 0, diff3;
		unsigned int mindiff4 = UINT_MAX, maxdiff4 = 0, diff4;
		double sumdiff1 = 0.0, sumdiff2 = 0.0, sumdiff3 = 0.0, sumdiff4 = 0;

		if (tracelimit)
			kernvar(O_WRONLY, "tracing_enabled", "1", 1);

		sprintf(interval_string, "%d", interval);
		sprintf(sigtest, "%d", SIGTEST);
		signal(SIGTEST, signalhandler);
		signal(SIGINT, signalhandler);
		signal(SIGTERM, signalhandler);

		if (verbose)
			printf("Thread 0 Interval: %d\n", interval);
		else
			printf("Interrupt latency driver, Enter CTRL+^C to get the result\n");

		write(path, response_latency, sizeof(response_latency));
		write(path, interval_string, sizeof(interval_string));
		write(path, enable_tracing, sizeof(enable_tracing));

		while (1) {
			/* blocking here until the data come */
			read(path, timestamp, sizeof(timestamp));

			gettimeofday(&schedtime, NULL);
			old_total = total;
			if (sscanf(timestamp, "%d %d,%d %d,%d %d,%d\n", &total,
				&interrupttime.tv_sec, &interrupttime.tv_usec,
				&handletime.tv_sec, &handletime.tv_usec,
				&exittime.tv_sec, &exittime.tv_usec) < 0)
				break;

			diffno++;

			if (max_cycles && diffno >= max_cycles)
				shutdown = 1;

			diffmiss = total - old_total - 1;
			if (old_total == 0)
				diffmiss = 0;
			if (diffmiss > maxmiss)
				maxmiss = diffmiss;

			if (misslimit && diffmiss >= misslimit)
				shutdown = 1;

			timersub(&handletime, &interrupttime, &diff);
			diff1 = diff.tv_usec;
			if (diff1 < mindiff1)
				mindiff1 = diff1;
			if (diff1 > maxdiff1)
				maxdiff1 = diff1;
			sumdiff1 += (double)diff1;

			timersub(&exittime, &handletime, &diff);
			diff2 = diff.tv_usec;
			if (diff2 < mindiff2)
				mindiff2 = diff2;
			if (diff2 > maxdiff2)
				maxdiff2 = diff2;
			sumdiff2 += (double)diff2;

			timersub(&schedtime, &exittime, &diff);
			diff3 = diff.tv_usec;
			if (diff3 < mindiff3)
				mindiff3 = diff3;
			if (diff3 > maxdiff3)
				maxdiff3 = diff3;
			sumdiff3 += (double)diff3;

			timersub(&schedtime, &interrupttime, &diff);
			diff4 = diff.tv_usec;
			if (diff4 < mindiff4)
				mindiff4 = diff4;
			if (diff4 > maxdiff4)
				maxdiff4 = diff4;
			sumdiff4 += (double)diff4;

			if (verbose) {
				switch (verbose) {
				case INTERRUPT_LATENCY:
					vdiff = diff1;
					break;
				case HANDLE_LATENCY:
					vdiff = diff2;
					break;
				case SCHEDULE_LATENCY:
					vdiff = diff3;
					break;
				case RESPONSE_LATENCY:
					vdiff = diff4;
					break;
				default:
					vdiff = diff2;
					break;
				}
				printf("%d\n", vdiff);
				if ((tracelimit && diff.tv_usec > tracelimit) ||
					shutdown) {
						if (tracelimit)
							stop_tracing();
					break;
				}
				continue;
			}

			after.tv_sec = 0;
			if ((tracelimit && diff.tv_usec > tracelimit) ||
			    shutdown) {
				if (tracelimit)
					stop_tracing();

				printf("Samples: %d\n", diffno);
				printf("Interval: %d\n", interval);
				printf("Total events: %d\n", total);
				printf("Missed events: Max: %8d Curr: %8d\n", maxmiss, diffmiss);

				printf("Interrupt Latency:  Min %8d, Cur %8d, Avg %8d, Max %8d\n",
				       mindiff1, (int)diff1,
				       (int)((sumdiff1 / diffno) + 0.5), maxdiff1);

				printf("Handler Latency:    Min %8d, Cur %8d, Avg %8d, Max %8d\n",
				       mindiff2, (int)diff2,
				       (int)((sumdiff2 / diffno) + 0.5), maxdiff2);

				printf("Scheduler Latency:  Min %8d, Cur %8d, Avg %8d, Max %8d\n",
				       mindiff3, (int)diff3,
				       (int)((sumdiff3 / diffno) + 0.5), maxdiff3);

				printf("Response Latency:   Min %8d, Cur %8d, Avg %8d, Max %8d\n",
				       mindiff4, (int)diff4,
				       (int)((sumdiff4 / diffno) + 0.5), maxdiff4);

				system("echo -n 'System Load:       '; cat /proc/loadavg");

				break;
			}
		}
	}

	close(path);
	return retval;
}
