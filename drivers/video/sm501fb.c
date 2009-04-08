/* linux/drivers/video/sm501fb.c
 *
 * Copyright (c) 2006 Simtec Electronics
 *	Vincent Sanders <vince@simtec.co.uk>
 *	Ben Dooks <ben@simtec.co.uk>
 *	Boyod.yang,  <boyod.yang@siliconmotion.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Framebuffer driver for the Silicon Motion SM501
 */

//#define DEBUG

#ifdef DEBUG
#define smdbg(format, arg...)	printk(KERN_DEBUG format , ## arg)
#else
#define smdbg(format, arg...)
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/console.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <linux/screen_info.h>

#include <linux/sm501.h>
#include <linux/sm501_regs.h>

#define SM501_MEMF_CURSOR		(1)
#define SM501_MEMF_PANEL		(2)
#define SM501_MEMF_CRT			(4)
#define SM501_MEMF_ACCEL		(8)

#define DEFAULT_MODE "640x480-16@60"


/*
 * globals
 */
 
static char *mode_option __devinitdata = DEFAULT_MODE;
static int noaccel = 0;
static int mirror = 0;
static int dcolor = 0;
static int nomtrr = 1;

static struct fb_var_screeninfo __devinitdata sm50xfb_default_var = {
	.xres = 640,
	.yres = 480,
	.xres_virtual = 640,
	.yres_virtual = 480,
	.bits_per_pixel = 8,
	.red = {0, 8, 0},
	.green = {0, 8, 0},
	.blue = {0, 8, 0},
	.transp = {0, 0, 0},
	.activate = FB_ACTIVATE_NOW,
	.height = -1,
	.width = -1,
	.pixclock = 39721,
	.left_margin = 40,
	.right_margin = 24,
	.upper_margin = 32,
	.lower_margin = 11,
	.hsync_len = 96,
	.vsync_len = 2,
	.vmode = FB_VMODE_NONINTERLACED
};


#define NR_PALETTE	256
#define PANEL_CRT_ADDR	0x400000

enum sm501_controller {
	HEAD_CRT	= 0,
	HEAD_PANEL	= 1,
};

/* SM501 memory adress */
struct sm501_mem {
	unsigned long	 size;
	unsigned long	 sm_addr;
	void __iomem	*k_addr;
};

/* private data that is shared between all frambuffers* */
struct sm501fb_info {
	struct device		*dev;
	struct fb_info		*fb[2];		/* fb info for both heads */
	struct resource		*fbmem_res;	/* framebuffer resource */
	struct resource		*regs_res;	/* registers resource */
	struct sm501_platdata_fb *pdata;	/* our platform data */

	unsigned long		 pm_crt_ctrl;	/* pm: crt ctrl save */

	int			 irq;
	void __iomem		*regs;		/* remapped registers */
	void __iomem		*fbmem;		/* remapped framebuffer */
	size_t			 fbmem_len;	/* length of remapped region */
};

/* per-framebuffer private data */
struct sm501fb_par {
	u32			 pseudo_palette[16];

	enum sm501_controller	 head;
	struct sm501_mem	 cursor;
	struct sm501_mem	 screen;
	struct fb_ops		 ops;

	void			*store_fb;
	void			*store_cursor;
	void __iomem		*cursor_regs;
	struct sm501fb_info	*info;
};

/* Helper functions */

static inline int h_total(struct fb_var_screeninfo *var)
{
	return var->xres + var->left_margin +
		var->right_margin + var->hsync_len;
}

static inline int v_total(struct fb_var_screeninfo *var)
{
	return var->yres + var->upper_margin +
		var->lower_margin + var->vsync_len;
}

/* sm501fb_sync_regs()
 *
 * This call is mainly for PCI bus systems where we need to
 * ensure that any writes to the bus are completed before the
 * next phase, or after completing a function.
*/

static inline void sm501fb_sync_regs(struct sm501fb_info *info)
{
	SmRead32(0);
}


/* sm501_alloc_mem
 *
 * This is an attempt to lay out memory for the two framebuffers and
 * everything else
 *
 * |fbmem_res->start	                                       fbmem_res->end|
 * |                                                                         |
 * |fb[0].fix.smem_start    |         |fb[1].fix.smem_start    |     2K      |
 * |-> fb[0].fix.smem_len <-| spare   |-> fb[1].fix.smem_len <-|-> cursors <-|
 *
 * The "spare" space is for the 2d engine data
 * the fixed is space for the cursors (2x1Kbyte)
 *
 * we need to allocate memory for the 2D acceleration engine
 * command list and the data for the engine to deal with.
 *
 * - all allocations must be 128bit aligned
 * - cursors are 64x64x2 bits (1Kbyte)
 *
 */

static int sm501_alloc_mem(struct sm501fb_info *inf, struct sm501_mem *mem,
			   unsigned int why, size_t size)
{
	unsigned int ptr = 0;

	switch (why) {
	case SM501_MEMF_CURSOR:
		ptr = inf->fbmem_len - size;
		inf->fbmem_len = ptr;
		break;

	case SM501_MEMF_PANEL:
/*
		ptr = inf->fbmem_len - size;
		if (ptr < inf->fb[0]->fix.smem_len)
			return -ENOMEM;
*/		ptr = 0x0;
		break;

	case SM501_MEMF_CRT:
		ptr = PANEL_CRT_ADDR;

		break;

	case SM501_MEMF_ACCEL:
		ptr = inf->fb[0]->fix.smem_len;

		if ((ptr + size) >
		    (inf->fb[1]->fix.smem_start - inf->fbmem_res->start))
			return -ENOMEM;
		break;

	default:
		return -EINVAL;
	}

	mem->size    = size;
	mem->sm_addr = ptr;
	mem->k_addr  = inf->fbmem + ptr;

	dev_dbg(inf->dev, "%s: result %08lx, %p - %u, %08lx\n",
		__func__, mem->sm_addr, mem->k_addr, why, size);

	return 0;
}



#ifdef CONFIG_MTRR

extern void sm501fb_set_mtrr(struct device *dev);

extern void sm501fb_unset_mtrr(struct device *dev);

#else
#define sm501fb_set_mtrr(x) do {smdbg("MTRR is disabled in the kernel\n");} while (0)

#define sm501fb_unset_mtrr(x) do { } while (0)
#endif /* CONFIG_MTRR */

/* sm501fb_ps_to_hz
 *
 * Converts a period in picoseconds to Hz.
 *
 * Note, we try to keep this in Hz to minimise rounding with
 * the limited PLL settings on the SM501.
*/

static unsigned long sm501fb_ps_to_hz(unsigned long psvalue)
{
	unsigned long long numerator=1000*1000*1000*1000ULL;

	/* 10^12 / picosecond period gives frequency in Hz */
	do_div(numerator, psvalue);
	return (unsigned long)numerator;
}

/* sm501fb_hz_to_ps is identical to the oposite transform */

#define sm501fb_hz_to_ps(x) sm501fb_ps_to_hz(x)


static void sm501fb_wait_panelvsnc(int vsync_count)
{
	unsigned long status;

	while (vsync_count-- > 0)
	{
		// Wait for end of vsync.
		do
		{
			status = FIELD_GET(SmRead32(CMD_INTPR_STATUS),
							   CMD_INTPR_STATUS,
							   PANEL_SYNC);
		}
		while (status == CMD_INTPR_STATUS_PANEL_SYNC_ACTIVE);

		// Wait for start of vsync.
		do
		{
			status = FIELD_GET(SmRead32(CMD_INTPR_STATUS),
							   CMD_INTPR_STATUS, 
							   PANEL_SYNC);
		}
		while (status == CMD_INTPR_STATUS_PANEL_SYNC_INACTIVE);
	}
	smdbg("w_panelvsync\n");

}

/* sm501fb_setup_gamma
 *
 * Programs a linear 1.0 gamma ramp in case the gamma
 * correction is enabled without programming anything else.
*/

static void sm501fb_setup_gamma(struct sm501fb_info *fbi,
				unsigned long palette)
{
	unsigned long value = 0;
	int offset;

	/* set gamma values */
	for (offset = 0; offset < 256 * 4; offset += 4) {
		SmWrite32(palette + offset, value);
		value += 0x010101; 	/* Advance RGB by 1,1,1.*/
	}
}

/* sm501fb_check_var
 *
 * check common variables for both panel and crt
*/

