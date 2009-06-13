/* machine-specific command line initialization */
#ifdef CONFIG_SYS_HAS_MACH_PROM_INIT_CMDLINE
extern void __init mach_prom_init_cmdline(void);
#else
void __init mach_prom_init_cmdline(void)
{
}
#endif

