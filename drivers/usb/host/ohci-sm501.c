/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 * (C) Copyright 2002 Hewlett-Packard Company
 *
 * USB Bus Glue for SM501
 *
 * Written by Boyod Yang <boyod.yang@siliconmotion.com.cn>
 * Modified for SM501 from ohci-sa1111.c, ohci-omap.c and ohci-lh7a40.c
 * Based on fragments of previous driver by  Christopher Hoover, Rusell King et al.
 *
 *	by Ben Dooks, <ben@simtec.co.uk>
 *	Copyright (C) 2004 Simtec Electronics
 *
 * Thanks to basprog@mail.ru for updates to newer kernels
 *
 * This file is licenced under the GPL.
*/

#include <linux/signal.h>	/* SA_INTERRUPT */
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/sm501_regs.h>
#include <linux/sm501.h>
#include "vgxpool.h"

unsigned long sm501_usb_buffer_phys_base;
static unsigned long usb_video_buf;

void SMI_WaitForNotBusy(void)
{

	unsigned long i = 0x1000000,dwVal=0;
	while (i--)
	{
	        dwVal = SmRead32(CMD_INTPR_STATUS);
			if ((FIELD_GET(dwVal, CMD_INTPR_STATUS, PANEL_SYNC) == CMD_INTPR_STATUS_PANEL_SYNC_INACTIVE)&&
			     (FIELD_GET(dwVal, CMD_INTPR_STATUS, 2D_FIFO) == CMD_INTPR_STATUS_2D_FIFO_EMPTY) &&
		            (FIELD_GET(dwVal, CMD_INTPR_STATUS, 2D_SETUP)     == CMD_INTPR_STATUS_2D_SETUP_IDLE) &&
		            (FIELD_GET(dwVal, CMD_INTPR_STATUS, CSC_STATUS)     == CMD_INTPR_STATUS_CSC_STATUS_IDLE) &&
		            (FIELD_GET(dwVal, CMD_INTPR_STATUS, 2D_MEMORY_FIFO) == CMD_INTPR_STATUS_2D_MEMORY_FIFO_EMPTY)&&
		            (FIELD_GET(dwVal, CMD_INTPR_STATUS, 2D_ENGINE) == CMD_INTPR_STATUS_2D_ENGINE_IDLE)
		            )
	         		break;
			else
				printk("INTPR_STATUS:%08lx\n",dwVal);
	}

}

EXPORT_SYMBOL (SMI_WaitForNotBusy);


void dump_ohci_register(void)
{

	printk("HC register : \n");
	printk("============== Control and Status Partition ===============\n");
	printk("HcRevision         : 0x%08lx\n", SmRead32(USB_HOST_HC_REVISION));
	printk("HcControl          : 0x%08lx\n", SmRead32(USB_HOST_HC_CONTROL));
	printk("HcCommandStatus    : 0x%08lx\n", SmRead32(USB_HOST_HC_COMMANDSTATUS));
	printk("HcInterruptStatus  : 0x%08lx\n", SmRead32(USB_HOST_HC_INTERRUPTSTATUS));
	printk("HcInterruptEnable  : 0x%08lx\n", SmRead32(USB_HOST_HC_INTERRUPTENABLE));
	printk("HcInterruptDisable : 0x%08lx\n", SmRead32(USB_HOST_HC_INTERRUPTDISABLE));
	printk("============== Memory Pointer Partition ===============\n");
	printk("HcHCCA             : 0x%08lx\n", SmRead32(USB_HOST_HC_HCCA));
	printk("HcPeriodCurrentED  : 0x%08lx\n", SmRead32(USB_HOST_HC_PERIODCURRENTED));
	printk("HcControlHeadED    : 0x%08lx\n", SmRead32(USB_HOST_HC_CONTROLHEADED));
	printk("HcControlCurrentED : 0x%08lx\n", SmRead32(USB_HOST_HC_CONTROLCURRENTED));
	printk("HcBulkHeadED       : 0x%08lx\n", SmRead32(USB_HOST_HC_BULKHEADED));
	printk("HcBulkCurrentED    : 0x%08lx\n", SmRead32(USB_HOST_HC_BULKCURRENTED));
	printk("HcDoneHead         : 0x%08lx\n", SmRead32(USB_HOST_HC_DONEHEAD));
	printk("============== Frame Counter Partition ===============\n");
	printk("HcFmInterval       : 0x%08lx\n", SmRead32(USB_HOST_HC_FMINTERVAL));
	printk("HcFmRemaining      : 0x%08lx\n", SmRead32(USB_HOST_HC_FMREMAINING));
	printk("HcFmNumber         : 0x%08lx\n", SmRead32(USB_HOST_HC_FMNUMBER));
	printk("HcPeriodicStart    : 0x%08lx\n", SmRead32(USB_HOST_HC_PERIODSTART));
	printk("HcLSThreshold      : 0x%08lx\n", SmRead32(USB_HOST_HC_LSTHRESHOLD));
	printk("============== Root Hub Partition ===============\n");
	printk("HcRhDescriptorA    : 0x%08lx\n", SmRead32(USB_HOST_HC_RHDESCRIPTORA));
	printk("HcRhDescriptorB    : 0x%08lx\n", SmRead32(USB_HOST_HC_RHDESCRIPTORB));
	printk("HcRhStatus         : 0x%08lx\n", SmRead32(USB_HOST_HC_RHSTATUS));
	printk("HcRhPortStatus     : 0x%08lx\n", SmRead32(USB_HOST_HC_RHPORT1));

}
EXPORT_SYMBOL(dump_ohci_register);


