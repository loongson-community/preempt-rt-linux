/*
 * OHCI HCD (Host Controller Driver) for SM501 USB Host.
 *
 * (C) Copyright 2007 Boyod Yang <Boyod.yang@siliconmotion.com.cn>
 *
 *
 * This file is licenced under the GPL.
 */

/*-------------------------------------------------------------------------*/

/*
 * OHCI deals with three types of memory:
 *	- data used only by the HCD ... kmalloc is fine
 *	- async and periodic schedules, shared by HC and HCD ... these
 *	  need to use dma_pool or dma_alloc_coherent
 *	- driver buffers, read/written by HC ... the hcd glue or the
 *	  device driver provides us with dma addresses
 *
 * There's also "register" data, which is memory mapped.
 * No memory seen by this driver (or any HCD) may be paged out.
 */

/*-------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/scatterlist.h>
#include <linux/list.h>
#include <linux/usb.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/poison.h>

#include <linux/sm501.h>
#include <linux/sm501_regs.h>

//#define USE_SYSTEM_MEMORY
extern unsigned long  sm501_usb_buffer_phys_base;

//#define MALLOCK_BOUNDART_64BYTE
#define MALLOCK_BOUNDART_32BYTE
//#define MALLOCK_BOUNDART_16BYTE
#define  OC_MEM_PCI_MAP_DEBUG
#define  OC_MEM_PCI_MAP_USE_MEMCPY

#define MAKE_POINTER(p, offset)	((oc_mem_head *)((unsigned long)(p) + (offset)))

#define INIT_OC_MEM_HEAD(p, s) \
	{\
		INIT_LIST_HEAD(&(p)->free_list);\
		(p)->size = (s);\
		(p)->flag = 0;\
	}
#define IS_ALLOCATED_MEMORY(p) ((struct list_head *)(p) == (p)->free_list.next \
		&& (p)->free_list.next == (p)->free_list.prev)

#if 0
#define USB_BASEOFFSET 0x700000
#define OC_MEM_VAR_TO_OFFSET(x) ((u32)x - OC_MEM_TOP +USB_BASEOFFSET)
#define OC_MEM_OFFSET_TO_VAR(x) ((u32)x + OC_MEM_TOP - USB_BASEOFFSET)
#else
#define OC_MEM_VAR_TO_OFFSET(x) ((unsigned long)x - OC_MEM_TOP + (sm50x_mem_size-USB_DMA_BUFFER_SIZE))
#define OC_MEM_OFFSET_TO_VAR(x) ((unsigned long)x + OC_MEM_TOP - (sm50x_mem_size-USB_DMA_BUFFER_SIZE))
#endif

#ifdef USE_SYSTEM_MEMORY
#define SM501_VIRT_TO_PHYS(x)   (__pa(x)&(0x000fffff)|(0x08000000))
#define SM501_PHYS_TO_VIRT(x)   __va(x&(0x000fffff)|(sm501_usb_buffer_phys_base&0xfff00000))

#else
#define SM501_VIRT_TO_PHYS(x) OC_MEM_VAR_TO_OFFSET(x)
#define SM501_PHYS_TO_VIRT(x) OC_MEM_OFFSET_TO_VAR(x)
#endif


/* memory block header */
typedef struct __oc_mem_head {
        struct list_head free_list;
	u32	  size;
	u32    flag;
	u32    dummy[2];	// for 32byte boundary
}oc_mem_head;

#define OHCI_MEM_HCCA_SIZE			0x100

static unsigned long OC_MEM_TOP = 0;
static unsigned long OC_MEM_BASE = 0;
static unsigned long OC_MEM_SIZE = 0;
static unsigned long OC_MEM_HCCA_BASE = 0;

static spinlock_t oc_mem_list_lock = SPIN_LOCK_UNLOCKED;
static int is_oc_hcca_allocated = 0;

/*
 * There's basically three types of memory:
 *      - data used only by the HCD ... kmalloc is fine
 *      - async and periodic schedules, shared by HC and HCD ... these
 *        need to use dma_pool or dma_alloc_coherent
 *      - driver buffers, read/written by HC ... the hcd glue or the
 *        device driver provides us with dma addresses
 *
 * There's also PCI "register" data, which is memory mapped.
 * No memory seen by this driver is pagable.
 */

