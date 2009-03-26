/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <linux/sm501.h>
#include <linux/sm501_regs.h>

#include "sm501-ac97.h"
#include "sm501-pcm.h"
#include "sm501-u8051.h"

#define DRV_NAME                        "sm501-audio"

#ifdef DEBUG
#define dprintk(msg...) do { printk(KERN_DEBUG DRV_NAME ": " msg); } while (0)
#else
#define dprintk(msg...)
#endif

struct sm501_runtime_data {
	spinlock_t lock;
	struct sm501_pcm_dma_params *params;
	struct snd_pcm_substream *substream;
	unsigned long ppointer; /* playback pointer in bytes */
	struct snd_pcm_substream *csubstream;
	unsigned long cpointer;
};

static struct snd_pcm_hardware snd_sm501_playback_hw = {
	.info = (SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_BLOCK_TRANSFER |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_HALF_DUPLEX |
			SNDRV_PCM_INFO_BATCH),
	.formats =          SNDRV_PCM_FMTBIT_S16_LE,
	.rates =            SNDRV_PCM_RATE_48000, /*SNDRV_PCM_RATE_8000_48000,*/
	.rate_min =         48000, /* 8000,*/
	.rate_max =         48000,
	.channels_min =     2,
	.channels_max =     2,
	.buffer_bytes_max = SM501_AC97_P_PERIOD_SIZE
			* SM501_AC97_P_PERIOD_COUNT,
	.period_bytes_min = SM501_AC97_P_PERIOD_SIZE,
	.period_bytes_max = SM501_AC97_P_PERIOD_SIZE,
	.periods_min =      4,
	.periods_max =      SM501_AC97_P_PERIOD_COUNT,
};

/*hardware definition */
static struct snd_pcm_hardware snd_sm501_capture_hw = {
	.info = (SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_BLOCK_TRANSFER |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_HALF_DUPLEX |
			SNDRV_PCM_INFO_BATCH),
	.formats =          SNDRV_PCM_FMTBIT_S16_LE,
	.rates =            SNDRV_PCM_RATE_48000,
	.rate_min =         48000,
	.rate_max =         48000,
	.channels_min =     2,
	.channels_max =     2,
	.buffer_bytes_max = SM501_AC97_C_PERIOD_SIZE
			* SM501_AC97_C_PERIOD_COUNT,
	.period_bytes_min = SM501_AC97_C_PERIOD_SIZE,
	.period_bytes_max = SM501_AC97_C_PERIOD_SIZE,
	.periods_min =      4,
	.periods_max =      SM501_AC97_C_PERIOD_COUNT,
};

/*
 * allocate a buffer via vmalloc_32().
 * called from hw_params
 * NOTE: this may be called not only once per pcm open!
*/
static int snd_sm501_pcm_alloc_vmalloc_buffer(struct snd_pcm_substream *subs,
					      size_t size)
{
	struct snd_pcm_runtime *runtime = subs->runtime;
	if (runtime->dma_area) {
		/* already allocated */
		if (runtime->dma_bytes >= size)
			return 0; /* already enough large */
		vfree(runtime->dma_area);
	}
	runtime->dma_area = vmalloc_32(size);
	if (! runtime->dma_area)
		return -ENOMEM;
	memset(runtime->dma_area, 0, size);
	runtime->dma_bytes = size;
	return 1; /* changed */
}

/*
 * free the buffer.
 * called from hw_free callback
 * NOTE: this may be called not only once per pcm open!
 */
static int snd_sm501_pcm_free_vmalloc_buffer(struct snd_pcm_substream *subs)
{
	struct snd_pcm_runtime *runtime = subs->runtime;
	if (runtime->dma_area) {
		vfree(runtime->dma_area);
		runtime->dma_area = NULL;
	}
	return 0;
}

