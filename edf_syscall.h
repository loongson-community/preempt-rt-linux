#include <linux/unistd.h>
#include <linux/types.h>

#define SCHED_EDF	6
/*
 * On x86-64 make the 64bit structure have the same alignment as the
 * 32bit structure. This makes 32bit emulation easier.
 *
 * UML/x86_64 needs the same packing as x86_64
 */
#ifdef __x86_64__
/* XXX use the proper syscall number */
#define __NR_sched_setscheduler2	297
#endif
#ifdef __i386__
/* XXX use the proper syscall number */
#define __NR_sched_setscheduler2	335
#endif

#ifdef __arm__
/* XXX use the proper syscall number */
#define __NR_sched_setscheduler2	363
#endif

struct sched_param2 {
	int sched_priority;
	struct timespec sched_edf_period;
	struct timespec sched_edf_runtime;
};


int sched_setscheduler2(pid_t pid, int policy,
			struct sched_param2 *param);