/*-------------------------------------------------------------------------*/

/* Sm501_mem_init : Memory initial for SM501 OHCI USB buffer
 *
 * u32 mem_base   : Virtual address for USB buffer
 * size_t size    : USB buffer size
 *
 */

int sm501_mem_init(unsigned long mem_base, size_t size)
{
	oc_mem_head *top, *cp;

	if (OC_MEM_BASE) return 0;

	OC_MEM_TOP = mem_base;
	OC_MEM_HCCA_BASE = mem_base;
	OC_MEM_BASE = mem_base + OHCI_MEM_HCCA_SIZE;	/* 0x100 for HCCA */
	OC_MEM_SIZE = size - OHCI_MEM_HCCA_SIZE;

	b_dbg("DRAM:%08x   memory size:%08d\n",SmRead32(DRAM_CONTROL),FIELD_GET(SmRead32(DRAM_CONTROL),DRAM_CONTROL,EXTERNAL_SIZE));

	top = MAKE_POINTER(OC_MEM_BASE, 0);   //OC_MEM_BASE  to a head point
	INIT_OC_MEM_HEAD(top, 0);          //build  td list
	cp = MAKE_POINTER(top, sizeof(oc_mem_head) + 0);
	INIT_OC_MEM_HEAD(cp, OC_MEM_SIZE - sizeof(oc_mem_head) - top->size - sizeof(oc_mem_head));  //build   ed list

	list_add(&cp->free_list, &top->free_list);    //  null->cp->top->null

	return 0;
}
EXPORT_SYMBOL(sm501_mem_init);

void sm501_mem_cleanup(void)
{
	OC_MEM_TOP = 0;
	OC_MEM_BASE = 0;
	OC_MEM_SIZE = 0;
	OC_MEM_HCCA_BASE = 0;
	oc_mem_list_lock = SPIN_LOCK_UNLOCKED;
	is_oc_hcca_allocated = 0;
}
EXPORT_SYMBOL(sm501_mem_cleanup);

static void sm501_mem_list_add(oc_mem_head *cp)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	struct list_head *n;

	/* address order */
	list_for_each(n, &top->free_list) {
		if ((unsigned long)n > (unsigned long)cp) {
			list_add_tail(&cp->free_list, n);
			return;
		}
	}
	list_add_tail(&cp->free_list, &top->free_list);
}

static void sm501_mfree(void *fp)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	oc_mem_head *cp = (oc_mem_head *)fp - 1; /* Get free memory's oc_mem_head address */
	struct list_head *n;

	if (fp == NULL){
		return;
	}

	if (!IS_ALLOCATED_MEMORY(cp)) {
		smi_dbg("%08lx is not allocated memory. n=%08lx p=%08x s=%04x\n",
				(unsigned long)fp, (unsigned long)cp->free_list.next, (unsigned long)cp->free_list.prev, cp->size);
		return;
	}

	list_for_each(n, &top->free_list) {
		/* cp is just previous of this free block */
		if (MAKE_POINTER(cp, cp->size + sizeof(oc_mem_head)) == (oc_mem_head *)n){
			oc_mem_head *pp = (oc_mem_head *)n->prev;
			smi_dbg("P %08lx-%04x merged to %08lx-%04x\n",
					(unsigned long)n, ((oc_mem_head *)n)->size, (unsigned long)cp, cp->size);
			list_del(n);
			INIT_OC_MEM_HEAD(cp, cp->size + ((oc_mem_head *)n)->size + sizeof(oc_mem_head));

			/* cp merge to previous free block */
			if (cp == MAKE_POINTER(pp, pp->size + sizeof(oc_mem_head)) && pp != top) {
				smi_dbg("P %08lx-%04x merged to %08lx-%04x\n", (unsigned long)cp, cp->size, (unsigned long)pp, pp->size);
				list_del(&pp->free_list);
				INIT_OC_MEM_HEAD(pp, pp->size + cp->size + sizeof(oc_mem_head));
				cp = pp;
			}
			break;
		}

		/* cp is just next of this free block */
		if (cp == MAKE_POINTER(n, ((oc_mem_head *)n)->size + sizeof(oc_mem_head))) {
			oc_mem_head *np = (oc_mem_head *)n->next;
			smi_dbg("N %08lx-%04x merged to %08lx-%04x\n",
					(unsigned long)cp, cp->size, (unsigned long)n, ((oc_mem_head *)n)->size);
			list_del(n);
			INIT_OC_MEM_HEAD((oc_mem_head *)n, ((oc_mem_head *)n)->size
					+ cp->size + sizeof(oc_mem_head));
			cp = (oc_mem_head *)n;
			if (np == MAKE_POINTER(cp, cp->size + sizeof(oc_mem_head))) {
				smi_dbg("N %08lx-%04x merged to %08lx-%04x\n", (unsigned long)np, np->size, (unsigned long)cp, cp->size);
				list_del(&np->free_list);
				INIT_OC_MEM_HEAD(cp, cp->size + np->size + sizeof(oc_mem_head));
			}
			break;
		}
	}

	/* Clear free memory content to 0xff */
	memset((void *)(cp + 1), POISON_FREE, cp->size);
	sm501_mem_list_add(cp);
}

