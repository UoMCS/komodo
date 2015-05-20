/*----------------------------------------------------------------------------*/
/* This file contains example handlers for 'jimulator' support.               */
/* They are somewhat tailored to the CSE project application but are in an    */
/* unfinished state.  Unnecessary diagnostics are also included.              */
/* Compile with "gcc -o <library_name> --shared lib_i2cmater.c"               */
/*                                                              JDG 13/9/10   */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#include <stdio.h>
#include "states.h"               /* Definitions used for 'status' checks     */
#include "address_spaces.h"       /* Definitions used for REG_CURRENT et alia */
#include "i2cmaster.h"

#define TRUE  (0==0)
#define FALSE (0!=0)

typedef int boolean;
typedef unsigned short pixel;

struct pollfd *SVC_poll;         /* Will alias to the main programme variable */

/*----------------------------------------------------------------------------*/
/* Called once when library opened                                            */

boolean constructor (unsigned char *me, unsigned char *string)
{
boolean okay;

okay = TRUE;

if (!init_i2c(me))                                   /* Started I2C emulator? */
  {
  okay = FALSE;                                  /* No - something went wrong */

  fprintf(stderr, "I2C initialisation failure\n");

  }

return okay;                                  /* FALSE if there was a problem */
}

/*----------------------------------------------------------------------------*/
/* Called once when process terminates                                        */

boolean destructor (unsigned char *me)
{
int status;

terminate_i2c();

return TRUE;
}

/*----------------------------------------------------------------------------*/

boolean mem_r_handler (unsigned int address, unsigned int *data, int size,
                       boolean sign, boolean T, int source, boolean* abort)
{
boolean mem_handled;

if ((address >= I2C_MIN_ADR) && (address <= I2C_MAX_ADR))
  {                                                       /* i2c master space */
  *data = read_i2c(address, size, SVC_poll);
  mem_handled = TRUE;                                           /* Dealt with */
  }
else
  mem_handled = FALSE;       /* Not handled: inform caller there's more to do */

return mem_handled;

}

/*----------------------------------------------------------------------------*/

boolean mem_w_handler (unsigned int address, unsigned int data, int size,
                       boolean T, int source, boolean* abort)
{
boolean mem_handled;

if ((address >= I2C_MIN_ADR) && (address <= I2C_MAX_ADR))
  {                                     /* The I2C master is on a 16-bit bus */
                     /* it is selected only if the LSB byte is being written */
  if ((size != 1) || ((address & 0x01) == 0))
    {
    write_i2c(address, size, data, SVC_poll);
    }
  mem_handled = TRUE;                                           /* Dealt with */
  }
else
  mem_handled = FALSE;       /* Not handled: inform caller there's more to do */

return mem_handled;

}

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/*-------------------------------- i2c emulator ------------------------------*/
/*----------------------------------------------------------------------------*/
//#include "i2cmaster.h"
//#include "global.h"


