#ifndef __CONFIG_H__
#define __CONFIG_H__
#include <linux/autoconf.h>
#ifdef CONFIG_64BIT
#undef CONFIG_64BIT
#define CONFIG_32BIT
#endif
#endif
