/* Basic clock management functionality for the Emtec Gdium
 *
 * (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 * Parts inspired by work done by Lemote on the Yeelong code.
 
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

#include <asm/clock.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/string.h>
#include <linux/spinlock.h>

static LIST_HEAD(clock_list);
//DEFINE_SPINLOCK(clock_list_lock);

void arch_init_clk_ops(struct clk_ops **ops, int type)
{

}

struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *c;
	unsigned long flags;
	//spin_lock_irqsave(&clock_list_lock, flags);
	list_for_each_entry(c, &clock_list, node) {
		if (!strcmp(id, c->name)) {
			return c;
		}
	}
	//spin_unlock_irqrestore(&clock_list_lock, flags);

	return NULL;
}

int clk_init(void)
{
	return 0;
}

int __clk_enable(struct clk *clk)
{
	return 0;
}

void __clk_disable(struct clk *clk)
{

}

void clk_recalc_rate(struct clk *clk)
{

}

int clk_register(struct clk *clk)
{
	unsigned long flags;
	printk("Registering clock %s\n", clk->name);

	if (clk_get(NULL, clk->name)) {
		return -EINVAL;
	}

	//spin_lock_irqsave(&clock_list_lock, flags);
	list_add(&clk->node, &clock_list);
	//spin_unlock_irqrestore(&clock_list_lock, flags);
	return 0;
}

void clk_unregister(struct clk *clk)
{
	if (!clk_get(NULL, clk->name)) {
		return;
	}

}