static int sm501fb_check_var(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *sm  = par->info;
	unsigned long tmp;

	/* check we can fit these values into the registers */

	if (var->hsync_len > 255 || var->vsync_len > 63)
		return -EINVAL;

	/* hdisplay end and hsync start */
	if ((var->xres + var->right_margin) > 4096)
		return -EINVAL;

	/* vdisplay end and vsync start */
	if ((var->yres + var->lower_margin) > 2048)
		return -EINVAL;

	/* hard limits of device */

	if (h_total(var) > 4096 || v_total(var) > 2048)
		return -EINVAL;

	/* check our line length is going to be 128 bit aligned */

	tmp = (var->xres * var->bits_per_pixel) / 8;
	if ((tmp & 15) != 0)
		return -EINVAL;

	/* check the virtual size */

	if (var->xres_virtual > 4096 || var->yres_virtual > 2048)
		return -EINVAL;

	/* can cope with 8,16 or 32bpp */

	if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel == 24)
		var->bits_per_pixel = 32;

	/* set r/g/b positions and validate bpp */
	switch(var->bits_per_pixel) {
	case 8:
		var->red.length		= var->bits_per_pixel;
		var->red.offset		= 0;
		var->green.length	= var->bits_per_pixel;
		var->green.offset	= 0;
		var->blue.length	= var->bits_per_pixel;
		var->blue.offset	= 0;
		var->transp.length	= 0;
		var->transp.offset	= 0;

		break;

	case 16:
			var->red.offset		= 11;
			var->blue.offset	= 0;
		var->green.offset	= 5;

		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;

		var->transp.offset	= 0;
		var->transp.length	= 0;

		break;

	case 32:
			var->transp.offset	= 24;
			var->red.offset		= 16;
			var->green.offset	= 8;
			var->blue.offset	= 0;

		var->red.length		= 8;
		var->green.length	= 8;
		var->blue.length	= 8;
		var->transp.length	= 8;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * sm501fb_check_var_crt():
 *
 * check the parameters for the CRT head, and either bring them
 * back into range, or return -EINVAL.
*/

static int sm501fb_check_var_crt(struct fb_var_screeninfo *var,
				 struct fb_info *info)
{
	return sm501fb_check_var(var, info);
}

/* sm501fb_check_var_pnl():
 *
 * check the parameters for the CRT head, and either bring them
 * back into range, or return -EINVAL.
*/

static int sm501fb_check_var_pnl(struct fb_var_screeninfo *var,
				 struct fb_info *info)
{
	return sm501fb_check_var(var, info);
}

/* sm501fb_set_par_common
 *
 * set common registers for framebuffers
*/

static int sm501fb_set_par_common(struct fb_info *info,
				  struct fb_var_screeninfo *var)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned long pixclock=0;      /* pixelclock in Hz */
	unsigned long sm501pixclock=0; /* pixelclock the 501 can achive in Hz */
	unsigned int mem_type;
	unsigned int clock_type;
	unsigned int head_addr;

	dev_dbg(fbi->dev, "%s: %dx%d, bpp = %d, virtual %dx%d\n",
		__func__, var->xres, var->yres, var->bits_per_pixel,
		var->xres_virtual, var->yres_virtual);

	switch (par->head) {
	case HEAD_CRT:
		mem_type = SM501_MEMF_CRT;
		clock_type = SM501_CLOCK_V2XCLK;
		head_addr = CRT_FB_ADDRESS;
		break;

	case HEAD_PANEL:
		mem_type = SM501_MEMF_PANEL;
		clock_type = SM501_CLOCK_P2XCLK;
		head_addr = PANEL_FB_ADDRESS;
		break;

	default:
		mem_type = 0;		/* stop compiler warnings */
		head_addr = 0;
		clock_type = 0;
	}

	switch (var->bits_per_pixel) {
	case 8:
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		break;

	case 16:
	case 32:
		if (info->var.nonstd)
			info->fix.visual = FB_VISUAL_DIRECTCOLOR;
		else
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
		break;
	}
	/* allocate fb memory within 501 */
	info->fix.line_length = (var->xres_virtual * var->bits_per_pixel)/8;
	info->fix.smem_len  = info->fix.line_length * var->yres_virtual;
	dev_dbg(fbi->dev, "%s: line length = %u\n", __func__,
		info->fix.line_length);
	if (sm501_alloc_mem(fbi, &par->screen, mem_type,
			    info->fix.smem_len)) {
		dev_err(fbi->dev, "no memory available\n");
		return -ENOMEM;
	}
	info->fix.smem_start = fbi->fbmem_res->start + par->screen.sm_addr;

	info->screen_base = fbi->fbmem + par->screen.sm_addr;
	info->screen_size = info->fix.smem_len;
	/* set start of framebuffer to the screen */

	SmWrite32(PANEL_FB_ADDRESS,
					FIELD_VALUE(0,PANEL_FB_ADDRESS, ADDRESS, par->screen.sm_addr) |
					FIELD_SET(0, PANEL_FB_ADDRESS, STATUS, FLIP));

	/* program CRT clock  */
	pixclock = sm501fb_ps_to_hz(var->pixclock);
	sm501pixclock = sm501_set_clock(fbi->dev->parent, clock_type, pixclock);
	/* update fb layer with actual clock used */
	var->pixclock = sm501fb_hz_to_ps(sm501pixclock);
	
	dev_dbg(fbi->dev, "%s: pixclock(ps) = %u, pixclock(Hz)  = %lu, "
	       "sm501pixclock = %lu,  error = %ld%%\n",
	       __func__, var->pixclock, pixclock, sm501pixclock,
	       ((pixclock - sm501pixclock)*100)/pixclock);

	return 0;
}

/* sm501fb_set_par_geometry
 *
 * set the geometry registers for specified framebuffer.
*/

static void sm501fb_set_par_geometry(struct fb_info *info,
				     struct fb_var_screeninfo *var)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned long  base;
	unsigned long reg;

	if (par->head == HEAD_CRT)
		base = CRT_HORIZONTAL_TOTAL;
	else
		base = PANEL_HORIZONTAL_TOTAL;

	/* set framebuffer width and display width */

	reg = info->fix.line_length;
	reg = FIELD_VALUE(reg, CRT_FB_WIDTH, WIDTH, (var->xres * var->bits_per_pixel)/8) ;

	SmWrite32((par->head == HEAD_CRT ? CRT_FB_WIDTH :  PANEL_FB_WIDTH), reg);

	/* program horizontal total */

	reg = (var->xres - 1);
	reg = FIELD_VALUE(reg, CRT_HORIZONTAL_TOTAL, TOTAL, (h_total(var) - 1) ) ;
	SmWrite32(base+SM501_OFF_DC_H_TOT, reg);

	/* program horizontal sync */

	reg = var->xres + var->right_margin - 1;
	reg = FIELD_VALUE(reg, CRT_HORIZONTAL_SYNC, WIDTH, var->hsync_len);

	SmWrite32(base + SM501_OFF_DC_H_SYNC, reg);

	/* program vertical total */

	reg = (var->yres - 1);
	reg  = FIELD_VALUE(reg, CRT_VERTICAL_TOTAL, TOTAL, (v_total(var) - 1));
	
	SmWrite32(base + SM501_OFF_DC_V_TOT, reg);

	/* program vertical sync */
	reg  = var->yres + var->lower_margin - 1;
	reg  = FIELD_VALUE(reg, CRT_VERTICAL_TOTAL, TOTAL, var->vsync_len);
	SmWrite32(base + SM501_OFF_DC_V_SYNC, reg);
}

/* sm501fb_pan_crt
 *
 * pan the CRT display output within an virtual framebuffer
*/

static int sm501fb_pan_crt(struct fb_var_screeninfo *var,
			   struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned int bytes_pixel = var->bits_per_pixel / 8;
	unsigned long reg;
	unsigned long xoffs;

	xoffs = var->xoffset * bytes_pixel;

	reg = SmRead32(CRT_DISPLAY_CTRL);
	reg = FIELD_VALUE(reg, CRT_DISPLAY_CTRL, PIXEL, (xoffs & 15) / bytes_pixel);
	
	SmWrite32(CRT_DISPLAY_CTRL, reg);

	reg = (par->screen.sm_addr + xoffs + var->yoffset * info->fix.line_length);
	reg = FIELD_SET(reg, CRT_FB_ADDRESS, STATUS , PENDING);
	SmWrite32(CRT_FB_ADDRESS, reg);

	sm501fb_sync_regs(fbi);
	return 0;
}