static void *sm501_malloc(size_t size, dma_addr_t *handle, int gfp)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	oc_mem_head *cp = NULL;
	struct list_head *n;

	if (!top)
		return NULL;

	if (size == 0){
		*handle = 0;
		return NULL;
	}

#if defined(MALLOCK_BOUNDART_64BYTE)
	size = ((size + 64 - 1) / 64) * 64;	// for 64byte boundary
#elif defined(MALLOCK_BOUNDART_32BYTE)
	size = ((size + 32 - 1) / 32) * 32;	// for 32byte boundary
#elif defined(MALLOCK_BOUNDART_16BYTE)
	size = ((size + 16 - 1) / 16) * 16;	// for 16byte boundary
#else
	size = ((size + sizeof(ulong) - 1) / sizeof(ulong)) * sizeof(ulong);
#endif

	list_for_each(n, &top->free_list) {
		cp = list_entry(n, oc_mem_head, free_list);

		if(cp->size >= size){
			break;
		}
	}

	if (!cp) {
		*handle = 0;
		return NULL;
	}

	if (cp->size > size + sizeof(oc_mem_head)) {
		oc_mem_head *fp;
		size_t alloc = sizeof(oc_mem_head) + size; /* memory_block_head + request_size */
		size_t new_size = cp->size - alloc;

		list_del(&cp->free_list);
		INIT_OC_MEM_HEAD(cp, size);

		fp = MAKE_POINTER(cp, alloc);
		INIT_OC_MEM_HEAD(fp, new_size);
		sm501_mem_list_add(fp);
		smi_dbg("separate %08lx-%04x %08lx-%04x\n", (unsigned long)SM501_VIRT_TO_PHYS(cp), cp->size, (unsigned long)SM501_VIRT_TO_PHYS(cp), fp->size);
	} else if (cp->free_list.next == cp->free_list.prev
				&& cp->free_list.next == &top->free_list) {
		printk(KERN_ERR "mallock error %08lx-%04x\n", (unsigned long)SM501_VIRT_TO_PHYS(cp), cp->size);
		*handle = 0;
		return NULL;
	} else {
		smi_dbg("alloc this %08x-%04x\n", (unsigned long)SM501_VIRT_TO_PHYS(cp), cp->size);
		list_del_init(&cp->free_list);
	}
	memset((void *)(cp + 1), POISON_INUSE, cp->size);
	*handle = (dma_addr_t)SM501_VIRT_TO_PHYS((cp + 1));
	return (void *)(cp + 1);
}



