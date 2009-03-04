/*
 *  linux/drivers/video/smtcfb.c -- Silicon Motion SM501 and SM7xx frame buffer device
 *
 *      Copyright (C) 2006 Silicon Motion Technology Corp.
 *      Ge Wang, gewang@siliconmotion.com
 *      Boyod boyod.yang@siliconmotion.com.cn
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
*
*
* Version 0.10.26192.21.01
    - Add PowerPC/Big endian support
	- Add 2D support for Lynx
    - Verified on 2.6.19.2                          Boyod.yang  <boyod.yang@siliconmotion.com.cn>

* Version 0.09.2621.00.01
    - Only support Linux Kernel's version 2.6.21.	Boyod.yang  <boyod.yang@siliconmotion.com.cn>

* Version 0.09
    - Only support Linux Kernel's version 2.6.12.	Boyod.yang  <boyod.yang@siliconmotion.com.cn>
    
*/

#ifndef __KERNEL__
#define __KERNEL__
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
#include <linux/pci.h>
#include <linux/init.h>

#include "smtcfb.h"
#include "smtc2d.h"

#ifdef DEBUG
#define smdbg(format, arg...)	printk(KERN_DEBUG format , ## arg)
#else
#define smdbg(format, arg...)
#endif

#define DEFAULT_VIDEO_MODE "800x600-16@60"
#ifdef __BIG_ENDIAN
struct screen_info screen_info;
#endif



/*
 * globals
 */
 
static char *mode_option __devinitdata = DEFAULT_VIDEO_MODE;

/*
* Private structure
*/
struct smtcfb_info {
        /*
        * The following is a pointer to be passed into the
        * functions below.  The modules outside the main
        * voyager.c driver have no knowledge as to what
        * is within this structure.
        */
        struct fb_info          fb;
        struct display_switch   *dispsw;
        struct pci_dev	        *dev;
        signed int              currcon;

        struct {
                u8 red, green, blue;
        } palette[NR_RGB];

        u_int                   palette_size;
};

struct par_info {
	/*
	 * Hardware
	 */
	u16	chipID;
	unsigned char   __iomem *m_pMMIO;
       char            __iomem *m_pLFB;
	char	*m_pDPR;
	char	*m_pVPR;
	char	*m_pCPR;

	u_int	width;
	u_int	height;
	u_int	hz;
    u_long  BaseAddressInVRAM;
	u8	chipRevID;
};

struct vesa_mode_table	{
	char mode_index[6];
	u16 lfb_width;	
	u16 lfb_height;		
	u16 lfb_depth;	
};


static struct vesa_mode_table vesa_mode[] = 
{
	{"0x301", 640,  480,  8},
	{"0x303", 800,  600,  8},
	{"0x305", 1024, 768,	8},
	{"0x307", 1280, 1024, 8},
	
	{"0x311", 640,  480,  16},
	{"0x314", 800,  600,  16},
	{"0x317", 1024, 768,	16},
	{"0x31A", 1280, 1024, 16},
	
	{"0x312", 640,  480,  24},
	{"0x315", 800,  600,  24},
	{"0x318", 1024, 768,	24},
	{"0x31B", 1280, 1024, 24},	
	
};

char __iomem *smtc_RegBaseAddress;	// Memory Map IO starting address
char __iomem *smtc_VRAMBaseAddress;	// video memory starting address


char *smtc_2DBaseAddress;	// 2D engine starting address
char *smtc_2Ddataport   ;	// 2D data port offset
short smtc_2Dacceleration = 0;  //default no 2D acceleration

static u32 colreg[17];
static struct par_info hw;	// hardware information

#if defined(CONFIG_FB_SM7XX_DUALHEAD)

static u32 colreg2[17];
static struct par_info hw2;	// hardware information for second display (CRT)
struct smtcfb_info smtcfb_info2; //fb_info for second display (CRT)

#endif //CONFIG_FB_SM501_DUALHEAD

u16 smtc_ChipIDs[] = {
    0x710,
    0x712,
    0x720
};

int sm712be_flag;

int numSMTCchipIDs = sizeof(smtc_ChipIDs)/sizeof(u16);

void deWaitForNotBusy(void)
{
	unsigned long i = 0x1000000;
	while (i--)
	{
        if ((smtc_seqr(0x16) & 0x18) == 0x10)
            break;
	}
    	smtc_de_busy = 0;
}


