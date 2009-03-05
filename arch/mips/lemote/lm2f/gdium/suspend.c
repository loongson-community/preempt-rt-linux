/*
 * Suspend to RAM for Loongson 2F
 * Specifically designed for the Emtec Gdium Netbook
 *
 * (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * See COPYING in the root directory of this source distribution for more
 * information.
 */

#include <linux/init.h>
#include <linux/suspend.h>

static int ls2f_valid(suspend_state_t state);
static int ls2f_prepare(void);
static int ls2f_enter(suspend_state_t state);
static void ls2f_finish(suspend_state_t state);

static struct platform_suspend_ops ls2f_suspend_ops = {
	.valid = ls2f_valid,
	.prepare = ls2f_prepare,
	.enter = ls2f_enter,
	.finish = ls2f_finish
};

static int ls2f_valid(suspend_state_t state)
{

}

static int ls2f_prepare(void)
{

}

static int ls2f_enter(suspend_state_t state)
{

}

static void ls2f_finish(suspend_state_t state)
{

}