void dump_sm501_register(void)
{

	printk("SM501 register: \n");
	printk("Misc Control       : 0x%08lx\n", SmRead32(MISC_CTRL));
	printk("Interrupt Status   : 0x%08lx\n", SmRead32(INTERRUPT_STATUS));
	printk("Interrupt Mask     : 0x%08lx\n", SmRead32(INTERRUPT_MASK));
	printk("Current Gate       : 0x%08lx\n", SmRead32(CURRENT_GATE));
	printk("Current Clock      : 0x%08lx\n", SmRead32(CURRENT_POWER_CLOCK));
	printk("Power Mode 0 Gate  : 0x%08lx\n", SmRead32(POWER_MODE0_GATE));
	printk("Power Mode 0 Clock : 0x%08lx\n", SmRead32(POWER_MODE0_CLOCK));
	printk("Power Mode 1 Gate  : 0x%08lx\n", SmRead32(POWER_MODE1_GATE));
	printk("Power Mode 1 Clock : 0x%08lx\n", SmRead32(POWER_MODE1_CLOCK));
	printk("Power Mode Control : 0x%08lx\n", SmRead32(POWER_MODE_CTRL));
	printk("Misc Timing        : 0x%08lx\n", SmRead32(MISCELLANEOUS_TIMING));

}
EXPORT_SYMBOL(dump_sm501_register);



void dump_sm501_ctrlhead(void)
{
#if 0

	u32 addr, addr2, val;
	struct ed *ed;
	struct td *td;
	int i;
	u8 *data;

	addr = (u32)SmRead32(USB_HOST_HC_CONTROLHEADED);
	addr2 = SM501_PHYS_TO_VIRT(addr);
	ed = (struct ed *)addr2;
	printk("ED hwINFO = 0x%08x\n", ed->hwINFO);
	printk("ED hwTailP = 0x%08x\n", ed->hwTailP);
	printk("ED hwHeadP = 0x%08x\n", ed->hwHeadP);
	printk("ED hwNextED = 0x%08x\n", ed->hwNextED);

	addr2 = SM501_PHYS_TO_VIRT(ed->hwHeadP);
	td = (struct td *)addr2;
	printk("TD hwINFO = 0x%08x\n", td->hwINFO);
	td->hwINFO = 0x02c00000;
	printk("TD hwCBP = 0x%08x\n", td->hwCBP);
	printk("TD hwNextTD = 0x%08x\n", td->hwNextTD);
	printk("TD hwBE = 0x%08x\n", td->hwBE);

	addr2 = SM501_PHYS_TO_VIRT(td->hwCBP);
	data = (u8 *)addr2;
	for(i = 0; i < 8; i++){
		printk("0x%02x ", data[i]);
	}
	printk("\n");

#if 1
	ed->hwINFO = USB_HOST_HC_REVISION;
	printk("Manual Trigger....\n");
	SmWrite32(USB_HOST_HC_CONTROLCURRENTED, SmRead32(USB_HOST_HC_CONTROLHEADED));
	val = SmRead32(0x40004);
	val |= (1UL << 4);
	SmWrite32(USB_HOST_HC_CONTROL, val);
#endif


	return;
#endif
}


