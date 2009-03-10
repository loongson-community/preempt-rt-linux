/*
 * include/linux/dmapool.h
 *
 * Allocation pools for DMAable (coherent) memory.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef DRUVERS_USB_VGXPOOL_H
#define	DRIVERS_USB_VGXPOOL_H

#include <asm/io.h>
#include <asm/scatterlist.h>

struct dma_pool *vgx_pool_create(const char *name, struct device *dev,
			size_t size, size_t align, size_t allocation);

void vgx_pool_destroy(struct dma_pool *pool);

void *vgx_pool_alloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle);

void vgx_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr);


/*
 * Managed DMA pool
 */
struct dma_pool *vgxm_pool_create(const char *name, struct device *dev,
				  size_t size, size_t align, size_t allocation);
void vgxm_pool_destroy(struct dma_pool *pool);

#endif

