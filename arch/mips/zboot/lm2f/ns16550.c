/*
 * NS16550 support
 */

#include <linux/autoconf.h>
#include <asm/serial.h>
#include "ns16550.h"

typedef struct NS16550 *NS16550_t;

//#ifdef CONFIG_LEMOTE_FULOONG2F
//#ifdef CONFIG_LEMOTE_NAS
//#define COM1 0xbff003f8
//#else 
//#define COM1 0xbfd002f8
//#endif
//#endif

//#ifdef CONFIG_LEMOTE_YEELOONG2F 
#define COM1 0xbff003f8
//#endif

const NS16550_t COM_PORTS[] = { 
	(NS16550_t) COM1
};

volatile struct NS16550 *
serial_init(int chan)
{
	volatile struct NS16550 *com_port;
	com_port = (struct NS16550 *) COM_PORTS[chan];
	/* See if port is present */
	com_port->lcr = 0x00;
	com_port->ier = 0xFF;
#if 0
	if (com_port->ier != 0x0F) return ((struct NS16550 *)0);
#endif
	com_port->ier = 0x00;
	com_port->lcr = 0x80;  /* Access baud rate */

#undef  SERIAL_CONSOLE_BAUD
#define SERIAL_CONSOLE_BAUD 115200

	com_port->dll = (BASE_BAUD / SERIAL_CONSOLE_BAUD) & 0xff;
	com_port->dlm = ((BASE_BAUD / SERIAL_CONSOLE_BAUD) >> 8 ) & 0xff;

	com_port->lcr = 0x03;  /* 8 data, 1 stop, no parity */
	com_port->mcr = 0x03;  /* RTS/DTR */
	com_port->fcr = 0x07;  /* Clear & enable FIFOs */
	return (com_port);
}

void
serial_putc(volatile struct NS16550 *com_port, unsigned char c)
{
	while ((com_port->lsr & LSR_THRE) == 0) ;
	com_port->thr = c;
}

unsigned char
serial_getc(volatile struct NS16550 *com_port)
{
	while ((com_port->lsr & LSR_DR) == 0) ;
	return (com_port->rbr);
}

int
serial_tstc(volatile struct NS16550 *com_port)
{
	return ((com_port->lsr & LSR_DR) != 0);
}