EXPORT_SYMBOL(dump_sm501_ctrlhead);


static void sm501_hc_start(struct platform_device *dev, struct usb_hcd *hcd)
{

	unsigned long val;
#if 0
	/*Use problemmable PLL	*/
	if (dev->revision >=0xC0)
		val = 0xC0011211;
	SmWrite32(POWER_MODE0_CLOCK, val);
	SmWrite32(POWER_MODE1_CLOCK, val);
#endif
	smi_dbg("SM501 USB Host Controllor start  \n");

	/* Enable usb host controller */
	val = SmRead32(CURRENT_GATE);
	val = SET_FIELD(val,POWER_MODE0_GATE_USB_HOST,ENABLE);
	sm501_set_gate(val);

//  Priorty Fix

//	SmWrite32(ARBITRATION_CONTROL,0x1367254);
	// MISC Control
/*
	val = SmRead32(MISCELLANEOUS_CONTROL);
	val &= ~(0x03 << 28);
	val |= (0x01 << 24);
	SmWrite32(MISCELLANEOUS_CONTROL, SET_FIELD(val,MISCELLANEOUS_CONTROL_USB_CLOCK_SELECT,HOST_48));

	// Misc Timing
	val = SmRead32(MISCELLANEOUS_TIMING);
	SmWrite32(MISCELLANEOUS_TIMING, SET_FIELD(val,MISCELLANEOUS_TIMING_USB_HOST_MODE,NORMAL));
*/
	// Clear Interrupt Status
	SmWrite32(USB_HOST_HC_INTERRUPTSTATUS, (1UL<<31)|0x7f);

	// Enable Interrupt Mask
	val = SmRead32(INTERRUPT_MASK);
	SmWrite32(INTERRUPT_MASK, SET_FIELD(val ,INTERRUPT_MASK_USB_HOST,ENABLE));

	//SmWrite32(DRAM_CONTROL, 0x0CB02C40);
	//SmWrite32(MISCELLANEOUS_TIMING, 0x01080800);

#if 1
	/* Force reset HC by HCD */
	val = SmRead32(USB_HOST_HC_COMMANDSTATUS);
	val |= (0x01 << 0); //Force Reset
	SmWrite32(USB_HOST_HC_COMMANDSTATUS, val);
	mdelay(10);
#endif

#if 0
	SmWrite32(SYSTEM_NON_CACHE,((sm50x_mem_size-USB_DMA_BUFFER_SIZE) & 0x03FFFFFF>>12));
#else
	SmWrite32(SYSTEM_NON_CACHE,USB_DMA_BUFFER_SIZE & 0x03FFFFFF>>12);
#endif
}


#if 0
/* Fixme */
static void sm501_hc_stop(struct pci_dev *dev)
#else
static void sm501_hc_stop(void)
{

	unsigned long val;

	/* Interrupt mask, disable usb host interrupt */

	val = SmRead32(INTERRUPT_MASK);
	SmWrite32(INTERRUPT_MASK, SET_FIELD(val ,INTERRUPT_MASK_USB_HOST,DISABLE));

	/* Disable usb host clock */
	val = SmRead32(CURRENT_GATE);
	val = SET_FIELD(val,POWER_MODE0_GATE_USB_HOST,DISABLE);
	sm501_set_gate(val);
	return;
}
#endif

/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/*
 * usb_hcd_sm501_remove - shutdown processing for HCD
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_sm501_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
*/

void usb_hcd_sm501_remove(struct usb_hcd *hcd, struct platform_device *dev)
{

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region((unsigned long)hcd->regs, hcd->rsrc_len);

	sm501_hc_stop();
	usb_put_hcd(hcd);

	return;
}

/**
 * usb_hcd_sm501_probe - initialize SM501-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 */

