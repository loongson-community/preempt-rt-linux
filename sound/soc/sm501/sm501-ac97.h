/*
 * AC97 support for SM50X chipsets
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _SM501_AC97_H
#define _SM501_AC97_H

#define SM501_AC97_CODEC_TIMEOUT	1000

struct sm501_audio {
	struct device	*dev;

        struct resource	*res;
//	void __iomem	*regs;
	int		irq;

	int		firmware_loaded;
	spinlock_t	lock;
};

extern struct snd_soc_dai sm501_ac97_dai[1];
extern struct snd_ac97_bus_ops sm501_ac97_ops;

#endif