void usb_free_coherent(void *p)
{
	//mill add
	unsigned long flags;
	b_dbg("usb_free_coherent %08x\n", (unsigned long)SM501_VIRT_TO_PHYS(p));
	//spin_lock(&oc_mem_list_lock);
	spin_lock_irqsave(&oc_mem_list_lock, flags);
	sm501_mfree((void *)p);
	//spin_unlock(&oc_mem_list_lock);
	spin_unlock_irqrestore (&oc_mem_list_lock, flags);

}
EXPORT_SYMBOL(usb_free_coherent);

unsigned long sm501_p2v(dma_addr_t addr)
{
	return (unsigned long)(SM501_PHYS_TO_VIRT(addr));
}
EXPORT_SYMBOL(sm501_p2v);

void *usb_alloc_coherent(size_t s, dma_addr_t *handle, int v)
{
	void *p;
	//mill add
	unsigned long flags;

	b_dbg("usb_alloc_coherent size %08d\n", s);

	if (!s)
		return NULL;

	//spin_lock(&oc_mem_list_lock);
	spin_lock_irqsave(&oc_mem_list_lock, flags);
	p = sm501_malloc(s, handle, v);
	//spin_unlock(&oc_mem_list_lock);
	spin_unlock_irqrestore (&oc_mem_list_lock, flags);

	if (p == NULL) {
		printk(KERN_ERR "oc_malloc: no more memory on chip RAM (%zdbyts)", s);
		WARN_ON(1);
		return p;
	}
	b_dbg("usb_alloc_coherent 0x%08x\n", *handle);

	return p;
}
EXPORT_SYMBOL(usb_alloc_coherent);


#ifdef OC_MEM_PCI_MAP_DEBUG
static inline char* DMA_FLAG_DESC(int direction)
{
	switch (direction) {
	case PCI_DMA_NONE:		return "NONE";
	case PCI_DMA_FROMDEVICE:	return "FROMDEVICE";
	case PCI_DMA_TODEVICE:		return "TODEVICE";
	case PCI_DMA_BIDIRECTIONAL:	return "BIDIRECTIONAL";
	}
	return "unknown";
}
#endif


dma_addr_t
DMA_MAP_SINGLE(struct device *hwdev, void *ptr, size_t size, enum dma_data_direction direction)
{
#ifndef OC_MEM_PCI_MAP_USE_MEMCPY
        return (dma_addr_t)SM501_VIRT_TO_PHYS(ptr);
#else
	void *p = NULL;
	dma_addr_t phys;
	size_t l = ((size + (sizeof(unsigned long)-1)) & ~(sizeof(unsigned long)-1)) + sizeof(unsigned long);

	switch (direction) {
	case DMA_NONE:
		WARN_ON(1);
		return 0;
		break;
	case DMA_FROM_DEVICE:        /* invalidate only */
		p = usb_alloc_coherent(l, &phys,GFP_KERNEL | GFP_DMA);
//		dma_cache_inv(p, size);
		break;
	case DMA_TO_DEVICE:          /* writeback only */
//		dma_cache_wback(p, size);
		memcpy(p, ptr, size);
	case DMA_BIDIRECTIONAL:     /* writeback and invalidate */
		p = usb_alloc_coherent(l, &phys,GFP_KERNEL | GFP_DMA);
		memcpy(p, ptr, size);
//		dma_cache_wback_inv(p, size);
		break;
	}

	if (p==NULL)
		return 0;
	*((unsigned long *)((unsigned long)p + l - sizeof(unsigned long)))
							= (unsigned long)ptr;

	smi_dbg("PCI_MAP_SINGLE:   %s %08x - %04x , %08x - %04x\n",
			DMA_FLAG_DESC(direction),
			ptr, size, SM501_VIRT_TO_PHYS(p), l);
       return SM501_VIRT_TO_PHYS(p);

#endif
}
EXPORT_SYMBOL(DMA_MAP_SINGLE);