static void sm712_set_timing(struct smtcfb_info *sfb,struct par_info *ppar_info)
{
		int i=0,j=0;
    u32 m_nScreenStride;
    
		smdbg("\nppar_info->width = %d ppar_info->height = %d sfb->fb.var.bits_per_pixel = %d ppar_info->hz = %d\n", 
		ppar_info->width, ppar_info->height, sfb->fb.var.bits_per_pixel , ppar_info->hz);
		
    for (j=0;j < numVGAModes;j++) {
		if (VGAMode[j].mmSizeX == ppar_info->width &&
			VGAMode[j].mmSizeY == ppar_info->height &&
			VGAMode[j].bpp == sfb->fb.var.bits_per_pixel &&
			VGAMode[j].hz == ppar_info->hz)
		{
			smdbg("\nVGAMode[j].mmSizeX  = %d VGAMode[j].mmSizeY = %d VGAMode[j].bpp = %d VGAMode[j].hz=%d\n", 
				VGAMode[j].mmSizeX , VGAMode[j].mmSizeY, VGAMode[j].bpp, VGAMode[j].hz);			
		  smdbg("VGAMode index=%d\n",j);
			 
			smtc_mmiowb(0x0,0x3c6);

			smtc_seqw(0,0x1);

			smtc_mmiowb(VGAMode[j].Init_MISC,0x3c2);

			for (i=0;i<SIZE_SR00_SR04;i++)	// init SEQ register SR00 - SR04
			{
				smtc_seqw(i,VGAMode[j].Init_SR00_SR04[i]);
			}

			for (i=0;i<SIZE_SR10_SR24;i++)	// init SEQ register SR10 - SR24
			{
				smtc_seqw(i+0x10,VGAMode[j].Init_SR10_SR24[i]);
			}

			for (i=0;i<SIZE_SR30_SR75;i++)	// init SEQ register SR30 - SR75
			{
				if (((i+0x30) != 0x62) && ((i+0x30) != 0x6a) && ((i+0x30) != 0x6b))
					smtc_seqw(i+0x30,VGAMode[j].Init_SR30_SR75[i]);
			}
			for (i=0;i<SIZE_SR80_SR93;i++)	// init SEQ register SR80 - SR93
			{
				smtc_seqw(i+0x80,VGAMode[j].Init_SR80_SR93[i]);
			}
			for (i=0;i<SIZE_SRA0_SRAF;i++)	// init SEQ register SRA0 - SRAF
			{
				smtc_seqw(i+0xa0,VGAMode[j].Init_SRA0_SRAF[i]);
			}

			for (i=0;i<SIZE_GR00_GR08;i++)	// init Graphic register GR00 - GR08
			{
				smtc_grphw(i,VGAMode[j].Init_GR00_GR08[i]);
			}

			for (i=0;i<SIZE_AR00_AR14;i++)	// init Attribute register AR00 - AR14
			{

				smtc_attrw(i,VGAMode[j].Init_AR00_AR14[i]);
			}

			for (i=0;i<SIZE_CR00_CR18;i++)	// init CRTC register CR00 - CR18
			{
				smtc_crtcw(i,VGAMode[j].Init_CR00_CR18[i]);
			}

			for (i=0;i<SIZE_CR30_CR4D;i++)	// init CRTC register CR30 - CR4D
			{
				smtc_crtcw(i+0x30,VGAMode[j].Init_CR30_CR4D[i]);
			}

			for (i=0;i<SIZE_CR90_CRA7;i++)	// init CRTC register CR90 - CRA7
			{
				smtc_crtcw(i+0x90,VGAMode[j].Init_CR90_CRA7[i]);
			}
			
		}
	}
	smtc_mmiowb(0x67,0x3c2);
	
	// set VPR registers
	writel(0x0,ppar_info->m_pVPR+0x0C);
	writel(0x0,ppar_info->m_pVPR+0x40);
	
	// set data width
	m_nScreenStride = (ppar_info->width * sfb->fb.var.bits_per_pixel) / 64;
	switch (sfb->fb.var.bits_per_pixel)
	{
		case 8:
			writel(0x0,ppar_info->m_pVPR+0x0);
			break;
		case 16:
			writel(0x00020000,ppar_info->m_pVPR+0x0);
			break;
		case 24:
			writel(0x00040000,ppar_info->m_pVPR+0x0);
            break;
    case 32:
      writel(0x00030000,ppar_info->m_pVPR+0x0);
			break;
	}
	writel((u32)(((m_nScreenStride + 2) << 16) | m_nScreenStride),ppar_info->m_pVPR+0x10);

}


