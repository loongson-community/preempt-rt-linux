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


int sched_setscheduler_ex(pid_t pid, int policy, unsigned len,
				struct sched_param_ex *param)
{
	return syscall(__NR_sched_setscheduler_ex, pid, policy, len, param);
}
