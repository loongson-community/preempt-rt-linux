/*
 * EC(Embedded Controller) KB3310B misc device driver header for Linux
 * Author:	liujl <liujl@lemote.com>
 * Date	 :	2008-03-14
 *
 *  EC relative header file. All the EC registers should be defined here.
 */

#include	<asm/uaccess.h>
#include	<asm/io.h>
#include	<asm/system.h>

#define VERSION		"1.37"

/*
 * The following registers are determined by the EC index configuration.
 * 1, fill the PORT_HIGH as EC register high part.
 * 2, fill the PORT_LOW as EC register low part.
 * 3, fill the PORT_DATA as EC register write data or get the data from it.
 */
#define	EC_IO_PORT_HIGH	0x0381
#define	EC_IO_PORT_LOW	0x0382
#define	EC_IO_PORT_DATA	0x0383

/* ec registers range */
#define	EC_MAX_REGADDR	0xFFFF
#define	EC_MIN_REGADDR	0xF000
/* #define       EC_RAM_ADDR     0xF400 */
#define	EC_RAM_ADDR	0xF800

/* temperature & fan registers */
#define	REG_TEMPERATURE_VALUE		0xF458	/*  current temperature value */
#define	REG_FAN_CONTROL				0xF4D2	/*  fan control */
#define	BIT_FAN_CONTROL_ON				(1 << 0)
#define	BIT_FAN_CONTROL_OFF				(0 << 0)
#define	REG_FAN_STATUS				0xF4DA	/*  fan status */
#define	BIT_FAN_STATUS_ON				(1 << 0)
#define	BIT_FAN_STATUS_OFF				(0 << 0)
#define	REG_FAN_SPEED_HIGH			0xFE22	/*  fan speed high byte */
#define	REG_FAN_SPEED_LOW			0xFE23	/*  fan speed low byte */
#define	REG_FAN_SPEED_LEVEL			0xF4E4	/*  fan speed level, from 1 to 5 */

/* battery registers */
#define	REG_BAT_DESIGN_CAP_HIGH		0xF77D	/*  design capacity high byte */
#define	REG_BAT_DESIGN_CAP_LOW		0xF77E	/*  design capacity low byte */
#define	REG_BAT_FULLCHG_CAP_HIGH	0xF780	/*  full charged capacity high byte */
#define	REG_BAT_FULLCHG_CAP_LOW		0xF781	/*  full charged capacity low byte */
#define	REG_BAT_DESIGN_VOL_HIGH		0xF782	/*  design voltage high byte */
#define	REG_BAT_DESIGN_VOL_LOW		0xF783	/*  design voltage low byte */
#define	REG_BAT_CURRENT_HIGH		0xF784	/*  battery in/out current high byte */
#define	REG_BAT_CURRENT_LOW			0xF785	/*  battery in/out current low byte */
#define	REG_BAT_VOLTAGE_HIGH		0xF786	/*  battery current voltage high byte */
#define	REG_BAT_VOLTAGE_LOW			0xF787	/*  battery current voltage low byte */
#define	REG_BAT_TEMPERATURE_HIGH	0xF788	/*  battery current temperature high byte */
#define	REG_BAT_TEMPERATURE_LOW		0xF789	/*  battery current temperature low byte */
#define	REG_BAT_RELATIVE_CAP_HIGH	0xF492	/*  relative capacity high byte */
#define	REG_BAT_RELATIVE_CAP_LOW	0xF493	/*  relative capacity low byte */
#define	REG_BAT_VENDOR				0xF4C4	/*  battery vendor number */
#define FLAG_BAT_VENDOR_SANYO			0x01
#define FLAG_BAT_VENDOR_SIMPLO			0x02
#define	REG_BAT_CELL_COUNT			0xF4C6	/*  how many cells in one battery */
#define FLAG_BAT_CELL_3S1P				0x03
#define FLAG_BAT_CELL_3S2P				0x06
#define	REG_BAT_CHARGE				0xF4A2	/*  macroscope battery charging */
#define FLAG_BAT_CHARGE_DISCHARGE		0x01
#define FLAG_BAT_CHARGE_CHARGE			0x02
#define FLAG_BAT_CHARGE_ACPOWER			0x00
#define	REG_BAT_STATUS				0xF4B0
#define	BIT_BAT_STATUS_LOW				(1 << 5)
#define BIT_BAT_STATUS_DESTROY			(1 << 2)
#define BIT_BAT_STATUS_FULL				(1 << 1)
#define BIT_BAT_STATUS_IN				(1 << 0)
#define	REG_BAT_CHARGE_STATUS		0xF4B1
#define	BIT_BAT_CHARGE_STATUS_OVERTEMP	(1 << 2)	/*  over temperature */
#define BIT_BAT_CHARGE_STATUS_PRECHG	(1 << 1)	/*  pre-charge the battery */
#define	REG_BAT_STATE				0xF482
#define	BIT_BAT_STATE_CHARGING			(1 << 1)
#define	BIT_BAT_STATE_DISCHARGING		(1 << 0)
#define	REG_BAT_POWER				0xF440
#define BIT_BAT_POWER_S3				(1 << 2)	/*  enter s3 standby mode */
#define BIT_BAT_POWER_ON				(1 << 1)	/*  system is on */
#define BIT_BAT_POWER_ACIN				(1 << 0)	/*  adapter is inserted */