/* i2c emulator initialisation                                                */
boolean init_i2c (char *my_name)
{
char rps[5], wps[5];
boolean okay;

/* establish the SIGCHLD signal  */
/* handler to terminate with i2c */
signal(SIGCHLD, sig_handler);
/* establish the SIGTERM signal */
/* handler to terminate i2c     */
signal(SIGTERM, sig_handler);

okay = FALSE;                            /* Lots of chances for errors, below */

if (pipe(i2c_wt_pipe) == -1)                /* open pipe to write data to i2c */
  g_printerr("%s: ERROR - cannot create pipe to i2c.\n", my_name);
else if (pipe(i2c_rd_pipe) == -1)          /* open pipe to read data from i2c */
  g_printerr("%s: ERROR - cannot create pipe from i2c.\n", my_name);
else if (fcntl(i2c_wt_pipe[0], F_SETFL, O_NONBLOCK) == -1)
                                           /* designate pipes as non-blocking */
  g_printerr("%s: ERROR - cannot designate write pipe as non blocking.", my_name);
else if (fcntl(i2c_wt_pipe[1], F_SETFL, O_NONBLOCK) == -1)
  g_printerr("%s: ERROR - cannot designate write pipe as non blocking.", my_name);
else if (fcntl(i2c_rd_pipe[0], F_SETFL, O_NONBLOCK) == -1)
  g_printerr("%s: ERROR - cannot designate read pipe as non blocking.", my_name);
else if (fcntl(i2c_rd_pipe[1], F_SETFL, O_NONBLOCK) == -1)
  g_printerr("%s: ERROR - cannot designate read pipe as non blocking.", my_name);
else okay = TRUE;                                          /* So far, so good */

if (okay)
  {                                    /* Bring up the I2C in a child process */
  i2c_PID = fork();
  if (i2c_PID == -1)
    {
    g_printerr("%s: ERROR - cannot start i2c.\n", my_name);
    okay = FALSE;
    }
  else if (i2c_PID == 0)                                     /* Child process */
    {
    /* --------- the i2c is the child process -------- */
    /* ----- close unused pipe ends ---- */
    close(i2c_rd_pipe[0]);	/* read end of caller read pipe */
    close(i2c_wt_pipe[1]);	/* write end of caller write pipe */

    /* --- fork the i2c executable --- */
    /* --- pass pipes as arguments --- */
    sprintf(rps, "%d", i2c_wt_pipe[0]);
    sprintf(wps, "%d", i2c_rd_pipe[1]);
    if (execlp("i2c", "i2c", rps, wps, (char *) NULL) < 0)
      {
      close(i2c_rd_pipe[1]);                          /* Close pipes on error */
      close(i2c_wt_pipe[0]);
      g_printerr("%s: ERROR - cannot start I2C.\n", my_name);
      okay = FALSE;
      }
    }
  else
    {
    /* ------- jimulator is the parent process ------ */
    /* ----- identify pipes and close unused ends ---- */
    close(i2c_rd_pipe[1]);	/* write end of read pipe */
    close(i2c_wt_pipe[0]);	/* read end of write pipe */
    }
  }

return okay;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void terminate_i2c(void)
{
int status;
/* close pipes */
close(i2c_rd_pipe[0]);	/* read end of read pipe */
close(i2c_wt_pipe[1]);	/* write end of write pipe */

/* just in case the screen closed: terminate i2c */
kill (i2c_PID, SIGTERM);
waitpid(i2c_PID, &status, 0);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int read_i2c(int address, int size, struct pollfd *SWI_poll)
{
  /* The i2c master is an 8-bit peripheral */
  /* All i2c master accesses are on (even) half-word addresses */
  /* Word accesses are translated into 2 half-word accesses */
  unsigned char buf;
  unsigned int  addr;
  unsigned int  data;
  unsigned char unaligned;

  addr = address >> 1; /* address bit 0 not connected */
  /* report unaligned accesses to i2c master */
  if ((size != 1) && (address & 0x01) != 0)
	unaligned = 0x40;
  else
	unaligned = 0x00;

  /* send read command/address */
  buf = unaligned | (addr & 0x07);
  while (write(i2c_wt_pipe[1], &buf, 1) == -1)
	comm(SWI_poll); /* retain monitor communications */

  /* receive read data */
  while (read(i2c_rd_pipe[0], &buf, 1) == -1)
	comm(SWI_poll); /* retain monitor communications */

  data = buf;

  if (size == 4) /* word access */
	{ 
    /* send second read command/address */
	buf = unaligned | ((addr + 1) & 0x07);
	while (write(i2c_wt_pipe[1], &buf, 1) == -1)
	  comm(SWI_poll); /* retain monitor communications */

	/* receive read data */
	while (read(i2c_rd_pipe[0], &buf, 1) == -1)
	  comm(SWI_poll); /* retain monitor communications */
	}

  return(data | (buf << 16));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void write_i2c(int address, int size, int data, struct pollfd *SWI_poll)
{
  /* The i2c master is an 8-bit peripheral */
  /* All i2c master accesses are on (even) half-word addresses */
  /* Word accesses are translated into 2 half-word accesses */
  unsigned char buf;
  unsigned int  addr;
  unsigned char unaligned;

  addr = address >> 1; /* half-word address */
  /* report unaligned accesses to i2c master */
  if ((size != 1) && (address & 0x01) != 0)
	unaligned = 0x40;
  else
	unaligned = 0x00;

  /* send write command/address */
  buf = 0x80 | unaligned | (addr & 0x07);
  while (write(i2c_wt_pipe[1], &buf, 1) == -1)
	comm(SWI_poll); /* retain monitor communications */

  /* send 8-bit write data */
  buf = data & 0xff;
  while (write(i2c_wt_pipe[1], &buf, 1) == -1)
	comm(SWI_poll); /* retain monitor communications */

  if (size == 4) /* word access */ 
    {
	/* send second write command/address */
	buf = 0x80 | unaligned | ((addr + 1) & 0x07);
	while (write(i2c_wt_pipe[1], &buf, 1) == -1)
	  comm(SWI_poll); /* retain monitor communications */

	/* send write data */
	buf = (data >> 16) & 0xff;
	while (write(i2c_wt_pipe[1], &buf, 1) == -1)
	  comm(SWI_poll); /* retain monitor communications */
    }

  return;
}

/*                                                                            */
/*---------------------------- end of i2c emulator----------------------------*/
/*----------------------------------------------------------------------------*/
