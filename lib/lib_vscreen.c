/*----------------------------------------------------------------------------*/
/* This file contains example handlers for 'jimulator' support.               */
/* They are somewhat tailored to the CSE project application but are in an    */
/* unfinished state.  Unnecessary diagnostics are also included.              */
/* Compile with "gcc -o <library_name> --shared lib_vscreen.c"                */
/*                                                               JDG 13/9/10  */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#include <stdio.h>
#include "states.h"               /* Definitions used for 'status' checks     */
#include "address_spaces.h"       /* Definitions used for REG_CURRENT et alia */
#include "vscreen.h"

#define TRUE  (0==0)
#define FALSE (0!=0)

typedef int boolean;
typedef unsigned short pixel;

struct pollfd *SVC_poll;         /* Will alias to the main programme variable */

unsigned char *lib_name_vscreen;// Unsafe in that it assumes string will remain @@@

unsigned int vscreen_w, vscreen_h, vscreen_s, vscreen_c, vscreen_l, vscreen_k;
unsigned int pixel_bytes, vscreen_size;
unsigned char *vscreen_base;
unsigned char *p_LEDs, *p_handshake;
pid_t vscreen_PID;
boolean verbose;                      /* inherited from the caller (jimulator */

/*----------------------------------------------------------------------------*/
/* Called once when library opened                                            */

boolean constructor (unsigned char *me, unsigned char *string)
{
boolean okay;
int number;
int refresh;                     /* Can be local as only passed on at startup */

okay = TRUE;

if (verbose)
  {
  fprintf(stderr, "Hello from %s\n", me);
  fprintf(stderr, "%s\n", string);
  }
lib_name_vscreen = me;               /* Example of keeping a 'local' variable */

vscreen_w = SCR_WIDTH;                                            /* Defaults */
vscreen_h = SCR_HEIGHT;
vscreen_s = SCR_SCALE;
vscreen_c = SCR_COL;
vscreen_l = 0;
vscreen_k = SCR_KEY;
refresh   = 54;

while ((*string != ' ') && (*string != '\t') && (*string != '\0')) string++;
                                                        /* Strip library name */
number = get_number(string, &string);
if (number >= 0)                                          /* Width specified? */
  {
  vscreen_w = number;
  vscreen_h = (number * 3) / 4;                                /* New default */
  number = get_number(string, &string);
  if (number >= 0)                                       /* Height specified? */
    {
    vscreen_h = number;
    number = get_number(string, &string);
    if (number >= 0)                                      /* Scale specified? */
      {
      vscreen_s = number;
      number = get_number(string, &string);
      if (number >= 0)                                  /* Colours specified? */
        {
        vscreen_c = number;
        number = get_number(string, &string);
        if (number >= 0)                             /* No of LEDs specified? */
          {
          vscreen_l = number;
          number = get_number(string, &string);
          if (number >= 0)                    /* Shared memory key specified? */
            {
            vscreen_k = number;
            number = get_number(string, &string);
            if (number >= 0)                       /* Refresh rate specified? */
              {
              refresh = number;
              }
            }
          }
        }
      }
    }
  }

{
unsigned int r, g, b;
if (vscreen_c < 10)           /* All colour depths specified as single number */
  { r = vscreen_c; g = vscreen_c; b = vscreen_c; }
else
  {
  r = (vscreen_c / 100) % 10;                  /* Separate colour components  */
  g = (vscreen_c / 10)  % 10;                  /* (Reflects screen behaviour) */
  b =  vscreen_c        % 10;
  }
if (r < 1) r = 1; else if (r > 8) r = 8;                              /* Clip */
if (g < 1) g = 1; else if (g > 8) g = 8;                              /* Clip */
if (b < 1) b = 1; else if (b > 8) b = 8;                              /* Clip */
vscreen_c = 100 * r + 10 * g + b;                 /* Reassemble clipped value */
pixel_bytes = (r + g + b + 7) / 8;                /* Find pixel size in bytes */
vscreen_size = vscreen_w * vscreen_h * pixel_bytes;    /* Frame size excl H/S */
}

if (init_vscreen(me, vscreen_w, vscreen_h, vscreen_s, vscreen_c,
                 pixel_bytes, vscreen_l, vscreen_k,
                 &vscreen_PID, &vscreen_base, refresh, verbose) < 0)
  {
  okay = FALSE;
  }
else
  {
  p_handshake = &vscreen_base[vscreen_size];
  p_LEDs      = &vscreen_base[vscreen_size+1];
  }

return okay;                                  /* FALSE if there was a problem */
}

