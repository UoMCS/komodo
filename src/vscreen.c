/*----------------------------------------------------------------------------*/
/*-------------------------------- virtual screen ----------------------------*/
/*----------------------------------------------------------------------------*/
#include "vscreen.h"
#include <stdio.h>                                           /* for sprintf   */
#include <gtk/gtk.h>                                         /* for MAX & MIN */

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
  sprintf(s_width,  "-w%d", width);          /* Convert parameters to strings */
  sprintf(s_height, "-h%d", height);
  sprintf(s_scale,  "-s%d", scale);
  sprintf(s_colour, "-c%d", colour);
  sprintf(s_key,    "-k%d", key);
  sprintf(s_LEDs,   "-l%d", LEDs);
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

