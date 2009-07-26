/* serial port debug support */

#ifdef SERIAL_PORT_DEBUG

#include <linux/types.h>
#include <linux/serial_reg.h>

#include <asm/addrspace.h>

#include <loongson.h>
#include <machine.h>

#define UART_BASE CKSEG1ADDR(LOONGSON_UART_BASE)

void putc(char c)
{
	int timeout;
	char reg;

	reg = *((char *)(UART_BASE + UART_LSR)) & UART_LSR_THRE;
	for (timeout = 1024; reg == 0 && timeout > 0; timeout--)
		reg = (*((char *)(UART_BASE + UART_LSR))) & UART_LSR_THRE;

	*((char *)(UART_BASE + UART_TX)) = c;
}

void puts(const char *s)
{
	char c;
	while ((c = *s++) != '\0') {
		putc(c);
		if (c == '\n')
			putc('\r');
	}
}

void puthex(unsigned long val)
{

	unsigned char buf[10];
	int i;
	for (i = 7; i >= 0; i--) {
		buf[i] = "0123456789ABCDEF"[val & 0x0F];
		val >>= 4;
	}
	buf[8] = '\0';
	puts(buf);
}
#else
void putc(char c)
{
}

void puts(const char *s)
{
}

void puthex(unsigned long val)
{
}
#endif