/* other registers */
#define	REG_AUDIO_MUTE				0xF4E7	/*  audio mute : rd/wr */
#define	BIT_AUDIO_MUTE_ON				(1 << 0)
#define	BIT_AUDIO_MUTE_OFF				(0 << 0)
#define	REG_AUDIO_BEEP				0xF4D0	/*  audio beep and reset : rd/wr */
#define	BIT_AUDIO_BEEP_ON				(1 << 0)
#define	BIT_AUDIO_BEEP_OFF				(0 << 0)
#define	REG_USB0_FLAG				0xF461	/*  usb0 port power or not : rd/wr */
#define	BIT_USB0_FLAG_ON				(1 << 0)
#define	BIT_USB0_FLAG_OFF				(0 << 0)
#define	REG_USB1_FLAG				0xF462	/*  usb1 port power or not : rd/wr */
#define	BIT_USB1_FLAG_ON				(1 << 0)
#define	BIT_USB1_FLAG_OFF				(0 << 0)
#define	REG_USB2_FLAG				0xF463	/*  usb2 port power or not : rd/wr */
#define	BIT_USB2_FLAG_ON				(1 << 0)
#define	BIT_USB2_FLAG_OFF				(0 << 0)
#define	REG_CRT_DETECT				0xF4AD	/*  detected CRT exist or not */
#define	BIT_CRT_DETECT_PLUG				(1 << 0)
#define	BIT_CRT_DETECT_UNPLUG			(0 << 0)
#define	REG_LID_DETECT				0xF4BD	/*  detected LID is on or not */
#define	BIT_LID_DETECT_ON				(1 << 0)
#define	BIT_LID_DETECT_OFF				(0 << 0)
#define	REG_RESET					0xF4EC	/*  reset the machine auto-clear : rd/wr */
#define	BIT_RESET_ON					(1 << 0)
#define	BIT_RESET_OFF					(0 << 0)
#define	REG_LED						0xF4C8	/*  light the led : rd/wr */
#define	BIT_LED_RED_POWER				(1 << 0)
#define	BIT_LED_ORANGE_POWER			(1 << 1)
#define	BIT_LED_GREEN_CHARGE			(1 << 2)
#define	BIT_LED_RED_CHARGE				(1 << 3)
#define	BIT_LED_NUMLOCK					(1 << 4)
#define	REG_LED_TEST				0xF4C2	/*  test led mode, all led on or off */
#define	BIT_LED_TEST_IN					(1 << 0)
#define	BIT_LED_TEST_OUT				(0 << 0)
#define	REG_DISPLAY_BRIGHTNESS		0xF4F5	/* 9 stages LCD backlight brightness adjust */
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_0		0
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_1		1
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_2		2
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_3		3
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_4		4
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_5		5
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_6		6
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_7		7
#define	FLAG_DISPLAY_BRIGHTNESS_LEVEL_8		8
#define	REG_CAMERA_STATUS			0xF46A	/* camera is in ON/OFF status. */
#define	BIT_CAMERA_STATUS_ON			(1 << 0)
#define	BIT_CAMERA_STATUS_OFF			(0 << 0)
#define	REG_CAMERA_CONTROL			0xF7B7	/* control camera to ON/OFF. */
#define	BIT_CAMERA_CONTROL_OFF			(1 << 1)
#define	BIT_CAMERA_CONTROL_ON			(1 << 1)
#define	REG_AUDIO_VOLUME			0xF46C	/* The register to show volume level */
#define	FLAG_AUDIO_VOLUME_LEVEL_0		0
#define	FLAG_AUDIO_VOLUME_LEVEL_1		1
#define	FLAG_AUDIO_VOLUME_LEVEL_2		2
#define	FLAG_AUDIO_VOLUME_LEVEL_3		3
#define	FLAG_AUDIO_VOLUME_LEVEL_4		4
#define	FLAG_AUDIO_VOLUME_LEVEL_5		5
#define	FLAG_AUDIO_VOLUME_LEVEL_6		6
#define	FLAG_AUDIO_VOLUME_LEVEL_7		7
#define	FLAG_AUDIO_VOLUME_LEVEL_8		8
#define	FLAG_AUDIO_VOLUME_LEVEL_9		9
#define	FLAG_AUDIO_VOLUME_LEVEL_10		0x0A
#define	REG_WLAN_STATUS				0xF4FA	/* Wlan Status */
#define	BIT_WLAN_STATUS_ON				(1 << 0)
#define	BIT_WLAN_STATUS_OFF				(0 << 0)
#define	REG_DISPLAY_LCD				0xF79F	/* Black screen Status */
#define	BIT_DISPLAY_LCD_ON				(1 << 0)
#define	BIT_DISPLAY_LCD_OFF				(0 << 0)
#define	REG_BACKLIGHT_CTRL			0xF7BD	/* LCD backlight control: off/restore */
#define	BIT_BACKLIGHT_ON				(1 << 0)
#define	BIT_BACKLIGHT_OFF				(0 << 0)

