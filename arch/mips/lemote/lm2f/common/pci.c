/*
 * pci.c
 *
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
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mips-boards/bonito64.h>

static int pci_highmem = 1;

extern struct pci_ops loongson2f_pci_ops;

static struct resource loongson2f_pci_mem_resource = {
	.name   = "LOONGSON2F PCI MEM",
	.start  = 0x14000000UL,
	.end    = 0x1fffffffUL,
	.flags  = IORESOURCE_MEM,
};

static struct resource loongson2f_pci_io_resource = {
	.name   = "LOONGSON2F PCI IO MEM",
	.start  = 0x00004000UL,
	.end    = IO_SPACE_LIMIT,
	.flags  = IORESOURCE_IO,
};

static struct pci_controller  loongson2f_pci_controller = {
	.pci_ops        = &loongson2f_pci_ops,
	.io_resource    = &loongson2f_pci_io_resource,
	.mem_resource   = &loongson2f_pci_mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

static void __init ict_pcimap(void)
{
	/*
	 * local to PCI mapping: [256M,512M] -> [256M,512M]; differ from PMON
	 *
	 * CPU address space [256M,448M] is window for accessing pci space
	 * we set pcimap_lo[0,1,2] to map it to pci space [256M,448M]
	 * pcimap: bit18,pcimap_2; bit[17-12],lo2;bit[11-6],lo1;bit[5-0],lo0
	 */
	/* 1,00 0110 ,0001 01,00 0000 */
	BONITO_PCIMAP = 0x46140;

	/* 1, 00 0010, 0000,01, 00 0000 */
	/* BONITO_PCIMAP = 0x42040; */

	/*
	 * PCI to local mapping: [2G,2G+256M] -> [0,256M]
	 */
	BONITO_PCIBASE0 = 0x80000000;
	BONITO_PCIBASE1 = 0x00000000;
	BONITO(BONITO_REGBASE + 0x50) = 0x8000000c;
	BONITO(BONITO_REGBASE + 0x54) = 0xffffffff;

	/*set pci 2G -> DDR 0 ,window size 2G*/
	asm(".set mips3;\n\t"
	    "dli $2,0x900000003ff00000;\n\t"
	    "li $3,0x80000000;\n\t"
	    "sd $3,0x60($2);\n\t"
	    "sd $0,0xa0($2);\n\t"
	    "dli $3,0xffffffff80000000;\n\t"
	    "sd $3,0x80($2);\n\t"
	    ".set mips0\n\t"
	    :::"$2","$3");
	/*
	 * PCI to local mapping: [8M,16M] -> [8M,16M]
	 */
	BONITO_PCI_REG(0x18) = 0x00800000;
	BONITO_PCI_REG(0x1c) = 0x0;
	BONITO(BONITO_REGBASE + 0x58) = 0xff80000c;
	BONITO(BONITO_REGBASE + 0x5c) = 0xffffffff;

	/*set pci 8-16M -> DDR 8-16M ,window size 8M*/
	asm(".set mips3;\n\t"
	    "dli $2,0x900000003ff00000;\n\t"
	    "li $3,0x800000;\n\t"
	    "sd $3,0x68($2);\n\t"
	    "sd $3,0xa8($2);\n\t"
	    "dli $3,0xffffffffff800000;\n\t"
	    "sd $3,0x88($2);\n\t"
	    ".set mips0\n\t"
	    :::"$2","$3");
	BONITO(BONITO_REGBASE + 0x68) = 0x00fe0105;

	if (pci_highmem) {
		/* not start from 0x4000000 */
		loongson2f_pci_mem_resource.start = 0x50000000UL;
		loongson2f_pci_mem_resource.end   = 0x7fffffffUL;

		/*
		 * set cpu window3 to map CPU 1G-> PCI 1G
		 * MASTER 0 LOCAL [0x4000000-0x7fffffff] --> SLAVE 1 PCI [0x40000000-0x7fffffff]
		 *
		 * M0_WIN3_BASE = 0x0000000040000000
		 * M0_WIN3_MMAP = 0x0000000040000001
		 * M0_WIN3_SIZE = 0xffffffffc0000000
		 *
		 */
		__asm__(".set mips3\n\t"
		    "dli $2,0x900000003ff00000\n\t"
		    "li $3,0x40000000\n\t"
		    "sd $3,0x18($2)\n\t"
		    "or $3,1\n\t"
		    "sd $3,0x58($2)\n\t"
		    "dli $3,0xffffffffc0000000\n\t"
		    "sd $3,0x38($2)\n\t"
		    ".set mips0\n\t"
		    :::"$2", "$3");
	}

	/* if needed then enable io cache for mem 0*/
	if (BONITO_PCIMEMBASECFG & BONITO_PCIMEMBASECFG_MEMBASE0_CACHED)
		BONITO_PCIMEMBASECFG = BONITO_PCIMEMBASECFG_MEMBASE0_CACHED;
	else
		BONITO_PCIMEMBASECFG = 0x0;
}

static int __init pcibios_init(void)
{
	extern int pci_probe_only;

	pci_probe_only = 0;

	ict_pcimap();
	
	loongson2f_pci_controller.io_map_base = 
		(phys_t)ioremap_nocache(0x14000000ull, 0xC00000) ;
		/* (phys_t)ioremap_nocache(BONITO_PCIIO_BASE, BONITO_PCIIO_SIZE); */

	register_pci_controller(&loongson2f_pci_controller);

	return 0;
}

static int __init pci_highmem_setup(char *options)
{
	if (options[0] == '0')
		pci_highmem = 0;
	else
		pci_highmem = 1;

	return 1;
}

arch_initcall(pcibios_init);
__setup("pci_highmem=", pci_highmem_setup);