/*
* Copy playback data into the hardware buffer
*/
static void snd_sm501_copy_pdata(struct sm501_runtime_data *rtdata)
{
	u16  *phalfframe;
	u32  *pdest;
	u32  *pborder;
	struct snd_pcm_runtime *runtime;
	unsigned char status;

	if (rtdata->substream==NULL) {
		return;
	}

	runtime = rtdata->substream->runtime;

	if (runtime->dma_area == NULL) {
		return;
	}

	status = SmRead8(SRAM(buffer_status)) & 0x3;
	if (status==0x2)
		pdest = (u32 *) SRAM(A.B.playback_buffer_1);
	else
		pdest = (u32 *) SRAM(A.B.playback_buffer_0);

	pborder = pdest + (SM501_AC97_P_PERIOD_SIZE / (sizeof(u32)));

	do {
		/* Sample format : */
		/* 32 ... 20 | 19 ... 4 | 3-0 */
		/* reserved  | 16b sampl| opt */
		/* Left */
		phalfframe = (u16 *) (runtime->dma_area + rtdata->ppointer);
		SmWrite32((unsigned long) pdest,
			*phalfframe  << 4);
		pdest++;
		phalfframe++;

		/* Right */
		SmWrite32((unsigned long) pdest,
			*phalfframe << 4);
		pdest++;
		rtdata->ppointer += 4;
	} while (pdest < pborder);

	rtdata->ppointer %= SM501_AC97_P_PERIOD_SIZE * runtime->periods;

	snd_pcm_period_elapsed(rtdata->substream);

}

/*
 * Fill the playback buffers with audio data
 */
static void snd_sm501_pfill(struct sm501_runtime_data *rtdata)
{
	u16 *phalfframe;
	u32 *pdest;
	u32 *pborder;
	struct snd_pcm_runtime *runtime;

	if (rtdata->substream==NULL) {
		return;
	}

	runtime = rtdata->substream->runtime;

	if (runtime->dma_area == NULL) {
		return;
	}

	phalfframe = (u16 *) (runtime->dma_area
			+ frames_to_bytes(runtime, rtdata->ppointer));

	pdest = (u32 *) SRAM(A.B.playback_buffer_0);

	pborder = pdest + (SM501_AC97_P_PERIOD_SIZE / (sizeof(u32)));

	do {
		phalfframe = (u16 *) (runtime->dma_area + rtdata->ppointer);
		SmWrite32((unsigned long) pdest,
			*phalfframe  << 4);
		pdest++;
		rtdata->ppointer += 2;

		phalfframe = (u16 *) (runtime->dma_area + rtdata->ppointer);
		SmWrite32((unsigned long) pdest,
			*phalfframe << 4);
		pdest++;
		rtdata->ppointer += 2;
	} while (pdest < pborder);
}

/*
 * Copy capture data into software buffer
 * This function can also be called as tasklet!
*/
static void snd_sm501_copy_cdata(struct sm501_runtime_data *rtdata)
{
	volatile u16 *phalfframe;
	u32  *psrc;
	u32  *pborder;
	struct snd_pcm_runtime *runtime;
	unsigned char status;

	if (rtdata->csubstream==NULL) {
		return;
	}

	runtime = rtdata->csubstream->runtime;

	if (runtime->dma_area == NULL) {
		return;
	}

	status = SmRead8(SRAM(buffer_status)) & 0x30;
	if (status==0x20)
		psrc = (u32 *) SRAM(A.C.capture_buffer_1);
	else
		psrc = (u32 *) SRAM(A.C.capture_buffer_0);

	pborder = psrc + (SM501_AC97_C_PERIOD_SIZE / (sizeof(u32)));

	do {
		/* Sample format : */
		/* 32 ... 20 | 19 ... 4 | 3-0 */
		/* reserved  | 16b sampl| opt */
		/* Left */
		phalfframe = (u16 *) (runtime->dma_area + rtdata->cpointer);
		*phalfframe = (SmRead32((unsigned long) psrc) >> 4) & 0xffff;

		psrc++;
		phalfframe++;

		/* Right */
		*phalfframe = (SmRead32((unsigned long) psrc) >> 4) & 0xffff;
		psrc++;
		rtdata->cpointer += 4;
	} while (psrc < pborder);

	rtdata->cpointer %= SM501_AC97_C_PERIOD_SIZE * runtime->periods;

	snd_pcm_period_elapsed(rtdata->csubstream);
}

