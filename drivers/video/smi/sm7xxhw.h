/*
 *  linux/drivers/video/sm7xxhw.h -- Silicon Motion SM7xx frame buffer device
 *
 *      Copyright (C) 2006 Silicon Motion, Inc.
 *      Ge Wang, gewang@siliconmotion.com
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */


#define SM712_VIDEOMEMORYSIZE    0x00400000  /*Assume SM712 graphics chip has 4MB VRAM */
#define SM722_VIDEOMEMORYSIZE    0x00800000  /*Assume SM722 graphics chip has 8MB VRAM */

#define dac_reg	(0x3c8)
#define dac_val	(0x3c9)

#define smtc_mmiowb(dat,reg)	writeb(dat, smtc_RegBaseAddress + reg)
#define smtc_mmioww(dat,reg)	writew(dat, smtc_RegBaseAddress + reg)
#define smtc_mmiowl(dat,reg)	writel(dat, smtc_RegBaseAddress + reg)

#define smtc_mmiorb(reg)	        readb(smtc_RegBaseAddress + reg)
#define smtc_mmiorw(reg)	        readw(smtc_RegBaseAddress + reg)
#define smtc_mmiorl(reg)	        readl(smtc_RegBaseAddress + reg)

#define SIZE_SR00_SR04      (0x04 - 0x00 + 1)
#define SIZE_SR10_SR24      (0x24 - 0x10 + 1)
#define SIZE_SR30_SR75      (0x75 - 0x30 + 1)
#define SIZE_SR80_SR93      (0x93 - 0x80 + 1)
#define SIZE_SRA0_SRAF      (0xAF - 0xA0 + 1)
#define SIZE_GR00_GR08      (0x08 - 0x00 + 1)
#define SIZE_AR00_AR14      (0x14 - 0x00 + 1)
#define SIZE_CR00_CR18      (0x18 - 0x00 + 1)
#define SIZE_CR30_CR4D      (0x4D - 0x30 + 1)
#define SIZE_CR90_CRA7      (0xA7 - 0x90 + 1)
#define SIZE_VPR            (0x6C + 1)
#define SIZE_DPR			(0x44 + 1)


static inline void smtc_crtcw(int reg, int val)
{
        smtc_mmiowb(reg, 0x3d4);
        smtc_mmiowb(val, 0x3d5);
}

static inline unsigned int smtc_crtcr(int reg)
{
        smtc_mmiowb(reg, 0x3d4);
        return smtc_mmiorb(0x3d5);
}

static inline void smtc_grphw(int reg, int val)
{
        smtc_mmiowb(reg, 0x3ce);
        smtc_mmiowb(val, 0x3cf);
}

static inline unsigned int smtc_grphr(int reg)
{
        smtc_mmiowb(reg, 0x3ce);
        return smtc_mmiorb(0x3cf);
}

static inline void smtc_attrw(int reg, int val)
{
        smtc_mmiorb(0x3da);
        smtc_mmiowb(reg, 0x3c0);
        smtc_mmiorb(0x3c1);
        smtc_mmiowb(val, 0x3c0);
}

static inline void smtc_seqw(int reg, int val)
{
        smtc_mmiowb(reg, 0x3c4);
        smtc_mmiowb(val, 0x3c5);
}

static inline unsigned int smtc_seqr(int reg)
{
        smtc_mmiowb(reg, 0x3c4);
        return smtc_mmiorb(0x3c5);
}

// The next structure holds all information relevant for a specific video mode.
struct ModeInit
{
	int			  mmSizeX;
	int			  mmSizeY;
	int			  bpp;
	int			  hz;
	unsigned char Init_MISC;
	unsigned char Init_SR00_SR04[SIZE_SR00_SR04];
	unsigned char Init_SR10_SR24[SIZE_SR10_SR24];
	unsigned char Init_SR30_SR75[SIZE_SR30_SR75];
	unsigned char Init_SR80_SR93[SIZE_SR80_SR93];
	unsigned char Init_SRA0_SRAF[SIZE_SRA0_SRAF];
	unsigned char Init_GR00_GR08[SIZE_GR00_GR08];
	unsigned char Init_AR00_AR14[SIZE_AR00_AR14];
	unsigned char Init_CR00_CR18[SIZE_CR00_CR18];
	unsigned char Init_CR30_CR4D[SIZE_CR30_CR4D];
	unsigned char Init_CR90_CRA7[SIZE_CR90_CRA7];
};

