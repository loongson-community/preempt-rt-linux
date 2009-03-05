/*
 * Copyright (C) 2004 ICT CAS
 * Author: Li xiaoyu, ICT CAS
 *   lixy@ict.ac.cn
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/mips-boards/bonito64.h>

#undef P6032_PCIDEV_SLOT1
#undef P6032_PCIDEV_SLOT2
#undef P6032_PCIDEV_SLOT3
#undef P6032_PCIDEV_SLOT4
#undef P6032_PCIDEV_ETH
#undef P6032_PCIDEV_I82371
#undef P6032_PCIDEV_BONITO
#undef P6032INT_IRQA
#undef P6032INT_IRQB
#undef P6032INT_IRQC
#undef P6032INT_IRQD

#define P6032_PCIDEV_SLOT1 13
#define P6032_PCIDEV_SLOT2 14
#define P6032_PCIDEV_SLOT3 15
#define P6032_PCIDEV_SLOT4 18
#define P6032_PCIDEV_ETH 16
#define P6032_PCIDEV_I82371 17
#define P6032_PCIDEV_BONITO 19


#define P6032INT_IRQA 36
#define P6032INT_IRQB 37
#define P6032INT_IRQC 38
#define P6032INT_IRQD 39

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	int irq = 0;

	switch(slot) {
		case 13:
			irq = P6032INT_IRQC + ((pin -1) & 3);
			break;
		case 14:
			irq = P6032INT_IRQA;
			break;
		case 15:
#if CONFIG_GDIUM_VERSION > 2
			irq = P6032INT_IRQB;
#else
			irq = P6032INT_IRQA + ((pin -1) & 3);
#endif
			break;
		case 16:
			irq = P6032INT_IRQD;
			break;
#if CONFIG_GDIUM_VERSION > 2
		case 17:
			irq = P6032INT_IRQC;
			break;
#endif
		default:
			break;
	}
	return irq;
}


/* Do platform specific device initialization at pci_enable_device() time */
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

#if 0
static void __init godson2e_686b_func0_fixup(struct pci_dev *pdev)
{
	unsigned char c;

	printk(KERN_INFO "via686b fix: ISA bridge\n");

	/*  Enable I/O Recovery time */
	pci_write_config_byte(pdev, 0x40, 0x08);

	/*  Enable ISA refresh */
	pci_write_config_byte(pdev, 0x41, 0x01);

	/*  disable ISA line buffer */
	pci_write_config_byte(pdev, 0x45, 0x00);

	/*  Gate INTR, and flush line buffer */
	pci_write_config_byte(pdev, 0x46, 0xe0);

	/*  Disable PCI Delay Transaction, Enable EISA ports 4D0/4D1.
	 *  enable time-out timer
	 */
	pci_write_config_byte(pdev, 0x47, 0xe6);

	/* enable level trigger on pci irqs: 9,10,11,13 */
	/* important! without this PCI interrupts won't work */
	outb(0x2e, 0x4d1);

	/*  512 K PCI Decode */
	pci_write_config_byte(pdev, 0x48, 0x01);

	/*  Wait for PGNT before grant to ISA Master/DMA */
	pci_write_config_byte(pdev, 0x4a, 0x84);

	/*  Plug'n'Play */
	/*  Parallel DRQ 3, Floppy DRQ 2 (default) */
	pci_write_config_byte(pdev, 0x50, 0x0e);

	/*  IRQ Routing for Floppy and Parallel port */
	/*  IRQ 6 for floppy, IRQ 7 for parallel port */
	pci_write_config_byte(pdev, 0x51, 0x76);

	/*  IRQ Routing for serial ports (take IRQ 3 and 4) */
	pci_write_config_byte(pdev, 0x52, 0x34);

	/*  All IRQ's level triggered. */
	pci_write_config_byte(pdev, 0x54, 0x00);


	/* route PIRQA-D irq */
	pci_write_config_byte(pdev, 0x55, 0x90); /* bit 7-4, PIRQA */
	pci_write_config_byte(pdev, 0x56, 0xba); /* bit 7-4, PIRQC; 3-0, PIRQB */
	pci_write_config_byte(pdev, 0x57, 0xd0); /* bit 7-4, PIRQD */

	/* enable function 5/6, audio/modem */
	pci_read_config_byte(pdev, 0x85, &c);
	c &= ~(0x3<<2);
	pci_write_config_byte(pdev, 0x85, c);

	printk(KERN_INFO "via686b fix: ISA bridge done\n");
}


