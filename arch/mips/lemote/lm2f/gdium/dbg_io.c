/*
 *  Early console support for the Emtec Gdium Liberty 1000
 *  Copyright (c) 2009 Philippe P. Vachon <philippe@cowpig.ca>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  
 *  USA
 */

#include <linux/io.h>
#include <linux/serial_reg.h>

#define UART_BASE ((void __iomem *)(CKSEG1ADDR(0x1ff00000ul) + 0x3f8))

void prom_putchar(char c)
{
#ifdef CONFIG_GDIUM_UART
	uint32_t timeout;

	for (timeout = 1024;
		!(readb(UART_BASE + UART_LSR) & UART_LSR_THRE) && timeout >= 0;
		timeout--);

	writeb(c, UART_BASE + UART_TX);
#endif
}
