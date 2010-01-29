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
#define __NR_sched_setscheduler_ex	299
#endif
#ifdef __i386__
/* XXX use the proper syscall number */
#define __NR_sched_setscheduler_ex	337
#endif

#ifdef __arm__
/* XXX use the proper syscall number */
#define __NR_sched_setscheduler_ex	365
#endif

#ifdef __mips__
/* XXX use the proper syscall number */
#if _MIPS_SIM == _MIPS_SIM_ABI32
#define __NR_Linux			4000
#define __NR_sched_setscheduler_ex	(__NR_Linux + 335)
#endif

#if _MIPS_SIM == _MIPS_SIM_ABI64
#define __NR_Linux			5000
#define __NR_sched_setscheduler_ex	(__NR_Linux + 294)
#endif

#if _MIPS_SIM == _MIPS_SIM_NABI32
#define __NR_Linux			6000
#define __NR_sched_setscheduler_ex	(__NR_Linux + 298)
#endif

#endif /* __mips__ */

#define SCHED_SIG_RORUN		0x80000000
#define SCHED_SIG_DMISS		0x40000000

struct sched_param_ex {
	int sched_priority;
	struct timespec sched_runtime;
	struct timespec sched_deadline;
	struct timespec sched_period;
	int sched_flags;
};


int sched_setscheduler_ex(pid_t pid, int policy, unsigned len,
			  struct sched_param_ex *param);
