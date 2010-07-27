/*
  sched_switch - Convert sched_switch ftrace data to vcd or matlab

  Copyright (C) 2009 Herman ten Brugge <hermantenbrugge@home.nl>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
  USA
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sched_switch_struct {
	unsigned int program_id;
	unsigned int from_cpu;
	double time;
	unsigned int from_pid;
	unsigned int from_prio;
	unsigned int from_state_id;
	unsigned int wakeup_id;
	unsigned int to_cpu;
	unsigned int to_pid;
	unsigned int to_prio;
	unsigned int to_state_id;
} sched_switch_type;

typedef struct program_struct {
	char *name;
	double time;
	unsigned int task;
	unsigned int prio;
	unsigned int idle_task;
	char *tag;
	unsigned int running;
	double last_time;
	double max_time;
	double max_pos;
} program_type;

static void print_help(const char *programname)
{
	printf("Usage: %s [options] input output\n", programname);
	printf("       -m  : output in matlab format\n");
	printf("       -v  : output in vcd format (default)\n");
	printf("       -p  : print priority inheritance lines\n");
	printf("       -s  : print max scheduling delay\n");
	printf("       -h  : print this help\n");
	exit(0);
}

int main(int argc, char **argv)
{
	enum { VCD, MATLAB } output = VCD;
	unsigned int priority_inheritance = 0;
	unsigned int max_sched_delay = 0;
	char *infilename = NULL;
	char *outfilename = NULL;
	unsigned int i;
	unsigned int j;
	unsigned int n;
	unsigned int last_comment = 0;
	FILE *fpi;
	FILE *fpo;
	unsigned int max_cpu = 0;
	unsigned int last_max_cpu = 0;
	unsigned int *first = NULL;
	double *last_time = NULL;
	unsigned int nof_bits = 0;
	unsigned int program_id;
	unsigned int from_cpu;
	double time;
	unsigned int from_pid;
	unsigned int from_prio;
	unsigned int from_state_id;
	unsigned int wakeup_id;
	unsigned int to_cpu;
	unsigned int to_pid;
	unsigned int to_prio;
	unsigned int to_state_id;
	char line[1000];
	char array[1000];
	char program_name[1000];
	char from_state[1000];
	char to_state[1000];
	char wakeup_str[1000];
	unsigned int n_program = 0;
	program_type *program = NULL;
	unsigned int n_state = 0;
	char **state = NULL;
	unsigned int n_wakeup = 0;
	char **wakeup = NULL;
	unsigned int n_sched_switch = 0;
	unsigned int m_sched_switch = 0;
	sched_switch_type *sched_switch = NULL;

	for (i = 1; i < (unsigned int)argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'm':
				output = MATLAB;
				break;
			case 'v':
				output = VCD;
				break;
			case 'p':
				priority_inheritance = 1;
				break;
			case 's':
				max_sched_delay = 1;
				break;
			case 'h':
			default:
				print_help(argv[0]);
				return (0);
			}
		} else if (infilename == NULL) {
			infilename = argv[i];
		} else if (outfilename == NULL) {
			outfilename = argv[i];
		}
	}
	if (infilename == NULL || outfilename == NULL) {
		print_help(argv[0]);
		return (0);
	}
	fpi = fopen(infilename, "r");
	if (fpi == NULL) {
		fprintf(stderr, "Cannot open filename %s\n", infilename);
		return 1;
	}
	fpo = fopen(outfilename, "w");
	if (fpo == NULL) {
		fprintf(stderr, "Cannot create filename %s\n", outfilename);
		return 1;
	}
	while (fgets(line, sizeof(line), fpi) != NULL) {
		if (line[0] == '#') {
			last_comment = n_sched_switch;
		}
		to_cpu = (unsigned int)-1;
		if (sscanf
		    (line, " %s [ %u ] %lf : %u : %u : %s %s %u : %u : %s ",
		     program_name, &from_cpu, &time, &from_pid, &from_prio,
		     from_state, wakeup_str, &to_pid, &to_prio, to_state) == 10
		    || sscanf(line,
			      " %s [ %u ] %lf : %u : %u : %s %s [ %u ] %u : %u : %s ",
			      program_name, &from_cpu, &time, &from_pid,
			      &from_prio, from_state, wakeup_str, &to_cpu,
			      &to_pid, &to_prio, to_state) == 11) {
			if (to_cpu == (unsigned int)-1) {
				/* incorrect but we have to set something */
				to_cpu = from_cpu;
			}
			if (from_cpu > max_cpu) {
				max_cpu = from_cpu;
			}
			if (to_cpu > max_cpu) {
				max_cpu = to_cpu;
			}
			if (first == NULL || max_cpu != last_max_cpu) {
				first =
				    (unsigned int *)realloc(first,
							    (max_cpu +
							     1) *
							    sizeof(unsigned
								   int));
				last_time =
				    (double *)realloc(last_time,
						      (max_cpu +
						       1) * sizeof(double));
				if (first == NULL || last_time == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
				for (i = last_max_cpu; i <= max_cpu; i++) {
					/* 2 because first time is not allways correct */
					first[i] = 2;
					last_time[i] = 0;
				}
				last_max_cpu = max_cpu;
			}
			for (program_id = 0; program_id < n_program;
			     program_id++) {
				if (strcmp
				    (program[program_id].name,
				     program_name) == 0) {
					break;
				}
			}
			if (program_id == n_program) {
				program =
				    (program_type *) realloc(program,
							     (n_program +
							      1) *
							     sizeof
							     (program_type));
				if (program == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
				program[n_program].name = strdup(program_name);
				program[n_program].time = 0;
				program[n_program].task = from_pid;
				program[n_program].prio = 0;
				program[n_program].idle_task =
				    strcmp(program_name, "<idle>-0") == 0;
				program[n_program].tag = NULL;
				if (program[n_program++].name == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
			}
			for (from_state_id = 0; from_state_id < n_state;
			     from_state_id++) {
				if (strcmp(state[from_state_id], from_state) ==
				    0) {
					break;
				}
			}
			if (from_state_id == n_state) {
				state =
				    (char **)realloc(state,
						     (n_state +
						      1) * sizeof(char *));
				if (state == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
				state[n_state] = strdup(from_state);
				if (state[n_state++] == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
			}
			for (to_state_id = 0; to_state_id < n_state;
			     to_state_id++) {
				if (strcmp(state[to_state_id], to_state) == 0) {
					break;
				}
			}
			if (to_state_id == n_state) {
				state =
				    (char **)realloc(state,
						     (n_state +
						      1) * sizeof(char *));
				if (state == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
				state[n_state] = strdup(to_state);
				if (state[n_state++] == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
			}
			for (wakeup_id = 0; wakeup_id < n_wakeup; wakeup_id++) {
				if (strcmp(wakeup[wakeup_id], wakeup_str) == 0) {
					break;
				}
			}
			if (wakeup_id == n_wakeup) {
				wakeup =
				    (char **)realloc(wakeup,
						     (n_wakeup +
						      1) * sizeof(char *));
				if (wakeup == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
				wakeup[n_wakeup] = strdup(wakeup_str);
				if (wakeup[n_wakeup++] == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
			}
			if (n_sched_switch >= m_sched_switch) {
				sched_switch =
				    (sched_switch_type *) realloc(sched_switch,
								  (m_sched_switch
								   +
								   1024) *
								  sizeof
								  (sched_switch_type));
				if (sched_switch == NULL) {
					fprintf(stderr, "Cannot malloc\n");
					return 1;
				}
				m_sched_switch += 1024;
			}
			sched_switch[n_sched_switch].program_id = program_id;
			sched_switch[n_sched_switch].from_cpu = from_cpu;
			sched_switch[n_sched_switch].time = time;
			sched_switch[n_sched_switch].from_pid = from_pid;
			sched_switch[n_sched_switch].from_prio = from_prio;
			sched_switch[n_sched_switch].from_state_id =
			    from_state_id;
			sched_switch[n_sched_switch].wakeup_id = wakeup_id;
			sched_switch[n_sched_switch].to_cpu = to_cpu;
			sched_switch[n_sched_switch].to_pid = to_pid;
			sched_switch[n_sched_switch].to_prio = to_prio;
			sched_switch[n_sched_switch].to_state_id = to_state_id;
			n_sched_switch++;
			if (wakeup_str[0] == '=') {
				if (first[from_cpu] == 0) {
					program[program_id].time +=
					    time - last_time[from_cpu];
				} else if (first[from_cpu]) {
					first[from_cpu]--;
				}
				last_time[from_cpu] = time;
			}
		}
	}
	for (nof_bits = 31; nof_bits > 0; nof_bits--) {
		if (max_cpu & (1 << nof_bits)) {
			break;
		}
	}
	time = 0;
	for (i = 0; i < n_program; i++) {
		time += program[i].time;
	}
	if (time == 0) {
		time = 1;
	}
	for (i = 0; i < n_sched_switch; i++) {
		for (j = 0; j < n_program; j++) {
			if (sched_switch[i].from_pid == program[j].task) {
				break;
			}
		}
		sched_switch[i].from_pid = j == n_program ? 0 : j;
		for (j = 0; j < n_program; j++) {
			if (sched_switch[i].to_pid == program[j].task) {
				break;
			}
		}
		sched_switch[i].to_pid = j == n_program ? 0 : j;
	}
	for (i = 0; i < n_program; i++) {
		/* Take the highest priority and hope that this is the correct one. */
		/* It might be incorrect due to priority inheritance. */
		n = 0;
		for (j = 0; j < n_sched_switch; j++) {
			if (sched_switch[j].from_pid == i) {
				if (sched_switch[j].from_prio > n) {
					n = sched_switch[j].from_prio;
				}
			}
			if (sched_switch[j].to_pid == i) {
				if (sched_switch[j].to_prio > n) {
					n = sched_switch[j].to_prio;
				}
			}
		}
		program[i].prio = n;
		if (program[i].prio < 100) {
			sprintf(line, "%s#r%u#%d", program[i].name,
				99 - program[i].prio,
				(int)(100.0 * program[i].time / time + 0.5));
		} else if (program[i].prio == 140) {
			sprintf(line, "%s#idle#%d", program[i].name,
				(int)(100.0 * program[i].time / time + 0.5));
		} else {
			sprintf(line, "%s#n%d#%d", program[i].name,
				(int)(120 - program[i].prio),
				(int)(100.0 * program[i].time / time + 0.5));
		}
		free(program[i].name);
		program[i].name = strdup(line);
		if (program[i].name == NULL) {
			fprintf(stderr, "Cannot malloc\n");
			return 1;
		}
		if (priority_inheritance) {
			for (n = 0; n < n_sched_switch; n++) {
				if (sched_switch[n].from_pid == i &&
				    sched_switch[n].from_prio !=
				    program[i].prio) {
					printf("%.6f %.0fus %s %u %u\n",
					       sched_switch[n].time,
					       (sched_switch[n].time -
						sched_switch[0].time) * 1e6,
					       program[i].name,
					       sched_switch[n].from_prio,
					       program[i].prio);
				}
				if (sched_switch[n].to_pid == i &&
				    sched_switch[n].to_prio !=
				    program[i].prio) {
					printf("%.6f %.0fus %s %u %u\n",
					       sched_switch[n].time,
					       (sched_switch[n].time -
						sched_switch[0].time) * 1e6,
					       program[i].name,
					       sched_switch[n].to_prio,
					       program[i].prio);
				}
			}
		}
	}
	if (max_sched_delay) {
		for (i = 0; i < n_program; i++) {
			program[i].running = 0;
			program[i].last_time = 0;
			program[i].max_time = 0;
			program[i].max_pos = 0;
		}
		for (i = last_comment; i < n_sched_switch; i++) {
			if (wakeup[sched_switch[i].wakeup_id][0] == '=') {
				if (program[sched_switch[i].to_pid].running ==
				    (unsigned int)-2) {
					if (sched_switch[i].time -
					    program[sched_switch[i].to_pid].
					    last_time >
					    program[sched_switch[i].to_pid].
					    max_time) {
						program[sched_switch[i].to_pid].
						    max_time =
						    sched_switch[i].time -
						    program[sched_switch[i].
							    to_pid].last_time;
						program[sched_switch[i].to_pid].
						    max_pos =
						    sched_switch[i].time;
					}
				}
				program[sched_switch[i].from_pid].running =
				    (unsigned int)-1;
				program[sched_switch[i].to_pid].running =
				    sched_switch[i].to_cpu;
			} else if (wakeup[sched_switch[i].wakeup_id][0] == '+') {
				program[sched_switch[i].to_pid].running =
				    (unsigned int)-2;
				program[sched_switch[i].to_pid].last_time =
				    sched_switch[i].time;
			}
		}
		for (i = 0; i < n_program; i++) {
			if (program[i].max_time != 0.0) {
				printf("%8.6f %8.6f %.0fus %s\n",
				       program[i].max_time, program[i].max_pos,
				       (program[i].max_pos -
					sched_switch[0].time) * 1e6,
				       program[i].name);
			}
		}
	}
	if (output == MATLAB) {
		for (i = 0; i < n_program; i++) {
			fprintf(fpo,
				"program(%u).str = '%s'; program_time(%u) = %.6f; "
				"cpuload(%u) = %.6f; program_task(%u) = %u;\n",
				i + 1, program[i].name, i + 1, program[i].time,
				i + 1, 100.0 * program[i].time / time, i + 1,
				program[i].task);
		}
		for (i = 0; i < n_state; i++) {
			fprintf(fpo, "state(%u).str = '%s';\n", i + 1,
				state[i]);
		}
		for (i = 0; i < n_wakeup; i++) {
			fprintf(fpo, "wakeup(%u).str = '%s';\n", i + 1,
				wakeup[i]);
		}
		fprintf(fpo, "program_id = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].program_id);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "from_cpu = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].from_cpu);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "time = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%.6f ", sched_switch[i].time);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "from_pid = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].from_pid);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "from_prio = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].from_prio);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "from_state_id = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].from_state_id);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "wakeup_id = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].wakeup_id);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "to_cpu = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].to_cpu);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "to_pid = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].to_pid);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "to_prio = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].to_prio);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo, "to_state_id = { ");
		for (i = 0; i < n_sched_switch; i++) {
			fprintf(fpo, "%u ", sched_switch[i].to_state_id);
		}
		fprintf(fpo, "};\n");
		fprintf(fpo,
			"save -v7 sched_switch.mat program program_time program_task "
			"cpuload state wakeup program_id from_cpu time from_pid "
			"from_prio from_state_id wakeup_id to_cpu to_pid to_prio "
			"to_state_id;\n");
	} else {
		fprintf(fpo, "$timescale 1us $end\n");
		fprintf(fpo, "$scope module sched_switch $end\n");
		for (i = 0; i < n_program; i++) {
			j = 0;
			n = i;
			do {
				line[j++] = (n % 94) + 33;
				n = n / 94;
			} while (n != 0);
			line[j] = '\0';
			array[0] = '\0';
			if (nof_bits > 0) {
				sprintf(array, "[%u:0] ", nof_bits);
			}
			fprintf(fpo, "$var wire %u %s %s %s$end\n",
				nof_bits + 1, line, program[i].name, array);
			program[i].tag = strdup(line);
			if (program[i].tag == NULL) {
				fprintf(stderr, "Cannot malloc\n");
				return 1;
			}
		}
		fprintf(fpo, "$upscope $end\n");
		fprintf(fpo, "$enddefinitions $end\n");
		/* Z   tri-state signal (no cpu assigned) */
		/* U   undefined (wakeup is done waiting for cpu to become ready) */
		/* 0/1 binary encoded cpu number */
		/* L/H binary encoded cpu number with priority inheritance */
		fprintf(fpo, "#0\n");
		for (i = 0; i < n_program; i++) {
			if (nof_bits > 0) {
				fprintf(fpo, "b");
			}
			for (j = 0; j <= nof_bits; j++) {
				fprintf(fpo, "Z");
			}
			if (nof_bits > 0) {
				fprintf(fpo, " ");
			}
			fprintf(fpo, "%s\n", program[i].tag);
		}
		for (i = 0; i < n_sched_switch; i++) {
			if (sched_switch[i].time - sched_switch[0].time != 0) {
				if (wakeup[sched_switch[i].wakeup_id][0] == '=') {
					if (i == 0
					    || sched_switch[i].time !=
					    sched_switch[i - 1].time) {
						fprintf(fpo, "#%.0f\n",
							(sched_switch[i].time -
							 sched_switch[0].time) *
							1e6);
					}
					if (sched_switch[i].from_pid !=
					    sched_switch[i].to_pid) {
						if (nof_bits > 0) {
							fprintf(fpo, "b");
						}
						for (j = 0; j <= nof_bits; j++) {
							fprintf(fpo, "Z");
						}
						if (nof_bits > 0) {
							fprintf(fpo, " ");
						}
						fprintf(fpo, "%s\n",
							program[sched_switch[i].
								from_pid].tag);
					}
					if (nof_bits > 0) {
						fprintf(fpo, "b");
					}
					if (sched_switch[i].to_prio !=
					    program[sched_switch[i].to_pid].
					    prio) {
						for (j = 0; j <= nof_bits; j++) {
							fprintf(fpo, "%c",
								"LH"[(sched_switch[i].to_cpu >> (nof_bits - j)) & 1]);
						}
					} else {
						for (j = 0; j <= nof_bits; j++) {
							fprintf(fpo, "%c",
								"01"[(sched_switch[i].to_cpu >> (nof_bits - j)) & 1]);
						}
					}
					if (nof_bits > 0) {
						fprintf(fpo, " ");
					}
					fprintf(fpo, "%s\n",
						program[sched_switch[i].to_pid].
						tag);
				} else if (wakeup[sched_switch[i].wakeup_id][0]
					   == '+') {
					if (i == 0
					    || sched_switch[i].time !=
					    sched_switch[i - 1].time) {
						fprintf(fpo, "#%.0f\n",
							(sched_switch[i].time -
							 sched_switch[0].time) *
							1e6);
					}
					if (nof_bits > 0) {
						fprintf(fpo, "b");
					}
					for (j = 0; j <= nof_bits; j++) {
						fprintf(fpo, "X");
					}
					if (nof_bits > 0) {
						fprintf(fpo, " ");
					}
					fprintf(fpo, "%s\n",
						program[sched_switch[i].to_pid].
						tag);
				}
			}
		}
	}
	fclose(fpi);
	fclose(fpo);
	free(first);
	free(last_time);
	for (i = 0; i < n_program; i++) {
		free(program[i].name);
		free(program[i].tag);
	}
	free(program);
	free(state);
	free(wakeup);
	free(sched_switch);
	return 0;
}