static void sm712_setpalette(int regno, unsigned red, unsigned green, unsigned blue, struct fb_info *info)
{
    struct par_info *cur_par = (struct par_info*)info->par;
    
    if (cur_par->BaseAddressInVRAM)
        smtc_seqw(0x66,(smtc_seqr(0x66) & 0xC3) | 0x20);//second display palette for dual head. Enable CRT RAM, 6-bit RAM
    else
        smtc_seqw(0x66,(smtc_seqr(0x66) & 0xC3) | 0x10); //primary display palette. Enable LCD RAM only, 6-bit RAM
    smtc_mmiowb(regno,       dac_reg);
    smtc_mmiowb(red   >> 10, dac_val);
    smtc_mmiowb(green >> 10, dac_val);
    smtc_mmiowb(blue  >> 10, dac_val);
}


static void smtc_set_timing(struct smtcfb_info *sfb,struct par_info *ppar_info)
{
    switch (ppar_info->chipID) 
    {
     case 0x710:
    case 0x712:
    case 0x720:
        sm712_set_timing(sfb,ppar_info);
        break;
    }
}

static struct fb_var_screeninfo smtcfb_var = {
	.xres 		= 1024,
	.yres 		= 600,
	.xres_virtual 	= 1024,
	.yres_virtual 	= 600,
	.bits_per_pixel = 16,
        .red            = { 16, 8, 0 },
        .green          = {  8, 8, 0 },
        .blue           = {  0, 8, 0 },
	.activate	= FB_ACTIVATE_NOW,
	.height		= -1,
	.width		= -1,
	.vmode		= FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo smtcfb_fix = {
	.id 		= "sm712fb",
	.type 		= FB_TYPE_PACKED_PIXELS,
	.visual 	= FB_VISUAL_TRUECOLOR,
	.line_length 	= 800*3,
	.accel 		= FB_ACCEL_SMI_LYNX,
};


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

static int smtc_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,  unsigned trans, struct fb_info *info)
{
	struct smtcfb_info *sfb = (struct smtcfb_info *)info;
	u32 *pal,val;

	if (regno > 255)
		return 1;

	switch (sfb->fb.fix.visual) {
	case FB_VISUAL_DIRECTCOLOR:
	case FB_VISUAL_TRUECOLOR:
		/* 16/32 bit true-colour, use pseuo-palette for 16 base color*/
		if (regno < 16) {
			if (sfb->fb.var.bits_per_pixel==16)	{
				u32 *pal = sfb->fb.pseudo_palette;	
				val  = chan_to_field(red,   &sfb->fb.var.red);
				val |= chan_to_field(green, &sfb->fb.var.green);
				val |= chan_to_field(blue,  &sfb->fb.var.blue);
#ifdef __BIG_ENDIAN
				pal[regno] =( (red & 0xf800) >> 8) | ((green & 0xe000) >> 13) |((green & 0x1c00) << 3) | ((blue & 0xf800) >> 3);
#else
				pal[regno] = val;
#endif
			}
			else{
				u32 *pal = sfb->fb.pseudo_palette;	
				val  = chan_to_field(red,   &sfb->fb.var.red);
				val |= chan_to_field(green, &sfb->fb.var.green);
				val |= chan_to_field(blue,  &sfb->fb.var.blue);
#ifdef __BIG_ENDIAN
				val = (val& 0xff00ff00>>8)|(val& 0x00ff00ff<<8);
#endif
				pal[regno] = val;
			}
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		/* color depth 8 bit*/
			sm712_setpalette(regno,red,green,blue, info);
		break;

	default:
		return 1;   /* unknown type */
	}

	return 0;

        
}


static ssize_t
smtcfb_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;

//	struct inode *inode = file->f_path.dentry->d_inode;
        struct inode *inode = file->f_dentry->d_inode;
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
smtcfb_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
//	struct inode *inode = file->f_path.dentry->d_inode;
	struct inode *inode = file->f_dentry->d_inode;
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



#include "smtc2d.c"


void smtcfb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
   struct par_info *p = (struct par_info*)info->par;

    if (smtc_2Dacceleration)
    {
        if (!area->width || !area->height)
            return;

        deCopy(p->BaseAddressInVRAM, 0, info->var.bits_per_pixel,  
            area->dx, area->dy, area->width, area->height, 
            p->BaseAddressInVRAM, 0, area->sx, area->sy, 0, 0xC);

    }
    else

        cfb_copyarea(info, area);
}

