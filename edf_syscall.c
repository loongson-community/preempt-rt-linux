#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <time.h>
#include "edf_syscall.h"


int sched_setscheduler2(pid_t pid, int policy,
				struct sched_param2 *param)
{
	return syscall(__NR_sched_setscheduler2, pid, policy, param);
}
