/*
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
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

#include <linux/pci.h>

#include <loongson.h>
#include <pci.h>

static struct resource loongson_pci_mem_resource = {
	.name   = "pci memory space",
	.start  = LOONGSON_PCI_MEM_START,
	.end    = LOONGSON_PCI_MEM_END,
	.flags  = IORESOURCE_MEM,
};

static struct resource loongson_pci_io_resource = {
	.name   = "pci io space",
	.start  = LOONGSON_PCI_IO_START,	/* reserve regacy I/O space */
	.end    = IO_SPACE_LIMIT,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller  loongson_pci_controller = {
	.pci_ops        = &loongson_pci_ops,
	.io_resource    = &loongson_pci_io_resource,
	.mem_resource   = &loongson_pci_mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

static void __init ict_pcimap(void)
{
	/*
	 * local to PCI mapping for CPU accessing PCI space
	 *
	 * CPU address space [256M,448M] is window for accessing pci space
	 * we set pcimap_lo[0,1,2] to map it to pci space[0M,64M], [320M,448M]
	 *
	 * pcimap: PCI_MAP2  PCI_Mem_Lo2 PCI_Mem_Lo1 PCI_Mem_Lo0
	 *           [<2G]   [384M,448M] [320M,384M] [0M,64M]
	 */
	LOONGSON_PCIMAP = LOONGSON_PCIMAP_PCIMAP_2 |
	    LOONGSON_PCIMAP_WIN(2, 0x18000000) |
	    LOONGSON_PCIMAP_WIN(1, 0x14000000) |
	    LOONGSON_PCIMAP_WIN(0, 0);

	/*
	 * PCI-DMA to local mapping: [2G,2G+256M] -> [0M,256M]
	 */
	LOONGSON_PCIBASE0 = 0x80000000ul;	/* base: 2G -> mmap: 0M */
	/* size: 256M, burst transmission, pre-fetch enable, 64bit */
	LOONGSON_PCI_HIT0_SEL_L = 0xc000000cul;
	LOONGSON_PCI_HIT0_SEL_H = 0xfffffffful;
	LOONGSON_PCI_HIT1_SEL_L = 0x00000006ul;	/* set this BAR as invalid */
	LOONGSON_PCI_HIT1_SEL_H = 0x00000000ul;
	LOONGSON_PCI_HIT2_SEL_L = 0x00000006ul;	/* set this BAR as invalid */
	LOONGSON_PCI_HIT2_SEL_H = 0x00000000ul;

	/* avoid deadlock of PCI reading/writing lock operation */
	LOONGSON_PCI_ISR4C = 0xd2000001ul;

	/* can not change gnt to break pci transfer when device's gnt not
	deassert for some broken device */
	LOONGSON_PXARB_CFG = 0x00fe0105ul;

#if defined(CONFIG_CPU_LOONGSON2F) && defined(CONFIG_64BIT)
	/*
	 * set cpu addr window2 to map CPU address space to PCI address space
	 */
	LOONGSON_ADDRWIN_CPUTOPCI(ADDRWIN_WIN2, LOONGSON_CPU_MEM_SRC,
			LOONGSON_PCI_MEM_DST, MMAP_CPUTOPCI_SIZE);
#endif
}

static int __init pcibios_init(void)
{
	ict_pcimap();

	loongson_pci_controller.io_map_base = mips_io_port_base;

	register_pci_controller(&loongson_pci_controller);

	return 0;
}

arch_initcall(pcibios_init);