void smtcfb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    struct par_info *p = (struct par_info*)info->par;
    
    if (smtc_2Dacceleration)
    {
        if (!rect->width || !rect->height)
            return;
	if (info->var.bits_per_pixel>=24)
        deFillRect(p->BaseAddressInVRAM, 0, rect->dx*3, rect->dy*3, rect->width*3, rect->height, rect->color);
	else
        deFillRect(p->BaseAddressInVRAM, 0, rect->dx, rect->dy, rect->width, rect->height, rect->color);
    }
    else

        cfb_fillrect(info, rect);
}

void smtcfb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	    struct par_info *p = (struct par_info*)info->par;
		u32 size, bg_col = 0, fg_col = 0;
    if (smtc_2Dacceleration)
    {
        	if (image->depth == 1){
		        if (smtc_de_busy)
       		     deWaitForNotBusy();

				switch (info->var.bits_per_pixel) {
				case 8:
					bg_col = image->bg_color;
					fg_col = image->fg_color;
					break;
				case 16:
					bg_col = ((u32 *) (info->pseudo_palette))[image->bg_color];
					fg_col = ((u32 *) (info->pseudo_palette))[image->fg_color];
					break;
				case 32:
					bg_col = ((u32 *) (info->pseudo_palette))[image->bg_color];
					fg_col = ((u32 *) (info->pseudo_palette))[image->fg_color];
					break;
					}
			deSystemMem2VideoMemMonoBlt(
				image->data, /* pointer to start of source buffer in system memory */
				image->width/8,          /* Pitch value (in bytes) of the source buffer, +ive means top down and -ive mean button up */
				0, /* Mono data can start at any bit in a byte, this value should be 0 to 7 */
				p->BaseAddressInVRAM,    /* Address of destination: offset in frame buffer */
				0,   /* Pitch value of destination surface in BYTE */
				0,      /* Color depth of destination surface */
				image->dx,
				image->dy,       /* Starting coordinate of destination surface */
				image->width, 
				image->height,   /* width and height of rectange in pixel value */
				fg_col,   /* Foreground color (corresponding to a 1 in the monochrome data */
				bg_col,   /* Background color (corresponding to a 0 in the monochrome data */
				0x0C)     /* ROP value */;
        	}
		else
       		 cfb_imageblit(info, image);
    }
    else
        cfb_imageblit(info, image);
}


static struct fb_ops smtcfb_ops = {
	.owner        =	THIS_MODULE,
		.fb_setcolreg = smtc_setcolreg,
		.fb_fillrect  = smtcfb_fillrect,
		.fb_imageblit = smtcfb_imageblit,
		.fb_copyarea  = smtcfb_copyarea,
#ifdef __BIG_ENDIAN	
		.fb_read = smtcfb_read,
		.fb_write = smtcfb_write,
#endif

};