/* hw_params callback */
static int snd_sm501_pcm_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params * hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sm501_runtime_data *rtdata = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sm501_pcm_dma_params *dma = rtd->dai->cpu_dai->dma_data;

	if (rtdata->params == NULL) {
		rtdata->params = dma;
	}

	return snd_sm501_pcm_alloc_vmalloc_buffer(substream,
			params_buffer_bytes(hw_params));
}


/* hw_free callback */
static int snd_sm501_pcm_hw_free(struct snd_pcm_substream *substream)
{
	dprintk("pcm_hw_free\n");

	return snd_sm501_pcm_free_vmalloc_buffer(substream);
}


/* prepare callback */
static int snd_sm501_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct sm501_runtime_data *rtdata = substream->runtime->private_data;

	dprintk("pcm_playback_prepare\n");
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		rtdata->ppointer = 0;
	else
		rtdata->cpointer = 0;

	return 0;
}

static irqreturn_t snd_sm501_u8051_irq(int irq, void* dev_id)
{
	struct sm501_runtime_data *rtdata = (struct sm501_runtime_data *)dev_id;
	struct sm501_audio *s = rtdata->params->s;
	unsigned int status, mask;
	unsigned long flags;


	/* Read interrupt status. */
	status = SmRead32(INTERRUPT_STATUS);
	if (TEST_FIELD(status, INTERRUPT_STATUS_8051, NOT_ACTIVE))
		return IRQ_NONE;

	/* Reset 8051 interrupt status. */
	mask = SmRead32(INTERRUPT_MASK);
	SmWrite32(INTERRUPT_MASK, SET_FIELD(mask, INTERRUPT_MASK_8051, DISABLE));

	spin_lock_irqsave(&s->lock, flags);
	SmRead32(U8051_8051_PROTOCOL_INTERRUPT);

	/* Check 8051 buffer status. */
	status = SmRead8(SRAM(buffer_status));

	if (status != 0) {
		if (status & 0x0F) {
			snd_sm501_copy_pdata(rtdata);
#if 0
			if (s->playing.error) {
				sm501_write_command(s, START_STOP_AUDIO_PLAYBACK,
					s->playing.running = 0, 0);
				s->playing.error = 0;
			}
			else if (s->playing.sw_buffer_size - s->playing.count >= s->playing.threshold)
				wake_up(&s->playing.wait);
#endif
		}
		if (status & 0xF0) {
			snd_sm501_copy_cdata(rtdata);
#if 0
			if (s->recording.error) {
				KDEBUG("recording overflow.\n");
				write_command(s, START_STOP_AUDIO_CAPTURE,
						s->recording.running = 0, 0);
				s->recording.error = 0;
			}
			else if (s->recording.count >= s->recording.threshold)
				wake_up(&s->recording.wait);
#endif
		}
	}

	spin_unlock_irqrestore(&s->lock, flags);
	mask = SmRead32(INTERRUPT_MASK);
	SmWrite32(INTERRUPT_MASK, SET_FIELD(mask, INTERRUPT_MASK_8051, ENABLE));

	return IRQ_HANDLED;
}