static void __init godson2e_686b_func1_fixup(struct pci_dev *pdev)
{
	printk(KERN_INFO "via686b fix: IDE\n");

	/* Modify IDE controller setup */
	pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 48); /* 0xd0 */
	pci_write_config_byte(pdev, PCI_COMMAND, PCI_COMMAND_IO|PCI_COMMAND_MEMORY|PCI_COMMAND_MASTER);
	pci_write_config_byte(pdev, 0x40, 0x0b);
	/* legacy mode */
	pci_write_config_byte(pdev, 0x42, 0x09);
#if 0
	/* disable read prefetch/write post buffers */
	pci_write_config_byte(pdev, 0x41, 0x02); /* 0xf2);   */

	/* use 3/4 as fifo thresh hold  */
	pci_write_config_byte(pdev, 0x43, 0x0a);

	pci_write_config_byte(pdev, 0x44, 0x00);

	pci_write_config_byte(pdev, 0x45, 0x00);
#else
	pci_write_config_byte(pdev, 0x41, 0xc2);
	pci_write_config_byte(pdev, 0x43, 0x35);
	pci_write_config_byte(pdev, 0x44, 0x1c);

	pci_write_config_byte(pdev, 0x45, 0x10);
#endif

	printk(KERN_INFO "via686b fix: IDE done\n");
}

static void __init godson2e_686b_func5_fixup(struct pci_dev *pdev)
{
	unsigned int val;
	unsigned char c;

	/* enable IO */
	pci_write_config_byte(pdev, PCI_COMMAND, PCI_COMMAND_IO|PCI_COMMAND_MEMORY|PCI_COMMAND_MASTER);
	pci_read_config_dword(pdev, 0x4, &val);
	pci_write_config_dword(pdev, 0x4, val | 1);

	/* route ac97 IRQ */
	pdev->irq = 9;
	pci_write_config_byte(pdev, 0x3c, pdev->irq);

	/* link control: enable link & SGD PCM output */
	pci_write_config_byte(pdev, 0x41, 0xcc);

	/* disable game port, FM, midi, sb, enable write to reg2c-2f */
	pci_write_config_byte(pdev, 0x42, 0x20);

	/* we are using Avance logic codec */
	pci_write_config_word(pdev, 0x2c, 0x1005);
	pci_write_config_word(pdev, 0x2e, 0x4710);
	pci_read_config_dword(pdev, 0x2c, &val);

	pci_read_config_byte(pdev, 0x8, &c);
	printk(KERN_INFO "ac97 interrupt = %d rev=%d sub vendor-device id=%x\n",
		pdev->irq, c, val);

	pci_write_config_byte(pdev, 0x42, 0x0);
}


DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686, godson2e_686b_func0_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_1, godson2e_686b_func1_fixup);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686_5, godson2e_686b_func5_fixup);
#endif
static void __init godson2f_usb_host_fixup(struct pci_dev *dev)
{
	u32 val;
	pci_read_config_dword(dev, 0xe0, &val);
#if CONFIG_GDIUM_VERSION > 2
	pci_write_config_dword(dev, 0xe0, (val & ~3) | 0x3);
#else
	pci_write_config_dword(dev, 0xe0, (val & ~7) | 0x5);
	pci_write_config_dword(dev, 0xe4, 1<<5);
#endif
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NEC,PCI_DEVICE_ID_NEC_USB, godson2f_usb_host_fixup);
#if CONFIG_GDIUM_VERSION > 2
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NEC,0x00e0, godson2f_usb_host_fixup);
#endif