void smtcfb_setmode(struct smtcfb_info *sfb)
{
	switch (sfb->fb.var.bits_per_pixel) {
	//kylin
	case 32:
		sfb->fb.fix.visual              = FB_VISUAL_TRUECOLOR;
		sfb->fb.fix.line_length= sfb->fb.var.xres * 4;
		sfb->fb.var.red.length  = 8;
		sfb->fb.var.green.length = 8;
		sfb->fb.var.blue.length = 8;
		sfb->fb.var.red.offset  = 16;
                sfb->fb.var.green.offset= 8;
                sfb->fb.var.blue.offset = 0;
		
		break;
	case 8:
		sfb->fb.fix.visual		= FB_VISUAL_PSEUDOCOLOR;
		sfb->fb.fix.line_length= sfb->fb.var.xres ;
		sfb->fb.var.red.offset	= 5;
		sfb->fb.var.red.length	= 3;
		sfb->fb.var.green.offset= 2;
		sfb->fb.var.green.length= 3;
		sfb->fb.var.blue.offset	= 0;
		sfb->fb.var.blue.length	= 2;
		break;
	case 24:
		sfb->fb.fix.visual		= FB_VISUAL_TRUECOLOR;
		sfb->fb.fix.line_length= sfb->fb.var.xres * 3;
		sfb->fb.var.red.length	= 8;
		sfb->fb.var.green.length=8;
		sfb->fb.var.blue.length	= 8;
		

		sfb->fb.var.red.offset	= 16;
		sfb->fb.var.green.offset= 8;
		sfb->fb.var.blue.offset	= 0;

		break;
	case 16:
	default:
		sfb->fb.fix.visual		= FB_VISUAL_TRUECOLOR;
		sfb->fb.fix.line_length= sfb->fb.var.xres * 2;

		sfb->fb.var.red.length	= 5;
		sfb->fb.var.green.length= 6;
		sfb->fb.var.blue.length	= 5;

		sfb->fb.var.red.offset	= 11;
		sfb->fb.var.green.offset= 5;
		sfb->fb.var.blue.offset	= 0;

		break;
	}

    hw.width = sfb->fb.var.xres;
    hw.height = sfb->fb.var.yres;
    hw.hz = 60;
    smtc_set_timing(sfb, &hw);
    if (smtc_2Dacceleration)
    {
    	printk("2D acceleration enabled!\n");
       deInit(sfb->fb.var.xres, sfb->fb.var.yres, sfb->fb.var.bits_per_pixel); /* Init smtc drawing engine */
    }
}


#if defined(CONFIG_FB_SM7XX_DUALHEAD)
void smtc_head2_init(struct smtcfb_info *sfb)
{
    smtcfb_info2 = *sfb;
    smtcfb_info2.fb.pseudo_palette = &colreg2;
    smtcfb_info2.fb.par = &hw2;
    sprintf(smtcfb_info2.fb.fix.id, "sm%Xfb2", hw.chipID);
    hw2.chipID = hw.chipID;
    hw2.chipRevID = hw.chipRevID;
    hw2.width = smtcfb_info2.fb.var.xres;
    hw2.height = smtcfb_info2.fb.var.yres;
    hw2.hz = 60;
    hw2.m_pMMIO = smtc_RegBaseAddress;
    hw2.BaseAddressInVRAM = smtcfb_info2.fb.fix.smem_len/2; /*hard code 2nd head starting from half VRAM size postion */
    smtcfb_info2.fb.screen_base = hw2.m_pLFB = smtc_VRAMBaseAddress+hw2.BaseAddressInVRAM;

//    sm712crtSetMode(hw2.width, hw2.height, 0, hw2.hz, smtcfb_info2.fb.var.bits_per_pixel); 
	writel(hw2.BaseAddressInVRAM >> 3,hw2.m_pVPR+0x10);
}
#endif


/*
 * Alloc struct smtcfb_info and assign the default value
 */
static struct smtcfb_info * __devinit smtc_alloc_fb_info(struct pci_dev *dev, char *name)
{
    struct smtcfb_info *sfb;

    sfb = kmalloc(sizeof(struct smtcfb_info), GFP_KERNEL);

    if (!sfb)
        return NULL;

    memset(sfb, 0, sizeof(struct smtcfb_info));

    sfb->currcon        = -1;
    sfb->dev            = dev;

	/*** Init sfb->fb with default value ***/
	sfb->fb.flags = FBINFO_FLAG_DEFAULT;
	sfb->fb.fbops = &smtcfb_ops;
	sfb->fb.var = smtcfb_var;
	sfb->fb.fix = smtcfb_fix;
    
	strcpy(sfb->fb.fix.id, name);

    sfb->fb.fix.type		= FB_TYPE_PACKED_PIXELS;
    sfb->fb.fix.type_aux	= 0;
    sfb->fb.fix.xpanstep	= 0;
    sfb->fb.fix.ypanstep	= 0;
    sfb->fb.fix.ywrapstep	= 0;
    sfb->fb.fix.accel		= FB_ACCEL_SMI_LYNX;