/* sm501fb_pan_pnl
 *
 * pan the panel display output within an virtual framebuffer
*/

static int sm501fb_pan_pnl(struct fb_var_screeninfo *var,
			   struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned long reg;

	reg = FIELD_VALUE(reg, PANEL_WINDOW_WIDTH, X, var->xoffset);
	reg = FIELD_VALUE(reg, PANEL_WINDOW_WIDTH, WIDTH, var->xres_virtual);
	SmWrite32(PANEL_WINDOW_WIDTH, reg);

	reg = FIELD_VALUE(reg, PANEL_WINDOW_HEIGHT, Y, var->yoffset);
	reg = FIELD_VALUE(reg, PANEL_WINDOW_HEIGHT, HEIGHT, var->yres_virtual);
	SmWrite32(PANEL_WINDOW_HEIGHT, reg);

	sm501fb_sync_regs(fbi);
	return 0;
}

/* sm501fb_set_par_crt
 *
 * Set the CRT video mode from the fb_info structure
*/

static int sm501fb_set_par_crt(struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	struct fb_var_screeninfo *var = &info->var;
	unsigned long control;       /* control register */
	int ret;

	/* activate new configuration */

	dev_dbg(fbi->dev, "%s(%p)\n", __func__, info);

	/* enable CRT DAC - note 0 is on!*/
	control = SmRead32(MISC_CTRL);
	SmWrite32(MISC_CTRL, FIELD_SET(control, MISC_CTRL, DAC_POWER, ENABLE) );
	
	control = SmRead32(CRT_DISPLAY_CTRL);

	/* set the sync polarities before we check data source  */

	if ((var->sync & FB_SYNC_HOR_HIGH_ACT) == 0)
		control = FIELD_SET(control, CRT_DISPLAY_CTRL, HSYNC_PHASE, ACTIVE_LOW);

	if ((var->sync & FB_SYNC_VERT_HIGH_ACT) == 0)
		control = FIELD_SET(control, CRT_DISPLAY_CTRL, VSYNC_PHASE, ACTIVE_LOW);

	if ( FIELD_GET(control, CRT_DISPLAY_CTRL, SELECT)== CRT_DISPLAY_CTRL_SELECT_PANEL) {
		/* the head is displaying panel data... */
		sm501_alloc_mem(fbi, &par->screen, SM501_MEMF_CRT, 0);
		goto out_update;
	}

	ret = sm501fb_set_par_common(info, var);
	if (ret) {
		dev_err(fbi->dev, "failed to set common parameters\n");
		return ret;
	}

	sm501fb_pan_crt(var, info);
	sm501fb_set_par_geometry(info, var);

	dev_dbg(fbi->dev, "CRT pixel:%d\n",var->bits_per_pixel);
	control = FIELD_SET(control, CRT_DISPLAY_CTRL, FIFO, 3);
	
	switch(var->bits_per_pixel) {
	case 8:
		control = FIELD_SET(control, CRT_DISPLAY_CTRL, FORMAT,  8);
		break;
	case 16:
		control = FIELD_SET(control, CRT_DISPLAY_CTRL, FORMAT,  16);
		sm501fb_setup_gamma(fbi, CRT_PALETTE_RAM);
		break;
	case 32:
		control = FIELD_SET(control, CRT_DISPLAY_CTRL, FORMAT,  32);
		sm501fb_setup_gamma(fbi, CRT_PALETTE_RAM);
		break;
	default:
		BUG();
	}

#ifdef CONFIG_FB_SM501_DUAL_HEAD
	control = FIELD_SET(control, CRT_DISPLAY_CTRL, SELECT,  CRT);
#else
	control = FIELD_SET(control, CRT_DISPLAY_CTRL, SELECT,  PANEL);
#endif
	control = FIELD_SET(control, CRT_DISPLAY_CTRL, TIMING,  ENABLE);
	control = FIELD_SET(control, CRT_DISPLAY_CTRL, PLANE, ENABLE);
	
 out_update:
	dev_dbg(fbi->dev, "new control is %08lx\n", control);

	SmWrite32(CRT_DISPLAY_CTRL, control);
	sm501fb_sync_regs(fbi);

	return 0;
}


/* sm501fb_panel_power
 *
 * Set power of panel
*/
static void sm501fb_panel_power(panel_state_t on_off, int vsync_delay)
{
	unsigned long panelControl = SmRead32(PANEL_DISPLAY_CTRL);

	if (on_off == PANEL_ON)
	{
		// Turn on FPVDDEN.
		panelControl = FIELD_SET(panelControl,
								 PANEL_DISPLAY_CTRL, FPVDDEN, HIGH);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);

		sm501fb_wait_panelvsnc(vsync_delay);

		// Turn on FPDATA.
		panelControl = FIELD_SET(panelControl, 
								 PANEL_DISPLAY_CTRL, DATA, ENABLE);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);

		sm501fb_wait_panelvsnc(vsync_delay);

		// Turn on FPVBIAS.
		panelControl = FIELD_SET(panelControl, 
								 PANEL_DISPLAY_CTRL, VBIASEN, HIGH);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);

		sm501fb_wait_panelvsnc(vsync_delay);

		// Turn on FPEN.
		panelControl = FIELD_SET(panelControl, 
								 PANEL_DISPLAY_CTRL, FPEN, HIGH);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);
	}

	else
	{
		// Turn off FPEN.
		panelControl = FIELD_SET(panelControl,
								 PANEL_DISPLAY_CTRL, FPEN, LOW);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);

		sm501fb_wait_panelvsnc(vsync_delay);


		// Turn off FPVBIASEN.
		panelControl = FIELD_SET(panelControl, 
								 PANEL_DISPLAY_CTRL, VBIASEN, LOW);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);

		sm501fb_wait_panelvsnc(vsync_delay);


		// Turn off FPDATA.
		panelControl = FIELD_SET(panelControl, 
								 PANEL_DISPLAY_CTRL, DATA, DISABLE);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);

		sm501fb_wait_panelvsnc(vsync_delay);


		// Turn off FPVDDEN.
		panelControl = FIELD_SET(panelControl, 
								 PANEL_DISPLAY_CTRL, FPVDDEN, LOW);
		SmWrite32(PANEL_DISPLAY_CTRL, panelControl);
	}
	

}



/* sm501fb_set_par_pnl
 *
 * Set the panel video mode from the fb_info structure
*/

static int sm501fb_set_par_pnl(struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	struct fb_var_screeninfo *var = &info->var;
	unsigned long control;
	unsigned long reg;
	int ret;

	dev_dbg(fbi->dev, "%s(%p)\n", __func__, info);

	/* activate this new configuration */

	ret = sm501fb_set_par_common(info, var);
	if (ret)
		return ret;

	sm501fb_pan_pnl(var, info);
	sm501fb_set_par_geometry(info, var);

	/* update control register */

	control = SmRead32(PANEL_DISPLAY_CTRL);

	control = FIELD_SET(control, PANEL_DISPLAY_CTRL, FIFO, 3);	

	dev_dbg(fbi->dev, "PANEL pixel:%d\n",var->bits_per_pixel);

	switch(var->bits_per_pixel) {
	case 8:
		control = FIELD_SET(control, PANEL_DISPLAY_CTRL, FORMAT,  8);
		break;
	case 16:
		control = FIELD_SET(control, PANEL_DISPLAY_CTRL, FORMAT,  16);
		sm501fb_setup_gamma(fbi, PANEL_PALETTE_RAM);
		break;
	case 32:
		control = FIELD_SET(control, PANEL_DISPLAY_CTRL, FORMAT,  32);
		sm501fb_setup_gamma(fbi, PANEL_PALETTE_RAM);
		break;
	default:
		BUG();
	}

	SmWrite32(PANEL_PAN_CTRL, 0x0);

	/* panel plane top left and bottom right location */

	SmWrite32(PANEL_PLANE_TL, 0x0);

	reg  = var->xres - 1;
	reg |= (var->yres - 1) << 16;
	SmWrite32( PANEL_PLANE_BR, reg);

	/* program panel control register */

	control = FIELD_SET(control, PANEL_DISPLAY_CTRL, TIMING, ENABLE);	/* DATA */
	control = FIELD_SET(control, PANEL_DISPLAY_CTRL, PLANE, ENABLE);	/* DATA */

	if ((var->sync & FB_SYNC_HOR_HIGH_ACT) == 0)
		control = FIELD_SET(control, PANEL_DISPLAY_CTRL, HSYNC_PHASE, ACTIVE_LOW);	/* DATA */

	if ((var->sync & FB_SYNC_VERT_HIGH_ACT) == 0)
		control = FIELD_SET(control, PANEL_DISPLAY_CTRL, VSYNC_PHASE, ACTIVE_LOW);	/* DATA */

	dev_dbg(fbi->dev, "%s H:%d  V:%d\n", __func__, var->sync, var->sync);

	SmWrite32(PANEL_DISPLAY_CTRL, control);
	sm501fb_wait_panelvsnc(4);

	/* power the panel up */

	sm501fb_panel_power(PANEL_ON, 4);

	return 0;
}