/*----------------------------------------------------------------------------*/
/* Called once when process terminates                                        */

boolean destructor (unsigned char *me)
{
int status;

 if (verbose) fprintf(stderr, "Goodbye from %s\n", me);

if (shmdt(vscreen_base) < 0)      /* Release the virtual screen shared memory */
  g_printerr("%s: ERROR - cannot release virtual screen.\n", me);

kill(vscreen_PID, SIGTERM);                              /* Terminate vscreen */
waitpid(vscreen_PID, &status, 0);

return TRUE;
}

/*----------------------------------------------------------------------------*/

boolean svc_handler (unsigned int svc_no, unsigned char *status)
{
boolean svc_handled;

svc_handled = TRUE;            /* Assume it is going to be in the list, below */

switch (svc_no)
  {
  case 0x100:                         /* Copy memory buffer to virtual screen */
    {
    unsigned int i, address;
    boolean abort;

    address =  get_reg (0, REG_CURRENT);
    for (i = 0; i < vscreen_size; i++)
      vscreen_base[i] = read_mem(address++, 1, FALSE, FALSE, MEM_SYSTEM, &abort);

    *p_handshake |= HNDSHK_NEW_IMAGE;
    }
    break;


  case 0x101:                         /* Copy virtual screen to memory buffer */
    {
    unsigned int i, address;
    boolean abort;

    address =  get_reg (0, REG_CURRENT);
    for (i = 0; i < vscreen_size; i++)
      write_mem(address++, vscreen_base[i], 1, FALSE, MEM_SYSTEM, &abort);
    }
    break;


  case 0x102:                              /* Request a new image from screen */
    {                                                    /* Request new image */
    *p_handshake |= HNDSHK_IMG_REQ;
    /* Bodge PC so that stall looks `correct' */
    put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
    /* wait for new image in buffer */
    while (((*p_handshake & HNDSHK_IMG_ACK) == 0)
         && (status != CLIENT_STATE_RESET))
      comm(SVC_poll);                        /* Retain monitor communications */
//    svc_comms();                           /* Retain monitor communications */

    *p_handshake &= ~HNDSHK_IMG_REQ;		// ??@@@ complete handshake?

    if (status != CLIENT_STATE_RESET)
      put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);       /* Correct PC */
    }
    break;


  case 0x104:                    /* Write to virtual screen LEDs - data in R0 */
    *p_LEDs = get_reg(0, REG_CURRENT);
    *p_handshake |= HNDSHK_LED;
    break;


  case 0x105:               /* Copy frame in memory to part of virtual screen */
    {       /* R0 frame address                 R1, R2 x, y size of local array
               R3, R4 Screen start coordinates, R5 screen step size           */
    unsigned int faddr;
    unsigned int image_w;
    unsigned int image_h;
    unsigned int screen_x;
    unsigned int screen_y;
    unsigned int step_x;
    unsigned int step_y;
    int          ip;
    int          hr;
    int          wr;
    int          line;
    int          col;
    int          abort;
    unsigned int i;

    faddr    = get_reg(0, REG_CURRENT);         /* Frame address in ARM space */
    image_w  = get_reg(1, REG_CURRENT);         /* Width of local array       */
    image_h  = get_reg(2, REG_CURRENT);         /* Height of local array      */
    screen_x = get_reg(3, REG_CURRENT);         /* LHS of screen display      */
    screen_y = get_reg(4, REG_CURRENT);         /* Top of screen display      */
    step_x   = get_reg(5, REG_CURRENT);         /* Scale up by ...            */
    step_y   = step_x;

    for (line = 0; line < image_h; line++)
      {
      for (col = 0; col < image_w; col++)
        {
        ip = ((screen_y + step_y * line) * vscreen_w
            + (screen_x + step_x * col)) * pixel_bytes;
        for (hr = 0; hr < step_y; hr++)
          {
          if ((screen_y + step_y * line + hr) < vscreen_h)
            for (wr = 0; wr < step_x; wr++)
              {
              if ((screen_x + step_x * col + wr) < vscreen_w)
                {                             /* Copy pixel to virtual screen */
                for (i = 0; i < pixel_bytes; i++)
                  vscreen_base[ip+i] = read_mem(faddr+i, 1, FALSE, FALSE,
                                                MEM_DATA, &abort);
                }
              ip += pixel_bytes;        /* Increment even if write suppressed */
              }
          /* Point to next line in virtual screen */
          ip += (vscreen_w - step_x) * pixel_bytes;
          }
        faddr += pixel_bytes;                          /* Point to next pixel */
        }
      }
    *p_handshake |= HNDSHK_NEW_IMAGE;           /* Trigger outgoing handshake */
    }
    break;


  case 0x106:               /* Copy part of virtual screen to frame in memory */
    {       /* R0 frame address                 R1, R2 x, y size of local array
               R3, R4 Screen start coordinates, R5 screen step size           */
    unsigned int faddr;
    unsigned int image_w;
    unsigned int image_h;
    unsigned int screen_x;
    unsigned int screen_y;
    unsigned int step_x;
    unsigned int step_y;
    int          line;
    int          col;
    int          abort;
    int          ip;
    unsigned int i;

    faddr    = get_reg(0, REG_CURRENT);         /* Frame address in ARM space */
    image_w  = get_reg(1, REG_CURRENT);         /* Width of local array       */
    image_h  = get_reg(2, REG_CURRENT);         /* Height of local array      */
    screen_x = get_reg(3, REG_CURRENT);         /* LHS of screen display      */
    screen_y = get_reg(4, REG_CURRENT);         /* Top of screen display      */
    step_x   = get_reg(5, REG_CURRENT);         /* Sample every ...           */
    step_y   = step_x;

    for (line = 0; line < image_h; line++)
      {
      for (col = 0; col < image_w; col++)
        {
        if (((screen_y + step_y * line) < vscreen_h)
          && ((screen_x + step_x * col) < vscreen_w))
          {
          ip = ((screen_y + step_y * line) * vscreen_w
              + (screen_x + step_x * col)) * pixel_bytes;

          for (i = 0; i < pixel_bytes; i++)
            write_mem(faddr+i, vscreen_base[ip+i], 1, FALSE, MEM_DATA, &abort);
          }
        else
          for (i = 0; i < pixel_bytes; i++)
            write_mem(faddr+i, 0x00, 1, FALSE, MEM_DATA, &abort);    /* Black */
        faddr += pixel_bytes;                          /* Point to next pixel */
        }
      }
    *p_handshake &= ~HNDSHK_IMG_ACK;              /* Clear incoming handshake */
    }
    break;


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

//fprintf(stderr, "?? %08X\n", address);

  mem_handled = FALSE;       /* Not handled: inform caller there's more to do */

return mem_handled;

}