    sfb->fb.var.nonstd		= 0;
    sfb->fb.var.activate	= FB_ACTIVATE_NOW;
    sfb->fb.var.height		= -1;
    sfb->fb.var.width		= -1;
    sfb->fb.var.accel_flags	= FB_ACCELF_TEXT; /* text mode acceleration */
    sfb->fb.var.vmode		= FB_VMODE_NONINTERLACED;
    sfb->fb.par		        = &hw;
    sfb->fb.pseudo_palette = colreg;
	
    return sfb;
}

/*
 * Unmap in the memory mapped IO registers
 *
 */

static void __devinit smtc_unmap_mmio(struct smtcfb_info *sfb)
{
    if (sfb && smtc_RegBaseAddress)
    {
        smtc_RegBaseAddress = NULL;
    }
}

/*
 * Map in the screen memory
 *
 */
static int __devinit smtc_map_smem(struct smtcfb_info *sfb, struct pci_dev *dev, u_long smem_len)
{
	if(sfb->fb.var.bits_per_pixel == 32)
    {
        #ifdef __BIG_ENDIAN    
        sfb->fb.fix.smem_start = pci_resource_start(dev, 0) + 0x800000;
        #else
        sfb->fb.fix.smem_start = pci_resource_start(dev, 0);
        #endif
    }
    else
    {
        sfb->fb.fix.smem_start = pci_resource_start(dev, 0);
    }

    sfb->fb.fix.smem_len  = smem_len;

    sfb->fb.screen_base = smtc_VRAMBaseAddress;

    if (!sfb->fb.screen_base)
    {
        printk("%s: unable to map screen memory\n",sfb->fb.fix.id);
        return -ENOMEM;
    }

    return 0;
}


/*
 * Unmap in the screen memory
 *
 */
static void __devinit smtc_unmap_smem(struct smtcfb_info *sfb)
{
    if (sfb && sfb->fb.screen_base)
    {
        iounmap(sfb->fb.screen_base);
        sfb->fb.screen_base = NULL;
    }
}

/*
 * We need to wake up the LynxEM+, and make sure its in linear memory mode.
 */
static inline void __devinit sm7xx_init_hw(void)
{
		outb_p(0x18, 0x3c4);
		outb_p(0x11, 0x3c5);
}




static void __devinit smtc_free_fb_info(struct smtcfb_info *sfb)
{
	if (sfb) {
		fb_alloc_cmap(&sfb->fb.cmap, 0, 0);
		kfree(sfb);
	}
}