/* chan_to_field
 *
 * convert a colour value into a field position
 *
 * from pxafb.c
*/

static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

/* sm501fb_setcolreg
 *
 * set the colour mapping for modes that support palettised data
*/

static int sm501fb_setcolreg(unsigned regno,
			     unsigned red, unsigned green, unsigned blue,
			     unsigned transp, struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned long base;
	unsigned int val;

	if (regno > 255)
		return 1;

	if (par->head == HEAD_CRT)
		base = CRT_PALETTE_RAM;
	else
		base = PANEL_PALETTE_RAM;

	switch (info->fix.visual) {
	case FB_VISUAL_DIRECTCOLOR:
	case FB_VISUAL_TRUECOLOR:
		/* 16/32 bit true-colour, use pseuo-palette for 16 base color*/
		if (regno < 16) {

			if (info->var.bits_per_pixel==16)	{
				u32 *pal = par->pseudo_palette;
			val  = chan_to_field(red,   &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue,  &info->var.blue);
#ifdef __BIG_ENDIAN
				 pal[regno] =( (red & 0xf800) >> 8) | ((green & 0xe000) >> 13) |((green & 0x1c00) << 3) | ((blue & 0xf800) >> 3);
#else
				pal[regno] = val;
#endif

			}
			else{
				u32 *pal = par->pseudo_palette;
				val  = chan_to_field(red,   &info->var.red);
				val |= chan_to_field(green, &info->var.green);
				val |= chan_to_field(blue,  &info->var.blue);
#ifdef __BIG_ENDIAN
				val = (val& 0xff00ff00>>8)|(val& 0x00ff00ff<<8);
#endif
			pal[regno] = val;
		}
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		/* color depth 8 bit*/
			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);
			SmWrite32(base + (regno * 4), val);
		break;

	default:
		return 1;   /* unknown type */
	}

	return 0;
}


/* field_to_chan
 *
 * convert  a field position intoa colour value
 *
 * from pxafb.c
*/

static inline u16 field_to_chan(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan >>= bf->offset;
	chan &=( (1<<bf->length)-1);
	chan <<= 16 - bf->length;
	return chan &0xffff;
}

int sm501fb_set_cmap(struct fb_cmap *cmap, struct fb_info *info)
{
	int i, start, rc = 0;
	u16 *red, *green, *blue, *transp;
	u_int hred, hgreen, hblue, htransp= 0xffff;
	unsigned int val;

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;
	start = cmap->start;

	if (start < 0 || (!info->fbops->fb_setcolreg))
		return -EINVAL;

		for (i = 0; i < cmap->len; i++) {
#if 1
			val  = chan_to_field(red[i],   &info->var.red);
			val |= chan_to_field(green[i], &info->var.green);
			val |= chan_to_field(blue[i],  &info->var.blue);

			if (transp){
				val |= chan_to_field(transp[i],  &info->var.transp);
			}

			val = (val& 0xff00ff00>>8)|(val& 0x00ff00ff<<8);

			red[i] |= field_to_chan(val,&info->var.red);
			green[i] |= field_to_chan(val,&info->var.green);
			blue[i] |= field_to_chan(val,&info->var.blue);

			if (transp)
				transp[i] |= field_to_chan(val,&info->var.transp);
#else

		           red[i] = i*65535/((1<<info->var.red.length)-1);
	 	           green[i] = i*65535/((1<<info->var.green.length)-1);
	 	           blue[i] = i*65535/((1<<info->var.blue.length)-1);
	 	           transp[i] = 0;
#endif
			if (info->fbops->fb_setcolreg(start++,
						      red[i], green[i], blue[i],
						      transp[i], info))
			break;
		}

	if (rc == 0)
		fb_copy_cmap(cmap, &info->cmap);

	return rc;
}

/* sm501fb_blank
 *
 * Blank or un-blank the crt interface
*/

static int sm501fb_blank(int blank_mode, struct fb_info *info)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned long ctrl, misc_value;

	dev_dbg(fbi->dev, "%s(mode=%d, %p)\n", __func__, blank_mode, info);

	ctrl = SmRead32(SYSTEM_CTRL);

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
		ctrl = FIELD_SET(ctrl, SYSTEM_CTRL, DPMS, VNHN);
		
	case FB_BLANK_NORMAL:
		ctrl = FIELD_SET(ctrl, SYSTEM_CTRL, DPMS, VPHP);
		break;

	case FB_BLANK_UNBLANK:
		ctrl = FIELD_SET(ctrl, SYSTEM_CTRL, DPMS, VPHP);
		break;

	case FB_BLANK_VSYNC_SUSPEND:
		ctrl = FIELD_SET(ctrl, SYSTEM_CTRL, DPMS, VNHP);
		break;		
	case FB_BLANK_HSYNC_SUSPEND:
		ctrl = FIELD_SET(ctrl, SYSTEM_CTRL, DPMS, VPHN);
		break;
	default:
		return 1;

	}

	SmWrite32(SYSTEM_CTRL, ctrl);
	sm501fb_wait_panelvsnc(4);

	return 0;
}

/* sm501fb_cursor
 *
 * set or change the hardware cursor parameters
*/

static int sm501fb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	struct sm501fb_par  *par = info->par;
	struct sm501fb_info *fbi = par->info;
	unsigned long base;
	unsigned long hwc_addr;
	unsigned long fg, bg;

//	dev_dbg(fbi->dev, "%s(%p,%p)\n", __func__, info, cursor);

	if (par->head == HEAD_CRT)
		base = CRT_HWC_ADDRESS;
	else
		base = PANEL_HWC_ADDRESS;

	/* check not being asked to exceed capabilities */

	if (cursor->image.width > 64)
		return -EINVAL;

	if (cursor->image.height > 64)
		return -EINVAL;

	if (cursor->image.depth > 1)
		return -EINVAL;

	hwc_addr = SmRead32(base + SM501_OFF_HWC_ADDR);

	if (cursor->enable)
		SmWrite32( base + SM501_OFF_HWC_ADDR, hwc_addr | SM501_HWC_EN);
	else
		SmWrite32(base + SM501_OFF_HWC_ADDR, hwc_addr & ~SM501_HWC_EN );

	/* set data */
	if (cursor->set & FB_CUR_SETPOS) {
		unsigned int x = cursor->image.dx;
		unsigned int y = cursor->image.dy;

		if (x >= 2048 || y >= 2048 )
			return -EINVAL;

//		dev_dbg(fbi->dev, "set position %d,%d\n", x, y);

		//y += cursor->image.height;

		SmWrite32(base + SM501_OFF_HWC_LOC, x | (y << 16));
	}

	if (cursor->set & FB_CUR_SETCMAP) {
		unsigned int bg_col = cursor->image.bg_color;
		unsigned int fg_col = cursor->image.fg_color;

		dev_dbg(fbi->dev, "%s: update cmap (%08x,%08x)\n",
			__func__, bg_col, fg_col);

		bg = ((info->cmap.red[bg_col] & 0xF8) << 8) |
			((info->cmap.green[bg_col] & 0xFC) << 3) |
			((info->cmap.blue[bg_col] & 0xF8) >> 3);

		fg = ((info->cmap.red[fg_col] & 0xF8) << 8) |
			((info->cmap.green[fg_col] & 0xFC) << 3) |
			((info->cmap.blue[fg_col] & 0xF8) >> 3);

		dev_dbg(fbi->dev, "fgcol %08x, bgcol %08x\n", fg, bg);

		SmWrite32(base + SM501_OFF_HWC_COLOR_1_2, bg);
		SmWrite32(base + SM501_OFF_HWC_COLOR_3, fg);
	}

	if (cursor->set & FB_CUR_SETSIZE ||
	    cursor->set & (FB_CUR_SETIMAGE | FB_CUR_SETSHAPE)) {
		/* SM501 cursor is a two bpp 64x64 bitmap this routine
		 * clears it to transparent then combines the cursor
		 * shape plane with the colour plane to set the
		 * cursor */
		int x, y;
		const unsigned char *pcol = cursor->image.data;
		const unsigned char *pmsk = cursor->mask;
		void __iomem   *dst = par->cursor.k_addr;
		unsigned char  dcol = 0;
		unsigned char  dmsk = 0;
		unsigned int   op;

//		dev_dbg(fbi->dev, "%s: setting shape (%d,%d)\n", __func__, cursor->image.width, cursor->image.height);

		for (op = 0; op < (64*64*2)/8; op+=4)
			writel(0x0, dst + op);

		for (y = 0; y < cursor->image.height; y++) {
			for (x = 0; x < cursor->image.width; x++) {
				if ((x % 8) == 0) {
					dcol = *pcol++;
					dmsk = *pmsk++;
				} else {
					dcol >>= 1;
					dmsk >>= 1;
				}

				if (dmsk & 1) {
					op = (dcol & 1) ? 1 : 3;
					op <<= ((x % 4) * 2);

					op |= readb(dst + (x / 4));
					writeb(op, dst + (x / 4));
				}
			}
			dst += (64*2)/8;
		}
	}

	sm501fb_sync_regs(fbi);	/* ensure cursor data flushed */
	return 0;
}