static int usb_hcd_sm501_probe(struct platform_device *pdev, struct hc_driver *driver)
{

	struct usb_hcd		*hcd;
	struct resource	*res;
	int			retval;
	unsigned int	irq;

        printk(KERN_INFO "SM501 HCD probe start  pdev->dev.dma_mask=0x%lx\n", *(pdev->dev.dma_mask));
	if (usb_disabled())
		return -ENODEV;
	/*struct pci_dev *dev*/
	/*
	dev->current_state = PCI_D0;
	dev->dev.power.power_state = PMSG_ON;
	*/
	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		pr_info("Found SMI501_HC with no IRQ.  Check BIOS/PCI %s setup!\n",
			pdev->name);
   	        return -ENODEV;
        }

	/*Save the usb buffer physical addr and length*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL) {
		dev_err(&pdev->dev, "no memory resource defined for usb buffer\n");
		return -ENODEV;
	}
	sm501_usb_buffer_phys_base = res->start;

	hcd = usb_create_hcd (driver, &pdev->dev, "sm501-ohci");
	if (!hcd) {
		retval = -ENOMEM;
		goto err_realse;
	}

	/* OHCI Host controller register set physical base address */
	/*The resource index is determined by core interface when register the subdev*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		retval = -ENXIO;
		goto err_get_resource;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = (res->end - res->start)+1;

	/* OHCI Host controller register set virtual base address */
	if (!request_mem_region (hcd->rsrc_start, hcd->rsrc_len,
		driver->description)) {
		dev_dbg (&pdev->dev, "sm501 usb controller already in use\n");
		retval = -EBUSY;
		goto err_get_resource;
	}
	hcd->regs = ioremap_nocache (hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed");
		retval = -ENOMEM;
		goto err_ioremap;
	}

/*	set_irq_type(irq, IRQT_RISING);*/
	sm501_hc_start(pdev, hcd);
	ohci_hcd_init(hcd_to_ohci(hcd));

	retval = usb_add_hcd(hcd, irq, IRQF_DISABLED | IRQF_SHARED);

	if (retval == 0)
		return retval;

	sm501_hc_stop();
	iounmap(hcd->regs);

err_ioremap:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

err_get_resource:
	usb_put_hcd(hcd);

err_realse:
	return retval;


}

/*-------------------------------------------------------------------------*/

static int ohci_sm501_init(struct usb_hcd *hcd)
{
	return ohci_init(hcd_to_ohci(hcd));
}

static int
ohci_sm501_start(struct usb_hcd *hcd)
{

	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	if ((ret = ohci_run (ohci)) < 0) {
		err ("can't start %s", hcd->self.bus_name);
		ohci_stop (hcd);
		return ret;
	}
//    	device_init_wakeup(&hcd->self.root_hub->dev, 0);
	return 0;

}


static irqreturn_t ohci_hcd_sm501_irq(struct usb_hcd *hcd)
{
	irqreturn_t ret;

// Check  interrupt status
/*
	if (TEST_FIELD(SmRead32(INTERRUPT_STATUS), INTERRUPT_STATUS_USB_HOST, 
		NOT_ACTIVE) )
	{
		//SmWrite32(USB_HOST_HC_INTERRUPTSTATUS, 0xffffffff);
		return IRQ_NONE;
	}
*/
	/**(volatile unsigned int *)(usb_video_buf); */

#ifdef DEBUG
	//printk("SM501 irq....................................\n");
#endif
/*
	mask = SmRead32(INTERRUPT_MASK);
//  Disable USB interrupt
	SmWrite32(INTERRUPT_MASK, SET_FIELD(mask ,INTERRUPT_MASK_USB_HOST,DISABLE)|SET_FIELD(0 ,INTERRUPT_MASK_8051, DISABLE));
*/
	ret = ohci_irq(hcd);
/*
	mdelay(3);
//  Enable USB interrupt

	SmWrite32(INTERRUPT_MASK, SET_FIELD(mask ,INTERRUPT_MASK_USB_HOST,ENABLE)|SET_FIELD(0 ,INTERRUPT_MASK_8051, ENABLE));
*/
	return ret;

}