/* trigger callback */
static int snd_sm501_pcm_playback_trigger(struct snd_pcm_substream *substream,
					  int cmd)
{
	struct sm501_runtime_data *rtdata = substream->runtime->private_data;
	struct sm501_audio *s = rtdata->params->s;

	spin_lock(&s->lock);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
			dprintk("pcm_playback_trigger: start\n");
			rtdata->substream = substream;
			snd_sm501_pfill(rtdata);
			sm501_write_command(s, SET_PLAYBACK_BUFFER_READY, 3, 0);
			sm501_write_command(s, START_STOP_AUDIO_PLAYBACK, 1, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
			dprintk("pcm_playback_trigger: stop\n");
			sm501_write_command(s, START_STOP_AUDIO_PLAYBACK, 0, 0);
			rtdata->substream = NULL;
			break;
		default:
			dprintk("pcm_playback_trigger: default (%i)\n", cmd);
			return -EINVAL;
	}
	spin_unlock(&s->lock);

	return 0;
}
static int snd_sm501_pcm_capture_trigger(struct snd_pcm_substream *substream,
					 int cmd)
{
	struct sm501_runtime_data *rtdata = substream->runtime->private_data;
	struct sm501_audio *s = rtdata->params->s;

	spin_lock(&s->lock);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
			sm501_write_command(s, SET_CAPTURE_BUFFER_EMPTY, 3, 0);
			dprintk("pcm_capture_trigger: start\n");
			rtdata->csubstream = substream;
			sm501_write_command(s, START_STOP_AUDIO_CAPTURE, 1, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
			dprintk("pcm_capture_trigger: stop\n");
			sm501_write_command(s, START_STOP_AUDIO_CAPTURE, 0, 0);
			rtdata->csubstream = NULL;
			break;
		default:
			dprintk("pcm_capture_trigger: default (%i)\n", cmd);
			return -EINVAL;
	}

	spin_unlock(&s->lock);

	return 0;
}

static int snd_sm501_pcm_trigger(struct snd_pcm_substream *substream,
					 int cmd)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return snd_sm501_pcm_playback_trigger(substream, cmd);
	else
		return snd_sm501_pcm_capture_trigger(substream, cmd);

}
/* open callback */
static int snd_sm501_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sm501_runtime_data *rtdata;

	dprintk("Entered %s\n", __FUNCTION__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_set_runtime_hwparams(substream, &snd_sm501_playback_hw);
	else
		snd_soc_set_runtime_hwparams(substream, &snd_sm501_capture_hw);

	rtdata = kzalloc(sizeof(struct sm501_runtime_data), GFP_KERNEL);
	if (rtdata == NULL)
		return -ENOMEM;

	spin_lock_init(&rtdata->lock);

	runtime->private_data = rtdata;
	request_irq(36, snd_sm501_u8051_irq, IRQF_SHARED | IRQF_DISABLED, "sm501-pcm", rtdata);

	return 0;
}

static int snd_sm501_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sm501_runtime_data *rtdata = runtime->private_data;

	dprintk("Entered %s\n", __FUNCTION__);

	free_irq(36, rtdata);
	if (rtdata)
		kfree(rtdata);
	else
		dprintk("%s called with rtdata == NULL\n", __func__);

	return 0;
}

/* pointer callback */
static snd_pcm_uframes_t snd_sm501_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct sm501_runtime_data *rtdata = substream->runtime->private_data;
	snd_pcm_uframes_t value = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		value = bytes_to_frames(substream->runtime, rtdata->ppointer);
	else
		value = bytes_to_frames(substream->runtime, rtdata->cpointer);
	return value;
}

/* get the physical page pointer on the given offset */
static struct page *snd_sm501_pcm_get_vmalloc_page(struct snd_pcm_substream *subs,
		unsigned long offset)
{
	void *pageptr = subs->runtime->dma_area + offset;

	return vmalloc_to_page(pageptr);
}

/* operators */
static struct snd_pcm_ops snd_sm501_ops = {
	.open =		snd_sm501_open,
	.close =	snd_sm501_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_sm501_pcm_hw_params,
	.hw_free =	snd_sm501_pcm_hw_free,
	.prepare =	snd_sm501_pcm_prepare,
	.trigger =	snd_sm501_pcm_trigger,
	.pointer =	snd_sm501_pcm_pointer,
	.page =		snd_sm501_pcm_get_vmalloc_page,
};

int sm501_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;
	return ret;
}

static void sm501_pcm_free(struct snd_pcm *pcm)
{
}

struct snd_soc_platform sm501_soc_platform = {
	.name		= "sm501-snd",
	.pcm_ops 	= &snd_sm501_ops,
	.pcm_new	= sm501_pcm_new,
	.pcm_free	= sm501_pcm_free,
};

EXPORT_SYMBOL_GPL(sm501_soc_platform);


MODULE_AUTHOR("Arnaud Patard <apatard@mandriva.com>");
MODULE_DESCRIPTION("SM501 pcm module");
MODULE_LICENSE("GPL");