static ssize_t
sm501fb_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct inode *inode = file->f_path.dentry->d_inode;
	int fbidx = iminor(inode);
	struct fb_info *info = registered_fb[fbidx];
	u32 *buffer, *dst;
	u32 __iomem *src;
	int c, i, cnt = 0, err = 0;
	unsigned long total_size;

	if (!info || ! info->screen_base)
		return -ENODEV;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->screen_size;

	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p >= total_size)
		return 0;

	if (count >= total_size)
		count = total_size;

	if (count + p > total_size)
		count = total_size - p;

	buffer = kmalloc((count > PAGE_SIZE) ? PAGE_SIZE : count,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	src = (u32 __iomem *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	while (count) {
		c  = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		dst = buffer;
		for (i = c >> 2; i--; ){
			*dst = fb_readl(src++);
			*dst  = (*dst & 0xff00ff00>>8)|(*dst & 0x00ff00ff<<8);
			dst++;
			}
		if (c & 3) {
			u8 *dst8 = (u8 *) dst;
			u8 __iomem *src8 = (u8 __iomem *) src;

			for (i = c & 3; i--;){
				if (i&1){
				*dst8++ = fb_readb(++src8);
				}
				else{
				*dst8++ = fb_readb(--src8);
				src8 +=2;
				}
			}
			src = (u32 __iomem *) src8;
		}

		if (copy_to_user(buf, buffer, c)) {
			err = -EFAULT;
			break;
		}
		*ppos += c;
		buf += c;
		cnt += c;
		count -= c;
	}

	kfree(buffer);

	return (err) ? err : cnt;
}

static ssize_t
sm501fb_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct inode *inode = file->f_path.dentry->d_inode;
	int fbidx = iminor(inode);
	struct fb_info *info = registered_fb[fbidx];
	u32 *buffer, *src;
	u32 __iomem *dst;
	int c, i, cnt = 0, err = 0;
	unsigned long total_size;

	if (!info || !info->screen_base)
		return -ENODEV;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;


	total_size = info->screen_size;

	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	buffer = kmalloc((count > PAGE_SIZE) ? PAGE_SIZE : count,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	dst = (u32 __iomem *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	while (count) {
		c = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		src = buffer;

		if (copy_from_user(src, buf, c)) {
			err = -EFAULT;
			break;
		}

		for (i = c >> 2; i--; ){
			fb_writel( (*src& 0xff00ff00>>8)|(*src& 0x00ff00ff<<8), dst++);
			src++;
		}
		if (c & 3) {
			u8 *src8 = (u8 *) src;
			u8 __iomem *dst8 = (u8 __iomem *) dst;

			for (i = c & 3; i--; ){
				if (i&1){
				fb_writeb(*src8++, ++dst8);
				}
				else{
				fb_writeb(*src8++, --dst8);
				dst8 +=2;
				}
			}
			dst = (u32 __iomem *) dst8;
		}

		*ppos += c;
		buf += c;
		cnt += c;
		count -= c;
	}

	kfree(buffer);

	return (cnt) ? cnt : err;
}


/* sm501fb_crtsrc_show
 *
 * device attribute code to show where the crt output is sourced from
*/

static ssize_t sm501fb_crtsrc_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sm501fb_info *info = dev_get_drvdata(dev);
	unsigned long ctrl;

	ctrl = SmRead32(CRT_DISPLAY_CTRL);
	ctrl = FIELD_GET(ctrl, CRT_DISPLAY_CTRL, SELECT);

	return snprintf(buf, PAGE_SIZE, "%s\n", ctrl ? "crt" : "panel");
}

/* sm501fb_crtsrc_show
 *
 * device attribute code to set where the crt output is sourced from
*/

static ssize_t sm501fb_crtsrc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct sm501fb_info *info = dev_get_drvdata(dev);
	enum sm501_controller head;
	unsigned long ctrl;

	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "crt", 3) == 0)
		head = HEAD_CRT;
	else if (strnicmp(buf, "panel", 5) == 0)
		head = HEAD_PANEL;
	else
		return -EINVAL;

	dev_info(dev, "setting crt source to head %d\n", head);

	ctrl = SmRead32(CRT_DISPLAY_CTRL);

	if (head == HEAD_CRT) {

#ifdef CONFIG_FB_SM501_DUAL_HEAD
		ctrl = FIELD_SET(ctrl, CRT_DISPLAY_CTRL, SELECT, CRT)||
#else
		ctrl = FIELD_SET(ctrl, CRT_DISPLAY_CTRL, SELECT, PANEL)||
#endif
			FIELD_SET(ctrl, CRT_DISPLAY_CTRL, PLANE, ENABLE)||
				FIELD_SET(ctrl, CRT_DISPLAY_CTRL, TIMING, ENABLE);
	} else {
		ctrl = FIELD_SET(ctrl, CRT_DISPLAY_CTRL, SELECT, PANEL)||
			FIELD_SET(ctrl, CRT_DISPLAY_CTRL, PLANE, DISABLE)||
				FIELD_SET(ctrl, CRT_DISPLAY_CTRL, TIMING, DISABLE);
	}
	SmWrite32(CRT_DISPLAY_CTRL, ctrl);
	
	sm501fb_sync_regs(info);

	return len;
}

/* Prepare the device_attr for registration with sysfs later */
static DEVICE_ATTR(crt_src, 0666, sm501fb_crtsrc_show, sm501fb_crtsrc_store);

/* sm501fb_show_regs
 *
 * show the primary sm501 registers
*/
static int sm501fb_show_regs(struct sm501fb_info *info, char *ptr,
			     unsigned int start, unsigned int len)
{
	char *buf = ptr;
	unsigned int reg;

	for (reg = start; reg < (len + start); reg += 4)
		ptr += sprintf(ptr, "%08x = %08x\n", reg, SmRead32(reg));

	return ptr - buf;
}

/* sm501fb_debug_show_crt
 *
 * show the crt control and cursor registers
*/

static ssize_t sm501fb_debug_show_crt(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sm501fb_info *info = dev_get_drvdata(dev);
	char *ptr = buf;

	ptr += sm501fb_show_regs(info, ptr, CRT_DISPLAY_CTRL, 0x40);
	ptr += sm501fb_show_regs(info, ptr, CRT_HWC_ADDRESS, 0x10);

	return ptr - buf;
}

static DEVICE_ATTR(fbregs_crt, 0444, sm501fb_debug_show_crt, NULL);

/* sm501fb_debug_show_pnl
 *
 * show the panel control and cursor registers
*/

static ssize_t sm501fb_debug_show_pnl(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sm501fb_info *info = dev_get_drvdata(dev);
	char *ptr = buf;

	ptr += sm501fb_show_regs(info, ptr, PANEL_DISPLAY_CTRL, 0x40);
	ptr += sm501fb_show_regs(info, ptr, PANEL_HWC_ADDRESS, 0x10);

	return ptr - buf;
}

#include "sm2d.c"

void sm501fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
    struct sm501fb_par *p = (struct sm501fb_par*)info->par;

    if (!noaccel)
    {
        if (!area->width || !area->height)
            return;

       deCopy(p->screen.sm_addr , 0, info->var.bits_per_pixel,  
            area->dx, area->dy, area->width, area->height, 
            p->screen.sm_addr, 0, area->sx, area->sy, 0, 0xC);
    }
    else
        cfb_copyarea(info, area);
}