extern void *sm501_hcca_alloc(struct device *dev, size_t size, dma_addr_t *handle);
extern void sm501_hcca_free(struct device *dev);
extern dma_addr_t DMA_MAP_SINGLE(struct device *hwdev, void *ptr, size_t size, enum dma_data_direction direction);
extern void DMA_UNMAP_SINGLE(struct device * hwdev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction);
extern int DMA_MAP_SG(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction direction);
extern void DMA_UNMAP_SG(struct device *dev, struct scatterlist *sg, int nhwentries, enum dma_data_direction direction);
extern int sm501_mem_init(unsigned long mem_base, size_t size);
extern void sm501_mem_cleanup(void);
extern unsigned long sm501_p2v(dma_addr_t dma_addr);

void *sm501_dma_alloc(struct usb_hcd *hcd, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
#ifdef USE_SYSTEM_MEMORY
	usb_video_buf = __get_free_pages(GFP_KERNEL | GFP_DMA , 4);
#else
	if (!request_mem_region(sm501_usb_buffer_phys_base, USB_DMA_BUFFER_SIZE, hcd_name)) {
		printk(KERN_ERR "request_region pFramebufferPhysical failed!\n");
		return NULL;
	}
	usb_video_buf = (unsigned long)ioremap_nocache(sm501_usb_buffer_phys_base, USB_DMA_BUFFER_SIZE);
#endif
	if(usb_video_buf) {
		memset((u8 *)usb_video_buf, 0x00, USB_DMA_BUFFER_SIZE);
		sm501_mem_init(usb_video_buf, USB_DMA_BUFFER_SIZE);
		printk(KERN_INFO "SM501 USB OnChip Buffer alloc success \n");
	} else {
		printk(KERN_ERR "SM501 USB OnChip Buffer alloc fail \n");
		 return NULL;
	}
	return sm501_hcca_alloc(hcd->self.controller, size, dma_handle);
}
void sm501_dma_free(struct usb_hcd *hcd, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	sm501_hcca_free(hcd->self.controller);
	sm501_mem_cleanup();
#ifndef USE_SYSTEM_MEMORY
	iounmap((void *)usb_video_buf);
	release_region(sm501_usb_buffer_phys_base, USB_DMA_BUFFER_SIZE);
#endif
}

static struct hc_driver ohci_sm501_hc_driver = {
	.description =		(char *)hcd_name,
	.product_desc =		"sm501 OHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_hcd_sm501_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */

	.reset =		ohci_sm501_init,
	.start =		ohci_sm501_start,
	.stop =			ohci_stop,
	//mill add
	.shutdown =		ohci_shutdown,
	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
	//mill add
	//.hub_irq_enable =	ohci_rhsc_enable,
#ifdef CONFIG_USB_SUSPEND
	//.hub_suspend =		ohci_hub_suspend,
	//.hub_resume =		ohci_hub_resume,
#endif
	//mill add
	.start_port_reset =	ohci_start_port_reset,

	//.pool_create =		vgx_pool_create,
	//.pool_destroy =		vgx_pool_destroy,
	//.pool_alloc =		vgx_pool_alloc,
	//.pool_free =		vgx_pool_free,
	//.map_single =		DMA_MAP_SINGLE,
	//.unmap_single =		DMA_UNMAP_SINGLE,
	//.dma_alloc =		sm501_dma_alloc,
	//.dma_free =		sm501_dma_free,
	//.map_sg =		DMA_MAP_SG,
	//.unmap_sg =		DMA_UNMAP_SG,
	//.p2v =			sm501_p2v,
};

/* device driver */

static int sm501usb_probe(struct platform_device *pdev)
{
	return usb_hcd_sm501_probe( pdev, &ohci_sm501_hc_driver);
}

static int sm501usb_remove(struct platform_device *pdev)
{
//	struct usb_hcd	*hcd = platform_get_drvdata(pdev);
//	usb_hcd_sm501_remove(hcd, pdev);
	return 0;
}

//#ifdef CONFIG_PM
#if 0

/* suspend and resume support */

static int sm501usb_suspend(struct platform_device *pdev, pm_message_t state)
{

}

static int sm501usb_resume(struct platform_device *pdev)
{

}

#else
#define sm501usb_suspend NULL
#define sm501usb_resume  NULL
#endif



static struct platform_driver sm501usb_driver = {
	.probe		= sm501usb_probe,
	.remove		= sm501usb_remove,
	.suspend	        = sm501usb_suspend,
	.resume		= sm501usb_resume,
	.driver		= {
		.name	= "sm501-usb",
		.owner	= THIS_MODULE,
	},
};



