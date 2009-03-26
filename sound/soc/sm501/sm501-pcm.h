/*
 *  Based on s3c24xx-pcm.h
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _SM501_PCM_H
#define _SM501_PCM_H

#define SM501_AC97_P_PERIOD_SIZE  768
#define SM501_AC97_P_PERIOD_COUNT 256
#define SM501_AC97_C_PERIOD_SIZE  768
#define SM501_AC97_C_PERIOD_COUNT 256

struct sm501_pcm_dma_params {
	struct sm501_audio *s;
};

/* platform data */
extern struct snd_soc_platform sm501_soc_platform;

#endif