void sm501fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    struct sm501fb_par *p = (struct sm501fb_par*)info->par;
    
    if (!noaccel)
    {
        if (!rect->width || !rect->height)
            return;
  
       deFillRect(p->screen.sm_addr , 0, rect->dx, rect->dy, rect->width, rect->height, rect->color);
    }
    else
        cfb_fillrect(info, rect);
}

void sm501fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
    if (!noaccel)
    {
        if (sm_accel_busy)
            sm501fb_Wait_Idle();
 
        cfb_imageblit(info, image);
    }
    else
        cfb_imageblit(info, image);
}




static DEVICE_ATTR(fbregs_pnl, 0444, sm501fb_debug_show_pnl, NULL);

/* framebuffer ops */

static struct fb_ops sm501fb_ops_crt = {
	.owner		= THIS_MODULE,
	.fb_check_var	= sm501fb_check_var_crt,
	.fb_set_par	= sm501fb_set_par_crt,
	.fb_blank	= sm501fb_blank,
	.fb_setcolreg	= sm501fb_setcolreg,
	.fb_pan_display	= sm501fb_pan_crt,
	.fb_cursor	= sm501fb_cursor,
	.fb_fillrect	= sm501fb_fillrect,
	.fb_copyarea	= sm501fb_copyarea,
	.fb_imageblit	= sm501fb_imageblit,
#ifdef __BIG_ENDIAN
//	.fb_setcmap = sm501fb_set_cmap,
	.fb_write = sm501fb_write,
	.fb_read = sm501fb_read,
#endif
};

static struct fb_ops sm501fb_ops_pnl = {
	.owner		= THIS_MODULE,
	.fb_check_var	= sm501fb_check_var_pnl,
	.fb_set_par	= sm501fb_set_par_pnl,
	.fb_pan_display	= sm501fb_pan_pnl,
	.fb_blank	= sm501fb_blank,
	.fb_setcolreg	= sm501fb_setcolreg,
	.fb_cursor	= sm501fb_cursor,
	.fb_fillrect	= sm501fb_fillrect,
	.fb_copyarea	= sm501fb_copyarea,
	.fb_imageblit	= sm501fb_imageblit,
#ifdef __BIG_ENDIAN
//	.fb_setcmap = sm501fb_set_cmap,
	.fb_write = sm501fb_write,
	.fb_read = sm501fb_read,
#endif
};

/* sm501fb_info_alloc
 *
 * creates and initialises an sm501fb_info structure
*/

static struct sm501fb_info *sm501fb_info_alloc(struct fb_info *fbinfo_crt,
					       struct fb_info *fbinfo_pnl)
{
	struct sm501fb_info *info;
	struct sm501fb_par  *par;

	info = kzalloc(sizeof(struct sm501fb_info), GFP_KERNEL);
	if (info) {
		/* set the references back */

		par = fbinfo_crt->par;
		par->info = info;
		par->head = HEAD_CRT;
		fbinfo_crt->pseudo_palette = &par->pseudo_palette;

		par = fbinfo_pnl->par;
		par->info = info;
		par->head = HEAD_PANEL;
		fbinfo_pnl->pseudo_palette = &par->pseudo_palette;

		/* store the two fbs into our info */
		info->fb[HEAD_CRT] = fbinfo_crt;
		info->fb[HEAD_PANEL] = fbinfo_pnl;
	}

	return info;
}

/* sm501_init_cursor
 *
 * initialise hw cursor parameters
*/

static int sm501_init_cursor(struct fb_info *fbi, unsigned int reg_base)
{
	struct sm501fb_par *par = fbi->par;
	struct sm501fb_info *info = par->info;
	int ret;

	par->cursor_regs = reg_base;

	ret = sm501_alloc_mem(info, &par->cursor, SM501_MEMF_CURSOR, 1024);
	if (ret < 0)
		return ret;

	/* initialise the colour registers */

	SmWrite32(par->cursor_regs + SM501_OFF_HWC_ADDR, par->cursor.sm_addr);

	SmWrite32(par->cursor_regs + SM501_OFF_HWC_LOC, 0x0);
	SmWrite32(par->cursor_regs + SM501_OFF_HWC_COLOR_1_2, 0x0);
	SmWrite32(par->cursor_regs + SM501_OFF_HWC_COLOR_3, 0x0);
	sm501fb_sync_regs(info);

	return 0;
}

/* sm501fb_info_start
 *
 * fills the par structure claiming resources and remapping etc.
*/

static int sm501fb_start(struct sm501fb_info *info,
			 struct platform_device *pdev)
{
	struct resource	*res;
	struct device *dev;
	int ret, value;

	info->dev = dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	info->irq = ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		/* we currently do not use the IRQ */
		dev_warn(dev, "no irq for device\n");
	}

	/* allocate, reserve resources for framebuffer */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res == NULL) {
		dev_err(dev, "no memory resource defined\n");
		ret = -ENXIO;
		goto err_regs_map;
	}

	info->fbmem_res = request_mem_region(res->start,
					     (res->end - res->start)+1,
					     pdev->name);
	
	if (info->fbmem_res == NULL) {
		dev_err(dev, "cannot claim framebuffer\n");
		ret = -ENXIO;
		goto err_regs_map;
	}

	info->fbmem = ioremap(res->start, (res->end - res->start)+1);
	if (info->fbmem == NULL) {
		dev_err(dev, "cannot remap framebuffer\n");
		goto err_mem_res;
	}

	info->fbmem_len = (res->end - res->start)+1;
		
	/* enable display controller */
	value = SmRead32(CURRENT_GATE);
	sm501_set_gate(FIELD_SET(value,POWER_MODE0_GATE, DISPLAY, ENABLE) );

	/* setup cursors */

	sm501_init_cursor(info->fb[HEAD_CRT], CRT_HWC_ADDRESS);
	sm501_init_cursor(info->fb[HEAD_PANEL], PANEL_HWC_ADDRESS);

	return 0; /* everything is setup */

 err_mem_res:
	release_resource(info->fbmem_res);
	kfree(info->fbmem_res);

 err_regs_map:
	iounmap(info->regs);

 err_regs_res:
	release_resource(info->regs_res);
	kfree(info->regs_res);

 err_release:
	return ret;
}

static void sm501fb_stop(struct sm501fb_info *info)
{
	int value;
	/* disable display controller */
	value = SmRead32(CURRENT_GATE);
	sm501_set_gate(FIELD_SET(value,POWER_MODE0_GATE, DISPLAY, DISABLE) );

	iounmap(info->fbmem);
	release_resource(info->fbmem_res);
	kfree(info->fbmem_res);

}

static void sm501fb_info_release(struct sm501fb_info *info)
{
	kfree(info);
}


void sm501fb_cmap_swap(struct fb_cmap *cmap, struct fb_info *info)
{
	int i, start = 0;
	u16 *red, *green, *blue, *transp;
	unsigned int val;

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;
	start = cmap->start;

		for (i = 0; i < cmap->len; i++) {
			val  = chan_to_field(red[i],   &info->var.red);
			val |= chan_to_field(green[i], &info->var.green);
			val |= chan_to_field(blue[i],  &info->var.blue);

			if (transp){
				val |= chan_to_field(transp[i],  &info->var.transp);
			}
			val = (val& 0xff00ff00>>8)|(val& 0x00ff00ff<<8);

			red[i] = field_to_chan(val,&info->var.red);
			green[i] = field_to_chan(val,&info->var.green);
			blue[i] = field_to_chan(val,&info->var.blue);

			if (transp+i)
				transp[i] |= field_to_chan(val,&info->var.transp);
		}

	return ;
}