/*----------------------------------------------------------------------------*/

boolean mem_w_handler (unsigned int address, unsigned int data, int size,
                       boolean T, int source, boolean* abort)
{
boolean mem_handled;

//fprintf(stderr, "?? %08X\n", address);

  mem_handled = FALSE;       /* Not handled: inform caller there's more to do */

return mem_handled;

}

/*----------------------------------------------------------------------------*/

/* attach virtual screen shared memory */
int setup_shm(char *my_name, char *info, unsigned int size, unsigned int key,
              unsigned char **addr)
{
int   shm_ID;                              /* virtual screen shared memory ID */

/* locate the virtual screen shared memory buffer */
if ((shm_ID = shmget((key_t) key, size, 0666)) < 0)
  {
  g_printerr("%s: ERROR - cannot locate %s shared memory.\n", my_name, info);
  return(-1);
  }
/* attach the buffer to local data space */
if ((*addr = (unsigned char *) shmat(shm_ID, NULL, 0)) == (unsigned char *) -1)
  {
  g_printerr("%s: ERROR - cannot attach %s shared memory.\n", my_name, info);
  return(-1);
  }
return(0);
}

/*----------------------------------------------------------------------------*/
///* release the virtual screen shared memory */
//int release_shm(char *my_name, char *addr, char *info)
//{
///* release the shared memory buffer that is used as frame store  */
//if (shmdt(addr) < 0)
//  {
////  g_printerr("%s: ERROR - cannot release %s shared memory.\n", my_name, info);
//  return(-1);
//  }
//return(0);
//}
//
/*----------------------------------------------------------------------------*/
/* virtual screen initialisation                                              */

