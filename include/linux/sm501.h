/* include/linux/sm501.h
 *
 * Copyright (C) 2007 SiliconMotion Inc.
 * Copyright (c) 2006 Simtec Electronics
 *
 *	Ben Dooks <ben@simtec.co.uk>
 *	Vincent Sanders <vince@simtec.co.uk>
  *	Boyod Yang <boyoddev@gmail.com>
 *	
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _LINUX_SM501_CORE
#define _LINUX_SM501_CORE

//#define USE_SYSTEM_MEMORY
//#define B_DEBUG
//#define SMI_DEBUG

//kylin
#include <linux/fb.h>
#include <linux/module.h>

#ifdef B_DEBUG
#define b_dbg printk
#else
#define b_dbg(format, arg...) do {} while (0)
#endif
#ifdef SMI_DEBUG
#define smi_dbg printk
#else
#define smi_dbg(format, arg...) do {} while (0)
#endif

#ifndef CONFIG_SMIVGX_UART_OUTPUT_BUF_SIZE
#define CONFIG_SMIVGX_UART_OUTPUT_BUF_SIZE 1024
#endif

#ifdef CONFIG_SM501_USE_EXTERNAL_MEMORY
#define USB_DMA_BUFFER_SIZE 0x80000
#else
#define USB_DMA_BUFFER_SIZE 0x80000
#endif

extern resource_size_t sm50x_mem_size;

extern unsigned long sm501_find_clock(int clksrc, unsigned long req_freq);
extern unsigned long sm501_set_clock(struct device *dev, int clksrc, unsigned long req_freq);
extern void sm501_set_gate( unsigned long gate);

extern void sm501_configure_gpio(unsigned int gpio, unsigned char mode);

#define MHZ (1000 * 1000)

/* Platform data definitions */

#define SM501FB_FLAG_USE_INIT_MODE	(1<<0)
#define SM501FB_FLAG_DISABLE_AT_EXIT	(1<<1)
#define SM501FB_FLAG_USE_HWCURSOR	(1<<2)
#define SM501FB_FLAG_USE_HWACCEL	(1<<3)


typedef enum _panel_state_t
{
	PANEL_OFF,
	PANEL_ON,
}
panel_state_t;

struct sm501_platdata_fbsub {
	struct fb_videomode	*def_mode;
	unsigned int		 def_bpp;
	unsigned long		 max_mem;
	unsigned int		 flags;
};

/* sm501_platdata_fb flag field bit definitions */

#define SM501_FBPD_SWAP_FB_ENDIAN	(1<<0)	/* need to endian swap */


enum sm501_fb_routing {
	SM501_FB_OWN		= 0,	/* CRT=>CRT, Panel=>Panel */
	SM501_FB_CRT_PANEL	= 1,	/* Panel=>CRT, Panel=>Panel */
};

/* sm501_platdata_fb
 *
 * configuration data for the framebuffer driver
*/

struct sm501_platdata_fb {
	enum sm501_fb_routing		 fb_route;
	unsigned int			 flags;   /*For big endian or other. ...*/
	struct sm501_platdata_fbsub	*fb_crt;
	struct sm501_platdata_fbsub	*fb_pnl;
};


#define SM501_USE_USB_HOST	(1<<0)
#define SM501_USE_FB			(1<<1)
#define SM501_USE_SSP0		(1<<2)
#define SM501_USE_SSP1		(1<<3)
#define SM501_USE_UART0		(1<<4)
#define SM501_USE_UART1		(1<<5)
#define SM501_USE_FBACCEL	(1<<6)
#define SM501_USE_AC97		(1<<7)
#define SM501_USE_I2S		(1<<8)
#define SM501_USE_GPIO		(1<<9)
#define SM501_USE_PWM		(1<<10)
#define SM501_USE_I2C		(1<<11)

#define SM501_USE_ALL		(0xffff)

/* panel clock */
#define SM501_CLOCK_P2XCLK		(24)
/* crt clock */
#define SM501_CLOCK_V2XCLK		(16)
/* main clock */
#define SM501_CLOCK_MCLK		(8)
/* SDRAM controller clock */
#define SM501_CLOCK_M1XCLK		(0)

struct sm501_initdata {
	unsigned int	 	flags;     /*For big endian or other. ...*/
	unsigned int		devices;
	unsigned long		mclk;		/* non-zero to modify */
	unsigned long		m1xclk;		/* non-zero to modify */
};


/* sm501_platdata
 *
 * This is passed with the platform device to allow the board
 * to control the behaviour of the SM501 driver(s) which attach
 * to the device.
 *
*/

struct sm501_platdata {
	struct sm501_initdata		*init;
	struct sm501_platdata_fb	*fb;
	int				gpio_base;
};


static const struct fb_videomode SM501modedb[] = {
	/* 34 1024x600-60 VESA */
	{NULL,  60, 1024, 600, 20423, 144,  40, 18, 1, 104, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED},
	/*35 1024x600-70 VESA */
	{NULL,  70, 1024, 600, 17211, 152,  48, 21, 1, 104, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED},
	/* 36 1024x600-75 VESA */
	{NULL,  75, 1024, 600, 15822, 160,  56, 23, 1, 104, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED},
	/* 37 1024x600-85 VESA */
	{NULL,  85, 1024, 600, 13730, 168,  56, 26, 1, 112, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED},
	/*720x480*/
	{NULL, 60,  720,  480,  37427, 88,   16, 13, 1,   72,  3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED},
};

/* Memory bases for the SM501 MFD */
extern unsigned char *sm50x_base_reg;
extern unsigned char *sm50x_base_mem;

/* SmRead8()
 * 
 *    Read the 8-bit value, to the 8-bit register location
 *    specified by nOffset
 *
*/
static inline unsigned long SmRead8(unsigned long nOffset)
{
    return readb(sm50x_base_reg + nOffset);
}

/* SmRead16()
 * 
 *    Read the 16-bit value, to the 16-bit register location
 *    specified by nOffset
 *
*/
static inline unsigned long SmRead16(unsigned long nOffset)
{
    return readw(sm50x_base_reg + nOffset);
}

/* SmRead32()
 * 
 *    Read the 32-bit value, to the 32-bit register location
 *    specified by nOffset
 *
*/
static inline unsigned long SmRead32(unsigned long nOffset)
{
    return readl(sm50x_base_reg + nOffset);
}


/* SmWrite8()
 * 
 *    Write the 8-bit value, to the 8-bit register location
 *    specified by nOffset
 *
*/
static inline void SmWrite8(unsigned long nOffset, unsigned long nData)
{
	writeb(nData,sm50x_base_reg + nOffset);
}

/* SmWrite16()
 * 
 *    Write the 16-bit value, to the 16-bit register location
 *    specified by nOffset
 *
*/
static inline void SmWrite16(unsigned long nOffset, unsigned long nData)
{
	writew(nData,sm50x_base_reg + nOffset);
}

/* SmWrite32()
 * 
 *    Write the 32-bit value, to the 32-bit register location
 *    specified by nOffset
 *
*/
static inline void SmWrite32(unsigned long nOffset, unsigned long nData)
{
	writel(nData,sm50x_base_reg + nOffset);
}

static inline void SmMemset(unsigned long nOffset, unsigned char val, int count)
{
	memset((void __force *) (sm50x_base_reg + nOffset), val, count);
}

#endif
