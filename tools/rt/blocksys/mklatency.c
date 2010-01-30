/*
 * mklatency.c
 *
 * Copyright (C) 2008 Carsten Emde <C.Emde@osadl.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define __USE_GNU
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#define VERSION "0.2"

/* How long is this? */
/*
   The nop operation plus the loop in the driver
   takes two CPU cycles
*/
#define CYCLES "2000000"
/*
   This value (2,000,000) will take
   2 times 2,000,000 divided by clock frequency.
   For example, on a 2.4-GHz processor:
   4,000,000 / 2,400,000,000 = 1,67 ms
*/

struct params {
	int path;
	int cpu;
	int max_cpus;
	int running;
	pthread_t threadid;
};

void *blockthread(void *param)
{
	struct params *par = param;
	char *cycles = CYCLES;

	if (par->max_cpus > 1) {
		cpu_set_t mask;

		CPU_ZERO(&mask);
		CPU_SET(par->cpu, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
			fprintf(stderr, "WARNING: Could not set CPU affinity "
				"to CPU #%d\n", par->cpu);
	}
	write(par->path, cycles, strlen(cycles));
	par->running = 0;
	return NULL;
}

int main(int argc, char *argv[])
{
	int i;
	int path;

	if (argc > 1) {
		puts("Usage: mklatency");
		puts("Version: " VERSION);
		puts("Function: Artificially create latency through a blocking driver");
		puts("Options: (none)");
		return 1;
	}

	path = open("/dev/blocksys", O_WRONLY);
	if (path < 0) {
		fprintf(stderr, "ERROR: Could not access blocksys device, "
			"try 'modprobe blocksys'\n");
	} else {
		int max_cpus = sysconf(_SC_NPROCESSORS_CONF);
		int runners = 0;
		struct params *par = calloc(max_cpus, sizeof(struct params));

		if (par == NULL)
			goto nomem;

		for (i = 0; i < max_cpus; i++) {
			par[i].path = path;
			par[i].cpu = i;
			par[i].max_cpus = max_cpus;
			par[i].running = 1;
			if (max_cpus == 1)
				blockthread(&par[i]);
			else {
				pthread_create(&par[i].threadid, NULL,
					       blockthread, &par[i]);
				usleep(10000);
			}
		}

		if (max_cpus > 1) {
			do {
				usleep(10000 * max_cpus);
				runners = 0;
				for (i = 0; i < max_cpus; i++)
					runners += par[i].running;
			} while (runners > 0);
		}

		free(par);

 nomem:
		close(path);
	}

	return 0;
}