int init_vscreen (char *my_name, unsigned int width,
                                 unsigned int height,
                                 unsigned int scale,
                                 unsigned int colour,
                                 unsigned int pixel_bytes,
                                 unsigned int LEDs,
                                 unsigned int key,
                                 pid_t          *screen_PID,
                                 unsigned char **shm_ADDR,
                                 unsigned int refresh,
                                 int verbose)
{
char s_width[10], s_height[10], s_scale[10], s_colour[10], s_key[10];
char s_LEDs[10], s_refresh[10];					// SIZES ?! @@@@
char *s_verbose;
struct sigaction sig_hndlr;
pid_t new_PID;

sig_hndlr.sa_handler = &sig_handler;    /* Catch exceptions in signal_handler */
sigemptyset(&sig_hndlr.sa_mask);
sig_hndlr.sa_flags = 0;

/* temporarily establish the SIGCONT signal handler to synchronise with the screen */
sigaction(SIGCONT, &sig_hndlr, NULL);
/* establish the SIGCHLD signal handler to terminate with screen */
sigaction(SIGCHLD, &sig_hndlr, NULL);
/* establish the SIGTERM signal handler to terminate screen      */
sigaction(SIGTERM, &sig_hndlr, NULL);

/* bring up the screen in a child process */
new_PID = fork();
if (new_PID == -1)
  {
  g_printerr("%s: ERROR - cannot fork virtual screen.\n", my_name);
  return(-1);
  }
else if (new_PID == 0)
  {
  /* --- the virtual screen is the child process --- */
  /* --- the screen updates from the frame store --- */
  nice(10);                          /* Reduce priority of the screen process */
  sprintf(s_width,   "-w%d", width);         /* Convert parameters to strings */
  sprintf(s_height,  "-h%d", height);
  sprintf(s_scale,   "-s%d", scale);
  sprintf(s_colour,  "-c%d", colour);
  sprintf(s_key,     "-k%d", key);
  sprintf(s_LEDs,    "-l%d", LEDs);
  sprintf(s_refresh, "-r%d", refresh);

  if (verbose) s_verbose = "-v"; else s_verbose = "";   /* Pass on debug flag */
  if (execlp("vscreen", "vscreen", s_width, s_height, s_scale, s_colour, s_key,
                         s_verbose, s_LEDs, s_refresh, NULL) < 0)
    {
    g_printerr("%s: ERROR - cannot start virtual screen.\n", my_name);
    return(-1);
    }
  }
else
  {
  /* ------------ jimulator is the parent process ----------- */

  /* wait until the screen finishes setting up the shared buffer */
  pause();

  /* attach to the shared memory buffer that is used as frame store */
  if (setup_shm(my_name, "virtual screen", (width * height * pixel_bytes) + 2,
                                            key, shm_ADDR) < 0)
    {
    g_printerr("%s: ERROR - cannot find virtual screen.\n", my_name);
//  kill(new_PID, SIGTERM);      /* Failed to attach so kill child process */ 
    return(-1);            /* (Kill seems to happen without the above call?!) */
    }
  }

*screen_PID = new_PID;
return(0);
}
/*                                                                            */
/*-------------------------------- virtual screen ----------------------------*/
/*----------------------------------------------------------------------------*/