static int __init smtcfb_init(void)
{
	struct smtcfb_info *sfb;
	u_long smem_size= 0x00800000; //default 8MB
	char name[16];
	int err;
	unsigned long pFramebufferPhysical;
	unsigned long pRegPhysical=0;
	struct pci_dev *pdev = NULL;



	printk("Silicon Motion display driver " SMTC_LINUX_FB_VERSION "\n");

	int i = 0;
    
	do {
		pdev = pci_find_device(0x126f,smtc_ChipIDs[i], pdev);
		if (pdev == NULL)
		{
			i++;
		}
		else
		{
			hw.chipID = smtc_ChipIDs[i];
			break;
		}
	} while (i< numSMTCchipIDs);

	err = pci_enable_device(pdev); // enable SMTC chip

	if (err) 
	{
		return err;
	}
	err = -ENOMEM;

	sprintf(name, "sm%Xfb", hw.chipID);

	sfb = smtc_alloc_fb_info(pdev, name);

	if (!sfb) 
	{
		goto failed;
	}

	sm7xx_init_hw();

/*get mode parameter from screen_info*/
	if(screen_info.lfb_width != 0)
	{
		sfb->fb.var.xres = screen_info.lfb_width;
		sfb->fb.var.yres = screen_info.lfb_height;
		sfb->fb.var.bits_per_pixel = screen_info.lfb_depth;
	}
	else
	{
		sfb->fb.var.xres = SCREEN_X_RES;	// default resolution 1024x600 16bit mode
		sfb->fb.var.yres = SCREEN_Y_RES;
		sfb->fb.var.bits_per_pixel = SCREEN_BPP;
	}	

	
	smdbg("\nsfb->fb.var.bits_per_pixel = %d sm712be_flag = %d\n", sfb->fb.var.bits_per_pixel, sm712be_flag);	
#ifdef __BIG_ENDIAN	
	if(sm712be_flag == 1 && sfb->fb.var.bits_per_pixel == 24)
	{
		sfb->fb.var.bits_per_pixel = screen_info.lfb_depth =32;	
	}
#endif
	// Map address and memory detection
	pFramebufferPhysical = pci_resource_start(pdev,0);
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw.chipRevID);
   
    switch (hw.chipID) 
    {

    case 0x710:
    case 0x712:
        sfb->fb.fix.mmio_start = pFramebufferPhysical + 0x00400000;
        sfb->fb.fix.mmio_len   = 0x00400000;
        smem_size = SM712_VIDEOMEMORYSIZE;        
#ifdef __BIG_ENDIAN
//        hw.m_pLFB = smtc_VRAMBaseAddress = ioremap(pFramebufferPhysical, 0x00a00000);
        hw.m_pLFB = smtc_VRAMBaseAddress = ioremap(pFramebufferPhysical, 0x00c00000);       
#else
        hw.m_pLFB = smtc_VRAMBaseAddress = ioremap(pFramebufferPhysical, 0x00800000);
#endif
        hw.m_pMMIO = smtc_RegBaseAddress = smtc_VRAMBaseAddress + 0x00700000;
        smtc_2DBaseAddress = hw.m_pDPR = smtc_VRAMBaseAddress + 0x00408000;
        smtc_2Ddataport = smtc_VRAMBaseAddress + DE_DATA_PORT_712;
        hw.m_pVPR = hw.m_pLFB + 0x0040c000;
	if(sfb->fb.var.bits_per_pixel == 32)
        {
#ifdef __BIG_ENDIAN
        smtc_VRAMBaseAddress += 0x800000;
        hw.m_pLFB += 0x800000;
				printk("\nsmtc_VRAMBaseAddress=0x%X hw.m_pLFB=0x%X\n", smtc_VRAMBaseAddress, hw.m_pLFB);
#endif
        }
        if (!smtc_RegBaseAddress)
        {
            printk("%s: unable to map memory mapped IO\n",sfb->fb.fix.id);
            return -ENOMEM;
        }
	
/*        
        smtc_seqw(0x62,0x7A);
        smtc_seqw(0x6a,0x0c);
        smtc_seqw(0x6b,0x02);
 
        //LynxEM+ memory detection
        *(u32 *)(smtc_VRAMBaseAddress + 4) = 0xAA551133;
        if (*(u32 *)(smtc_VRAMBaseAddress + 4) != 0xAA551133)
        {

            smem_size = 0x00200000;
            // Program the MCLK to 130 MHz
            smtc_seqw(0x6a,0x12);
            smtc_seqw(0x6b,0x02);
            smtc_seqw(0x62,0x3e);
        }
*/
	smtc_seqw(0x6a,0x16); //set MCLK = 14.31818 *  (0x16 / 0x2)
	smtc_seqw(0x6b,0x02);
	smtc_seqw(0x62,0x3e);
	smtc_seqw(0x17,0x20); //enable PCI burst
	//enabel word swap
	if(sfb->fb.var.bits_per_pixel == 32)
	{
#ifdef __BIG_ENDIAN
		smtc_seqw(0x17,0x30);
#endif
	}

#ifdef CONFIG_FB_SM7XX_ACCEL			
        smtc_2Dacceleration = 1;
#endif

        break;

    case 0x720:
        sfb->fb.fix.mmio_start = pFramebufferPhysical;
        sfb->fb.fix.mmio_len   = 0x00200000;
        smem_size = SM722_VIDEOMEMORYSIZE;        
        smtc_2DBaseAddress = hw.m_pDPR = ioremap(pFramebufferPhysical, 0x00a00000);
        hw.m_pLFB = smtc_VRAMBaseAddress = smtc_2DBaseAddress + 0x00200000;
        hw.m_pMMIO = smtc_RegBaseAddress = smtc_2DBaseAddress + 0x000c0000;
        smtc_2Ddataport = smtc_2DBaseAddress + DE_DATA_PORT_722;
        hw.m_pVPR = smtc_2DBaseAddress + 0x800;
        
        smtc_seqw(0x62,0xff);
        smtc_seqw(0x6a,0x0d);
        smtc_seqw(0x6b,0x02);
        smtc_2Dacceleration = 0;
        break;
	default:
		printk("No valid Silicon Motion display chip was detected!\n");
		smtc_free_fb_info(sfb);
		return err;
    }


	//can support 32 bpp
	if (15 == sfb->fb.var.bits_per_pixel)
		sfb->fb.var.bits_per_pixel = 16;
	//else if (32==sfb->fb.var.bits_per_pixel)
	//	sfb->fb.var.bits_per_pixel = 24; 

   	sfb->fb.var.xres_virtual = sfb->fb.var.xres;

	sfb->fb.var.yres_virtual = sfb->fb.var.yres;
	err = smtc_map_smem(sfb, pdev, smem_size);
	if (err)
	{
		goto failed;
	}

	smtcfb_setmode(sfb);
	hw.BaseAddressInVRAM = 0;  //Primary display starting from 0 postion 
	sfb->fb.par = &hw;

	err = register_framebuffer(&sfb->fb);
	if (err < 0) 
	{
		goto failed;
	}

	printk("Silicon Motion SM%X Rev%X primary display mode %dx%d-%d Init Complete.\n",
		hw.chipID, hw.chipRevID, sfb->fb.var.xres, sfb->fb.var.yres, sfb->fb.var.bits_per_pixel);

