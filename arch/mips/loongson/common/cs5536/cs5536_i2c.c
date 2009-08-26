/*
 * CS5536 I2C/SMBUS support
 *
 *  Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 *  Author: Wu Zhangjin <wuzj@lemote.com>
 *
 *  This source code is originally from PMON, contributed by Liu Junliang and
 *  Tao Hongliang.
 *
 *  TODO: implement a standard i2c driver based on the I2C framework
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/delay.h>

#include <loongson.h>
#include <cs5536/cs5536.h>

static unsigned int smbus_base;

static int i2c_wait(void)
{
	char c;
	int i;

	udelay(1000);
	for (i = 0; i < 20; i++) {
		c = inb(smbus_base | SMB_STS);
		if (c & (SMB_STS_BER | SMB_STS_NEGACK)) {
			return -1;
		}
		if (c & SMB_STS_SDAST)
			return 0;
		udelay(100);
	}
	return -2;
}

void cs5536_i2c_inb(int addr, int regNo, char *value)
{
	unsigned char c;

	/* Start condition */
	c = inb(smbus_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_START, smbus_base | SMB_CTRL1);
	i2c_wait();

	/* Send slave address */
	outb(addr & 0xfe, smbus_base | SMB_SDA);
	i2c_wait();

	/* Acknowledge smbus */
	c = inb(smbus_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_ACK, smbus_base | SMB_CTRL1);

	/* Send register index */
	outb(regNo, smbus_base | SMB_SDA);
	i2c_wait();

	/* Acknowledge smbus */
	c = inb(smbus_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_ACK, smbus_base | SMB_CTRL1);

	/* Start condition again */
	c = inb(smbus_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_START, smbus_base | SMB_CTRL1);
	i2c_wait();

	/* Send salve address again */
	outb(1 | addr, smbus_base | SMB_SDA);
	i2c_wait();

	/* Acknowledge smbus */
	c = inb(smbus_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_ACK, smbus_base | SMB_CTRL1);

	/* Read data */
	*value = inb(smbus_base | SMB_SDA);

	/* Stop condition */
	outb(SMB_CTRL1_STOP, smbus_base | SMB_CTRL1);
	i2c_wait();
}
EXPORT_SYMBOL(cs5536_i2c_inb);

void cs5536_i2c_outb(int addr, int regNo, char value)
{
	unsigned char c;

	/* Start condition */
	c = inb(smbus_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_START, smbus_base | SMB_CTRL1);
	i2c_wait();
	/* Send slave address */
	outb(addr & 0xfe, smbus_base | SMB_SDA);
	i2c_wait();;

	/* Send register index */
	outb(regNo, smbus_base | SMB_SDA);
	i2c_wait();

	/* Write data */
	outb(value, smbus_base | SMB_SDA);
	i2c_wait();
	/* Stop condition */
	outb(SMB_CTRL1_STOP, smbus_base | SMB_CTRL1);
	i2c_wait();
}
EXPORT_SYMBOL(cs5536_i2c_outb);

int __init cs5536_i2c_init(void)
{
	unsigned int hi;

	/* initialize the smbus base address */
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_SMB), &hi, &smbus_base);

	printk(KERN_INFO "initialize the cs5536 i2c function\n");

	return 0;
}

arch_initcall(cs5536_i2c_init);