static int sm501fb_init_fb(struct fb_info *fb,
			   enum sm501_controller head,
			   const char *fbname)
{
	struct sm501_platdata_fbsub *pd;
	struct sm501fb_par *par = fb->par;
	struct sm501fb_info *info = par->info;
	unsigned long ctrl;
	unsigned int enable;
	int ret;


	switch (head) {
	case HEAD_CRT:

		pd = info->pdata->fb_crt;
		ctrl = SmRead32(CRT_DISPLAY_CTRL);
		enable = FIELD_GET(ctrl, CRT_DISPLAY_CTRL, PLANE);
		/* ensure we set the correct source register */
		if (info->pdata->fb_route != SM501_FB_CRT_PANEL) {
#ifdef CONFIG_FB_SM501_DUAL_HEAD
		ctrl = FIELD_SET(ctrl, CRT_DISPLAY_CTRL, SELECT, CRT);
#else
		ctrl = FIELD_SET(ctrl, CRT_DISPLAY_CTRL, SELECT, PANEL/*CRT*/);
#endif
			SmWrite32(CRT_DISPLAY_CTRL, ctrl);
		}
		break;

	case HEAD_PANEL:
		pd = info->pdata->fb_pnl;
		ctrl = SmRead32(PANEL_DISPLAY_CTRL);
		enable =FIELD_GET(ctrl, PANEL_DISPLAY_CTRL, PLANE);

		break;

	default:
		pd = NULL;		/* stop compiler warnings */
		ctrl = 0;
		enable = 0;
		BUG();
	}



	dev_info(info->dev, "fb %s %sabled at start\n",
		 fbname, enable ? "en" : "dis");

	/* check to see if our routing allows this */


	
	if (head == HEAD_CRT && info->pdata->fb_route == SM501_FB_CRT_PANEL) {
		ctrl = FIELD_SET(ctrl, CRT_DISPLAY_CTRL, SELECT, PANEL);
		SmWrite32(CRT_DISPLAY_CTRL, ctrl);
		enable = 0;
	}

	strlcpy(fb->fix.id, fbname, sizeof(fb->fix.id));

	memcpy(&par->ops,
	       (head == HEAD_CRT) ? &sm501fb_ops_crt : &sm501fb_ops_pnl,
	       sizeof(struct fb_ops));
		
	/* update ops dependant on what we've been passed */

	if ((pd->flags & SM501FB_FLAG_USE_HWCURSOR) == 0)
		par->ops.fb_cursor = NULL;
	
	fb->fbops = &par->ops;
	fb->flags = FBINFO_FLAG_DEFAULT |
		FBINFO_HWACCEL_XPAN | FBINFO_HWACCEL_YPAN;

	/* fixed data */

	fb->fix.type		= FB_TYPE_PACKED_PIXELS;
	fb->fix.type_aux	= 0;
	fb->fix.xpanstep	= 1;
	fb->fix.ypanstep	= 1;
	fb->fix.ywrapstep	= 0;
	fb->fix.accel		= FB_ACCEL_NONE;

	/* screenmode */

	if (dcolor)
		fb->var.nonstd = 1;
	fb->var.activate	= FB_ACTIVATE_NOW;
	fb->var.accel_flags	= 0;
	fb->var.vmode		= FB_VMODE_NONINTERLACED;
	fb->var.bits_per_pixel  = 16;

	if (enable && (pd->flags & SM501FB_FLAG_USE_INIT_MODE)&&0) {
/*		fb->var.xres = screen_info.lfb_width;
		fb->var.yres = screen_info.lfb_height;
		fb->var.bits_per_pixel = screen_info.lfb_depth;
		fb->var.xres_virtual = fb->var.xres;
		fb->var.yres_virtual = fb->var.yres;
*/
	} else {
		if (pd->def_mode) {
			dev_info(info->dev, "using supplied mode\n");
			fb_videomode_to_var(&fb->var, pd->def_mode);

			fb->var.bits_per_pixel = pd->def_bpp ? pd->def_bpp : 8;
			fb->var.xres_virtual = fb->var.xres;
			fb->var.yres_virtual = fb->var.yres;
		} else {
			ret = fb_find_mode(&fb->var, fb, mode_option, SM501modedb, ARRAY_SIZE(SM501modedb), NULL, 16);
			if(ret != 1)
			{
			ret = fb_find_mode(&fb->var, fb, mode_option, NULL, 0, NULL, 16);
			}
			if (ret == 0 || ret == 4) {
				dev_err(info->dev,
					"failed to get initial mode\n");
				return -EINVAL;
			}
		}
	}
	
	/* initialise and set the palette */
	if (fb_alloc_cmap(&fb->cmap, NR_PALETTE, 0)) {
		dev_err(info->dev, "failed to allocate cmap memory\n");
		return -ENOMEM;
	}
	fb_set_cmap(&fb->cmap, fb);

	ret = (fb->fbops->fb_check_var)(&fb->var, fb);
	if (ret)
		dev_err(info->dev, "check_var() failed on initial setup?\n");

	/* ensure we've activated our new configuration */
	(fb->fbops->fb_set_par)(fb);

	return 0;
}

/* default platform data if none is supplied (ie, PCI device) */

static struct fb_videomode default_mode = {
	/* 640x480 @ 60 Hz, 31.5 kHz hsync */
	NULL, 60, 640, 480, 39721, 40, 24, 32, 11, 96, 2,
	0, FB_VMODE_NONINTERLACED
};

static struct sm501_platdata_fbsub sm501fb_pdata_crt = {
//	.def_mode	= &default_mode;
	.flags		= (SM501FB_FLAG_USE_INIT_MODE |
			   SM501FB_FLAG_USE_HWCURSOR |
			   SM501FB_FLAG_USE_HWACCEL |
			   SM501FB_FLAG_DISABLE_AT_EXIT),

};

static struct sm501_platdata_fbsub sm501fb_pdata_pnl = {
//	.def_mode	= &default_mode;
	.flags		= (SM501FB_FLAG_USE_INIT_MODE |
			   SM501FB_FLAG_USE_HWCURSOR |
			   SM501FB_FLAG_USE_HWACCEL |
			   SM501FB_FLAG_DISABLE_AT_EXIT),
};

static struct sm501_platdata_fb sm501fb_def_pdata = {
	.fb_route		= SM501_FB_OWN,
	.fb_crt			= &sm501fb_pdata_crt,
	.fb_pnl			= &sm501fb_pdata_pnl,
};

static char driver_name_crt[] = "sm501fb-crt";
static char driver_name_pnl[] = "sm501fb-panel";

static int __init sm501fb_probe(struct platform_device *pdev)
{
	struct sm501fb_info *info;
	struct device	    *dev = &pdev->dev;
	struct fb_info	    *fbinfo_crt;
	struct fb_info	    *fbinfo_pnl;
	int		     ret;

	/* allocate our framebuffers */

	fbinfo_crt = framebuffer_alloc(sizeof(struct sm501fb_par), dev);
	if (fbinfo_crt == NULL) {
		dev_err(dev, "cannot allocate crt framebuffer\n");
		return -ENOMEM;
	}

	fbinfo_pnl = framebuffer_alloc(sizeof(struct sm501fb_par), dev);
	if (fbinfo_pnl == NULL) {
		dev_err(dev, "cannot allocate panel framebuffer\n");
		ret = -ENOMEM;
		goto fbinfo_crt_alloc_fail;
	}

	info = sm501fb_info_alloc(fbinfo_crt, fbinfo_pnl);
	if (info == NULL) {
		dev_err(dev, "cannot allocate par\n");
		ret = -ENOMEM;
		goto sm501fb_alloc_fail;
	}

	if (dev->parent->platform_data) {
		struct sm501_platdata *pd = dev->parent->platform_data;
		info->pdata = pd->fb;
	}

	if (info->pdata == NULL) {
		dev_dbg(dev, "using default configuration data\n");
		info->pdata = &sm501fb_def_pdata;
	}

	if (!nomtrr)
		sm501fb_set_mtrr(info->dev->parent);

	/* start the framebuffers */

	ret = sm501fb_start(info, pdev);
	if (ret) {
		dev_err(dev, "cannot initialise SM501\n");
		goto sm501fb_start_fail;
	}

	/* CRT framebuffer setup */

	ret = sm501fb_init_fb(fbinfo_crt, HEAD_CRT, driver_name_crt);
	if (ret) {
		dev_err(dev, "cannot initialise CRT fb\n");
		goto sm501fb_start_fail;
	}

	/* Panel framebuffer setup */

	ret = sm501fb_init_fb(fbinfo_pnl, HEAD_PANEL, driver_name_pnl);
	if (ret) {
		dev_err(dev, "cannot initialise Panel fb\n");
		goto sm501fb_start_fail;
	}

	/* Init sm501 drawing engine */
	if (!noaccel)
       	deInit(fbinfo_crt->var.xres, fbinfo_crt->var.yres, fbinfo_crt->var.bits_per_pixel); 
	
	/* register framebuffers */


	ret = register_framebuffer(fbinfo_pnl);
	if (ret < 0) {
		dev_err(dev, "failed to register panel fb (%d)\n", ret);
		goto register_pnl_fail;
	}

	ret = register_framebuffer(fbinfo_crt);
	if (ret < 0) {
		dev_err(dev, "failed to register CRT fb (%d)\n", ret);
		goto register_crt_fail;
	}
	

	dev_dbg(dev, "accel: x:%dx%d bit_pixel: %d\n", fbinfo_crt->var.xres, fbinfo_crt->var.yres, fbinfo_crt->var.bits_per_pixel);	
	
	dev_dbg(dev, "fb%d: %s frame buffer device\n",
		 fbinfo_crt->node, fbinfo_crt->fix.id);

	dev_dbg(dev, "fb%d: %s frame buffer device\n",
	       fbinfo_pnl->node, fbinfo_pnl->fix.id);

	/* create device files */
	ret = device_create_file(dev, &dev_attr_crt_src);
	if (ret)
		goto crtsrc_fail;
	ret = device_create_file(dev, &dev_attr_fbregs_pnl);
	if (ret)
		goto fbregs_pnl_fail;
	ret = device_create_file(dev, &dev_attr_fbregs_crt);
	if (ret)
		goto fbregs_crt_fail;

	/* we registered, return ok */
	return 0;

 fbregs_crt_fail:
	device_remove_file(dev, &dev_attr_fbregs_pnl);

 fbregs_pnl_fail:
	device_remove_file(dev, &dev_attr_crt_src);

 crtsrc_fail:
	unregister_framebuffer(fbinfo_pnl);

 register_pnl_fail:
	unregister_framebuffer(fbinfo_crt);

 register_crt_fail:
	sm501fb_stop(info);

 sm501fb_start_fail:
	sm501fb_info_release(info);

 sm501fb_alloc_fail:
	framebuffer_release(fbinfo_pnl);

 fbinfo_crt_alloc_fail:
	framebuffer_release(fbinfo_crt);

	return ret;
}


