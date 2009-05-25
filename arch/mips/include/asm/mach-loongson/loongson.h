/*
 * loongson-specific header file
 */

/* loongson internal northbridge initialization */
extern void bonito_irq_init(void);

/* command line */
extern unsigned long bus_clock, cpu_clock_freq;
extern unsigned long memsize, highmemsize;

/* loongson-based machines specific reboot setup */
extern void loongson_reboot_setup(void);

