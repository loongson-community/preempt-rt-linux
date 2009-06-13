/*  early printk support
 *
 *  Copyright (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/types.h>
#include <linux/serial_reg.h>

#include <loongson.h>
#include <machine.h>

void prom_putchar(char c)
{
	int timeout;
	phys_addr_t uart_base =
	    (phys_addr_t) ioremap_nocache(LOONGSON_UART_BASE, 8);
	char reg = readb((u8 *) (uart_base + UART_LSR)) & UART_LSR_THRE;

	for (timeout = 1024; reg == 0 && timeout > 0; timeout--)
		reg = readb((u8 *) (uart_base + UART_LSR)) & UART_LSR_THRE;

	writeb(c, (u8 *) (uart_base + UART_TX));
}
