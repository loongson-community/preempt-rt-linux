/*
 * rtl8187b specific rfkill support
 *
 * TODO:
 *    1. make basic rfkill support work(in progress)
 *    2. change gpio polling method to sci interrupt method
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

DEFINE_SPINLOCK(rfkill_lock);
enum rfkill_state r8187b_rfkill_state = RFKILL_STATE_UNBLOCKED;

static int r8187b_wifi_set(void *data, enum rfkill_state state)
{
	/* we need to call the callback function of the gpio polling function here!! */
	return 0;
}

static int r8187b_wifi_get(void *data, enum rfkill_state *state)
{
	spin_lock(&rfkill_lock);
	*state = r8187b_rfkill_state;
	spin_unlock(&rfkill_lock);

	return 0;
}

int r8187b_wifi_report_state(r8180_priv *priv)
{
	spin_lock(&rfkill_lock);
	r8187b_rfkill_state = RFKILL_STATE_UNBLOCKED;
	if (priv->ieee80211->bHwRadioOff) {
		if (priv->eRFPowerState == eRfOff)
			r8187b_rfkill_state = RFKILL_STATE_HARD_BLOCKED;
		else
			r8187b_rfkill_state = RFKILL_STATE_SOFT_BLOCKED;
	}
	spin_unlock(&rfkill_lock);
	rfkill_force_state(r8187b_rfkill, r8187b_rfkill_state);

	return 0;
}

int __init r8187b_rfkill_init(struct net_device *dev)
{
	int ret;

	r8187b_rfkill = rfkill_allocate(&dev->dev, RFKILL_TYPE_WLAN);
	if (!r8187b_rfkill) {
		printk(KERN_WARNING "r8187b: Unable to allocate rfkill\n");
		return -ENOMEM;
	}

	r8187b_rfkill->name = "wifi";
	r8187b_rfkill->data = ieee80211_priv(dev);
	r8187b_rfkill->toggle_radio = r8187b_wifi_set;
	r8187b_rfkill->get_state = r8187b_wifi_get;
	r8187b_rfkill->state = RFKILL_STATE_UNBLOCKED;

	ret = rfkill_register(r8187b_rfkill);
	if (ret) {
		printk(KERN_WARNING "r8187b: Unable to register rfkill\n");
		rfkill_free(r8187b_rfkill);
	}

	return 0;
}

void __exit r8187b_rfkill_exit(void)
{
	if (r8187b_rfkill)
		rfkill_free(r8187b_rfkill);
	r8187b_rfkill = NULL;
}
