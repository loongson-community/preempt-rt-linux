#ifndef __MEM_H
#define __MEM_H

/*
 * high memory space
 *
 * in loongson2e, starts from 512M
 * in loongson2f, starts from 2G + 256M
 */
#ifdef CONFIG_CPU_LOONGSON2E
#define LOONGSON_HIGHMEM_START	0x20000000
#else
#define LOONGSON_HIGHMEM_START	0x90000000
#endif

/*
 * the peripheral registers(MMIO):
 *
 * On the Lemote Loongson 2e system, reside between 0x1000:0000 and 0x2000:0000.
 * On the Lemote Loongson 2f system, reside between 0x1000:0000 and 0x8000:0000.
 */

#define LOONGSON_MMIO_MEM_START 0x10000000

#ifdef CONFIG_CPU_LOONGSON2E
#define LOONGSON_MMIO_MEM_END	0x20000000
#else
#define LOONGSON_MMIO_MEM_END	0x80000000
#endif

#endif	/* !__MEM_H */
