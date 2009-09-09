/*
 * rtl8187b specific rfkill support
 *
 * NOTE: we only concern about two states
 *   eRfOff: RFKILL_STATE_SOFT_BLOCKED
 *   eRfOn: RFKILL_STATE_UNBLOCKED
 * TODO: move led controlling source code to rfkill framework
 *
 *  Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 *  Author: Wu Zhangjin <wuzj@lemote.com>
 */

#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/device.h>

/* LED macros are defined in r8187.h and rfkill.h, we not use any of them here
 * just avoid compiling erros here.
 */
#undef LED

#include "r8187.h"
#include "ieee80211/ieee80211.h"
#include "linux/netdevice.h"

static struct rfkill *r8187b_rfkill;
static struct work_struct r8187b_rfkill_task;
static int initialized;
/* turn off by default */
enum rfkill_state r8187b_rfkill_state = RFKILL_STATE_SOFT_BLOCKED;
RT_RF_POWER_STATE eRfPowerStateToSet = eRfOff;
DEFINE_SPINLOCK(statetoset_lock);
DEFINE_SPINLOCK(state_lock);

void r8187b_wifi_rfkill_task(struct work_struct *work)
{
	struct net_device *dev = r8187b_rfkill->data;

	printk(KERN_INFO "%s : dev: %p : state: %d\n", __func__, dev, eRfPowerStateToSet);

	if (dev) {
		spin_lock(&statetoset_lock);
		r8187b_wifi_change_rfkill_state(dev, eRfPowerStateToSet);
		spin_unlock(&statetoset_lock);
	}
}

int r8187b_wifi_update_rfkill_state(int status)
{
	struct net_device *dev = r8187b_rfkill->data;

	printk("%s, dev: %p, status: %d\n", __func__, dev, status);

	/* ensure r8187b_rfkill is initialized if dev is not initialized, means
	 * wifi driver is not start, the status is eRfOff be default.
	 */
	if (!dev)
		return eRfOff;

	if (initialized == 0) {
		/* init the rfkill work task */
		INIT_WORK(&r8187b_rfkill_task, r8187b_wifi_rfkill_task);
		initialized = 1;
	}


	spin_lock(&statetoset_lock);
	if (status == 1)
		eRfPowerStateToSet = eRfOn;
	else if (status == 0)
		eRfPowerStateToSet = eRfOff;
	else if (status == 2) {
		/* if the KEY_WLAN is pressed, just switch it! */
		spin_lock(&state_lock);
		if (r8187b_rfkill_state == RFKILL_STATE_UNBLOCKED)
			eRfPowerStateToSet = eRfOff;
		else if (r8187b_rfkill_state == RFKILL_STATE_SOFT_BLOCKED)
			eRfPowerStateToSet = eRfOn;
		spin_unlock(&state_lock);
	}
	spin_unlock(&statetoset_lock);

	schedule_work(&r8187b_rfkill_task);

	return eRfPowerStateToSet;
}
EXPORT_SYMBOL(r8187b_wifi_update_rfkill_state);

static int r8187b_wifi_set(void *data, enum rfkill_state state)
{
	int status;

	if (state == RFKILL_STATE_UNBLOCKED)
		status = 1;
	else if (state == RFKILL_STATE_SOFT_BLOCKED)
		status = 0;
	else
		return -1;

	r8187b_wifi_update_rfkill_state(status);

	return 0;
}

static int r8187b_wifi_get(void *data, enum rfkill_state *state)
{
	spin_lock(&state_lock);
	*state = r8187b_rfkill_state;
	spin_unlock(&state_lock);

	return 0;
}

int r8187b_wifi_report_state(r8180_priv *priv)
{
	spin_lock(&state_lock);
	r8187b_rfkill_state = RFKILL_STATE_UNBLOCKED;
	if (priv->ieee80211->bHwRadioOff && priv->eRFPowerState == eRfOff)
		r8187b_rfkill_state = RFKILL_STATE_SOFT_BLOCKED;
	spin_unlock(&state_lock);

	printk(KERN_INFO "%s, state: %d\n", __func__, r8187b_rfkill_state);

	rfkill_force_state(r8187b_rfkill, r8187b_rfkill_state);

	return 0;
}

int r8187b_rfkill_init(struct net_device *dev)
{
	int ret;

	printk(KERN_INFO "%s: %p\n", __func__, dev);

	/* init the rfkill struct */
	r8187b_rfkill = rfkill_allocate(&dev->dev, RFKILL_TYPE_WLAN);
	if (!r8187b_rfkill) {
		printk(KERN_WARNING "r8187b: Unable to allocate rfkill\n");
		return -ENOMEM;
	}

	r8187b_rfkill->name = "wifi";
	r8187b_rfkill->data = dev;
	r8187b_rfkill->toggle_radio = r8187b_wifi_set;
	r8187b_rfkill->get_state = r8187b_wifi_get;
	r8187b_rfkill->state = r8187b_rfkill_state;

	ret = rfkill_register(r8187b_rfkill);
	if (ret) {
		printk(KERN_WARNING "r8187b: Unable to register rfkill\n");
		rfkill_free(r8187b_rfkill);
	}

	/* power off the wifi by default */
	r8187b_wifi_change_rfkill_state(dev, eRfOff);

	return 0;
}

void r8187b_rfkill_exit(void)
{
	if (r8187b_rfkill)
		rfkill_free(r8187b_rfkill);
	r8187b_rfkill = NULL;
}
