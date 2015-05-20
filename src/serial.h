/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*		Name:		serial.h				*/
/*		Version:	1.3.0					*/
/*		Date:		19/07/2007				*/
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <glib.h>
#include "config.h"

#define uchar unsigned char

#define MAX_SERIAL_WORD 4	 /* Largest number of bytes allowed in a word */
                                         /* Set no larger than size of `word' */

/* the following will define some of the properties to be considered */

#ifdef  LINUX
#define PORTNAME "/dev/ttyS0"
#define PORTSPEED B115200
//#define PORTSPEED B19200
#endif

#ifdef  SUNOS
#define PORTNAME "/dev/ttya"
#define PORTSPEED B115200
#endif

#ifndef  SUNOS
#ifndef  LINUX
#define PORTNAME "/dev/ttyS0"
#define PORTSPEED B115200
#endif
#endif

#ifdef  SERIAL_PORT
#define PORTNAME SERIAL_PORT
#endif

#ifdef  SERIAL_SPEED
#define PORTNAME BSERIAL_SPEED
#endif

int serial_FD;
int read_pipe;
int write_pipe;
struct termios serial_originalportsettings;
char *portname;
//GList *serial_list_2_board;
//GList *serial_list_2_computer;

/* prototypes: */

int board_getchararray (int, unsigned char *);
int board_sendchararray (int, unsigned char *);
int board_getchar (unsigned char *);
int board_sendchar (unsigned char);
int serial_setup (int);
void serial_crash (void);
void serial_error (char *errorstring);
int board_getbN (int *val_ptr, int N);
int board_sendbN (int value, int N);

#endif

/*									*/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 	*/
/*                     end of serial.h					*/
/************************************************************************/
