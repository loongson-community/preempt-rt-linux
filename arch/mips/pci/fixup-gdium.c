/*
 * Copyright (C) 2004 ICT CAS
 * Author: Li xiaoyu, ICT CAS
 *   lixy@ict.ac.cn
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <loongson.h>

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

	switch (slot) {
	case 13:
		irq = P6032INT_IRQC + ((pin - 1) & 3);
		break;
	case 14:
		irq = P6032INT_IRQA;
		break;
	case 15:
#if CONFIG_GDIUM_VERSION > 2
		irq = P6032INT_IRQB;
#else
		irq = P6032INT_IRQA + ((pin - 1) & 3);
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

static void __init loongson2f_usb_host_fixup(struct pci_dev *dev)
{
	u32 val;
	pci_read_config_dword(dev, 0xe0, &val);
#if CONFIG_GDIUM_VERSION > 2
	pci_write_config_dword(dev, 0xe0, (val & ~3) | 0x3);
#else
	pci_write_config_dword(dev, 0xe0, (val & ~7) | 0x5);
	pci_write_config_dword(dev, 0xe4, 1 << 5);
#endif
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_USB,
			 loongson2f_usb_host_fixup);
#if CONFIG_GDIUM_VERSION > 2
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NEC, 0x00e0, loongson2f_usb_host_fixup);
#endif
