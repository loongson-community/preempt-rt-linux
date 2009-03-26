#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/ac97_codec.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <linux/sm501.h>
#include <linux/sm501_regs.h>

#include "sm501-ac97.h"
#include "sm501-u8051.h"
#include "sm501_fw.h"

#define DRV_NAME                        "sm501-audio"
#if 0
#define dprintk(msg...) do { printk(KERN_DEBUG DRV_NAME ": " msg); } while (0)
#else
#define dprintk(msg...)
#endif

/* Functions for talking to the codec when firmware loaded */
int sm501_write_command(struct sm501_audio *s, unsigned char command, unsigned int data0,
			 unsigned int data1)
{
	int i;

	/* Reset status. */
	SmWrite8(SRAM(status_byte), 0);

	/* Fill command and data bytes. */
	dprintk("%s: %u %08x %08x\n", __func__, command, data0, data1);

	SmWrite8(SRAM(command_byte), command);
#if 1
	SmWrite32(SRAM(data_byte[0]), data0);
	SmWrite32(SRAM(data_byte[4]), data1);
#else
	SmWrite8(SRAM(data_byte[0]), LOBYTE(LOWORD(data0)));
	SmWrite8(SRAM(data_byte[1]), HIBYTE(LOWORD(data0)));
	SmWrite8(SRAM(data_byte[2]), LOBYTE(HIWORD(data0)));
	SmWrite8(SRAM(data_byte[3]), HIBYTE(HIWORD(data0)));
	SmWrite8(SRAM(data_byte[4]), LOBYTE(LOWORD(data1)));
	SmWrite8(SRAM(data_byte[5]), HIBYTE(LOWORD(data1)));
	SmWrite8(SRAM(data_byte[6]), LOBYTE(HIWORD(data1)));
	SmWrite8(SRAM(data_byte[7]), HIBYTE(HIWORD(data1)));
#endif

	/* Interrupt 8051. */
	SmWrite32(U8051_CPU_PROTOCOL_INTERRUPT, 1);

	/* Wait for command to start. */
	i = SM501_AC97_CODEC_TIMEOUT;
	while (SmRead8(SRAM(command_busy)) == 0) {
		if (i-- <= 0) {
			dev_err(s->dev, "firmware did not accept command %u\n",
				 command);
			break;
		}
		udelay(1);
	}

	/* Wait for command to finish. */
	while (SmRead8(SRAM(command_busy)) == 1)
		udelay(1);

	/* Return status byte. */
	return(SmRead8(SRAM(status_byte)) == 1);
}
EXPORT_SYMBOL_GPL(sm501_write_command);

static unsigned int __devinit ascii_hex(const char* text, int* index, int count)
{
	/* Initialize result. */
	unsigned int result = 0;

	/* Convert nibbles. */
	while (count-- > 0)
	{
		/* Get Ascii nibble. */
		char c = text[*index];

		/* Convert from Ascii into hex. */
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'F')
			c -= 'A' - 10;
		else if (c >= 'a' && c <= 'f')
			c -= 'a' - 10;
		else
			/* Invalid digits, return 0. */
			return 0;

		/* Add nibble to result. */
		result = (result << 4) | c;

		/* Increment string index. */
		(*index)++;
	}

	/* Return result. */
	return result;
}

int __devinit sm501_load_firmware(struct sm501_audio *s, const char* firmware)
{
	unsigned int value;
	int i, errors, retry;
	unsigned int bytes, address, finish;

	/* Put 8051 in reset state. */
	value = SET_FIELD(0, U8051_RESET_CONTROL, RESET);
	SmWrite32( U8051_RESET, value);

	/* Retry loop for loading firmware. */
	for (retry = 1; retry <= 3; retry++) {
		dprintk("%s: loading firmware: retry %d\n", __func__, retry);

		/* Reset error count. */
		errors = 0;

		/* !!BUGBUG!! Settle down 8051 or SRAM, seems to be required. */
		msleep(250);

		/* Zero SRAM. */
		SmMemset(U8051_SRAM, 0, U8051_SRAM_SIZE);

		/* Walk entire firmware. */
		for (i = 0; i < strlen(firmware);) {
			/* Skip colon (start of line). */
			if (firmware[i] != ':') {
				dev_err(s->dev, "%s: invalid firmware character '%c'\n",
				       __func__, firmware[i]);
				break;
			}
			i++;

			/* Get number of bytes (first byte). */
			bytes = ascii_hex(firmware, &i, 2);

			/* Get address (second and third byte). */
			address = ascii_hex(firmware, &i, 4);

			/* Get finish flag (fourth byte). */
			finish = ascii_hex(firmware, &i, 2);

			/* Loop through all bytes. */
			while (bytes-- > 0) {
				/* Get byte. */
				unsigned int value = ascii_hex(firmware,
						&i, 2);

				/* Write byte into SRAM address. */
				SmWrite8( U8051_SRAM + address,
						value);
				if (SmRead8( U8051_SRAM + address) != value) {
					if (errors++ < 10) {
						dev_warn(s->dev, "%s: %04x: mismatch: "
								"code(%02x) "
								"sram(%02x)\n",
						__func__, address, value,
						(unsigned char)SmRead8( U8051_SRAM + address));
					}
				}

				/* Increment address. */
				address++;
			}

			/* Skip checksum. */
			i += 2;

			/* Finish when end of data. */
			if (finish != 0)
				break;
		}

		if (errors == 0)
			break;
	}

	if (errors > 0)
		return -EFAULT;

	/* Enable 8051. */
	value = SET_FIELD(0, U8051_RESET_CONTROL, ENABLE);
	SmWrite32( U8051_RESET, value);

	/* Wait until the init is complete. During its initialization the
	firmware will increment the "init counter" twice, so that when the
	counter becomes 2, we know that the initialization is complete. */

	i = 5000;
	while (SmRead8( SRAM(init_count)) != 2) {
		if (i-- <= 0) {
			dev_err(s->dev, "%s: firmware timed out during reset\n", __func__);
			return -ETIMEDOUT;
		}
		msleep(1);
	}

	dprintk("%s: firmware loaded and running\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(sm501_load_firmware);


