/*
 * sm501-ac97-dev.c  --  SoC audio for gdium
 *
 * Based on smdk2443_wm9710.c
 *
 * Arnaud Patard <apatard@mandriva.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <asm/gpio.h>

#include "../codecs/ac97.h"
#include "sm501-pcm.h"
#include "sm501-ac97.h"

#define SPEAKER_PWM		2
#define SPEAKER_PWM_PERIOD	(1000000000/20000) /* 20 kHz */

#define MUTE_GPIO		192+0 /* pin 32 is mute */
#define AMP_MUTE		1
#define AMP_UNMUTE		0

static struct pwm_device *ap4838_pwm;
static int ap4838_pwm_duty;
static char ap4838_pwm_mute;

static int ap4838_get_vol(struct snd_kcontrol *kcontrol,
		        struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ap4838_pwm_duty;
	return 0;
}

static int ap4838_set_vol(struct snd_kcontrol *kcontrol,
		        struct snd_ctl_elem_value *ucontrol)
{
	/* Set duty */
	ap4838_pwm_duty = ucontrol->value.integer.value[0];
	pwm_config(ap4838_pwm, (100-ap4838_pwm_duty) * SPEAKER_PWM_PERIOD / 100, SPEAKER_PWM_PERIOD);

	/* Set Mute */
	if (ap4838_pwm_duty == 0)
		pwm_disable(ap4838_pwm);
	else
		pwm_enable(ap4838_pwm);

	return 1;
}

static int ap4838_set_mute(struct snd_kcontrol *kcontrol,
		        struct snd_ctl_elem_value *ucontrol)
{
        if (ucontrol->value.integer.value[0] == 0) {
		ap4838_pwm_mute = 0;
		gpio_set_value(MUTE_GPIO, AMP_MUTE); /* gpio_direction_input(MUTE_GPIO); */
	}
	else {
		gpio_set_value(MUTE_GPIO, AMP_UNMUTE); /* gpio_direction_output(MUTE_GPIO, 0); */
		ap4838_pwm_mute = 1;
	}

	return 1;
}
static int ap4838_get_mute(struct snd_kcontrol *kcontrol,
		        struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ap4838_pwm_mute;
	return 0;
}

static const struct snd_kcontrol_new ap4838_controls[] = {
	SOC_SINGLE_EXT("Amp Playback Volume", 0, 0, 100, 0,
		ap4838_get_vol, ap4838_set_vol),
	SOC_SINGLE_EXT("Amp Playback Switch", 0, 0, 1, 0,
		ap4838_get_mute, ap4838_set_mute),
};

static int ap4838_init(struct snd_soc_codec *codec)
{
	int i, err;

	for (i = 0; i < ARRAY_SIZE(ap4838_controls); i++) {
		err = snd_ctl_add(codec->card,
				snd_soc_cnew(&ap4838_controls[i],
				codec, NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

static int gdium_probe(struct platform_device *pdev)
{
	int ret;

	/* Configure GPIO32 as GPIO */
	ret = gpio_request(MUTE_GPIO, "audio-mute");
	if (ret) {
		dev_err(&pdev->dev, "unable to requestio audio mute gpio\n");
		return ret;
	}
	gpio_set_value(MUTE_GPIO, AMP_MUTE);
	gpio_direction_output(MUTE_GPIO, 1);

	ap4838_pwm = pwm_request(SPEAKER_PWM, "speaker");
	if (ap4838_pwm == NULL) {
		dev_err(&pdev->dev, "unable to request PWM for Gdium speaker\n");
		return -EBUSY;
	}

	pwm_config(ap4838_pwm, 0, SPEAKER_PWM_PERIOD);
	pwm_enable(ap4838_pwm);

	return 0;
}

static int gdium_remove(struct platform_device *pdev)
{
	gpio_free(MUTE_GPIO);

	pwm_config(ap4838_pwm, 0, SPEAKER_PWM_PERIOD);
	pwm_disable(ap4838_pwm);
	pwm_free(ap4838_pwm);

	return 0;
}

static struct snd_soc_dai_link gdium_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &sm501_ac97_dai[0],
	.codec_dai = &ac97_dai,
	.init = ap4838_init,
},
};

static struct snd_soc_card gdium_machine = {
	.name = "gdium",
	.dai_link = gdium_dai,
	.num_links = ARRAY_SIZE(gdium_dai),
	.probe = gdium_probe,
	.remove = gdium_remove,
	.platform = &sm501_soc_platform,
};

static struct snd_soc_device gdium_snd_devdata = {
	.card = &gdium_machine,
	.codec_dev = &soc_codec_dev_ac97,
};

static struct platform_device *gdium_snd_ac97_device;

static int snd_gdium_probe(struct platform_device *pdev)
{
	int ret;

	gdium_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!gdium_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(gdium_snd_ac97_device,
				&gdium_snd_devdata);
	gdium_snd_devdata.dev = &gdium_snd_ac97_device->dev;
	ret = platform_device_add(gdium_snd_ac97_device);

	if (ret)
		platform_device_put(gdium_snd_ac97_device);

	return ret;
}

static int snd_gdium_remove(struct platform_device *pdev)
{
	platform_device_unregister(gdium_snd_ac97_device);
	return 0;
}

static struct platform_driver gdium_audio_driver = {
	.driver	= {
		.name	= "gdium-audio",
		.owner	= THIS_MODULE,
	},
	.probe		= snd_gdium_probe,
	.remove		= snd_gdium_remove,
};

static int __init snd_gdium_init(void)
{
	return platform_driver_register(&gdium_audio_driver);
}

static void __exit snd_gdium_exit(void)
{
	platform_driver_unregister(&gdium_audio_driver);
}
module_init(snd_gdium_init);
module_exit(snd_gdium_exit);

/* Module information */
MODULE_AUTHOR("Arnaud Patard <apatard@mandriva.com>");
MODULE_DESCRIPTION("ALSA SoC Gdium");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gdium-audio");
