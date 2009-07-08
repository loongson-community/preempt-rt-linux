
/*
 * EC(Embedded Controller) KB3310B misc device export functions header file
 *
 * Author:	liujl <liujl@lemote.com>
 * Date	 :	2009-03-16
 *
 *	EC relative export header file.
 */

/* the general ec index-io port read action */
extern unsigned char ec_read(unsigned short addr);
/* the general ec index-io port write action */
extern void ec_write(unsigned short addr, unsigned char val);
/* query sequence of 62/66 port access routine */
extern int ec_query_seq(unsigned char cmd);
