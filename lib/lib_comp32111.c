/*--------------------------------------------*/
/* This file contains jimulator library       */
/* support for the systemC transaction-level  */
/* model used in COMP32111.                   */
/*                                            */
/* Uses shared memory to comunicate with the  */
/* transactor that interfaces with the        */
/* command channel of the drawing engine.     */
/*                                            */
/* Also uses shared memory to access the      */
/* virtual screen pixel buffer via SWI calls. */
/*                                            */
/* version 0.1 04/10/2010-lap                 */
/*                                            */
/*--------------------------------------------*/

#include <stdio.h>
#include "states.h"               /* Definitions used for 'status' checks     */

#define TRUE  (0==0)
#define FALSE (0!=0)

/* --- microController --- */
#define MC_KEY  1234 /* shared memory buffer identifier  */
#define MC_SIZE  256 /* size of the shared memory buffer */
#define SM_ACK_WAIT 0
#define ADR      1   /* register address for command channel          */
#define NCS      2   /* active low chip select for command channel    */
#define NUB      3   /* active low upper byte (half-word transaction) */
#define NOE      4   /* active low output enable (read transaction)   */
#define NWE      5   /* active low write enable (write transaction)   */
#define DATA_l   6   /* data for command channel (low-order byte)     */
#define DATA_h   7   /* data for command channel (high-order byte)    */

/* --- virtual screen --- */
#define VS_KEY  2345        /* shared memory buffer identifier  */
#define VS_SIZE (640*480)+1 /* size of the shared memory buffer */

typedef int boolean;

boolean verbose;                     /* inherited from the caller (jimulator) */
char  *mc_shm;
char  *vs_shm;

/*----------------------------------------------------------------------------*/
/* Called once when library opened                                            */

boolean constructor (unsigned char *me, unsigned char *string)
{
boolean okay;
int shmid;

if (verbose) {
  g_printerr("Hello from %s\n", me);
  g_printerr("%s\n", string);
}

okay = TRUE;

/* locate the shared memory buffer that communicates with the transactor      */
if ((shmid = shmget(MC_KEY, MC_SIZE, 0666)) < 0) {
    g_printerr("%s: ERROR - cannot locate MC shared memory.\n", me);
    okay = FALSE;
}
/* attach the segment to local data space */
if ((mc_shm = (char *) shmat(shmid, NULL, 0)) == (char *) -1) {
    g_printerr("%s: ERROR - cannot attach MC shared memory.\n", me);
    okay = FALSE;
}

/* locate the shared memory buffer that used as virtual screen frame store    */
if ((shmid = shmget(VS_KEY, VS_SIZE, 0666)) < 0) {
    g_printerr("%s: ERROR - cannot locate VS shared memory.\n", me);
    okay = FALSE;
}
/* attach the segment to local data space */
if ((vs_shm = (char *) shmat(shmid, NULL, 0)) == (char *) -1) {
    g_printerr("%s: ERROR - cannot attach VS shared memory.\n", me);
    okay = FALSE;
}

return okay;                                  /* FALSE if there was a problem */
}

/*----------------------------------------------------------------------------*/
/* Called once when process terminates                                        */

boolean destructor (unsigned char *me)
{
int status;

if (verbose) fprintf(stderr, "Goodbye from %s\n", me);

/* Release the emulator shared memory */
if (shmdt(mc_shm) < 0)
  g_printerr("%s: ERROR - cannot release MC shared memory.\n", me);

/* Release the virtual screen shared memory */
if (shmdt(vs_shm) < 0)
  g_printerr("%s: ERROR - cannot release VS shared memory.\n", me);

return TRUE;
}

/*----------------------------------------------------------------------------*/

boolean svc_handler (unsigned int svc_no, unsigned char *status) {
boolean svc_handled;

svc_handled = TRUE;            /* Assume it is going to be in the list, below */

switch (svc_no) {
  case 0x11:                                                             /* Halt */
    *status = CLIENT_STATE_BYPROG;
    break;

//  case 0x100: {
//    FILE *out_file;
//    unsigned int x, y;
//    char p, r, g, b;
//    float rr, gg, bb;
//
//    if ((out_file = fopen("vsDump.txt", "w")) == NULL) {
//        g_printerr("lib_comp32111: Warning - could not open dump file 'vsDump.txt'.\n");
//    } else {
//        /* write non-BLACK pixels to dump file */
//        /* pixels are "dumped" screen line by  */
//        /* screen line as they are "scanned"   */
//        fprintf(out_file, "    X    Y   R%%  G%%  B%%\n");
//        for (y=0; y<480; y++)
//            for (x=0; x<640; x++)
//                if ((p = vs_shm[640*y+x]) != 0) {
//                    r = (p >> 5) & 7;
//                    g = (p >> 2) & 7;
//                    b = p & 3;
//                    rr = 100.0 * (r/7);
//                    gg = 100.0 * (g/7);
//                    bb = 100.0 * (b/3);
//                    fprintf(out_file,"%5d%5d %4.0f%4.0f%4.0f\n", j, i, rr, gg, bb);
//                }
//        fclose(out_file);
//    }
//    break;
//  }

  default:
    svc_handled = FALSE;     /* Not handled: inform caller there's more to do */
    break;
}

return svc_handled;
}

/*----------------------------------------------------------------------------*/

boolean mem_r_handler (unsigned int address, unsigned int *data, int size,
                       boolean sign, boolean T, int source, boolean* abort)
{
boolean mem_handled;
unsigned int value;

  if ((address >> 28) == 3) {
    if (size != 2) {
      g_printerr("lib_comp32111: Warning - unsupported size memory access.\n");
    } else {
      mc_shm[ADR] = address;
      mc_shm[NUB] = 0;
      mc_shm[NOE] = 0;
      mc_shm[NWE] = 1;
      /* ncs = 0 triggers the transaction in the TLM model */
      mc_shm[NCS] = 0;

      /* wait for transactor acknowledge */
      while (mc_shm[NCS] == 0 || mc_shm[NOE] == 0 || mc_shm[NWE] == 0) {usleep(SM_ACK_WAIT);}

      value = mc_shm[DATA_l];
      value = (value & 0x000000ff) | (mc_shm[DATA_h] << 8);
      *data = value & 0x0000ffff;
    }
    mem_handled = TRUE;
  } else {
    mem_handled = FALSE;       /* Not handled: inform caller there's more to do */
  }
*abort = FALSE;
return mem_handled;

}

/*----------------------------------------------------------------------------*/

boolean mem_w_handler (unsigned int address, unsigned int data, int size,
                       boolean T, int source, boolean* abort)
{
boolean mem_handled;

  if ((address >> 28) == 3) {
    if (size != 2) {
      g_printerr("lib_comp32111: Warning - unsupported size memory access.\n");
    } else {
      mc_shm[ADR] = address;
      mc_shm[NUB] = 0;
      mc_shm[NOE] = 1;
      mc_shm[NWE] = 0;
      mc_shm[DATA_l] = data;
      mc_shm[DATA_h] = data >> 8;
      /* ncs = 0 triggers the transaction in the TLM model */
      mc_shm[NCS] = 0;

      /* wait for transactor acknowledge */
      while (mc_shm[NCS] == 0 || mc_shm[NOE] == 0 || mc_shm[NWE] == 0) {usleep(SM_ACK_WAIT);}
    }
    mem_handled = TRUE;
    *abort = FALSE;
  } else {
    mem_handled = FALSE;       /* Not handled: inform caller there's more to do */
  }
*abort = FALSE;
return mem_handled;

}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