#if defined(CONFIG_FB_SM7XX_DUALHEAD)
	smtc_head2_init(sfb);
	err = register_framebuffer(&smtcfb_info2.fb);

	if (err < 0)
	{
		printk("Silicon Motion, Inc.  second head init fail\n");
		goto failed; //if second head display fails, also fails the primary display
	}

	printk("Silicon Motion SM%X Rev%X secondary display mode %dx%d-%d Init Complete.\n",
		hw.chipID, hw.chipRevID, hw2.width, hw2.height, smtcfb_info2.fb.var.bits_per_pixel);

#endif

	return 0;

failed:
	printk("Silicon Motion, Inc.  primary display init fail\n");
	smtc_unmap_smem(sfb);
	smtc_unmap_mmio(sfb);
	smtc_free_fb_info(sfb);

	return err;
}

static void __exit smtcfb_exit(void){}

module_init(smtcfb_init);
module_exit(smtcfb_exit);


//#ifndef MODULE
/**
	*	sm712be_setup - process command line options 
	*	@options: string of options 
	*	Returns zero. 
	* 
*/
static int __init sm712be_setup(char *options)
{	
	int retval = 0;	
	sm712be_flag = 0;	
	if (!options || !*options) 	
	{
		retval = 1;	
		smdbg("\n No sm712be parameter\n", __LINE__);
	}
	if (!retval && strstr(options, "enable")) 	
	{		
		sm712be_flag = 1;		 	
	}	
	smdbg("\nsm712be_setup = %s sm712be_flag = %d\n", options, sm712be_flag); 	
	return 1;
}

__setup("sm712be=", sm712be_setup);

//#endif

#ifdef __BIG_ENDIAN
/**
	*	sm712vga_setup - process command line options, get vga parameter 
	*	@options: string of options 
	*	Returns zero. 
	* 
*/
static int __init sm712vga_setup(char *options)
{	
	int retval = 0;	
	int index ;
	sm712be_flag = 0;	
	
	if (!options || !*options) 	
	{
		retval = 1;	
		smdbg("\n No vga parameter\n", __LINE__);
	}
		
	screen_info.lfb_width = 0;
	screen_info.lfb_height = 0;
	screen_info.lfb_depth = 0;
	
	for (index = 0; index < (sizeof(vesa_mode) / sizeof(struct vesa_mode_table)); index++)
	{
		if(strstr(options, vesa_mode[index].mode_index))
		{
			screen_info.lfb_width = vesa_mode[index].lfb_width;
			screen_info.lfb_height = vesa_mode[index].lfb_height;
			screen_info.lfb_depth = vesa_mode[index].lfb_depth;
		}
	}
	smdbg("\nsm712vga_setup = %s\n", options); 
	return 1;
}

__setup("vga=", sm712vga_setup);
#endif

MODULE_AUTHOR("Siliconmotion ");
MODULE_DESCRIPTION("Framebuffer driver for SMI Graphic Cards");
MODULE_LICENSE("GPL");

