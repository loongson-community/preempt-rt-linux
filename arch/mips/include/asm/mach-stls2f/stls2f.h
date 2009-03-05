/*
 * Common defines, macros and routines for accessing the STLS2F's 
 * memory mapped registers
 *
 * Copyright (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 */

#include <linux/types.h>
#include <asm/io.h>

extern void *ddr_cont; /* ioremap'd address for DDR controller */
extern void *core_config; /* for core config registers */
extern void *addr_win_config; /* address window config */

extern unsigned long cpu_clock_freq;

/* Loongson 2F CPU IDs */
#define STLS2F_CPU_ID	0x6303u

/* helper defines */

#define MSK(n) (1 << (n))

/* IRQ-related */
#define LS2F_IRQ_BASE 32

/* register regions */
#define LS2F_CONFIG_PHYS 0x1fe00100ull
#define LS2F_CONFIG_SIZE 0x100

#define LS2F_DDR_CONT_PHYS 0x0ffffe00ull
#define LS2F_DDR_CONT_SIZE 0x200

#define LS2F_ADDR_WINDOW_CONFIG_PHYS 0x3ff00000ull
#define LS2F_ADDR_WINDOW_CONFIG_SIZE 0x200

/* register definitions for core config */
#define LS2F_CORE_CONFIG_REG 0x80

#define LS2F_CORE_CONFIG_FREQ_SCALE_MSK 0x7 /* first 3 bits, 3'b000 - 0'b111 */
  #define LS2F_FREQ_SCALE_MAX 0x7 /* maximum possible clock */
  #define LS2F_FREQ_SCALE_OFF 0x0 /* disable CPU clock */
#define LS2F_CORE_CONFIG_DISABLE_SCACHE_MSK MSK(3)
#define LS2F_CORE_CONFIG_IMP_WORD_FIRST_MSK MSK(4)
/* bits 5-7 are reserved */
#define LS2F_CORE_CONFIG_DISABLE_DDR_CONF_MSK MSK(8)
#define LS2F_CORE_CONFIG_DDR_BUFFER_CPU_MSK MSK(9)
#define LS2F_CORE_CONFIG_DDR_BUFFER_PCI_MSK MSK(10)
/* bits 11-31 are reserved */

/* register definitions for DDR controller configuration */
#define LS2F_DDR_CONT_CONF_CTL 0x0
#define LS2F_DDR_CONT_CONF_CTL_AREFRESH_MSK MSK(24)
#define LS2F_DDR_CONT_CONF_CTL_AUTOPRECHARGE_MSK MSK(16)

/* address window configuration registers of interes */
#define LS2F_ADDRCONF_M0_WIN2_BASE 0x10
#define LS2F_ADDRCONF_M0_WIN2_SIZE 0x20
#define LS2F_ADDRCONF_M0_WIN2_MMAP 0x50

/* accessor methods for the Loongson-2F CPU */
#define ls2f_readb(addr) (readb(addr))
#define ls2f_readw(addr) (readw(addr))
#define ls2f_readl(addr) (readl(addr))
#define ls2f_readll(addr) (*(volatile uint64_t *)(addr))

#define ls2f_writeb(value, addr) (writeb(value, addr))
#define ls2f_writew(value, addr) (writew(value, addr))
#define ls2f_writel(value, addr) (writel(value, addr))
#define ls2f_writell(value, addr) (*(volatile uint64_t *)(addr) = (value))

/* accessor methods specific to address window configuration */
#define ls2f_addr_win_writell(value, reg) (ls2f_writell(value, \
	addr_win_config + (reg)))
#define ls2f_addr_win_readll(reg) (ls2f_readll(addr_win_config + (reg)))

/* accessor methods specific to the config registers */
#define ls2f_config_writel(reg, value) (ls2f_writel(value, core_config + (reg)))
#define ls2f_config_readl(reg) (ls2f_readl(core_config + (reg)))