void
DMA_UNMAP_SINGLE(struct device * hwdev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction)
{
#ifndef OC_MEM_PCI_MAP_USE_MEMCPY
        /* nothing to do */
#else
	void *p, *ptr;
	size_t l = ((size + (sizeof(unsigned long)-1)) & ~(sizeof(unsigned long)-1)) + sizeof(unsigned long);
	p = (void *)SM501_PHYS_TO_VIRT(dma_addr);
	ptr = (void *)*((unsigned long *)((unsigned long)p + l - sizeof(unsigned long)));

	switch (direction) {
	case DMA_NONE:
		WARN_ON(1);
		break;
	case DMA_FROM_DEVICE:        /* invalidate only */
//		dma_cache_inv(p, size);
		memcpy(ptr, p, size);
	case DMA_BIDIRECTIONAL:     /* writeback and invalidate */
		memcpy(ptr, p, size);
		usb_free_coherent(p);
//		dma_cache_wback_inv(p, size);
		break;
	case DMA_TO_DEVICE:          /* writeback only */
		usb_free_coherent(p);
//		dma_cache_wback(p, size);
		break;
	}

	smi_dbg("PCI_UNMAP_SINGLE: %s %08x - %04x , %08x\n",
			DMA_FLAG_DESC(direction),  ptr, size, dma_addr);

#endif
}

EXPORT_SYMBOL(DMA_UNMAP_SINGLE);


int DMA_MAP_SG(struct device *dev, struct scatterlist *sg, int nents,
	enum dma_data_direction direction)
{
	int i;

	WARN_ON(direction == DMA_NONE);

	for (i = 0; i < nents; i++, sg++) {
		unsigned long addr;

		addr = sg_virt(&sg[0]);
		sg->dma_address = DMA_MAP_SINGLE(dev,
				                   (void *)(addr + sg->offset),
						   sg->length, direction);
		if (sg->dma_address==0){
		for (i--, sg--; i >0; i--, sg--)
			DMA_UNMAP_SINGLE(dev,sg->dma_address,sg->length, direction);
		return 0;
		}
	}

	return nents;
}

EXPORT_SYMBOL(DMA_MAP_SG);

void DMA_UNMAP_SG(struct device *dev, struct scatterlist *sg, int nhwentries,
	enum dma_data_direction direction)
{
	int i;

	WARN_ON(direction == DMA_NONE);

	for (i = 0; i < nhwentries; i++, sg++) {
		DMA_UNMAP_SINGLE(dev,sg->dma_address,sg->length, direction);
	}
}

EXPORT_SYMBOL(DMA_UNMAP_SG);

void *sm501_hcca_alloc(struct device *dev, size_t size, dma_addr_t *handle)
{
	void *p;

	/* No assignment size */
	if (!size)
		return NULL;

	if (!OC_MEM_HCCA_BASE) {
		printk(KERN_ERR "oc_malloc: not set OC_MEM_BASE.");
		return NULL;
	}

	if (is_oc_hcca_allocated) {
		printk(KERN_ERR "oc_hcca_alloc: HCCA memory was alrady allocated.");
		return NULL;
	}

	if (size > OHCI_MEM_HCCA_SIZE) {
		printk(KERN_ERR "oc_hcca_alloc: no more memory on RAM0 (size=%zd)", size);
		return NULL;
	}

	p = (void *)OC_MEM_HCCA_BASE;	/* 256byte boundary */
	*handle = (dma_addr_t)SM501_VIRT_TO_PHYS(p);
	is_oc_hcca_allocated=1;
	return p;
}
EXPORT_SYMBOL(sm501_hcca_alloc);

void sm501_hcca_free(struct device *dev)
{
	is_oc_hcca_allocated = 0;
}
EXPORT_SYMBOL(sm501_hcca_free);

#if 0
static void free_list_dump(void)
{
	oc_mem_head *top = (oc_mem_head *)OC_MEM_BASE;
	oc_mem_head *cp = NULL;
	struct list_head *n;

	smi_dbg("FILE: %s, FUNC: %s, LINE: %d\n", __FILE__, __FUNCTION__, __LINE__);

	list_for_each(n, &top->free_list){
		cp = list_entry(n, oc_mem_head, free_list);
		smi_dbg("free_block phys addr = 0x%08x\n", SM501_VIRT_TO_PHYS(cp));
		smi_dbg("free_block hold space = 0x%08x\n", cp->size);
	}
}
#endif

