/*
 * To make schedtool works with O32 ABI under 64bit kernel, the following
 * timespec is needed. without this, the kernel will get the wrong arguments of
 * sched_setscheduler_ex() for the different timespec struct type in kernel and
 * user-space.
 * 
 * NOTE: perhaps the better method is touching the kernel space? of course,
 * this is also a not that bad method.
 */

#ifdef __mips__
#if _MIPS_SIM != _MIPS_SIM_ABI64	/* does N32 need this new timespec? */
#define __timespec_defined
struct timespec
  {
    long long tv_sec;		/* Seconds.  */
    long long tv_nsec;		/* Nanoseconds.  */
  };
#endif
#endif