/* SCI Event Number from EC */
#define	SCI_EVENT_NUM_LID				0x23	/*  press the lid or not */
#define	SCI_EVENT_NUM_DISPLAY_TOGGLE	0x24	/*  Fn+F3 for display switch */
#define	SCI_EVENT_NUM_SLEEP				0x25	/*  Fn+F1 for entering sleep mode */
#define	SCI_EVENT_NUM_OVERTEMP			0x26	/*  Over-temperature happened */
#define	SCI_EVENT_NUM_CRT_DETECT		0x27	/*  CRT is connected */
#define	SCI_EVENT_NUM_CAMERA			0x28	/*  Camera is on or off */
#define	SCI_EVENT_NUM_USB_OC2			0x29	/*  USB2 Over Current occurred */
#define	SCI_EVENT_NUM_USB_OC0			0x2A	/*  USB0 Over Current occurred */
#define	SCI_EVENT_NUM_AC_BAT			0x2E	/*  ac & battery relative issue */
#define BIT_AC_BAT_BAT_IN			0
#define	BIT_AC_BAT_AC_IN			1
#define	BIT_AC_BAT_INIT_CAP			2
#define	BIT_AC_BAT_CHARGE_MODE		3
#define	BIT_AC_BAT_STOP_CHARGE		4
#define	BIT_AC_BAT_BAT_LOW			5
#define	BIT_AC_BAT_BAT_FULL			6
#define	SCI_EVENT_NUM_DISPLAY_BRIGHTNESS	0x2D	/*  LCD backlight brightness adjust */
#define	SCI_EVENT_NUM_AUDIO_VOLUME		0x2F	/*  Volume adjust */
#define	SCI_EVENT_NUM_WLAN				0x30	/*  Wlan is on or off */
#define	SCI_EVENT_NUM_AUDIO_MUTE		0x2C	/*  Mute is on or off */
#define	SCI_EVENT_NUM_BLACK_SCREEN		0x2B	/*  Black screen is on or off */

#define	SCI_INDEX_LID					0x00
#define	SCI_INDEX_DISPLAY_TOGGLE		0x01
#define	SCI_INDEX_SLEEP					0x02
#define	SCI_INDEX_OVERTEMP				0x03
#define	SCI_INDEX_CRT_DETECT			0x04
#define	SCI_INDEX_CAMERA				0x05
#define	SCI_INDEX_USB_OC2				0x06
#define	SCI_INDEX_USB_OC0				0x07
#define	SCI_INDEX_AC_BAT				0x08
#define	SCI_INDEX_DISPLAY_BRIGHTNESS_INC	0x09
#define	SCI_INDEX_DISPLAY_BRIGHTNESS_DEC	0x0A
#define	SCI_INDEX_AUDIO_VOLUME_INC			0x0B
#define	SCI_INDEX_AUDIO_VOLUME_DEC			0x0C
#define	SCI_INDEX_WLAN					0x0D
#define	SCI_INDEX_AUDIO_MUTE			0x0E
#define	SCI_INDEX_BLACK_SCREEN			0x0F

#define	SCI_MAX_EVENT_COUNT			0x10

/* EC access port for sci communication */
#define	EC_CMD_PORT		0x66
#define	EC_STS_PORT		0x66
#define	EC_DAT_PORT		0x62
#define	CMD_INIT_IDLE_MODE	0xdd
#define	CMD_EXIT_IDLE_MODE	0xdf
#define	CMD_INIT_RESET_MODE	0xd8
#define	CMD_REBOOT_SYSTEM	0x8c
#define	CMD_GET_EVENT_NUM	0x84
#define	CMD_PROGRAM_PIECE	0xda

/* #define       DEBUG_PRINTK */

#ifdef DEBUG_PRINTK
#define PRINTK_DBG(args...)	printk(args)
#else
#define PRINTK_DBG(args...)
#endif

extern void _rdmsr(u32 addr, u32 *hi, u32 *lo);
extern void _wrmsr(u32 addr, u32 hi, u32 lo);