/*
 *  Cleanup
 */
static int sm501fb_remove(struct platform_device *pdev)
{
	struct sm501fb_info *info = platform_get_drvdata(pdev);
	struct fb_info	   *fbinfo_crt = info->fb[0];
	struct fb_info	   *fbinfo_pnl = info->fb[1];

	device_remove_file(&pdev->dev, &dev_attr_fbregs_crt);
	device_remove_file(&pdev->dev, &dev_attr_fbregs_pnl);
	device_remove_file(&pdev->dev, &dev_attr_crt_src);

	unregister_framebuffer(fbinfo_pnl);
	unregister_framebuffer(fbinfo_crt);
	if (!nomtrr)
		sm501fb_unset_mtrr(info->dev->parent);
	sm501fb_stop(info);
	sm501fb_info_release(info);

	framebuffer_release(fbinfo_pnl);
	framebuffer_release(fbinfo_crt);

	return 0;
}

#ifdef CONFIG_PM

static int sm501fb_suspend_fb(struct sm501fb_info *info,
			      enum sm501_controller head)
{
	struct fb_info *fbi = info->fb[head];
	struct sm501fb_par *par = fbi->par;

	if (par->screen.size == 0)
		return 0;

	/* blank the relevant interface to ensure unit power minimised */
	(par->ops.fb_blank)(FB_BLANK_POWERDOWN, fbi);

	/* tell console/fb driver we are suspending */

	acquire_console_sem();
	fb_set_suspend(fbi, 1);
	release_console_sem();
	/* backup copies in case chip is powered down over suspend */

	par->store_fb = vmalloc(par->screen.size);
	if (par->store_fb == NULL) {
		dev_err(info->dev, "no memory to store screen\n");
		return -ENOMEM;
	}

	par->store_cursor = vmalloc(par->cursor.size);
	if (par->store_cursor == NULL) {
		dev_err(info->dev, "no memory to store cursor\n");
		goto err_nocursor;
	}

	memcpy_fromio(par->store_fb, par->screen.k_addr, par->screen.size);
	memcpy_fromio(par->store_cursor, par->cursor.k_addr, par->cursor.size);

	return 0;

 err_nocursor:
	vfree(par->store_fb);
	par->store_fb = NULL;

	return -ENOMEM;
}

static void sm501fb_resume_fb(struct sm501fb_info *info,
			      enum sm501_controller head)
{
	struct fb_info *fbi = info->fb[head];
	struct sm501fb_par *par = fbi->par;

	if (par->screen.size == 0)
		return;

	/* re-activate the configuration */

	(par->ops.fb_set_par)(fbi);

	/* restore the data */

	dev_dbg(info->dev, "restoring screen from %p\n", par->store_fb);
	dev_dbg(info->dev, "restoring cursor from %p\n", par->store_cursor);

	if (par->store_fb)
		memcpy_toio(par->screen.k_addr, par->store_fb,
			    par->screen.size);

	if (par->store_cursor)
		memcpy_toio(par->cursor.k_addr, par->store_cursor,
			    par->cursor.size);

	acquire_console_sem();
	fb_set_suspend(fbi, 0);
	release_console_sem();

	vfree(par->store_fb);
	vfree(par->store_cursor);
}


/* suspend and resume support */

static int sm501fb_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sm501fb_info *info = platform_get_drvdata(pdev);

	sm501fb_suspend_fb(info, HEAD_CRT);
	sm501fb_suspend_fb(info, HEAD_PANEL);

	/* turn off the clocks, in case the device is not powered down */

	int value = SmRead32(CURRENT_GATE);
	sm501_set_gate(FIELD_SET(value,POWER_MODE0_GATE, DISPLAY,DISABLE)&
		FIELD_SET(value, POWER_MODE0_GATE, 2D, DISABLE)&
		FIELD_SET(value, POWER_MODE0_GATE, CSC, DISABLE) );

	return 0;
}

static int sm501fb_resume(struct platform_device *pdev)
{
	struct sm501fb_info *info = platform_get_drvdata(pdev);

	int value = SmRead32(CURRENT_GATE);
	sm501_set_gate(FIELD_SET(value, POWER_MODE0_GATE, DISPLAY, ENABLE)|
		FIELD_SET(value, POWER_MODE0_GATE, 2D, ENABLE)|
		FIELD_SET(value, POWER_MODE0_GATE, CSC, ENABLE) );
	
	sm501fb_resume_fb(info, HEAD_CRT);
	sm501fb_resume_fb(info, HEAD_PANEL);

	return 0;
}

#else
#define sm501fb_suspend NULL
#define sm501fb_resume  NULL
#endif

static struct platform_driver sm501fb_driver = {
	.probe		= sm501fb_probe,
	.remove		= sm501fb_remove,
	.suspend		= sm501fb_suspend,
	.resume		= sm501fb_resume,
	.driver		= {
		.name	= "sm501-fb",
		.owner	= THIS_MODULE,
	},
};

/*
 * Interface to the world
 */
static int  __devinit sm501fb_setup(char *options)
{
	char *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt) continue;

		smdbg("option %s\n", this_opt);

		if (!strncmp(this_opt,"noaccel",7))
			noaccel = 1;
		else	if (!strncmp(this_opt,"dcolor",6))
			dcolor = 1;
		else
			mode_option = this_opt;
	}
	return 0;
}


static int __devinit sm501fb_init(void)
{
	char *option = NULL;

	if (fb_get_options("sm501fb", &option))
		return -ENODEV;
	sm501fb_setup(option);
	return platform_driver_register(&sm501fb_driver);
}

static void __exit sm501fb_cleanup(void)
{
	platform_driver_unregister(&sm501fb_driver);
}

module_init(sm501fb_init);
module_exit(sm501fb_cleanup);

module_param(mode_option, charp, 0);
MODULE_PARM_DESC(mode_option, "Initial mode (default=" DEFAULT_MODE ")");
module_param(noaccel, bool, 0);
MODULE_PARM_DESC(noaccel, "bool: Disable acceleration support (0 or 1=disabled) (default=0)");
module_param(mirror, bool, 0);
MODULE_PARM_DESC(mirror, "bool: Mirror the display to both monitors");
module_param(dcolor, bool, 0);
MODULE_PARM_DESC(dcolor, "bool: Enable Direct color.");

module_param(nomtrr, bool, 1);
MODULE_PARM_DESC(nomtrr, "bool: disable use of MTRR registers");

MODULE_AUTHOR("Boyod Yang, Ben Dooks, Vincent Sanders");
MODULE_DESCRIPTION("SM50x Framebuffer driver");
MODULE_LICENSE("GPL v2");
