/******************************************************************************/
/*                Name:           serial.c                                    */
/*                Version:        1.5.0                                       */
/*                Date:           19/7/2007                                   */
/*                Serial interface for board or emulated board                */
/*                                                                            */
/*============================================================================*/


#include "serial.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <string.h>

#include "global.h"

#include "interface.h"
#include "view.h"


void (*XSV_Cleanup) (void) = NULL;


#define IN_POLL_TIMEOUT  1000
#define OUT_POLL_TIMEOUT  100                         /* 100ms iteration time */


//GList *serial_list_2_board = NULL;
//GList *serial_list_2_computer = NULL;
//uchar simwotrudata[] = { 13, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0 }; /*22 */
//
//uchar simregdata[] = { 0, 2, 3, 4, 1, 2, 3, 4, 2, 2, 3, 4, 3, 2, 3, 4,
//                       0, 2, 3, 4, 1, 2, 3, 4, 2, 2, 3, 4, 3, 2, 3, 4,
//                       0, 2, 3, 4, 1, 2, 3, 4, 2, 2, 3, 4, 3, 2, 3, 4,
//                       0, 2, 3, 4, 1, 2, 3, 4, 2, 2, 3, 4, 3, 2, 3, 4,
//                       200, 255, 255, 255, 255
//};

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Frees up the serial line in case of a crash to avoid possible locking.     */
/* This procedure is called when failing to set up serial connection          */
/*                                                                            */

