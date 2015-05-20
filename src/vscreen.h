#include <sys/shm.h>
#include <unistd.h>
#include <sys/signal.h>

/* virtual screen geometry: 480 lines, 640 cloumns, 24-bit pixels */
/* image frame geometry: 240 lines, 320 columns, 15-bit pixels */
#define SCR_KEY      123	/* segment ID sent to screen program */
#define SCR_WIDTH    640	/* default virtual screen width */
#define SCR_HEIGHT   480	/* default virtual screen height */
#define SCR_SCALE      1	/* default virtual screen scaling */
#define SCR_COL      332	/* default virtual screen colour res. */
//#define SCR_NC         3	/* virtual screen channels */
/* size in bytes of the virtual screen buffer */
#define SCR_SZ      (SCR_NC*SCR_WIDTH*SCR_HEIGHT)
//#define SHM_SZ      (SCR_SZ+2)	/* one byte for handshaking and one for LEDs */
//#define LED_SHM     (SCR_SZ+1)	/* pointer to LEDs */

#define IMAGE_WIDTH   320	/* image width (!= screen width) */
#define IMAGE_HEIGHT  240	/* image height (!= screen height) */
#define IMAGE_NC        3	/* image channels (!= screen channels) */

#define WD_RATIO     (SCR_WIDTH / IMAGE_WIDTH)
#define HT_RATIO     (SCR_HEIGHT / IMAGE_HEIGHT)


#define HNDSHK_LED        0x20	 /* These 4 definitions must be the same as   */
#define HNDSHK_IMG_ACK    0x80	 /* used in vscreen.  They are defined there  */
#define HNDSHK_IMG_REQ    0x40	 /* in params.h     @@@                       */
#define HNDSHK_NEW_IMAGE  0x01



pid_t screen_PID;		/* screen process ID (global) */
//int   shm_ID;			/* virtual screen shared memory ID */
unsigned char* shm_ADDR;	/* virtual screen shared memory address */

				/* virtual screen initialisation */
int   init_vscreen (char*, unsigned int, unsigned int, unsigned int, unsigned int,
                           unsigned int, unsigned int, unsigned int,
                           pid_t*, unsigned char**, unsigned int, int);
			     	/* attach to virtual screen shared memory */
int   setup_shm(char*, char*, unsigned int, unsigned int, unsigned char**);
//int   release_shm(char*, char*, char*);

void  sig_handler (int sigNum);	/* inter-process synchronisation */