void serial_crash(void)
{
if (TRACE > 10) g_print("serial_crash\n");

if (SERIAL == interface_type)              /* If connection was set to serial */
  {                                          /* set back to original settings */
  tcsetattr(serial_FD, TCSANOW, &serial_originalportsettings);
  close(serial_FD);                                               /* close it */
  }
exit(1);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Frees up the serial line in case of an error during setup.                 */
/* input: the error description as string                                     */

void serial_error(char *errorstring)
{
if (TRACE > 10) g_print("serial_error\n");

close(serial_FD);                         /* close the serial line connection */
g_print("Serial Error: %s\n", errorstring);           /* inform user of error */
exit(1);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sets up the properties of the serial port connection                       */
/*                                                                            */

int serial_setup(int waittime)
{
struct termios serialsettings;
struct sigaction cleanup;
struct flock lock;

if (TRACE > 10) g_print("serial_setup\n");

XSV_Cleanup = serial_crash;        /* setting up the crash handling function */
memset((void *) &serialsettings, 0, sizeof(struct termios));

cleanup.sa_handler = (void (*)()) XSV_Cleanup;
sigemptyset(&cleanup.sa_mask);
cleanup.sa_flags = 0;

sigaction(SIGINT,  &cleanup, NULL);
sigaction(SIGHUP,  &cleanup, NULL);
sigaction(SIGTERM, &cleanup, NULL);
sigaction(SIGSEGV, &cleanup, NULL);
sigaction(SIGBUS,  &cleanup, NULL);

serial_FD = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
if (serial_FD < 0) serial_error("Couldn't open serial port");

lock.l_type   = F_WRLCK;
lock.l_whence = SEEK_SET;
lock.l_len    = 0;
lock.l_start  = 0;

if (0 > fcntl(serial_FD, F_SETLK, &lock))
  serial_error("Failed to get serial port lock");

fcntl(serial_FD, F_SETFL, 0);

if (tcgetattr(serial_FD, &serial_originalportsettings) == -1)
  serial_error("Failed to set serial port attributes");

if (cfsetispeed(&serialsettings, PORTSPEED))              /* used to be B9600 */
  serial_error("Failed to set input baud rate");

if (cfsetospeed(&serialsettings, PORTSPEED))
  serial_error("Failed to set output baud rate");

serialsettings.c_oflag &= ~OPOST;
serialsettings.c_iflag |= IGNPAR;
serialsettings.c_cflag |= CS8 | CREAD | CLOCAL;
serialsettings.c_cc[VMIN] = 0;
serialsettings.c_cc[VTIME] = waittime;                         /* <-wait time */

if (tcsetattr(serial_FD, TCSADRAIN, &serialsettings))
  serial_error("Failed to update serial port attributes");

return 1;                        /* What does this mean ?? @@@ */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* get a single character/byte from the board using the general case of       */
/*  getting an array of bytes.                                                */
/*                                                                            */

int board_getchar(unsigned char *to_get)
{return board_getchararray(1, to_get);}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* send a single character/byte to the board using the general case of        */
/* sending an array of bytes                                                  */
/*                                                                            */
/*                                                                            */

int board_sendchar(unsigned char to_send)
{return board_sendchararray(1, &to_send);}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sends N bytes from the supplied value to the board, LSB first.             */
/* Returns the number of bytes believed received successfully (i.e. N=>"Ok")  */

int board_sendbN(int value, int N)
{
char buffer[MAX_SERIAL_WORD];
int i;

if (TRACE > 15) g_print("board_sendbN\n");

if (N > MAX_SERIAL_WORD) N = MAX_SERIAL_WORD;       /* Clip, just in case ... */

for (i = 0; i < N; i++)
  {
  buffer[i] = value & 0xFF;                               /* Byte into buffer */
  value = value >> 8;                                        /* Get next byte */
  }

return board_sendchararray(N, buffer);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Gets N bytes from the board into the indicated val_ptr, LSB first.         */
/* Returns the number of bytes received successfully (i.e. N=>"Ok")           */
/* If error suspected sets `board_version' to not present                     */

int board_getbN(int *val_ptr, int N)
{
char buffer[MAX_SERIAL_WORD];
int i, No_received;

if (TRACE > 15) g_print("board_getbN\n");

if (N > MAX_SERIAL_WORD) N = MAX_SERIAL_WORD;       /* Clip, just in case ... */

No_received = board_getchararray(N, buffer);

*val_ptr = 0;

for (i = 0; i < No_received; i++)
  *val_ptr = *val_ptr | ((buffer[i] & 0xFF) << (i * 8));  /* Assemble integer */

if (No_received != N) board_version = -1;         /* Really do this here? @@@ */
return No_received;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/
/* Print the data sent/received byte by byte                                  */

void serial_debug_print(char *direction, unsigned char *buffer, int max)
{
int i;

for (i = 0; i < max; i++) g_print("%s: %02X ", direction, buffer[i]);
g_print("\n");
return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Gets a character array from the board.                                     */
/* Returns up to char_number number of characters in array at data_ptr.       */
/* Returns number of bytes received                                           */
/*                                                                            */

int board_getchararray(int char_number, unsigned char *data_ptr)
{
int reply_count;                 /* Number of chars fetched in latest attempt */
int reply_total;           /* Total number of chars fetched during invocation */
struct pollfd pollfd;

if (TRACE > 15) g_print("board_getchararray\n");

pollfd.fd = read_pipe;
pollfd.events = POLLIN;
reply_total = 0;

if ((interface_type == SERIAL)         /* Under all cases - serial connection */
 || (interface_type == EMULATOR)                             /*  or emulation */
 || (interface_type == NETWORK))                               /*  or network */
  {
  while (char_number > 0)                       /* while there is more to get */
    {
    if(!poll (&pollfd, 1, IN_POLL_TIMEOUT))           /* If nothing available */
      {
      reply_count = 0;                         /* Will force loop termination */
      if (VERBOSE || SERIAL_DEBUG)
        g_print("Timeout while polling. Details follow:\n");   /* Count below */
      }
    else
      reply_count = read(read_pipe, data_ptr, char_number);/* attempt to read */
     /* the number of bytes requested  and store the number of bytes received */

    if (reply_count == 0)         /* If no bytes received (or polling failed) */
      {
      if (VERBOSE || SERIAL_DEBUG)
        g_print("Timeout while reading. Got (%d) bytes out of (%d).\n",
                reply_total, reply_total + char_number);
      char_number = -1;                                   /* Set to terminate */
      }

    if (reply_count < 0) reply_count = 0;                 /* Set minimum to 0 */

    if (SERIAL_DEBUG) serial_debug_print("reply", &data_ptr[0], char_number);

    reply_total += reply_count;
    char_number -= reply_count;   /* Update No. bytes that are still required */
    data_ptr += reply_count;     /* Move the data pointer to its new location */
    }
  }
return reply_total;                    /* return the number of bytes received */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sends char_number bytes located at data_ptr to current client              */
/* Returns number of bytes transmitted (currently always same as input)       */
/*                                                                            */

int board_sendchararray(int char_number, unsigned char *data_ptr)
{
struct pollfd pollfd;
int blocked_flag;

if (TRACE > 15) g_print(" board_sendchararray\n");

pollfd.fd     = write_pipe;
pollfd.events = POLLOUT;
blocked_flag  = FALSE;                                    /* Hasn't timed out */

while (poll(&pollfd, 1, OUT_POLL_TIMEOUT) == 0)     /* See if output possible */
  {
  if (!blocked_flag)
    {
    g_print("Client system not responding!\n"); /* Warn; poss. comms. problem */
    blocked_flag = TRUE;	                          /* Hasn't timed out */
    }
  }

if (blocked_flag) g_print("Client system okay!\n");


if (SERIAL_DEBUG) serial_debug_print("sent", &data_ptr[0], char_number);

if ((interface_type == SERIAL)         /* Under all cases - serial connection */
 || (interface_type == EMULATOR)                             /*  or emulation */
 || (interface_type == NETWORK))                               /*  or network */
  write(write_pipe, data_ptr, char_number);        /* Write char_number bytes */

return char_number;      /* Return No of chars written (always same as input) */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/*                                end of serial.c                             */
/*============================================================================*/
