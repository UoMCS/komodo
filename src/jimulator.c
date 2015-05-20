/******************************************************************************/
/*                Name:           jimulator.c                                 */
/*                Version:        1.5.3                                       */
/*                Date:           2/9/2010                                    */
/*                Emulation library for KMD                                   */
/*                                                                            */
/*============================================================================*/
#include <stdio.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <glib.h>
#include <dlfcn.h>     /* Dynamic library support for importing memory models */

#include "definitions.h"
#include "interface.h"
#include "address_spaces.h"

#define NO_OF_BREAKPOINTS     32  /* Max 32 */
#define NO_OF_WATCHPOINTS      4  /* Max 32 */

#define MAX_CONFIG_LINE       80

#define VERBOSE FALSE
//#define VERBOSE TRUE

/*  uses JDG's arm_v3

notes:

  random changed to rand to allow linking
  N.B. check rand as may not produce 32-bit no.s


needs:

  1. long multiply  - drafted BUT NOT TESTED
  3. more thumb testing

lsl routine added but not fully tested (28/10/98)
Long multiplication written (27/1/00)
Architecture V5 added (2/2/00)

To do:
   check long muls
         Flag checking (immediates ?!)
      Validation
   interrupt enable behaviour on exceptions (etc.)
*/


/* NB "int" is assumed to be at least 32-bit */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define RING_BUF_SIZE 64

typedef struct
{
unsigned int iHead;
unsigned int iTail;
unsigned char buffer[RING_BUF_SIZE];
}
ring_buffer;


struct pollfd pollfd;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/* Local prototypes */

void clean_up_1(pid_t);
void clean_up_2(void);
void print_help(void);
void step(void);
void comm(struct pollfd*);

int emulsetup (void);
void save_state(uchar);
void initialise (unsigned int start_address, int initial_mode);
void execute (unsigned int op_code);


/* ARM execute */
void data_op (unsigned int);
void clz (unsigned int);
void transfer (unsigned int);
void transfer_sbhw (unsigned int);
void multiple (unsigned int);
void branch (unsigned int);
void coprocessor (unsigned int);
void my_system (unsigned int);
boolean svc_char_out(char);
boolean svc_char_in(char*);
void wr_LEDs(char);
//void svc_comms(void);
void breakpoint ();
void undefined ();
void data_abort();

void mrs (unsigned int op_code);
void msr (unsigned int op_code);
void bx (unsigned int Rm, int link);
void my_multi (unsigned int op_code);
void swap (unsigned int op_code);
void normal_data_op (unsigned int op_code, int operation);
void ldm (int mode, int Rn, int reg_list, boolean write_back, boolean hat);
void stm (int mode, int Rn, int reg_list, boolean write_back, boolean hat);

int transfer_offset (int op2, int add, int imm, boolean sbhw);

int b_reg (int op2, int *cf);
int b_immediate (int op2, int *cf);

int bit_count (unsigned int source, int *first);

int check_cc (int condition);

int not (int x);
int and (int x, int y);
int or (int x, int y);
int xor (int x, int y);
int zf (int cpsr);
int cf (int cpsr);
int nf (int cpsr);
int vf (int cpsr);

void set_flags (int operation, int a, int b, int rd, int carry);
void set_NZ (unsigned int value);
void set_CF (unsigned int a, unsigned int rd, int carry);
void set_VF_ADD (int a, int b, int rd);
void set_VF_SUB (int a, int b, int rd);
int get_reg (int reg_no, int force_mode);
                                     /* Returns PC+4 for ARM & PC+2 for Thumb */
int get_reg_monitor (int reg_no, int force_mode);
void put_reg (int reg_no, int value, int force_mode);
int instruction_length ();

unsigned int fetch (boolean *);
void inc_pc ();
void endian_swap (unsigned int start, unsigned int end);
int  read_mem (unsigned int address, int size, boolean sign, boolean T, int source, boolean *abort);
void write_mem(unsigned int address, int data, int size,     boolean T, int source, boolean *abort);


/* THUMB execute */
void data0 (unsigned int op_code);
void data1 (unsigned int op_code);
void data_transfer (unsigned int op_code);
void transfer0 (unsigned int op_code);
void transfer1 (unsigned int op_code);
void sp_pc (unsigned int op_code);
void lsm_b (unsigned int op_code);
void thumb_branch (unsigned int op_code);


int load_fpe ();
void fpe_install ();

int check_watchpoints(unsigned int address, int data, int size, int direction);

int lsl (int value, int distance, int *cf);
int lsr (unsigned int value, int distance, int *cf);
int asr (int value, int distance, int *cf);
int ror (unsigned int value, int distance, int *cf);

unsigned int getmem32 (int number);
void setmem32 (int number, unsigned int reg);
void execute_instruction (void);

int emul_getchar (unsigned char *to_get);
int emul_sendchar (unsigned char to_send);
int emul_sendbN (int value, int N);
int emul_getbN(int *val_ptr, int N);
int emul_getchararray (int char_number, unsigned char *data_ptr);
int emul_sendchararray (int char_number, unsigned char *data_ptr);

void boardreset (void);

void init_buffer(ring_buffer*);
int count_buffer(ring_buffer*);
boolean put_buffer(ring_buffer*, unsigned char);
int get_buffer(ring_buffer*, unsigned char*);
boolean set_options(int argc, char *argv[], boolean*);    /* handling options */

char   *get_name_string(int *p_argc, char **p_argv[]);
int     get_number(char*, char**);
void    append(char*, char*, unsigned int);
boolean input_line(FILE*, char*, unsigned int); 

boolean link_so_handler(char*, boolean, char*);
void    link_shared_mem(unsigned int, unsigned int, unsigned int);
void    free_so_handler_list(void);
void    free_shared_mem_list(void);
int     release_shm(char*, char*, char*);

/* signal handler to synchronise with child processes */
void  sig_handler (int sigNum);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

struct sigaction sig_hndlr;

typedef boolean (* constructor) (uchar*, uchar*);
typedef boolean (* destructor)  (uchar*);
typedef boolean (* svccall)   (unsigned int, uchar*);
typedef boolean (* memrcall)  (unsigned int, unsigned int*, int, boolean, boolean, int, boolean*);
typedef boolean (* memwcall)  (unsigned int, unsigned int,  int, boolean, int, boolean*);
typedef boolean (* coprocall) (unsigned int, unsigned int);
typedef boolean (* sigcall)   (int);

typedef struct handler_entry_name
{
constructor                constructor;
destructor                 destructor;
svccall                    svc_handler;
memrcall                   mem_r_handler;
memwcall                   mem_w_handler;
coprocall                  copro_handler;
sigcall                    signal_handler;
void                      *lib;
boolean                    has_string;
unsigned char             *name;
struct handler_entry_name *next;
}
so_handler_entry;

so_handler_entry *so_handler_list;

typedef struct shared_mem_entry_name
{
unsigned int                  key;
unsigned int                  base;        /* Location in virtual address map */
unsigned int                  top;
unsigned int                  size;
boolean                       owner;
unsigned char                *addr;              /* Pointer to 'real' address */
struct shared_mem_entry_name *next;
}
shared_mem_entry;

shared_mem_entry *shared_mem_list;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/* <lap change> */
/*----------------------------------------------------------------------------*/
/*------------------------------ LAP's functions -----------------------------*/
/*----------------------- virtual screen and <excised> -----------------------*/
/*----------------------------------------------------------------------------*/
//#define VSCREEN_SUPPORT

int   use_vscreen = FALSE;  /* can be modified by set_options */

/* conditionally include support for virtual screen   */
#ifdef VSCREEN_SUPPORT
  #include "vscreen.h"
#endif

/*                             end of lap's functions                         */
/*----------------------------------------------------------------------------*/
/* </lap change> */

#define MEM_SIZE     0X100000                                       /* 1Mbyte */
#define MEM_MIN      0x400                 /* Smallest allowed default memory */
#define RAMSIZE      0X100000                                       /* 1Mbyte */
// Why add "RAMSIZE", and then get it wrong?!?!  @@@
// Memory is modulo this to the monitor; excise and use the proper routines @@@
// Make memory size into passed parameter @@@

//#define RESERVED_MEM 0X002000                                     /* 32Kbytes */
//#define USER_STACK   (MEM_SIZE - RESERVED_MEM) << 2
//#define START_STRING_ADDR 0X00007000                            /*ARM address */

//#define MAX_INSTRUCTIONS 10000000

#define NF_MASK   0X80000000
#define ZF_MASK   0X40000000
#define CF_MASK   0X20000000
#define VF_MASK   0X10000000
#define IF_MASK   0X00000080
#define FF_MASK   0X00000040
#define MODE_MASK 0X0000001F
#define TF_MASK   0X00000020                                     /* THUMB bit */

#define BIT_31    0X80000000
#define BIT_0     0X00000001

#define IMM_MASK      0X02000000                     /* orginal word versions */
#define IMM_HW_MASK   0X00400000                        /* half word versions */
#define DATA_OP_MASK  0X01E00000                         /* ALU function code */
#define DATA_EXT_MASK 0X01900000                  /* To sort out CMP from MRS */
#define ARITH_EXT     0X01000000                /* Poss. arithmetic extension */
#define S_MASK        0X00100000
#define RN_MASK       0X000F0000
#define RD_MASK       0X0000F000
#define RS_MASK       0X00000F00
#define RM_MASK       0X0000000F
#define OP2_MASK      0X00000FFF
//#define HW_MASK     0X00000020
//#define SIGN_MASK   0X00000040

#define MUL_MASK      0X0FC000F0
#define LONG_MUL_MASK 0X0F8000F0
#define MUL_OP        0X00000090
#define LONG_MUL_OP   0X00800090
#define MUL_ACC_BIT   0X00200000
#define MUL_SIGN_BIT  0X00400000
#define MUL_LONG_BIT  0X00800000

//#define SBHW_MASK   0X0E000FF0

#define SWP_MASK      0X0FB00FF0
#define SWP_OP        0X01000090

#define PRE_MASK        0X01000000
#define UP_MASK         0X00800000
#define BYTE_MASK       0X00400000
#define WRITE_BACK_MASK 0X00200000
#define LOAD_MASK       0X00100000
//#define BYTE_SIGN     0X00000080
//#define HW_SIGN       0X00008000

#define USER_MASK       0X00400000

#define LINK_MASK       0X01000000
#define BRANCH_FIELD    0X00FFFFFF
#define BRANCH_SIGN     0X00800000

#define UNDEF_MASK      0X0E000010
#define UNDEF_CODE      0X06000010

#define FLAG_ADD    1
#define FLAG_SUB    2

#define user_mode   0X00000010
#define fiq_mode    0X00000011
#define irq_mode    0X00000012
#define sup_mode    0X00000013
#define abt_mode    0X00000017
#define undef_mode  0X0000001B
#define system_mode 0X0000001F

/*----------------------------------------------------------------------------*/

//#define REGSIZE 65536
#define uchar unsigned char

typedef struct
{
  int state;
  char cond;
  char size;
  int addra;
  int addrb;
  int dataa[2];
  int datab[2];
}
BreakElement;

/*----------------------------------------------------------------------------*/
/* Global data */

#define ID_FEATURES 1
#define ID_MEM_SEGS 1
#define ID_STR_LEN (8 + 3 * ID_FEATURES + 8 * ID_MEM_SEGS)

uchar *ID_string;

BreakElement breakpoints[NO_OF_BREAKPOINTS];
BreakElement watchpoints[NO_OF_WATCHPOINTS];

unsigned int emul_bp_flag[2];
unsigned int emul_wp_flag[2];

unsigned int memory_size;	// NEW - under commissioning process @@@
//uchar memory[RAMSIZE];       // @@@
uchar *memory;

uchar status, old_status;
unsigned int steps_togo;/*Number of left steps before halting (0 is infinite) */
unsigned int steps_reset;                 /* Number of steps since last reset */
char runflags;
uchar rtf;
boolean breakpoint_enable;                     /* Breakpoints will be checked */
boolean breakpoint_enabled;                /* Breakpoints will be checked now */
boolean run_through_BL;                          /* Treat BL as a single step */
boolean run_through_SVC;                        /* Treat SVC as a single step */

int r[16];
int fiq_r[7];
int irq_r[2];
int sup_r[2];
int abt_r[2];
int undef_r[2];
unsigned int cpsr;
unsigned int spsr[32];          /* Lots of wasted space - safe for any "mode" */

boolean print_out;
int run_until_PC, run_until_SP, run_until_mode;     /* Used to determine when */
uchar run_until_status;      /*   to finish a `stepped' subroutine, SVC, etc. */

unsigned int exception_para[9];

int next_file_handle;
FILE *(file_handle[20]);

int count;

unsigned int last_addr;

int glob1, glob2;

int past_opc_addr[32];        /* History buffer of fetched op. code addresses */
int past_size;                                         /* Used size of buffer */
int past_opc_ptr;                                        /* Pointer into same */
int past_count;                       /* Count of hits in instruction history */

/* Thumb stuff */
int PC;
int BL_prefix, BL_address;
int next_char;
int ARM_flag;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean verbose;
char   *my_name;
pid_t   my_parent;

struct pollfd *SVC_poll;    /* Pointer to allow SVCs to scan input - YUK! @@@ */ 

char  *config_filename;
FILE  *config_handle;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#ifdef VSCREEN_SUPPORT
unsigned int vscreen_ww, vscreen_hh, vscreen_ss, vscreen_cc, vscreen_ll, vscreen_kk;
unsigned int pixel_bytess, vscreen_sizee;
unsigned char *pp_LEDs, *pp_handshake;
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

//  int emul_bp_flag[2];
//  int emul_wp_flag[2];

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Terminal data structures                                                   */

ring_buffer terminal0_Tx, terminal0_Rx;
ring_buffer terminal1_Tx, terminal1_Rx;
ring_buffer *terminal_table[16][2];			// @@@

/*----------------------------------------------------------------------------*/
/* Entry point                                                                */

int main(int argc, char *argv[])
{

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

{
int  i;

for (i = 0; i < 16; i++)				// @@
  { terminal_table[i][0] = NULL; terminal_table[i][1] = NULL; }
init_buffer(&terminal0_Tx);                            /* Initialise terminal */
init_buffer(&terminal0_Rx);
terminal_table[0][0] = &terminal0_Tx;
terminal_table[0][1] = &terminal0_Rx;
init_buffer(&terminal1_Tx);                            /* Initialise terminal */
init_buffer(&terminal1_Rx);
terminal_table[1][0] = &terminal1_Tx;
terminal_table[1][1] = &terminal1_Rx;
}

boolean okay, help;


verbose = VERBOSE;                                             /* Set default */
my_parent = getppid();

sig_hndlr.sa_handler = &sig_handler;    /* Catch exceptions in signal_handler */
sigemptyset(&sig_hndlr.sa_mask);
sig_hndlr.sa_flags = 0;

sigaction(SIGINT,  &sig_hndlr, NULL);                                 /*  ^C  */
sigaction(SIGQUIT, &sig_hndlr, NULL);                                 /*  ^\  */
sigaction(SIGTERM, &sig_hndlr, NULL);                                 /* kill */
sigaction(SIGCONT, &sig_hndlr, NULL);                              /* Restart */
sigaction(SIGCHLD, &sig_hndlr, NULL);                     /* Child terminated */
sigaction(SIGSEGV, &sig_hndlr, NULL);                           /* Seg. fault */

pollfd.fd = 0;
pollfd.events = POLLIN;
SVC_poll = &pollfd;		// Grubby pass to "my_system" @@@

so_handler_list = NULL;                         /* No SVCs trapped as default */
shared_mem_list = NULL;                 /* Dynamically linked shared memories */

my_name = argv[0];                          /* Keep handy pointer to filename */

emulsetup();

okay = set_options(argc, argv, &help);                /* Returns TRUE if okay */

memory_size = MAX(MEM_MIN, memory_size);

if (verbose)
  {
  g_printerr("I am %s: my process ID is %d and my parent is %d\n",
                       my_name, getpid(), my_parent);
  if (!okay) g_printerr("There is something amiss and I will shut down.\n");
  g_printerr("Setup options: 0x%X bytes main memory\n", memory_size);

  if (shared_mem_list != NULL)
    {
    shared_mem_entry *p_current;

    g_printerr("               Shared memories found\n");
    g_printerr("                  Address       Size  Key\n");

    p_current = shared_mem_list;                             /* Start of list */

    while (p_current != NULL)
      {
      g_printerr("               0x%08X 0x%08X  %d\n", p_current->base,
                                                      p_current->size,
                                                      p_current->key);
      p_current = p_current->next;                   /* Move to next in chain */
      }
    }

  if (so_handler_list != NULL)
    {
    so_handler_entry *p_current;

    g_printerr("\n               Library handlers found\n");
    g_printerr("               const  dest   svc  memr  memw coproc  sig   Library\n");

    p_current = so_handler_list;                             /* Start of list */

    while (p_current != NULL)
      {
      g_printerr("               ");
      if (p_current->constructor    != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      if (p_current->destructor     != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      if (p_current->svc_handler    != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      if (p_current->mem_r_handler  != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      if (p_current->mem_w_handler  != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      if (p_current->copro_handler  != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      if (p_current->signal_handler != NULL) fprintf(stderr, "   X  ");
      else                                   fprintf(stderr, "      ");
      fprintf(stderr, "  %s\n", p_current->name);
      p_current = p_current->next;                   /* Move to next in chain */
      }
    }
#ifdef VSCREEN_SUPPORT
  if (use_vscreen)
    {
    g_printerr("\n               Screen %d x %d  RGB:%d  key %d", vscreen_ww,
                                                                  vscreen_hh,
                                                                  vscreen_cc,
                                                                  vscreen_kk);
    if (vscreen_ll > 0) g_printerr(" ... plus %d LEDs", vscreen_ll);
    g_printerr("\n\n");
    }
#endif
  }

if (help) { print_help(); okay = FALSE; }            /* Help option selected? */

if (okay)
  {
#ifdef VSCREEN_SUPPORT
  /* if required start virtual screen */
  if (use_vscreen)
    {
    unsigned int r, g, b;
    if (vscreen_cc < 10)      /* All colour depths specified as single number */
      { r = vscreen_cc; g = vscreen_cc; b = vscreen_cc; }
    else
      {
      r = (vscreen_cc / 100) % 10;             /* Separate colour components  */
      g = (vscreen_cc / 10)  % 10;             /* (Reflects screen behaviour) */
      b =  vscreen_cc        % 10;
      }
    r = MAX(1, MIN(8, r));                                            /* Clip */
    g = MAX(1, MIN(8, g));
    b = MAX(1, MIN(8, b));
    vscreen_cc = 100 * r + 10 * g + b;            /* Reassemble clipped value */
    pixel_bytess = (r + g + b + 7) / 8;           /* Find pixel size in bytes */
    vscreen_sizee = vscreen_ww*vscreen_hh*pixel_bytess;/* Frame size excl H/S */
  
    if (init_vscreen(my_name, vscreen_ww, vscreen_hh, vscreen_ss, vscreen_cc,
                     pixel_bytess, vscreen_ll, vscreen_kk, &screen_PID, &shm_ADDR,
                     0, verbose) < 0)
      {
      okay = FALSE;
      }
    else
      {
      pp_handshake = &shm_ADDR[vscreen_sizee]; /* Byte just after frame buffer */
      pp_LEDs      = &shm_ADDR[vscreen_sizee+1];/* Byte two after frame buffer */
      }
    }
#endif

  }

if (okay)                      /* Still surviving? Finish allocation and loop */
  {
  uchar *p_temp;
  ID_string = malloc(ID_STR_LEN);

  ID_string[0] =   ID_STR_LEN - 1;           /* Length of rest of record HERE */
  ID_string[1] =  (ID_STR_LEN - 3) & 0xFF;   /* Length of rest of message (H) */
  ID_string[2] = ((ID_STR_LEN - 3) >> 8) & 0xFF;
  ID_string[3] = 1;                                  /* Processor type (B, H) */
  ID_string[4] = 0;
  ID_string[5] = 0;

  p_temp = &ID_string[6];
  *p_temp++ = ID_FEATURES;                               /* Feature count (B) */
  *p_temp++ = 0;                                         /* Feature ID (B, H) */
  *p_temp++ = 9;
  *p_temp++ = 0;

  *p_temp++ = ID_MEM_SEGS;                        /* Memory segment count (B) */
  *p_temp++ = 0;                                /* Memory segment address (W) */
  *p_temp++ = 0;
  *p_temp++ = 0;
  *p_temp++ = 0;
  *p_temp++ =  memory_size & 0xFF;               /* Memory segment length (W) */
  *p_temp++ = (memory_size >> 8)  & 0xFF;
  *p_temp++ = (memory_size >> 16) & 0xFF;
  *p_temp++ = (memory_size >> 24) & 0xFF;

  memory = malloc(memory_size);


/* These can cause compiler warnings; C does not understand shifts properly.  */
/* They are safe.                                                             */
//emul_bp_flag[0] = 0; emul_bp_flag[1] = (1 << NO_OF_BREAKPOINTS) - 1;
//emul_wp_flag[0] = 0; emul_wp_flag[1] = (1 << NO_OF_WATCHPOINTS) - 1;
  emul_bp_flag[0] = 0;
  if (NO_OF_BREAKPOINTS == 0) emul_bp_flag[1] = 0x00000000;  /* C work around */
  else emul_bp_flag[1] = ((1 << (NO_OF_BREAKPOINTS - 1)) << 1) - 1;
  emul_wp_flag[0] = 0;
  if (NO_OF_WATCHPOINTS == 0) emul_wp_flag[1] = 0x00000000;  /* C work around */
  else emul_wp_flag[1] = ((1 << (NO_OF_WATCHPOINTS - 1)) << 1) - 1;

  while (TRUE)                                                   /* Main loop */
    {
//    comm(&pollfd);                               /* Check for monitor command */
    comm(SVC_poll);                               /* Check for monitor command */
    if ((status & CLIENT_STATE_CLASS_MASK) == CLIENT_STATE_CLASS_RUNNING)
      step();                                    /* Step emulator as required */
    else
//      poll(&pollfd,1,-1); /* If not running, deschedule until command arrives */
      poll(SVC_poll,1,-1); /* If not running, deschedule until command arrives */
    }
  }

else
  clean_up_1(my_parent);/* Failed to set everything up; undo things and finish*/

if (verbose) fprintf(stderr, "Goodbye from %d\n", getpid());

exit(0);/*  Here only if error in start up; 'correct' termination via signal. */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Dispose of dynamically linked handlers and shared memory.  Signal parent.  */

void clean_up_1(pid_t parent)
{
if (parent != 0) kill(parent, SIGTERM);   /* Ask parent program to terminate. */
free_so_handler_list();   /* Free handler space, calling destructors first.   */
free_shared_mem_list();   /* Unlink from shared memories and dispose of list. */
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Free memory which was allocated for Komodo.                                */

void clean_up_2(void) { free(memory); free(ID_string); return; }

/*----------------------------------------------------------------------------*/

void print_help(void)
{
g_printerr("Help for %s ARM emulator\n", my_name);
g_printerr("%s <options>\n", my_name);
g_printerr("\n");
g_printerr("Options: -h            Help.  This is it.\n");
g_printerr("         -l <filename> Link Library file\n");
g_printerr("                       Any number of library files can be specified.\n");
g_printerr("                       They are scanned in the same order as specified.\n");
g_printerr("                       Libraries may specify handler routines\n");
g_printerr("                        as described below.\n");
g_printerr("         -m <size>     Specify main Memory size (default: 0x%X", MEM_SIZE);
if (memory_size != MEM_SIZE) g_printerr(" (0x%X))\n", memory_size);
else                         g_printerr(")\n");
g_printerr("                       This memory starts at address 0x00000000\n");
g_printerr("                       (The minimum size is 0x%X)\n", MEM_MIN);
g_printerr("                       <Not currently changeable>\n");			// @@@
g_printerr("         -s <key> <base> <size>\n");
g_printerr("                       Specify Shared memory block.  These blocks intercept memory\n");
g_printerr("                        references after library handlers.  If overlapped the first\n");
g_printerr("                        one specified wins.\n");
g_printerr("                       Caution: do not overlap boundaries with references.\n");
g_printerr("                        Checking relies on only a single byte address.\n");
#ifdef VSCREEN_SUPPORT
g_printerr("         -v {<width> {<height> {<scale> {<colours> {<LEDs> {<key>}}}}}}\n");
g_printerr("                       Start virtual screen.\n");
g_printerr("                       Defaults: Width  %4d", SCR_WIDTH);
if (use_vscreen && (vscreen_ww != SCR_WIDTH))  g_printerr("   (%d)", vscreen_ww);
g_printerr("\n                                 Height %4d", SCR_HEIGHT);
if (use_vscreen && (vscreen_hh != SCR_HEIGHT)) g_printerr("   (%d)", vscreen_hh);
g_printerr("\n                                 Scale  %4d", SCR_SCALE);
if (use_vscreen && (vscreen_ss != SCR_SCALE))  g_printerr("   (%d)", vscreen_ss);
g_printerr("\n                                 Colours %3d", SCR_COL);
if (use_vscreen && (vscreen_cc != SCR_COL))    g_printerr("   (%d)", vscreen_cc);
g_printerr("\n                                 LEDs   none");
if (use_vscreen && (vscreen_ll != 0))          g_printerr("   (%d)", vscreen_ll);
g_printerr("\n                                 Shared memory key %d", SCR_KEY);
if (use_vscreen && (vscreen_kk != SCR_KEY))    g_printerr(" (%d)", vscreen_kk);
g_printerr("\n");
g_printerr("                       The list can be overwritten from the start.\n");
g_printerr("                       Setting just the width changes the height in a 4:3 ratio.\n");
g_printerr("                       Colours are specified in bits/pixel.\n");
g_printerr("                                 <colours>  < 10 gives RGB at that resolution\n");
g_printerr("                                 <colours> >= 10 gives RGB at xyz bits/pixel\n");
g_printerr("                                              1 <= bits/pixel <= 8\n");
g_printerr("                       From 0 to 8 status LEDs can be specified.\n");
g_printerr("                       The shared memory key is (under development)\n");
#endif
g_printerr("         -o <filename> Read config. file.\n");
g_printerr("                       The (single) config. file can contain the -l -m -s -v\n");
g_printerr("                        options at one per line.\n");
g_printerr("                       The config. file is processed -after- the command line options.\n");
g_printerr("\n");
g_printerr("Library handlers\n");
g_printerr("~~~~~~~~~~~~~~~~\n");
g_printerr("The headers for library handlers are given below.\n");
g_printerr("These are followed by the description of 'helper' routines the handlers can call.\n");
g_printerr("All handlers return a boolean (int) with TRUE indicating that no further action\n");
g_printerr("should be taken for this particular call.\n");
g_printerr("\n");
g_printerr("boolean svc_handler   (unsigned int svc_no,     // SVC number\n");
g_printerr("                       unsigned char *status);  // (see below)\n");
g_printerr("boolean mem_r_handler (unsigned int address,    // Memory address to read\n");
g_printerr("                       unsigned int *data,      // Pointer to returned data\n");
g_printerr("                       int size,                // Access size {1, 2, 4} bytes\n");
g_printerr("                       boolean sign,            // TRUE for 32-bit sign extension\n");
g_printerr("                       boolean T,               // TRUE to force user space\n");
g_printerr("                       int source,              // Source of request\n");
g_printerr("                       boolean *abort);         // Change to TRUE to abort\n");
g_printerr("boolean mem_w_handler (unsigned int address,    // Memory address to read\n");
g_printerr("                       unsigned int data,       // Data to write\n");
g_printerr("                       int size,                // Access size {1, 2, 4} bytes\n");
g_printerr("                       boolean T,               // TRUE to force user space\n");
g_printerr("                       int source,              // Source of request\n");
g_printerr("                       boolean *abort);         // Change to TRUE to abort\n");
g_printerr("boolean copro_handler (unsigned int copro,      // Coprocessor number\n");
g_printerr("                       unsigned int instr);     // Executed instruction\n");
g_printerr("\n");
g_printerr("                       'status' is the current simulator operating condition\n");
g_printerr("                       *** more details TBA ***\n");		// @@@
g_printerr("                       In particular, cases involving external reset may\n");
g_printerr("                       require testing for.\n");
g_printerr("\n");
g_printerr("                       'source' should be one of:\n");
g_printerr("                       {MEM_SYSTEM, MEM_INSTRUCTION, MEM_DATA} (= {0, 1, 2})\n");
g_printerr("                       and reflects the instigator of the access\n");
g_printerr("                       {monitor, instruction fetch, data access}.\n");
g_printerr("                       Definitions may be found in 'address_spaces.h'\n");
g_printerr("\n");
g_printerr("If the following routines exist they are called once for each library specified.\n");
g_printerr("\n");
g_printerr("boolean constructor   (unsigned char *name,     // Library name\n");
g_printerr("                       unsigned char *params);  // Name + parameter list (volatile)\n");
g_printerr("                       Called at start-up time.\n");
g_printerr("boolean destructor    (unsigned char *name);    // Library name\n");
g_printerr("                       Called at programme termination.\n");
g_printerr("                       These both return TRUE if unless there are errors.\n");
g_printerr("\n");
g_printerr("Helper functions\n");
g_printerr("~~~~~~~~~~~~~~~~\n");
g_printerr("The following may be called from within libraries.\n");
g_printerr("Care should be taken not to set up non-terminating recursive calls.\n");
g_printerr("Particular facilities (such as a terminal or screen) must be provided\n"); 
g_printerr("if the appropriate calls are to be made.\n"); 
g_printerr("\n");
g_printerr("int  get_number(char *string, char **num_end);  // Read an unsigned number from 'string'\n");
g_printerr("                                                // Returns -1 on error\n");
g_printerr("int  read_mem (unsigned int address, int size,  // Read the default memory\n");
g_printerr("             boolean sign, boolean T, int source, boolean *abort);\n");
g_printerr("void write_mem(unsigned int address, int data,  // Write the default memory\n");
g_printerr("               int size, boolean sign, boolean T, int source, boolean *abort);\n");
g_printerr("int  get_reg(int reg_no, int regbank);          // Read a register\n");
g_printerr("void put_reg(int reg_no, int data int regbank); // Write a register\n");
g_printerr("\n");
g_printerr("                       'regbank' definitions may be found in 'address_spaces.h'\n");
g_printerr("                        for most purposes REG_CURRENT (= 0) suffices'\n");
g_printerr("\n");
g_printerr("boolean svc_char_out(char c);                   // Output character to terminal #0\n");
g_printerr("boolean svc_char_in(char *c);                   // Input character from terminal #0\n");
g_printerr("boolean wr_LEDs(char LEDs);                     // Output to status LEDs\n");
g_printerr("boolean rd_screen(unsigned int address);        // Copy vscreen buffer to 'address'\n");
g_printerr("boolean wr_screen(unsigned int address);        // Copy 'address' to vscreen buffer\n");
g_printerr("\n");
g_printerr("More TBA ...\n");		// @@@

return;
}

/*----------------------------------------------------------------------------*/

void step(void)
{
old_status = status;
execute_instruction();

if ((status & CLIENT_STATE_CLASS_MASK) == CLIENT_STATE_CLASS_RUNNING)
  {                        /* Still running - i.e. no breakpoint (etc.) found */
  if (status == CLIENT_STATE_RUNNING_SVC)      // OR _BL  @@@
    {                                /* Don't count the instructions from now */
    if ((get_reg_monitor(15, REG_CURRENT) == run_until_PC)
     && (get_reg_monitor(13, REG_CURRENT) == run_until_SP)
     &&((get_reg_monitor(16, REG_CURRENT) & 0x3F) == run_until_mode))
      status = run_until_status;
    }                 /* This can have changed status - hence no "else" below */

  if (status != CLIENT_STATE_RUNNING_SVC)      // OR _BL  @@@
    {                                    /* Count steps unless inside routine */
    steps_reset++;
    if (steps_togo > 0)                                           /* Stepping */
      {
      steps_togo--;                  /* If -decremented- to reach zero, stop. */
      if (steps_togo == 0) status = CLIENT_STATE_STOPPED;
      }
    }                                              /* Running a whole routine */
  }

if ((status & CLIENT_STATE_CLASS_MASK) != CLIENT_STATE_CLASS_RUNNING)
  breakpoint_enabled = FALSE;         /* No longer running - allow "continue" */

return;
}

/*----------------------------------------------------------------------------*/

void comm(struct pollfd *pPollfd)
{

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void monitor_options_misc(uchar command)
  {
  uchar tempchar;
  int temp;
  switch (command & 0x3F)
    {
    case BR_NOP:   break;
    case BR_PING:  write (1, "OK00", 4); break;
    case BR_WOT_R_U:
      emul_sendchararray (ID_string[0], &ID_string[1]);
      /* ID_string[0] holds length, followed by message */
      break;

    case BR_RESET:
      boardreset();
      break;

    case BR_RTF_GET: emul_sendchar(rtf);  break;
    case BR_RTF_SET: emul_getchar (&rtf); break;

    case BR_WOT_U_DO:
      emul_sendchar (status);
      emul_sendbN (steps_togo, 4);
      emul_sendbN (steps_reset, 4);
      break;

    case BR_PAUSE:
    case BR_STOP:
      if ((status & CLIENT_STATE_CLASS_MASK) == CLIENT_STATE_CLASS_RUNNING)
        {
        old_status = status;
        status = CLIENT_STATE_STOPPED;
        }
      break;

    case BR_CONTINUE:
      if (((status & CLIENT_STATE_CLASS_MASK) == CLIENT_STATE_CLASS_STOPPED)
      && (status != CLIENT_STATE_BYPROG))    // Maybe others @@@
                                               /* Only act if already stopped */
        if ((old_status = CLIENT_STATE_STEPPING) || (steps_togo != 0))
          status = old_status;
      break;

    case BR_BP_GET:
      emul_sendbN (emul_bp_flag[0], 4);
      emul_sendbN (emul_bp_flag[1], 4);
      break;

    case BR_BP_SET:
      {
      int data[2];
      emul_getbN (&data[0], 4);
      emul_getbN (&data[1], 4);
                                 /* Note ordering to avoid temporary variable */
      emul_bp_flag[1] = (~emul_bp_flag[0] & emul_bp_flag[1])
                     | (emul_bp_flag[0] & ((emul_bp_flag[1] & ~data[0]) | data[1]));
      emul_bp_flag[0] = emul_bp_flag[0] & (data[0] | ~data[1]);
      }
      break;

    case BR_BP_READ:
      emul_getchar(&tempchar);
      temp = tempchar;
      emul_sendchar(breakpoints[temp].cond);
      emul_sendchar(breakpoints[temp].size);
      emul_sendbN(breakpoints[temp].addra, 4);
      emul_sendbN(breakpoints[temp].addrb, 4);
      emul_sendbN(breakpoints[temp].dataa[0], 4);
      emul_sendbN(breakpoints[temp].dataa[1], 4);
      emul_sendbN(breakpoints[temp].datab[0], 4);
      emul_sendbN(breakpoints[temp].datab[1], 4);
      break;

   case BR_BP_WRITE:
      emul_getchar(&tempchar);
      temp = tempchar;
      emul_getchar(&breakpoints[temp].cond);
      emul_getchar(&breakpoints[temp].size);
      emul_getbN(&breakpoints[temp].addra, 4);
      emul_getbN(&breakpoints[temp].addrb, 4);
      emul_getbN(&breakpoints[temp].dataa[0], 4);
      emul_getbN(&breakpoints[temp].dataa[1], 4);
      emul_getbN(&breakpoints[temp].datab[0], 4);
      emul_getbN(&breakpoints[temp].datab[1], 4);
      /* add breakpoint */
      temp = (1 << temp) & ~emul_bp_flag[0];
      emul_bp_flag[0] |= temp;
      emul_bp_flag[1] |= temp;
      break;

    case BR_WP_GET:
      emul_sendbN(emul_wp_flag[0], 4);
      emul_sendbN(emul_wp_flag[1], 4);
      break;

    case BR_WP_SET:
      {
      int data[2];
      emul_getbN(&data[0], 4);
      emul_getbN(&data[1], 4);
      temp = data[1] & ~data[0];
      emul_wp_flag[0] &= ~temp;
      emul_wp_flag[1] |= temp;
      temp = data[0] & emul_wp_flag[0];
      emul_wp_flag[1] = (emul_wp_flag[1] & ~temp) | (data[1] & temp);
      }
      break;

    case BR_WP_READ:
      emul_getchar(&tempchar);
      temp = tempchar;
      emul_sendchar(watchpoints[temp].cond);
      emul_sendchar(watchpoints[temp].size);
      emul_sendbN(watchpoints[temp].addra, 4);
      emul_sendbN(watchpoints[temp].addrb, 4);
      emul_sendbN(watchpoints[temp].dataa[0], 4);
      emul_sendbN(watchpoints[temp].dataa[1], 4);
      emul_sendbN(watchpoints[temp].datab[0], 4);
      emul_sendbN(watchpoints[temp].datab[1], 4);
      break;

    case BR_WP_WRITE:
      emul_getchar(&tempchar);
      temp = tempchar;
      emul_getchar(&watchpoints[temp].cond);
      emul_getchar(&watchpoints[temp].size);
      emul_getbN(&watchpoints[temp].addra, 4);
      emul_getbN(&watchpoints[temp].addrb, 4);
      emul_getbN(&watchpoints[temp].dataa[0], 4);
      emul_getbN(&watchpoints[temp].dataa[1], 4);
      emul_getbN(&watchpoints[temp].datab[0], 4);
      emul_getbN(&watchpoints[temp].datab[1], 4);
      temp = 1 << temp & ~emul_wp_flag[0];
      emul_wp_flag[0] |= temp;
      emul_wp_flag[1] |= temp;
      break;

    case BR_FR_WRITE:
      {
      uchar device, length;
      ring_buffer *pBuff;

      emul_getchar(&device);
      pBuff = terminal_table[device][1];
      emul_getchar(&length);
      temp = tempchar;
      while (length-- > 0)
        {
        emul_getchar(&tempchar);                            /* Read character */
        if (pBuff != NULL) put_buffer(pBuff, tempchar); /*  and put in buffer */
        }
      emul_sendchar(0);
      }
      break;

    case BR_FR_READ:
      {
      unsigned char device, max_length;
      unsigned int  i, length, available;
      ring_buffer *pBuff;

      emul_getchar(&device);
      pBuff = terminal_table[device][0];
      emul_getchar(&max_length);
      available = count_buffer(&terminal0_Tx);  /* See how many chars we have */
      if (pBuff == NULL) length = 0;       /* Kill if no corresponding buffer */
      else length = MIN(available, max_length);  /* else clip to message max. */
      emul_sendchar(length);
      for (i = 0; i < length; i++)            /* Send zero or more characters */
        {
        unsigned char c;
        get_buffer(pBuff, &c);
        emul_sendchar(c);
        }
      }
      break;

    default: break;
    }
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void monitor_memory(uchar c)
    {
    unsigned int addr;
    unsigned char *pointer;
    unsigned int length;
    unsigned int size;
    boolean  abort;

    emul_getbN(&addr, 4);                             /* Start address really */
    if ((c & 0x30) == 0x10)
      {
      unsigned int temp;
      int reg_bank, reg_number;

      switch (addr & 0xE0)
        {
        case 0x00: reg_bank = REG_CURRENT; break;
        case 0x20: reg_bank = REG_USER;    break;
        case 0x40: reg_bank = REG_SVC;     break;
        case 0x60: reg_bank = REG_ABT;     break;
        case 0x80: reg_bank = REG_UNDEF;   break;
        case 0xA0: reg_bank = REG_IRQ;     break;
        case 0xC0: reg_bank = REG_FIQ;     break;
        default:   reg_bank = REG_CURRENT; break;
        }
      reg_number = addr & 0x1F;

      emul_getbN(&size, 2);                             /* Length of transfer */

      while (size--)
        if ((c & 8) != 0)
          emul_sendbN(get_reg_monitor(reg_number++, reg_bank), 4);
        else
          {
          emul_getbN(&temp, 4);
          put_reg(reg_number++, temp, reg_bank);
          }
      }
    else
      {
	  /* <lap change> */
      /* new approach: all memory accesses use read_mem and write_mem */
      /* accesses are made with the requested size */
      /* <deprecated>
      pointer = memory + (addr & (memory_size - 1));   // @@@ @@@
      emul_getbN (&size, 2);
      size *= 1 << (c & 7);
      if (((uchar *) pointer + size) > ((uchar *) memory + memory_size))
        pointer -= memory_size;
      if (c & 8) emul_sendchararray(size, pointer);
      else        emul_getchararray(size, pointer);
      </deprecated> */

	  unsigned int  i;
	  unsigned int buf;

      emul_getbN (&length, 2);
	  /* NOTE: transmitted size encoding differs from */
	  /* that used for memory accesses */
      size = 1 << (c & 7);

      pointer = (unsigned char *) &buf;

	  if ((c & 8) != 0) /* read memory */
		{
		for (i=0; i < length; i++)
		  {
		  if (size <= 4) /* byte, half-word, word */
			{
			buf = read_mem(addr, size, FALSE, FALSE, MEM_SYSTEM, &abort);
			emul_sendchararray(size, pointer);
			addr += size;
			}
		  else /* double-word: do two word accesses */
			{
			buf = read_mem(addr, 4, FALSE, FALSE, MEM_SYSTEM, &abort);
			emul_sendchararray(4, pointer);
			addr += 4;
			buf = read_mem(addr, 4, FALSE, FALSE, MEM_SYSTEM, &abort);
			emul_sendchararray(4, pointer);
			addr += 4;
			}
		  }
		}
	  else /* write memory */
		{
		for (i=0; i < length; i++)
		  {
		  if (size <= 4) /* byte, half-word, word */
			{
            emul_getchararray(size, pointer);
			write_mem(addr, buf, size, FALSE, MEM_SYSTEM, &abort);
			addr += size;
			}
		  else /* double-word: do two word accesses */
			{
			emul_getchararray(size, pointer);
			write_mem(addr, buf, size, FALSE, MEM_SYSTEM, &abort);
			addr += 4;
			emul_getchararray(size, pointer);
			write_mem(addr, buf, size, FALSE, MEM_SYSTEM, &abort);
			addr += 4;
			}
		  }
		}
      /* </lap change> */
	  }

    return;
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void monitor_breakpoints(uchar c)
    {
    runflags = c & 0x3F;
    breakpoint_enable  = (runflags & 0x10) != 0;
    breakpoint_enabled = (runflags & 0x01) != 0;       /* Break straight away */
    run_through_BL     = (runflags & 0x02) != 0;
    run_through_SVC    = (runflags & 0x04) != 0;
    emul_getbN(&steps_togo, 4);
    if (steps_togo == 0) status = CLIENT_STATE_RUNNING;
    else                 status = CLIENT_STATE_STEPPING;
    return;
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

uchar c;

if (poll(pPollfd, 1, 0) > 0)
  {
  read(0, &c, 1);		// Look at error return - find EOF & exit @@@
  switch (c & 0xC0)
    {
    case 0x00: monitor_options_misc(c); break;
    case 0x40: monitor_memory(c);       break;
    case 0x80: monitor_breakpoints(c);  break;
    case 0xC0: break;
    }
  }
return;
}

/******************************************************************************/

int emul_getchar(unsigned char *to_get)          /* Get 1 character from host */
{ return emul_getchararray(1, to_get); }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int emul_sendchar(unsigned char to_send)          /* Send 1 character to host */
{ return emul_sendchararray(1, &to_send); }

/******************************************************************************/

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sends N bytes from the supplied value to the host (??), LSB first.         */
/* Returns the number of bytes believed received successfully (i.e. N=>"Ok")  */

int emul_sendbN(int value, int N)
{
char buffer[MAX_SERIAL_WORD];
int i;

if (N > MAX_SERIAL_WORD) N = MAX_SERIAL_WORD;       /* Clip, just in case ... */

for (i = 0; i < N; i++)
  {
  buffer[i] = value & 0xFF;                               /* Byte into buffer */
  value = value >> 8;                                        /* Get next byte */
  }

return emul_sendchararray(N, buffer);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/



/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Gets N bytes from the host (??) into the indicated val_ptr, LSB first.     */
/* Returns the number of bytes received successfully (i.e. N=>"Ok")           */
/* If error suspected sets `board_version' to not present                     */

int emul_getbN(int *val_ptr, int N)
{
char buffer[MAX_SERIAL_WORD];
int i, No_received;

if (N > MAX_SERIAL_WORD) N = MAX_SERIAL_WORD;       /* Clip, just in case ... */

No_received = emul_getchararray (N, buffer);

*val_ptr = 0;

for (i = 0; i < No_received; i++)
  {
  *val_ptr = *val_ptr | ((buffer[i] & 0xFF) << (i * 8));  /* Assemble integer */
  }

if (No_received != N) board_version = -1;         /* Really do this here? @@@ */
return No_received;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* reads a character array from buffer                                        */
/* sends char_number number of characters given by data_ptr.                  */
/* returns number of bytes received                                           */
/*                                                                            */
/*                                                                            */

int emul_getchararray(int char_number, unsigned char *data_ptr)
{
int ret = char_number;
int replycount = 0;
struct pollfd pollfd;

pollfd.fd = 0;
pollfd.events = POLLIN;

while (char_number)
  {
  if (!poll (&pollfd, 1, -1)) return ret - char_number;
  replycount = read (0, data_ptr, char_number);
  if (replycount < 0) replycount = 0;
  char_number -= replycount;
  data_ptr += replycount;
  }
return ret;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* writes an array of bytes in the buffer                                     */
/* sends char_number - number of bytes given by data_ptr                      */
/* data pointer points to the beginning of the sequence to be sent            */
/*                                                                            */
/*                                                                            */

int emul_sendchararray (int char_number, unsigned char *data_ptr)
{
write(1, data_ptr, char_number);
return char_number;                           /* send char array to the board */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

int emulsetup(void)
{
  int initial_mode;

  glob1 = 0;
  glob2 = 0;

  {
  int i;

  for (i = 0; i < 32; i++) past_opc_addr[i] = 1;  /* Illegal op. code address */
  past_opc_ptr = 0;
  past_count = 0;
  past_size = 4;
  }

  initial_mode = 0xC0 | sup_mode;
  print_out = FALSE;

  next_file_handle = 1;

  initialise(0, initial_mode);

  return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void execute_instruction(void)
{
unsigned int instr_addr, instr;
boolean abort;
int i;

/* - - - - - - - - - - - - - - breakpoints - - - - - - - - - - - - - - - - - -*/

  /* Breakpoints - ignoring lots of things yet @@@              */
  /* Needs amalgamating with watchpoint checker */

  int check_breakpoint(unsigned int instr_addr, unsigned int instr)
  {
  boolean may_break;

  for (i = 0, may_break = FALSE; (i < NO_OF_BREAKPOINTS) && !may_break; i++)
    {                                                   /* Search breakpoints */
    may_break = ((emul_bp_flag[0] & emul_bp_flag[1] & (1<<i)) != 0);
                                                      /* Breakpoint is active */

    if (may_break)                                  /* Try address comparison */
      switch (breakpoints[i].cond & 0x0C)
        {
        case 0x00: may_break = FALSE; break;
        case 0x04: may_break = FALSE; break;
         case 0x08:                 /* Case of between address A and address B */
          if ((instr_addr < breakpoints[i].addra)
           || (instr_addr > breakpoints[i].addrb))
            may_break = FALSE;
          break;

        case 0x0C:                                            /* case of mask */
          if ((instr_addr & breakpoints[i].addrb) != breakpoints[i].addra)
            may_break = FALSE;
          break;
        }

    if (may_break)                                     /* Try data comparison */
      switch (breakpoints[i].cond & 0x03)
      {
      case 0x00: may_break = FALSE; break;
      case 0x01: may_break = FALSE; break;
      case 0x02:                         /* Case of between data A and data B */
        if ((instr < breakpoints[i].dataa[0]) || (instr > breakpoints[i].datab[0]))
          may_break = FALSE;
        break;

      case 0x03:                                              /* Case of mask */
        if ((instr & breakpoints[i].datab[0]) != breakpoints[i].dataa[0])
          may_break = FALSE;
        break;
      }

    if (may_break)
      {
      // Expansion space for more comparisons @@@
//      fprintf(stderr, "BREAKING  %08X  %08X\n", instr_addr, instr);
      }

    }                                                      /* End of for loop */

  return may_break;
  }

/* - - - - - - - - - - - - - end breakpoint - - - - - - - - - - - - - - - - - */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

instr_addr = get_reg(15, REG_CURRENT) - instruction_length();
last_addr  = get_reg(15, REG_CURRENT) - instruction_length();

instr = fetch(&abort);                                               /* FETCH */

if (!abort)
  {
  if ((breakpoint_enabled)                    /* Check ID breakpoints enabled */
   && (status != CLIENT_STATE_RUNNING_SVC))      // OR _BL  @@@
/* Don't look for breakpoints inside single stepped call ... correct ???? @@@ */
    if (check_breakpoint(instr_addr, instr))
      {
      status = CLIENT_STATE_BREAKPOINT;
             // YUK @@@
      return;                            /* and return the appropriate status */
      }
  breakpoint_enabled = breakpoint_enable;    /* More likely after first fetch */

  /* BL instruction */
  if (((instr & 0x0F000000) == 0x0B000000) && run_through_BL)
    save_state(CLIENT_STATE_RUNNING_BL);
  else
    if (((instr & 0x0F000000) == 0x0F000000) && run_through_SVC)
      save_state(CLIENT_STATE_RUNNING_SVC);

  execute(instr);                                                  /* Execute */
  }
else                                                        /* Prefetch abort */
  {								// Under development @@@
  breakpoint();		// like a breakpoint instruction (change name?) @@@
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Save state for leaving "procedure" {PC, SP, Mode, current state} */

void save_state(uchar new_status)
{
run_until_PC     = get_reg(15, REG_CURRENT);/* Incremented once: correct here */
run_until_SP     = get_reg(13, REG_CURRENT);
run_until_mode   = get_reg(16, REG_CURRENT) & 0x3F;     /* Just the mode bits */
run_until_status = status;

status = new_status;

return;
}

/*----------------------------------------------------------------------------*/

void boardreset(void)
{
steps_reset = 0;
initialise(0, sup_mode);
return;
}

void initialise(unsigned int start_address, int initial_mode)
{
cpsr = 0X000000C0 | initial_mode;                       /* Disable interrupts */
r[15] = start_address;
old_status = CLIENT_STATE_RESET;
status = CLIENT_STATE_RESET;

return;
}

/*----------------------------------------------------------------------------*/

void execute(unsigned int op_code)
{

inc_pc();                                           /* Easier here than later */
                                                            /* ARM or THUMB ? */
if ((cpsr & TF_MASK) != 0)                                           /* Thumb */
  {
  op_code = op_code & 0XFFFF;                              /* 16-bit op. code */
  switch (op_code & 0XE000)
    {
    case 0X0000: data0(op_code);         break;
    case 0X2000: data1(op_code);         break;
    case 0X4000: data_transfer(op_code); break;
    case 0X6000: transfer0(op_code);     break;
    case 0X8000: transfer1(op_code);     break;
    case 0XA000: sp_pc(op_code);         break;
    case 0XC000: lsm_b(op_code);         break;
    case 0XE000: thumb_branch(op_code);  break;
    }
  }
else
  {
  if ((check_cc (op_code >> 28) == TRUE)                   /* Check condition */
  || ((op_code & 0XFE000000) == 0XFA000000))      /* Nasty non-orthogonal BLX */
    {
    switch ((op_code >> 25) & 0X00000007)
      {
      case 0X0: data_op(op_code);     break;   /* includes load/store hw & sb */
      case 0X1: data_op(op_code);     break;       /* data processing & MSR # */
      case 0X2: transfer(op_code);    break;
      case 0X3: transfer(op_code);    break;
      case 0X4: multiple(op_code);    break;
      case 0X5: branch(op_code);      break;
      case 0X6: coprocessor(op_code); break;
      case 0X7: my_system(op_code);   break;
      }
    }
  }
return;
}

/*----------------------------------------------------------------------------*/

int is_it_sbhw(unsigned int op_code)
{
if (((op_code & 0X0E000090) == 0X00000090) && ((op_code & 0X00000060)
                                       != 0X00000000)        /* No multiplies */
      && ((op_code & 0X00100040) != 0X00000040))          /* No signed stores */
  {
  if (((op_code & 0X00400000) != 0) || ((op_code & 0X00000F00) == 0))
    return TRUE;
  else
    return FALSE;
  }
else
  return FALSE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void data_op(unsigned int op_code)
{
int operation;

if (((op_code & MUL_MASK) == MUL_OP)
 || ((op_code & LONG_MUL_MASK) == LONG_MUL_OP))
  my_multi(op_code);
else if (is_it_sbhw (op_code) == TRUE)
  transfer_sbhw(op_code);
else if ((op_code & SWP_MASK) == SWP_OP)
  swap(op_code);
else
  {
  operation = (op_code & DATA_OP_MASK) >> 21;

           /* TST, TEQ, CMP, CMN - all lie in following range, but have S set */
  if ((op_code & DATA_EXT_MASK) == ARITH_EXT)          /* PSR transfers OR BX */
    {
    if ((op_code & 0X0FBF0FFF) == 0X010F0000) mrs(op_code);            /* MRS */
    else if (((op_code & 0X0DB6F000) == 0X0120F000)
          && ((op_code & 0X02000010) != 0X00000010)) msr(op_code);     /* MSR */
    else if ((op_code & 0X0FFFFFD0) == 0X012FFF10)                  /* BX/BLX */
      bx (op_code & RM_MASK, op_code & 0X00000020);
    else if ((op_code & 0XFFF000F0) == 0XE1200070) breakpoint();/* Breakpoint */
    else if ((op_code & 0X0FFF0FF0) == 0X016F0F10) clz(op_code);       /* CLZ */
    else undefined();
    }
  else
    normal_data_op(op_code, operation);     /* All data processing operations */
  }
return;
}

/*----------------------------------------------------------------------------*/

void transfer_sbhw (unsigned int op_code)
{
unsigned int address, data;
int size;
int offset, rd;
boolean sign;
boolean abort;

switch (op_code & 0X00000060)
  {
  case 0X00:
    fprintf(stderr, "Multiply shouldn't be here!\n");
    break;                        /* Error! */
  case 0X20: size = 2; sign = FALSE; break;                              /* H */
  case 0X40: size = 1; sign = TRUE;  break;                             /* SB */
  case 0X60: size = 2; sign = TRUE;  break;                             /* SH */
  }

rd = ((op_code & RD_MASK) >> 12);

address = get_reg (((op_code & RN_MASK) >> 16), REG_CURRENT);

offset = transfer_offset (op_code & OP2_MASK, op_code & UP_MASK,
                          op_code & IMM_HW_MASK, TRUE);

if ((op_code & PRE_MASK) != 0) address = address + offset;       /* Pre-index */

if ((op_code & LOAD_MASK) == 0)                                      /* Store */
  write_mem(address, get_reg(rd, REG_CURRENT), size, FALSE, MEM_DATA, &abort);
else                                                                  /* load */
  {
  data = read_mem(address, size, sign, FALSE, MEM_DATA, &abort);/* Post index */
  if (!abort) put_reg(rd, data, REG_CURRENT);
  }

if (!abort)
  {
  if ((op_code & PRE_MASK) == 0)                 /* Post index with writeback */
    put_reg((op_code & RN_MASK) >> 16, address + offset, REG_CURRENT);
  else if ((op_code & WRITE_BACK_MASK) != 0)
    put_reg((op_code & RN_MASK) >> 16, address, REG_CURRENT);
  }
else
  data_abort();

return;
}

/*----------------------------------------------------------------------------*/

void mrs(unsigned int op_code)
{
if ((op_code & 0X00400000) == 0)
  put_reg((op_code & RD_MASK) >> 12, cpsr, REG_CURRENT);
else
  put_reg((op_code & RD_MASK) >> 12, spsr[cpsr & MODE_MASK], REG_CURRENT);

return;
}

/*----------------------------------------------------------------------------*/

void msr(unsigned int op_code)
{
int mask, source;

switch (op_code & 0X00090000)
  {
  case 0X00000000: mask = 0X00000000; break;
  case 0X00010000: mask = 0X0FFFFFFF; break;
  case 0X00080000: mask = 0XF0000000; break;
  case 0X00090000: mask = 0XFFFFFFFF; break;
  }
if ((cpsr & MODE_MASK) == 0X10) mask = mask & 0XF0000000;        /* User mode */

if ((op_code & IMM_MASK) == 0)                 /* Test applies for both cases */
  source = get_reg (op_code & RM_MASK, REG_CURRENT) & mask;
else
  {
  unsigned int x, y, dummy;

  x = op_code & 0X0FF;                                     /* Immediate value */
  y = (op_code & 0XF00) >> 7;                            /* Number of rotates */
  source = ((x >> y) | lsl (x, 32 - y, &dummy)) & mask;
  }

if ((op_code & 0X00400000) == 0)
  cpsr = (cpsr & ~mask) | source;
else
  spsr[cpsr & MODE_MASK] = (spsr[cpsr & MODE_MASK] & ~mask) | source;

return;
}

/*----------------------------------------------------------------------------*/

void bx (unsigned int Rm, int link)/* Link is performed if "link" is NON-ZERO */
{
int PC, offset;
int t_bit;

PC = get_reg (15, REG_CURRENT);
if ((cpsr & TF_MASK) != 0)
  {
  PC = PC - 2;
  PC = PC | 1;
  }                                                    /* Remember Thumb mode */
else
  PC = PC - 4;

offset = get_reg (Rm, REG_CURRENT) & 0XFFFFFFFE;
t_bit = get_reg (Rm, REG_CURRENT) & 0X00000001;

if (t_bit == 1) cpsr = cpsr |  TF_MASK;
else            cpsr = cpsr & ~TF_MASK;

put_reg(15, offset, REG_CURRENT);                                /* Update PC */
if (link != 0) put_reg(14, PC, REG_CURRENT);                   /* Link if BLX */

return;
}

/*----------------------------------------------------------------------------*/

void my_multi(unsigned int op_code)
{
int acc;

if ((op_code & MUL_LONG_BIT) == 0)                                  /* Normal */
  {
  acc = get_reg(op_code & RM_MASK, REG_CURRENT)
      * get_reg((op_code & RS_MASK) >> 8, REG_CURRENT);

  if ((op_code & MUL_ACC_BIT) != 0)
    acc = acc + get_reg ((op_code & RD_MASK) >> 12, REG_CURRENT);

  put_reg((op_code & RN_MASK) >> 16, acc, REG_CURRENT);

  if ((op_code & S_MASK) != 0) set_NZ (acc);                         /* Flags */
  }
else                                                                  /* Long */
  {
  unsigned int Rm, Rs, th, tm, tl;
  int sign;

  Rm = get_reg (op_code & RM_MASK, REG_CURRENT);
  Rs = get_reg ((op_code & RS_MASK) >> 8, REG_CURRENT);

  sign = 0;
  if ((op_code & MUL_SIGN_BIT) != 0)                                /* Signed */
    {
    if ((Rm & BIT_31) != 0)
      {
      Rm = ~Rm + 1;
      sign = 1;
      }
    if ((Rs & BIT_31) != 0)
      {
      Rs = ~Rs + 1;
      sign = sign ^ 1;
      }
    }
  /* Everything now `positive' */
  tl = (Rm & 0X0000FFFF) * (Rs & 0X0000FFFF);
  th = ((Rm >> 16) & 0X0000FFFF) * ((Rs >> 16) & 0X0000FFFF);
  tm = ((Rm >> 16) & 0X0000FFFF) * (Rs & 0X0000FFFF);
  Rm = ((Rs >> 16) & 0X0000FFFF) * (Rm & 0X0000FFFF);  /* Rm no longer needed */
  tm = tm + Rm;
  if (tm < Rm) th = th + 0X00010000;                       /* Propagate carry */
  tl = tl + (tm << 16);
  if (tl < (tm << 16)) th = th + 1;
  th = th + ((tm >> 16) & 0X0000FFFF);

  if (sign != 0)                                     /* Change sign of result */
    {
    th = ~th;
    tl = ~tl + 1;
    if (tl == 0) th = th + 1;
    }

  if ((op_code & MUL_ACC_BIT) != 0)
    {
    tm = tl + get_reg ((op_code & RD_MASK) >> 12, REG_CURRENT);
    if (tm < tl) th = th + 1;                              /* Propagate carry */
    tl = tm;
    th = th + get_reg ((op_code & RN_MASK) >> 16, REG_CURRENT);
    }

  put_reg((op_code & RD_MASK) >> 12, tl, REG_CURRENT);
  put_reg((op_code & RN_MASK) >> 16, th, REG_CURRENT);

  if ((op_code & S_MASK) != 0)
    set_NZ (th | (((tl >> 16) | tl) & 0X0000FFFF));                  /* Flags */
  }

return;
}

/*----------------------------------------------------------------------------*/

void swap(unsigned int op_code)
{
unsigned int address, data, size;
boolean abort;

address = get_reg((op_code & RN_MASK) >> 16, REG_CURRENT);

if ((op_code & BYTE_MASK) != 0) size = 1;
else                            size = 4;

data = read_mem(address, size, FALSE, FALSE, MEM_DATA, &abort);
if (!abort) write_mem(address, get_reg(op_code & RM_MASK, REG_CURRENT),
                      size, FALSE, MEM_DATA, &abort);       /* Can also abort */

if (!abort) put_reg((op_code & RD_MASK) >> 12, data, REG_CURRENT);
else        data_abort();

return;
}

/*----------------------------------------------------------------------------*/

void normal_data_op(unsigned int op_code, int operation)
{
int rd, a, b, mode;
int shift_carry;
int CPSR_special;

mode = cpsr & MODE_MASK;
CPSR_special = FALSE;
shift_carry = 0;
a = get_reg((op_code & RN_MASK) >> 16, REG_CURRENT);    /* force_user = FALSE */

if ((op_code & IMM_MASK) == 0)
  b = b_reg (op_code & OP2_MASK, &shift_carry);
else
  b = b_immediate (op_code & OP2_MASK, &shift_carry);

switch (operation)                                               /* R15s @@?! */
  {
  case 0X0: rd = a & b; break;                                         /* AND */
  case 0X1: rd = a ^ b; break;                                         /* EOR */
  case 0X2: rd = a - b; break;                                         /* SUB */
  case 0X3: rd = b - a; break;                                         /* RSB */
  case 0X4: rd = a + b; break;                                         /* ADD */
  case 0X5: rd = a + b;
            if ((cpsr & CF_MASK) != 0) rd = rd + 1; break;             /* ADC */
  case 0X6: rd = a - b - 1;
            if ((cpsr & CF_MASK) != 0) rd = rd + 1; break;             /* SBC */
  case 0X7: rd = b - a - 1;
            if ((cpsr & CF_MASK) != 0) rd = rd + 1; break;             /* RSC */
  case 0X8: rd = a & b; break;                                         /* TST */
  case 0X9: rd = a ^ b;                                                /* TEQ */
            if ((op_code & RD_MASK) == 0XF000)                        /* TEQP */
              {
              CPSR_special = TRUE;
              if (mode != user_mode) cpsr = spsr[mode];
              }
            break;
  case 0XA: rd = a - b; break;                                         /* CMP */
  case 0XB: rd = a + b; break;                                         /* CMN */
  case 0XC: rd = a | b; break;                                         /* ORR */
  case 0XD: rd = b;     break;                                         /* MOV */
  case 0XE: rd = a & ~b;break;                                         /* BIC */
  case 0XF: rd = ~b;    break;                                         /* MVN */
  }

if ((operation & 0XC) != 0X8)               /* Return result unless a compare */
  put_reg((op_code & RD_MASK) >> 12, rd, REG_CURRENT);

if (((op_code & S_MASK) != 0) && (CPSR_special != TRUE))             /* S-bit */
  {                                                    /* Want to change CPSR */
  if (((op_code & RD_MASK) >> 12) == 0XF)                     /* PC and S-bit */
    {
    if (mode != user_mode) cpsr = spsr[mode];           /* restore saved CPSR */
    else fprintf(stderr, "SPSR_user read attempted\n");
    }
  else                                               /* other dest. registers */
    {
    switch (operation)
      {                                                           /* LOGICALs */
      case 0X0:                                                        /* AND */
      case 0X1:                                                        /* EOR */
      case 0X8:                                                        /* TST */
      case 0X9:                                                        /* TEQ */
      case 0XC:                                                        /* ORR */
      case 0XD:                                                        /* MOV */
      case 0XE:                                                        /* BIC */
      case 0XF:                                                        /* MVN */
        set_NZ (rd);
        if (shift_carry == TRUE) cpsr = cpsr |  CF_MASK;      /* CF := output */
        else                     cpsr = cpsr & ~CF_MASK;      /* from shifter */
        break;

      case 0X2:                                                        /* SUB */
      case 0XA:                                                        /* CMP */
        set_flags (FLAG_SUB, a, b, rd, 1);
        break;

      case 0X6:                        /* SBC - Needs testing more !!!   @@@@ */
        set_flags (FLAG_SUB, a, b, rd, cpsr & CF_MASK);
        break;

      case 0X3:                                                        /* RSB */
        set_flags (FLAG_SUB, b, a, rd, 1);
        break;

      case 0X7:                                                        /* RSC */
        set_flags (FLAG_SUB, b, a, rd, cpsr & CF_MASK);
        break;

      case 0X4:                                                        /* ADD */
      case 0XB:                                                        /* CMN */
        set_flags (FLAG_ADD, a, b, rd, 0);
        break;

      case 0X5:                                                        /* ADC */
        set_flags (FLAG_ADD, a, b, rd, cpsr & CF_MASK);
        break;
      }
    }
  }

return;
}

/*----------------------------------------------------------------------------*/
/* shift type: 00 = LSL, 01 = LSR, 10 = ASR, 11 = ROR                         */

int b_reg(int op2, int *cf)
{
unsigned int shift_type, reg, distance, result;
reg = get_reg (op2 & 0X00F, REG_CURRENT);                         /* Register */
shift_type = (op2 & 0X060) >> 5;                             /* Type of shift */
if ((op2 & 0X010) == 0)
  {                                                        /* Immediate value */
  distance = (op2 & 0XF80) >> 7;
  if (distance == 0)                                         /* Special cases */
    {
    if (shift_type == 3)
      {
      shift_type = 4;                                                  /* RRX */
      distance = 1;                                     /* Something non-zero */
      }
    else if (shift_type != 0) distance = 32;                  /* LSL excluded */
    }
  }
else
  distance = (get_reg ((op2 & 0XF00) >> 8, REG_CURRENT) & 0XFF);
                                                            /* Register value */

*cf = ((cpsr & CF_MASK) != 0);                              /* Previous carry */
switch (shift_type)
  {
  case 0X0: result = lsl(reg, distance, cf); break;                    /* LSL */
  case 0X1: result = lsr(reg, distance, cf); break;                    /* LSR */
  case 0X2: result = asr(reg, distance, cf); break;                    /* ASR */
  case 0X3: result = ror(reg, distance, cf); break;                    /* ROR */
  case 0X4:                                                         /* RRX #1 */
    result = reg >> 1;
    if ((cpsr & CF_MASK) == 0) result = result & ~BIT_31;
    else                       result = result |  BIT_31;
    *cf = ((reg & BIT_0) != 0);
    break;
  }

if (*cf) *cf = TRUE; else *cf = FALSE;                 /* Change to "Boolean" */
return result;
}

/*----------------------------------------------------------------------------*/

int b_immediate(int op2, int *cf)
{
unsigned int x, y, dummy;

x = op2 & 0X0FF;                                           /* Immediate value */
y = (op2 & 0XF00) >> 7;                                  /* Number of rotates */
if (y == 0) *cf = ((cpsr & CF_MASK) != 0);                  /* Previous carry */
else        *cf = (((x >> (y - 1)) & BIT_0) != 0);
if (*cf) *cf = TRUE; else *cf = FALSE;                 /* Change to "Boolean" */
return ror(x, y, &dummy);                                /* Circular rotation */
}

/*----------------------------------------------------------------------------*/

void clz(unsigned int op_code)
{
int i, j;

j = get_reg(op_code & RM_MASK, REG_CURRENT);

if (j == 0) i = 32;
else
  {
  i = 0;
  while ((j & 0X80000000) == 0) { i++; j = j << 1; }
  }

put_reg((op_code & RD_MASK) >> 12, i, REG_CURRENT);

return;
}

/*----------------------------------------------------------------------------*/

void transfer(unsigned int op_code)
{
unsigned int address, data;
int offset, rd, size;
boolean T;
boolean abort;

if ((op_code & UNDEF_MASK) == UNDEF_CODE) undefined();
else
  {
  if ((op_code & BYTE_MASK) == 0) size = 4;
  else                            size = 1;

//  if (((op_code & PRE_MASK) == 0) && ((op_code & WRITE_BACK_MASK) != 0))
//    T = TRUE;
//  else
//    T = FALSE;

  T = (((op_code & PRE_MASK) == 0) && ((op_code & WRITE_BACK_MASK) != 0));

  rd = (op_code & RD_MASK) >> 12;

  address = get_reg ((op_code & RN_MASK) >> 16, REG_CURRENT);

  offset = transfer_offset(op_code & OP2_MASK, op_code & UP_MASK,
                           op_code & IMM_MASK, FALSE);  /* bit(25) = 1 -> reg */

  if ((op_code & PRE_MASK) != 0) address = address + offset;     /* Pre-index */

  if ((op_code & LOAD_MASK) == 0)
    write_mem(address, get_reg (rd, REG_CURRENT), size, T, MEM_DATA, &abort);
  else
    {
    data = read_mem(address, size, FALSE, T, MEM_DATA, &abort);
    if (!abort) put_reg(rd, data, REG_CURRENT);
    }

  if (!abort)
    {
    if ((op_code & PRE_MASK) == 0)                              /* Post-index */
      put_reg((op_code & RN_MASK) >> 16, address + offset, REG_CURRENT);
                                                     /* Post index write-back */
    else if ((op_code & WRITE_BACK_MASK) != 0)
      put_reg((op_code & RN_MASK) >> 16, address, REG_CURRENT);
                                                      /* Pre index write-back */
    }
  else data_abort();
  }
return;
}

/*----------------------------------------------------------------------------*/

int transfer_offset(int op2, int add, int imm, boolean sbhw)
{                                   /* add and imm are zero/non-zero Booleans */
int offset;
int cf;                                                    /* Dummy parameter */

if (!sbhw)                                               /* Addressing mode 2 */
  {
  if (imm != 0) offset = b_reg(op2, &cf);               /* bit(25) = 1 -> reg */
  else          offset = op2 & 0XFFF;
  }
else                                                     /* Addressing mode 3 */
  {
  if (imm != 0) offset = ((op2 & 0XF00) >> 4) | (op2 & 0X00F);
  else          offset = b_reg(op2 & 0xF, &cf);
  }

if (add == 0) offset = -offset;

return offset;
}

/*----------------------------------------------------------------------------*/

void multiple(unsigned int op_code)
{

if ((op_code & LOAD_MASK) == 0)
  stm((op_code & 0X01800000) >> 23, (op_code & RN_MASK) >> 16,
       op_code & 0X0000FFFF, (op_code & WRITE_BACK_MASK) != 0,
      (op_code & USER_MASK) != 0);
else
  ldm((op_code & 0X01800000) >> 23, (op_code & RN_MASK) >> 16,
       op_code & 0X0000FFFF, (op_code & WRITE_BACK_MASK) != 0,
      (op_code & USER_MASK) != 0);

return;
}

/*----------------------------------------------------------------------------*/

int bit_count(unsigned int source, int *first)
{
int count, reg;

count = 0;
reg = 0;
*first = -1;

while (source != 0)
  {
  if ((source & BIT_0) != 0)
    {
    count = count + 1;
    if (*first < 0) *first = reg;
    }
  source = source >> 1;
  reg = reg + 1;
  }

return count;
}

/*----------------------------------------------------------------------------*/

void ldm(int mode, int Rn, int reg_list, boolean write_back, boolean hat)
{					// Review register changes @@@

int address, new_base, count, first_reg, reg, data;
int force_user;
boolean r15_inc;
boolean abort;

address = get_reg(Rn, REG_CURRENT);
count = bit_count(reg_list, &first_reg);
r15_inc = (reg_list & 0X00008000) != 0;        /* R15 in list */

switch (mode)
  {
  case 0: new_base = address - 4 * count; address = new_base + 4; break;
  case 1: new_base = address + 4 * count;                         break;
  case 2: new_base = address - 4 * count; address = new_base;     break;
  case 3: new_base = address + 4 * count; address = address + 4;  break;
  }

address = address & 0XFFFFFFFC;           /* Bottom 2 bits ignored in address */

if ((hat != 0) && !r15_inc) force_user = REG_USER;
else                        force_user = REG_CURRENT;
                                             /* Force user unless R15 in list */
reg = 0;

abort = FALSE;                                 /* Need to initialise for loop */
while ((reg_list != 0) && !abort)
  {
  if ((reg_list & BIT_0) != 0)
    {
    data = read_mem(address, 4, FALSE, FALSE, MEM_DATA, &abort); /* For later */
    if (!abort) put_reg(reg, data, force_user);
    address = address + 4;
    }
  reg_list = reg_list >> 1;
  reg = reg + 1;
  }

if (!abort)
  {
  if (write_back != 0) put_reg(Rn, new_base, REG_CURRENT);// WB now *last*; check legality @@@

  if (r15_inc)                                                 /* R15 in list */
    {
    if ((data & 1) != 0) cpsr = cpsr | TF_MASK;   /* data from last load used */
    else                 cpsr = cpsr & ~TF_MASK;    /* to set instruction set */
    if (hat != 0) cpsr = spsr[cpsr & MODE_MASK];      /* ... and if S bit set */
    }
  }
else
  data_abort();

return;
}

/*----------------------------------------------------------------------------*/

void stm(int mode, int Rn, int reg_list, boolean write_back, boolean hat)
{
int address, old_base, new_base, count, first_reg, reg;
int force_user;
boolean special;
boolean abort;

address = get_reg(Rn, REG_CURRENT);
old_base = address;                                 /* Keep in case of aborts */
count = bit_count(reg_list, &first_reg);

switch (mode)
  {
  case 0: new_base = address - 4 * count; address = new_base + 4; break;
  case 1: new_base = address + 4 * count;                         break;
  case 2: new_base = address - 4 * count; address = new_base;     break;
  case 3: new_base = address + 4 * count; address = address + 4;  break;
  }

address = address & 0XFFFFFFFC;           /* Bottom 2 bits ignored in address */

special = FALSE;
if (write_back != 0)
  {
  if (Rn == first_reg) special = TRUE;
  else put_reg(Rn, new_base, REG_CURRENT);
  }

if (hat != 0) force_user = REG_USER;
else          force_user = REG_CURRENT;

reg = 0;

abort = FALSE;                                 /* Need to initialise for loop */
while ((reg_list != 0) && !abort)
  {
  if ((reg_list & BIT_0) != 0)
    {
    write_mem (address, get_reg (reg, force_user), 4, FALSE, MEM_DATA, &abort);
    address = address + 4;
    }
  reg_list = reg_list >> 1;
  reg = reg + 1;
  }

if (abort)
  {
  put_reg(Rn, old_base, REG_CURRENT);           /* Restore base reg. if abort */
  data_abort();
  }
else
  if (special) put_reg(Rn, new_base, REG_CURRENT);

return;
}

/*----------------------------------------------------------------------------*/

void branch(unsigned int op_code)
{
int offset, PC;

PC = get_reg(15, REG_CURRENT);           /* Get this now in case mode changes */

if (((op_code & LINK_MASK) != 0) || ((op_code & 0XF0000000) == 0XF0000000))
  put_reg(14, get_reg(15, REG_CURRENT) - 4, REG_CURRENT);

offset = (op_code & BRANCH_FIELD) << 2;
if ((op_code & BRANCH_SIGN) != 0)
  offset = offset | (~(BRANCH_FIELD << 2) & 0XFFFFFFFC);       /* sign extend */

if ((op_code & 0XF0000000) == 0XF0000000)                 /* Other BLX fix-up */
  {
  offset = offset | ((op_code >> 23) & 2);
  cpsr = cpsr | TF_MASK;
  }

put_reg(15, PC + offset, REG_CURRENT);

return;
}

/*----------------------------------------------------------------------------*/

void coprocessor(unsigned int op_code)
{
//    printf("Coprocessor %d data transfer\n", (op_code>>8) & 0XF);

boolean done;
so_handler_entry *p_handler_entry;

done = FALSE;               /* Indicator that op. has (not) been serviced yet */
p_handler_entry = so_handler_list;               /* List of handler libraries */
while (!done && (p_handler_entry != NULL) && (status != CLIENT_STATE_RESET))
  {                                                 /* Explore list, in order */
  if ((p_handler_entry->copro_handler != NULL)
  && ((p_handler_entry->copro_handler)((op_code >> 8) & 0xF, op_code)))
    done = TRUE;
  else p_handler_entry = p_handler_entry->next;
  }

if (!done) undefined();                                            /* Default */

return;
}

/*----------------------------------------------------------------------------*/

void my_system (unsigned int op_code)
{
if (((op_code & 0X0F000000) == 0X0E000000)/* Bodge allowing Thumb to use this */
 || ((op_code & 0X0F000000) == 0X0C000000)
 || ((op_code & 0X0F000000) == 0X0D000000))
  {                                                       /* Coprocessor ops. */
  boolean done;
  so_handler_entry *p_handler_entry;

/*
  if ((op_code & 0X00000010) == 0X00000000)
    printf("Coprocessor %d data operation\n", (op_code>>8) & 0XF);
  else
    printf("Coprocessor %d register transfer\n", (op_code>>8) & 0XF);
*/

  done = FALSE;             /* Indicator that op. has (not) been serviced yet */
  p_handler_entry = so_handler_list;             /* List of handler libraries */
  while (!done && (p_handler_entry != NULL) && (status != CLIENT_STATE_RESET))
    {                                               /* Explore list, in order */
    if ((p_handler_entry->copro_handler != NULL)
    && ((p_handler_entry->copro_handler)((op_code >> 8) & 0xF, op_code)))
      done = TRUE;
    else p_handler_entry = p_handler_entry->next;
    }

  if (!done) undefined();                                          /* Default */
  }
else
  {
  boolean done;
  so_handler_entry *p_handler_entry;

  done = FALSE;             /* Indicator that SVC has (not) been serviced yet */
  p_handler_entry = so_handler_list;        /* List of SVC handling libraries */
  while (!done && (p_handler_entry != NULL) && (status != CLIENT_STATE_RESET))
    {                                               /* Explore list, in order */
    if ((p_handler_entry->svc_handler != NULL)
    && ((p_handler_entry->svc_handler)(op_code & 0X00FFFFFF, &status))) done = TRUE;
    else p_handler_entry = p_handler_entry->next;
    }

  if (!done)                                         /* Default SVC behaviour */
    {
    if (print_out)
      fprintf(stderr, "Untrapped SVC call %06X\n", op_code & 0X00FFFFFF);
    spsr[sup_mode] = cpsr;
    cpsr = (cpsr & ~MODE_MASK) | sup_mode;
    cpsr = cpsr & ~TF_MASK;                             /* Always in ARM mode */
    cpsr = cpsr |  IF_MASK;                             /* Disable interrupts */
    put_reg (14, get_reg (15, REG_CURRENT) - 4, REG_CURRENT);
    put_reg (15, 8, REG_CURRENT);
    }
  }
}

/*----------------------------------------------------------------------------*/
/* Support routines for external SVC handlers                                 */

boolean svc_char_out(char c)                           /* Assumes terminal #0 */
  {
  int okay;

  okay = TRUE;
  while (!put_buffer(&terminal0_Tx, c))
    {
    if (status == CLIENT_STATE_RESET) { okay = FALSE; break; }
    else comm(SVC_poll);         /* If stalled, retain monitor communications */
    }
  return okay;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean svc_char_in(char *pc)                          /* Assumes terminal #0 */
  {
  while ((!get_buffer(&terminal0_Rx, pc)) && (status != CLIENT_STATE_RESET))
    comm(SVC_poll);

  return (status != CLIENT_STATE_RESET);
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#ifdef VSCREEN_SUPPORT

void wr_LEDs(char LEDs)
{
*pp_LEDs = LEDs;
*pp_handshake |= HNDSHK_LED;
return;
}


void rd_screen(unsigned int address)
{
unsigned int i;
boolean abort;

for (i = 0; i < vscreen_sizee; i++)
  write_mem(address++, shm_ADDR[i], 1, FALSE, MEM_SYSTEM, &abort);

return;
}

void wr_screen(unsigned int address)
{
unsigned int i;
boolean abort;

for (i = 0; i < vscreen_sizee; i++)
  shm_ADDR[i] = read_mem(address++, 1, FALSE, FALSE, MEM_SYSTEM, &abort);

*pp_handshake |= HNDSHK_NEW_IMAGE;

return;
}

#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

//void svc_comms(void) { comm(SVC_poll); }        /* Wrapper for external calls */

/*----------------------------------------------------------------------------*/
/* This is the breakpoint *instruction*                                       */

void breakpoint()                              /* Looks like a prefetch abort */
{
spsr[abt_mode] = cpsr;
cpsr = (cpsr & ~MODE_MASK & ~TF_MASK) | abt_mode | IF_MASK;
put_reg (14, get_reg (15, REG_CURRENT) - 4, REG_CURRENT);
put_reg (15, 0x0000000C, REG_CURRENT);
return;
}

/*----------------------------------------------------------------------------*/

void undefined()
{
spsr[undef_mode] = cpsr;
cpsr = (cpsr & ~MODE_MASK & ~TF_MASK) | undef_mode | IF_MASK;
put_reg (14, get_reg (15, REG_CURRENT) - 4, REG_CURRENT);
put_reg (15, 0x00000004, REG_CURRENT);
return;
}

/*----------------------------------------------------------------------------*/

void data_abort()
{
spsr[abt_mode] = cpsr;
cpsr = (cpsr & ~MODE_MASK & ~TF_MASK) | abt_mode | IF_MASK;
put_reg (14, get_reg (15, REG_CURRENT), REG_CURRENT);
put_reg (15, 0x00000010, REG_CURRENT);
return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void set_flags(int operation, int a, int b, int rd, int carry)
{
set_NZ(rd);
set_CF(a, rd, carry);
switch (operation)
  {
  case FLAG_ADD: set_VF_ADD (a, b, rd); break;
  case FLAG_SUB: set_VF_SUB (a, b, rd); break;
  default: fprintf(stderr, "Flag setting error\n"); break;
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void set_NZ(unsigned int value)
{
if (value == 0)            cpsr = cpsr |  ZF_MASK;
else                       cpsr = cpsr & ~ZF_MASK;
if ((value & BIT_31) != 0) cpsr = cpsr |  NF_MASK;
else                       cpsr = cpsr & ~NF_MASK;
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void set_CF(unsigned int a, unsigned int rd, int carry)
{                                     /* Two ways result can equal an operand */
if ((rd > a) || ((rd == a) && (carry == 0))) cpsr = cpsr & ~CF_MASK;
else                                         cpsr = cpsr |  CF_MASK;
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void set_VF_ADD(int a, int b, int rd)
{
cpsr = cpsr & ~VF_MASK;                                           /* Clear VF */
if (((~(a ^ b) & (a ^ rd)) & BIT_31) != 0) cpsr = cpsr | VF_MASK;
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void set_VF_SUB(int a, int b, int rd)
{
cpsr = cpsr & ~VF_MASK;                                           /* Clear VF */
if ((((a ^ b) & (a ^ rd)) & BIT_31) != 0) cpsr = cpsr | VF_MASK;
return;
}

/*----------------------------------------------------------------------------*/

int check_cc(int condition)                   /*checks CC against flag status */
{
int go;

switch (condition & 0XF)
  {
  case 0X0: go = zf(cpsr);                                         break;
  case 0X1: go = not(zf(cpsr));                                    break;
  case 0X2: go = cf(cpsr);                                         break;
  case 0X3: go = not(cf(cpsr));                                    break;
  case 0X4: go = nf(cpsr);                                         break;
  case 0X5: go = not(nf(cpsr));                                    break;
  case 0X6: go = vf(cpsr);                                         break;
  case 0X7: go = not(vf(cpsr));                                    break;
  case 0X8: go = and(cf(cpsr), not(zf(cpsr)));                     break;
  case 0X9: go = or(not(cf(cpsr)), zf(cpsr));                      break;
  case 0XA: go = not(xor(nf(cpsr), vf(cpsr)));                     break;
  case 0XB: go = xor(nf(cpsr), vf(cpsr));                          break;
  case 0XC: go = and(not(zf(cpsr)), not(xor(nf(cpsr), vf(cpsr)))); break;
  case 0XD: go = or(zf(cpsr), xor(nf(cpsr), vf(cpsr)));            break;
  case 0XE: go = TRUE;                                             break;
  case 0XF: go = FALSE;                                            break;
  }
return go;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

int not(int x) { if (x == TRUE) return FALSE; else return TRUE; }

int and(int x, int y) 
{
if ((x == TRUE) && (y == TRUE)) return TRUE; else return FALSE;
}

int or(int x, int y)
{
if ((x == TRUE) || (y == TRUE)) return TRUE; else return FALSE;
}

int xor(int x, int y)
{
if (((x == TRUE) && (y == FALSE)) || ((x == FALSE) && (y == TRUE)))
  return TRUE;
else
  return FALSE;
}

/*----------------------------------------------------------------------------*/

int zf(int cpsr) { if ((ZF_MASK & cpsr) != 0) return TRUE; else return FALSE; }
int cf(int cpsr) { if ((CF_MASK & cpsr) != 0) return TRUE; else return FALSE; }
int nf(int cpsr) { if ((NF_MASK & cpsr) != 0) return TRUE; else return FALSE; }
int vf(int cpsr) { if ((VF_MASK & cpsr) != 0) return TRUE; else return FALSE; }

/*----------------------------------------------------------------------------*/

int get_reg(int reg_no, int force_mode)
{
int mode, value;

switch (force_mode)
  {
  case REG_CURRENT: mode = cpsr & MODE_MASK; break;
  case REG_USER:    mode = user_mode;        break;
  case REG_SVC:     mode = sup_mode;         break;
  case REG_FIQ:     mode = fiq_mode;         break;
  case REG_IRQ:     mode = irq_mode;         break;
  case REG_ABT:     mode = abt_mode;         break;
  case REG_UNDEF:   mode = undef_mode;       break;
  }

if (reg_no == 16) value = cpsr;                  /* Trap for status registers */
else if (reg_no == 17)
  {
  if ((mode == user_mode) || (mode == system_mode)) value = cpsr;
  else                                              value = spsr[mode];
  }
else if (reg_no != 15)
  {
  switch (mode)
    {
    case user_mode:
    case system_mode:
      value = r[reg_no];
      break;

    case fiq_mode:
      if (reg_no < 8) value = r[reg_no];
      else            value = fiq_r[reg_no - 8];
      break;

    case irq_mode:
      if (reg_no < 13) value = r[reg_no];
      else             value = irq_r[reg_no - 13];
      break;

    case sup_mode:
      if (reg_no < 13) value = r[reg_no];
      else             value = sup_r[reg_no - 13];
      break;

    case abt_mode:
      if (reg_no < 13) value = r[reg_no];
      else             value = abt_r[reg_no - 13];
      break;

    case undef_mode:
      if (reg_no < 13) value = r[reg_no];
      else             value = undef_r[reg_no - 13];
      break;
    }
  }
else                                                             /* PC access */
  value = r[15] + instruction_length();
                          /* PC := PC+4 (or +2 - Thumb) at start of execution */

return value;
}

/*----------------------------------------------------------------------------*/
/* Modified "get_reg" to give unadulterated copy of PC                        */

int get_reg_monitor(int reg_no, int force_mode)
{
if (reg_no != 15) return get_reg (reg_no, force_mode);
else              return r[15];                                  /* PC access */
}

/*----------------------------------------------------------------------------*/
/* Write to a specified processor register                                    */

void put_reg(int reg_no, int value, int force_mode)
{
int mode;

switch (force_mode)
  {
  case REG_CURRENT: mode = cpsr & MODE_MASK; break;
  case REG_USER:    mode = user_mode;        break;
  case REG_SVC:     mode = sup_mode;         break;
  case REG_FIQ:     mode = fiq_mode;         break;
  case REG_IRQ:     mode = irq_mode;         break;
  case REG_ABT:     mode = abt_mode;         break;
  case REG_UNDEF:   mode = undef_mode;       break;
  }

if (reg_no == 16) cpsr = value;                  /* Trap for status registers */
else if (reg_no == 17)
  {
  if ((mode == user_mode) || (mode == system_mode)) cpsr = value;
  else spsr[mode] = value;
  }
else if (reg_no != 15)
  {
  switch (mode)
    {
    case user_mode:
    case system_mode:
      r[reg_no] = value;
      break;

    case fiq_mode:
      if (reg_no < 8) r[reg_no] = value;
      else        fiq_r[reg_no - 8] = value;
      break;

    case irq_mode:
      if (reg_no < 13) r[reg_no] = value;
      else         irq_r[reg_no - 13] = value;
      break;

    case sup_mode:
      if (reg_no < 13) r[reg_no] = value;
      else         sup_r[reg_no - 13] = value;
      break;

    case abt_mode:
      if (reg_no < 13) r[reg_no] = value;
      else         abt_r[reg_no - 13] = value;
      break;

    case undef_mode:
      if (reg_no < 13) r[reg_no] = value;
      else       undef_r[reg_no - 13] = value;
      break;
    }
  }
else
  r[15] = value & 0XFFFFFFFE;      /* Lose bottom bit, but NOT mode specific! */

return;
}

/*----------------------------------------------------------------------------*/
/* Return the length, in bytes, of the currently expected instruction.        */
/* (4 for ARM, 2 for Thumb)                                                   */

int instruction_length()
{
if ((cpsr & TF_MASK) == 0) return 4;
else                       return 2;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

unsigned int fetch(boolean *abort)
{
unsigned int op_code;
int i;

op_code = read_mem((get_reg(15, REG_CURRENT) - instruction_length ()),
                   instruction_length(), FALSE, FALSE, MEM_INSTRUCTION, abort);

for (i = 0; i < 32; i++)
  if (past_opc_addr[i] == get_reg(15, REG_CURRENT) - instruction_length())
    {
    past_count++;
    i = 32;                                        /* bodged escape from loop */
    }

past_opc_addr[past_opc_ptr++] = get_reg(15, REG_CURRENT) - instruction_length();
past_opc_ptr = past_opc_ptr % past_size;

return op_code;
}

/*----------------------------------------------------------------------------*/

void inc_pc()
{
/*fprintf(stderr, "get PC: %08x\n", get_reg(15, REG_CURRENT) ); */

put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);
                             /* get_reg returns PC+4 for ARM & PC+2 for THUMB */
return;
}

/*----------------------------------------------------------------------------*/

void endian_swap (unsigned int start, unsigned int end)
{
unsigned int i, j;

for (i = start; i < end; i++)
  {
  j = getmem32(i);
  setmem32(i, ((j >> 24) & 0X000000FF) | ((j >>  8) & 0X0000FF00)
            | ((j <<  8) & 0X00FF0000) | ((j << 24) & 0XFF000000));
  }
return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* Source indicates type of read {MEM_SYSTEM, MEM_INSTRUCTION, MEM_DATA}      */

int read_mem(unsigned int address, int size, boolean sign, boolean T,
             int source, boolean *abort)
{
int data;
boolean done;
so_handler_entry *p_handler_entry;

*abort = FALSE;                                       /* Default: don't abort */
done   = FALSE;          /* Indicator that access has (not) been serviced yet */

p_handler_entry = so_handler_list;              /* List of handling libraries */
while (!done && (p_handler_entry != NULL))
  {                                                 /* Explore list, in order */
  if ((p_handler_entry->mem_r_handler != NULL)
  && ((p_handler_entry->mem_r_handler)(address,&data,size,sign,T,source,abort)))
    done = TRUE;
  else p_handler_entry = p_handler_entry->next;
  }

if (!done)                         /* See if access is in shared memory block */
  {
  shared_mem_entry *p_temp;

  p_temp = shared_mem_list;
  while ((p_temp != NULL) && !done)
    {
    if ((address >= p_temp->base) && (address < p_temp->top))
      {
      data = (p_temp->addr[address - p_temp->base + 3] << 24)
           | (p_temp->addr[address - p_temp->base + 2] << 16)
           | (p_temp->addr[address - p_temp->base + 1] <<  8)
           | (p_temp->addr[address - p_temp->base]);
      done = TRUE;
      }
    else p_temp = p_temp->next;
    }
  }

if (!done)                                        /* Default memory behaviour */
  {
  if (address < memory_size)                               /* Address decoder */
    {                                                          /* Main memory */
    data = getmem32(address >> 2);
    }
  else
    {                                              /* Unassigned memory space */
    data = 0X12345678;
    print_out = FALSE;
    }

  switch (address & 0X00000003)                   /* Apply alignment rotation */
    {
    case 0:
      break;                                                         /* RR  0 */
    case 1:
      data = (data << 24) | ((data >> 8) & 0X00FFFFFF);
      break;                                                         /* RR  8 */
    case 2:
      data = (data << 16) | ((data >> 16) & 0X0000FFFF);
      break;                                                         /* RR 16 */
    case 3:
      data = (data << 8) | ((data >> 24) & 0X000000FF);
      break;                                                         /* RR 24 */
    }

  switch (size)
    {
    case 0:
      data = 0;
      break;                                            /* A bit silly really */

    case 1:                                                    /* byte access */
      if ((sign) && ((data & 0X00000080) != 0))
        data = data | 0XFFFFFF00;
      else
        data = data & 0X000000FF;
      break;

    case 2:                                               /* half-word access */
      if ((sign) && ((data & 0X00008000) != 0))
        data = data | 0XFFFF0000;
      else
        data = data & 0X0000FFFF;
      break;

    case 4:
      break;                                                   /* word access */

    default:
      fprintf(stderr, "Illegally sized memory read\n");
    }


  if ((runflags & 0x20) && (source == MEM_DATA)) /* check watchpoints enabled */
    {
    if (check_watchpoints(address, data, size, 1))      // @@@
      {
      status = CLIENT_STATE_WATCHPOINT;
      }
    }

  }

/*if (T == TRUE) printf("User space forced\n");                               */

/*
   switch(source)
   {
   case 0: break;
   case 1: printf("Instruction address %08X data %08X\n", address, data); break;
   case 2: printf("Data read   address %08X data %08X\n", address, data); break;
   }
*/

//if (address >= 0x1000) *abort = TRUE;			// TEST EXPT @@@@@@@@@
return data;
}

/*----------------------------------------------------------------------------*/

void write_mem(unsigned int address, int data, int size, boolean T, int source,
               boolean *abort)
{
unsigned int mask;
boolean done;
so_handler_entry *p_handler_entry;

*abort = FALSE;                                       /* Default: don't abort */
done   = FALSE;          /* Indicator that access has (not) been serviced yet */

p_handler_entry = so_handler_list;              /* List of handling libraries */
while (!done && (p_handler_entry != NULL))
  {                                                 /* Explore list, in order */
  if ((p_handler_entry->mem_w_handler != NULL)
  && ((p_handler_entry->mem_w_handler)(address, data, size, T, source, abort)))
    done = TRUE;
  else p_handler_entry = p_handler_entry->next;
  }

if (!done)                                           /* Try for shared memory */
  {
  shared_mem_entry *p_temp;
  unsigned int i;

  p_temp = shared_mem_list;        /* See if access is in shared memory block */
  while ((p_temp != NULL) && !done)
    {
    if ((address >= p_temp->base) && (address < p_temp->top))
      {
      for (i = 0; i < size; i++)
        {
        p_temp->addr[address - p_temp->base + i] = data;
        data = data >> 8;
        }
      done = TRUE;
      }
    else p_temp = p_temp->next;
    }

//if (address >= 0x1000) {*abort = TRUE; done = TRUE;}			// TEST EXPT @@@@@@@

  if (!done)                                             /* Default behaviour */
    {
    switch (size)
      {
      case 0:
        break;                                          /* A bit silly really */

      case 1:                                                  /* byte access */
        mask = 0X000000FF << (8 * (address & 0X00000003));
        data = data & 0xFF;
        data = data | (data << 8) | (data << 16) | (data << 24);
        break;

      case 2:                                            /* half-word acccess */
        mask = 0X0000FFFF << (8 * (address & 0X00000002));
        data = data & 0xFFFF;
        data = data | (data << 16);
        break;

      case 4:                                                  /* word access */
        mask = 0XFFFFFFFF;
        break;

      default:
        fprintf(stderr, "Illegally sized memory write\n");
      }

    if (address < memory_size)                             /* Address decoder */
      {
      setmem32(address >> 2, (getmem32(address >> 2) & ~mask) | (data & mask));
      }
    else
      {
      // fprintf(stderr, "Writing %08X  data = %08X\n", address, data);
      print_out = FALSE;
      }
    if ((runflags & 0x20) && (source == MEM_DATA))/* Check watchpoints enabled*/
      {
      if (check_watchpoints(address, data, size, 0))      // @@@
        {
        status = CLIENT_STATE_WATCHPOINT;
        }
      }
    }

/*if (T == TRUE) printf("User space forced\n");   */

    /*
     switch(source)
     {
     case 0: break;
     case 1: printf("Shouldn't happen\n"); break;
     case 2: printf("Data write  address %08X data %08X\n", address, data); break;
     }
    */
  }

return;
}

/*----------------------------------------------------------------------------*/

/*- - - - - - - - - - - - watchpoints - - - - - - - - - - - - - - - - - - - */
/*                      to be completed                                     */

/* Needs privilege information @@@*/

int check_watchpoints(unsigned int address, int data, int size, int direction)
{
int i, may_break;

//if (direction == 0)
//  fprintf(stderr, "Data write, address %08X, data %08X, size %d\n", address, data, size);
//else
//  fprintf(stderr, "Data read,  address %08X, data %08X, size %d\n", address, data, size);

for (i = 0, may_break = FALSE; (i < NO_OF_WATCHPOINTS) && !may_break; i++)
  {                                                     /* Search watchpoints */
  may_break = ((emul_wp_flag[0] & emul_wp_flag[1] & (1<<i)) != 0);
                                                      /* Watchpoint is active */

  may_break &= ((watchpoints[i].size & size) != 0);       /* Size is allowed? */

  if (may_break)                                           /* Check direction */
    if (direction == 0)                                          /* Write @@@ */
      may_break = (watchpoints[i].cond & 0x10) != 0;
    else                                                          /* Read @@@ */
      may_break = (watchpoints[i].cond & 0x20) != 0;

  if (may_break)                                    /* Try address comparison */
    switch (watchpoints[i].cond & 0x0C)
      {
      case 0x00: may_break = FALSE; break;
      case 0x04: may_break = FALSE; break;
      case 0x08:                   /* Case of between address A and address B */
        if ((address < watchpoints[i].addra) || (address > watchpoints[i].addrb))
          may_break = FALSE;
        break;

      case 0x0C:                                              /* Case of mask */
        if ((address & watchpoints[i].addrb) != watchpoints[i].addra)
          may_break = FALSE;
        break;
      }

  if (may_break)                                       /* Try data comparison */
    switch (watchpoints[i].cond & 0x03)
      {
      case 0x00: may_break = FALSE; break;
      case 0x01: may_break = FALSE; break;
      case 0x02:                         /* Case of between data A and data B */
        if ((data < watchpoints[i].dataa[0]) || (data > watchpoints[i].datab[0]))
          may_break = FALSE;
        break;

      case 0x03:                                              /* Case of mask */
        if ((data & watchpoints[i].datab[0]) != watchpoints[i].dataa[0])
          may_break = FALSE;
        break;
      }
      // Expansion space for more comparisons @@@  e.g. privilege

  }                                                        /* End of for loop */

//
//if (may_break) fprintf(stderr, "Watchpoint!\n");
//
//for (i = 0; i < NO_OF_WATCHPOINTS; i++) 
//  {
//  fprintf(stderr,"====== WATCHPOINT %d ====\n", i); 
//  fprintf(stderr,"address A: %08x\n",  watchpoints[i].addra); 
//  fprintf(stderr,"address B: %08x\n",  watchpoints[i].addrb);
//  fprintf(stderr,"Data A: %08x%08x\n", watchpoints[i].dataa[1],
//                                       watchpoints[i].dataa[0]);
//  fprintf(stderr,"Data B: %08x%08x\n", watchpoints[i].datab[1],
//                                       watchpoints[i].datab[0]);
//  fprintf(stderr,"State: %08x\n",      watchpoints[i].state); 
//  fprintf(stderr,"Condition: %08X\n",  watchpoints[i].cond); 
//  fprintf(stderr,"Size: %d\n", watchpoints[i].size); 
//  }
//
//
//     typedef struct {
//     int state;
//     char cond;
//     char size;
//     int addra;
//     int addrb;
//     int dataa[2];
//     int datab[2];
//     } BreakElement;

return may_break;
}


/*- - - - - - - - - - - - end watchpoints - - - - - - - - - - - - - - - - - */


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* As the compiler can't manage it ...                                        */

int lsl(int value, int distance, int *cf)         /* cf is -internal- Boolean */
{
int result;

if (distance != 0)
  {
  if (distance < 32)
    {
    result = value << distance;
    *cf = (((value << (distance - 1)) & BIT_31) != 0);
    }
  else
    {
    result = 0X00000000;
    if (distance == 32) *cf = ((value & BIT_0) != 0);
    else                *cf = (0 != 0);             /* internal "false" value */
    }
  }
else
  result = value;

return result;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int lsr(unsigned int value, int distance, int *cf)/* cf is -internal- Boolean */
{
unsigned int result, mask;

if (distance != 0)
  {
  if (distance < 32)
    {
    if (distance != 0) mask = ~(0XFFFFFFFF << (32 - distance));
    else               mask = 0XFFFFFFFF;
    result = (value >> distance) & mask;             /* Enforce logical shift */
    *cf = (((value >> (distance - 1)) & BIT_0) != 0);
    }
  else
    {                             /* Make a special case because C is so crap */
    result = 0X00000000;
    if (distance == 32) *cf = ((value & BIT_31) != 0);
    else                *cf = (0 != 0);             /* internal "false" value */
    }
  }
else
  result = value;

return result;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int asr(int value, int distance, int *cf)         /* cf is -internal- Boolean */
{
int result;

if (distance != 0)
  {
  if (distance < 32)
    {
    result = value >> distance;
    if (((value & BIT_31) != 0) && (distance != 0))
      result = result | (0XFFFFFFFF << (32 - distance));
                                  /* Sign extend - I don't trust the compiler */
    *cf = (((value >> (distance - 1)) & BIT_0) != 0);
    }
  else
    {                             /* Make a special case because C is so crap */
    *cf = ((value & BIT_31) != 0);
    if ((value & BIT_31) == 0) result = 0X00000000;
    else                       result = 0XFFFFFFFF;
    }
  }
else
  result = value;

return result;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int ror(unsigned int value, int distance, int *cf)/* cf is -internal- Boolean */
{
int result;

if (distance != 0)
  {
  distance = distance & 0X1F;
  result = lsr (value, distance, cf) | lsl (value, 32 - distance, cf);
                                                     /* cf acts as dummy here */
  *cf = (((value >> (distance - 1)) & BIT_0) != 0);
  }
else
  result = value;

return result;
}

/*----------------------------------------------------------------------------*/

/*------------------------------ THUMB EXECUTE -------------------------------*/

/*----------------------------------------------------------------------------*/

void
data0 (unsigned int op_code)
{
  unsigned int op2, rn;
  unsigned int shift, result, cf;

  rn = get_reg (((op_code >> 3) & 7), REG_CURRENT);        /* Called "Rm" in shifts */
  shift = ((op_code >> 6) & 0X0000001F);        /* Extracted speculatively */

  if ((op_code & 0X1800) != 0X1800)        /* Shifts */
    {
      cf = ((cpsr & CF_MASK) != 0);        /* default */
      switch (op_code & 0X1800)
        {
        case 0X0000:
          result = lsl (rn, shift, &cf);
          break;                /* LSL (1) */
        case 0X0800:                /* LSR (1) */
          if (shift == 0)
            shift = 32;
          result = lsr (rn, shift, &cf);
          break;
        case 0X1000:                /* ASR (1) */
          if (shift == 0)
            shift = 32;
          result = asr (rn, shift, &cf);
          break;
        default:
          fprintf(stderr, "This compiler is broken\n");
          break;
        }

      if (cf)
        cpsr = cpsr |  CF_MASK;
      else
        cpsr = cpsr & ~CF_MASK;
      set_NZ (result);
      put_reg ((op_code & 7), result, REG_CURRENT);
    }
  else
    {
      if ((op_code & 0X0400) == 0)        /* ADD(3)/SUB(3) */
        {
          op2 = get_reg ((op_code >> 6) & 7, REG_CURRENT);
        }
      else                        /* ADD(1)/SUB(1) */
        {
          op2 = (op_code >> 6) & 7;
        };

      if ((op_code & 0X0200) == 0)
        {
          result = rn + op2;
          set_flags (FLAG_ADD, rn, op2, result, 0);
        }
      else
        {
          result = rn - op2;
          set_flags (FLAG_SUB, rn, op2, result, 1);
        }

      put_reg (op_code & 7, result, REG_CURRENT);
    }

  return;
}

/*----------------------------------------------------------------------------*/

void
data1 (unsigned int op_code)
{
  int rd, imm;
  int result;

  rd = (op_code >> 8) & 7;
  imm = op_code & 0X00FF;

  switch (op_code & 0X1800)
    {
    case 0X0000:                /* MOV (1) */
      result = imm;
      set_NZ (result);
      put_reg (rd, result, REG_CURRENT);
      break;

    case 0X0800:                /* CMP (1) */
      result = (get_reg (rd, REG_CURRENT) - imm);
      set_flags (FLAG_SUB, get_reg (rd, REG_CURRENT), imm, result, 1);
      break;

    case 0X1000:                /* ADD (2) */
      result = (get_reg (rd, REG_CURRENT) + imm);
      set_flags (FLAG_ADD, get_reg (rd, REG_CURRENT), imm, result, 0);
      put_reg (rd, result, REG_CURRENT);
      break;

    case 0X1800:                /* SUB (2) */
      result = (get_reg (rd, REG_CURRENT) - imm);
      set_flags (FLAG_SUB, get_reg (rd, REG_CURRENT), imm, result, 1);
      put_reg (rd, result, REG_CURRENT);
      break;
    }

  return;
}

/*----------------------------------------------------------------------------*/

void data_transfer(unsigned int op_code)
{
unsigned int rd, rm;
int cf;
boolean abort;


signed int result;

if ((op_code & 0X1000) == 0)                                /* NOT load/store */
  {
  if ((op_code & 0X0800) == 0)                       /* NOT load literal pool */
    {
    if ((op_code & 0X0400) == 0)                           /* Data processing */
      {
      rd = get_reg ((op_code & 7), REG_CURRENT);
      rm = get_reg (((op_code >> 3) & 7), REG_CURRENT);

      switch (op_code & 0X03C0)                     /* data processing opcode */
        {
        case 0X0000:                                                   /* AND */
          result = rd & rm;
          put_reg (op_code & 7, result, REG_CURRENT);
          set_NZ (result);
          break;

        case 0X0040:                                                   /* EOR */
          result = rd ^ rm;
          put_reg (op_code & 7, result, REG_CURRENT);
          set_NZ (result);
          break;

        case 0X0080:                                               /* LSL (2) */
          cf = ((cpsr & CF_MASK) != 0);                            /* default */
          result = lsl (rd, rm & 0X000000FF, &cf);
          if (cf) cpsr = cpsr |  CF_MASK;
          else    cpsr = cpsr & ~CF_MASK;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X00C0:                                               /* LSR (2) */
          cf = ((cpsr & CF_MASK) != 0);                            /* default */
          result = lsr (rd, rm & 0X000000FF, &cf);
          if (cf) cpsr = cpsr |  CF_MASK;
          else    cpsr = cpsr & ~CF_MASK;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X0100:                                               /* ASR (2) */
          cf = ((cpsr & CF_MASK) != 0);                            /* default */
          result = asr (rd, rm & 0X000000FF, &cf);
          if (cf) cpsr = cpsr |  CF_MASK;
          else    cpsr = cpsr & ~CF_MASK;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X0140:                                                   /* ADC */
          result = rd + rm;
          if ((cpsr & CF_MASK) != 0) result = result + 1;           /* Add CF */
          set_flags (FLAG_ADD, rd, rm, result, cpsr & CF_MASK);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X0180:                                                   /* SBC */
          result = rd - rm - 1;
          if ((cpsr & CF_MASK) != 0) result = result + 1;
          set_flags (FLAG_SUB, rd, rm, result, cpsr & CF_MASK);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X01C0:                                                   /* ROR */
          cf = ((cpsr & CF_MASK) != 0);                            /* default */
          result = ror (rd, rm & 0X000000FF, &cf);
          if (cf) cpsr = cpsr |  CF_MASK;
          else    cpsr = cpsr & ~CF_MASK;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X0200:                                                   /* TST */
          set_NZ(rd & rm);
          break;

        case 0X0240:                                                   /* NEG */
          result = -rm;
          put_reg (op_code & 7, result, REG_CURRENT);
          set_flags (FLAG_SUB, 0, rm, result, 1);
          break;

        case 0X0280:                                               /* CMP (2) */
          set_flags (FLAG_SUB, rd, rm, rd - rm, 1);
          break;

        case 0X02C0:                                                   /* CMN */
          set_flags (FLAG_ADD, rd, rm, rd + rm, 0);
          break;

        case 0X0300:                                                   /* ORR */
          result = rd | rm;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X00340:                                                  /* MUL */
          result = rm * rd;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X0380:                                                   /* BIC */
          result = rd & ~rm;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;

        case 0X03C0:                                                   /* MVN */
          result = ~rm;
          set_NZ (result);
          put_reg (op_code & 7, result, REG_CURRENT);
          break;
        }                                                    /* End of switch */
      }
    else                                           /* special data processing */
      {                                                     /* NO FLAG UPDATE */
      switch (op_code & 0X0300)
        {
        case 0X0000:                                /* ADD (4) high registers */
          rd = ((op_code & 0X0080) >> 4) | (op_code & 7);
          rm = get_reg (((op_code >> 3) & 15), REG_CURRENT);
          put_reg (rd, get_reg (rd, REG_CURRENT) + rm, REG_CURRENT);
          break;

        case 0X0100:                                /* CMP (3) high registers */
          rd = get_reg((((op_code & 0X0080) >> 4) | (op_code & 7)),
                          REG_CURRENT);
          rm = get_reg(((op_code >> 3) & 15), REG_CURRENT);
          set_flags (FLAG_SUB, rd, rm, rd - rm, 1);
          break;

        case 0X0200:                                /* MOV (2) high registers */
          rd = ((op_code & 0X0080) >> 4) | (op_code & 7);
          rm = get_reg (((op_code >> 3) & 15), REG_CURRENT);

          if (rd == 15)
            rm = rm & 0XFFFFFFFE;                          /* Tweak mov to PC */
          put_reg (rd, rm, REG_CURRENT);
          break;

        case 0X0300:                                             /* BX/BLX Rm */
          bx ((op_code >> 3) & 0XF, op_code & 0X0080);
          break;
        }                                                    /* End of switch */
      }
    }
  else                                              /* load from literal pool */
    {                                                               /* LDR PC */
    unsigned int address, data;
    rd = ((op_code >> 8) & 7);
    address = (((op_code & 0X00FF) << 2)) +
                         (get_reg(15, REG_CURRENT) & 0XFFFFFFFC);
    data = read_mem(address, 4, FALSE, FALSE, MEM_DATA, &abort);
    if (!abort) put_reg(rd, data, REG_CURRENT);
    else data_abort();
    }
  }
else
  {                             /* load/store word, halfword, byte, signed byte */
  int rm, rn;
  int data;

  rd = (op_code & 7);
  rn = get_reg (((op_code >> 3) & 7), REG_CURRENT);
  rm = get_reg (((op_code >> 6) & 7), REG_CURRENT);

  switch (op_code & 0X0E00)
    {
    case 0X0000:                                          /* STR (2) register */
      write_mem(rn+rm, get_reg (rd, REG_CURRENT), 4, FALSE, MEM_DATA, &abort);
      break;

    case 0X0200:                                         /* STRH (2) register */
      write_mem(rn+rm, get_reg (rd, REG_CURRENT), 2, FALSE, MEM_DATA, &abort);
      break;

    case 0X0400:                                         /* STRB (2) register */
      write_mem(rn+rm, get_reg (rd, REG_CURRENT), 1, FALSE, MEM_DATA, &abort);
      break;

    case 0X0600:                                            /* LDRSB register */
      data = read_mem(rn+rm, 1, TRUE, FALSE, MEM_DATA, &abort);  /* Sign ext. */
      if (!abort) put_reg (rd, data, REG_CURRENT);
      break;

    case 0X0800:                                          /* LDR (2) register */
      data = read_mem(rn+rm, 4, FALSE, FALSE, MEM_DATA, &abort);
      if (!abort) put_reg (rd, data, REG_CURRENT);
      break;

    case 0X0A00:                                         /* LDRH (2) register */
      data = read_mem(rn+rm, 2, FALSE, FALSE, MEM_DATA, &abort);  /* Zero ext.*/
      if (!abort) put_reg (rd, data, REG_CURRENT);
      break;

    case 0X0C00:                                                  /* LDRB (2) */
      data = read_mem(rn+rm, 1, FALSE, FALSE, MEM_DATA, &abort);  /* Zero ext.*/
      if (!abort) put_reg (rd, data, REG_CURRENT);
      break;

    case 0X0E00:                                                 /* LDRSH (2) */
      data = read_mem(rn+rm, 2, TRUE, FALSE, MEM_DATA, &abort);  /* Sign ext. */
      if (!abort) put_reg (rd, data, REG_CURRENT);
      break;
    }
  if (abort) data_abort();
  }
return;
}

/*----------------------------------------------------------------------------*/

void transfer0(unsigned int op_code)
{
int rd, rn;
int location, data;
boolean abort;

rn = get_reg (((op_code >> 3) & 7), REG_CURRENT);

if ((op_code & 0X0800) == 0)                                           /* STR */
  {
  rd = get_reg ((op_code & 7), REG_CURRENT);
  if ((op_code & 0X1000) == 0)                           /* STR (1) 5-bit imm */
    {
    location = rn + ((op_code >> 4) & 0X07C);             /* shift twice = *4 */
    write_mem (location, rd, 4, FALSE, MEM_DATA, &abort);
    }
  else                                                            /* STRB (1) */
    {
    location = rn + ((op_code >> 6) & 0X1F);
    write_mem (location, rd, 1, FALSE, MEM_DATA, &abort);
    }
  }
else                                                               /* LDR (1) */
  {
  rd = op_code & 7;
  if ((op_code & 0X1000) == 0)
    {
    location = (rn + ((op_code >> 4) & 0X07C));           /* shift twice = *4 */
    data = read_mem(location, 4, FALSE, FALSE, MEM_DATA, &abort);
    }
  else                                                            /* LDRB (1) */
    {
    location = (rn + ((op_code >> 6) & 0X1F));
    data = read_mem(location, 1, FALSE, FALSE, MEM_DATA, &abort);/* 0 extended*/
    }
  if (!abort) put_reg (rd, data, REG_CURRENT);
  }

if (abort) data_abort();

return;
}

/*----------------------------------------------------------------------------*/

void transfer1 (unsigned int op_code)
{
int rd, rn;
int data, location;
boolean abort;

switch (op_code & 0X1800)
  {
  case 0X0000:                                                    /* STRH (1) */
    rn = get_reg ((op_code >> 3) & 7, REG_CURRENT);
    rd = op_code & 7;
    data = get_reg (rd, REG_CURRENT);
    location = rn + ((op_code >> 5) & 0X3E);                   /* x2 in shift */
    write_mem (location, data, 2, FALSE, MEM_DATA, &abort);
    break;

  case 0X0800:                                                    /* LDRH (1) */
    rd = op_code & 7;
    rn = get_reg ((op_code >> 3) & 7, REG_CURRENT);
    location = rn + ((op_code >> 5) & 0X3E);                   /* x2 in shift */
    data = read_mem(location, 2, FALSE, FALSE, MEM_DATA, &abort);/* 0 extended*/
    if (!abort) put_reg (rd, data, REG_CURRENT);
    break;

  case 0X1000:                                                 /* STR (3) -SP */
    data = get_reg (((op_code >> 8) & 7), REG_CURRENT);
    rn = get_reg (13, REG_CURRENT);                                     /* SP */
    location = rn + ((op_code & 0X00FF) * 4);
    write_mem (location, data, 4, FALSE, MEM_DATA, &abort);
    break;

  case 0X1800:                                                 /* LDR (4) -SP */
    rd = (op_code >> 8) & 7;
    rn = get_reg (13, REG_CURRENT);                                     /* SP */
    location = rn + ((op_code & 0X00FF) * 4);                  /* x2 in shift */
    data = read_mem (location, 4, FALSE, FALSE, MEM_DATA, &abort);
    if (!abort) put_reg (rd, data, REG_CURRENT);
    break;
  }

if (abort) data_abort();

return;
}

/*----------------------------------------------------------------------------*/

void sp_pc(unsigned int op_code)
{
int rd, sp, data;

if ((op_code & 0X1000) == 0)                                  /* ADD SP or PC */
  {
  rd = (op_code >> 8) & 7;

  if ((op_code & 0X0800) == 0)                                  /* ADD(5) -PC */
    data = (get_reg(15, REG_CURRENT) & 0XFFFFFFFC) + ((op_code & 0X00FF) << 2);
                                                   /* get_reg supplies PC + 2 */
  else                                                          /* ADD(6) -SP */
    data = (get_reg(13, REG_CURRENT)) + ((op_code & 0X00FF) << 2);
  put_reg(rd, data, REG_CURRENT);
  }
else                                                             /* Adjust SP */
  {
  switch (op_code & 0X0F00)
    {
    case 0X0000:
      if ((op_code & 0X0080) == 0)                              /* ADD(7) -SP */
        sp = get_reg(13, REG_CURRENT) + ((op_code & 0X7F) << 2);
      else                                                      /* SUB(4) -SP */
        sp = get_reg(13, REG_CURRENT) - ((op_code & 0X7F) << 2);
      put_reg(13, sp, REG_CURRENT);
      break;

    case 0X0400:
    case 0X0500:
    case 0X0C00:
    case 0X0D00:
      {
      int reg_list;

      reg_list = op_code & 0X000000FF;

      if ((op_code & 0X0800) == 0)                                    /* PUSH */
        {
        if ((op_code & 0X0100) != 0) reg_list = reg_list | 0X4000;
        stm(2, 13, reg_list, TRUE, FALSE);
        }
      else                                                             /* POP */
        {
        if ((op_code & 0X0100) != 0) reg_list = reg_list | 0X8000;
        ldm (1, 13, reg_list, TRUE, FALSE);
        }
      }
      break;

    case 0X0E00:                                                /* Breakpoint */
//      fprintf(stderr, "Breakpoint\n");
      breakpoint();
      break;

    case 0X0100:
    case 0X0200:
    case 0X0300:
    case 0X0600:
    case 0X0700:
    case 0X0800:
    case 0X0900:
    case 0X0A00:
    case 0X0B00:
    case 0X0F00:
//      fprintf(stderr, "Undefined\n");
      undefined();
      break;
    }

  return;
  }
}

/*----------------------------------------------------------------------------*/

void lsm_b(unsigned int op_code)
{
unsigned int offset;

if ((op_code & 0X1000) == 0)
  {
  if ((op_code & 0X0800) == 0)                                    /* STM (IA) */
    stm(1, (op_code >> 8) & 7, op_code & 0X000000FF, TRUE, FALSE);
  else                                                            /* LDM (IA) */
    ldm(1, (op_code >> 8) & 7, op_code & 0X000000FF, TRUE, FALSE);
  }
else                                               /* conditional BRANCH B(1) */
  {
  if ((op_code & 0X0F00) != 0X0F00)                      /* Branch, not a SVC */
    {
    if (check_cc (op_code >> 8) == TRUE)
      {
      offset = (op_code & 0X00FF) << 1;                        /* sign extend */
      if ((op_code & 0X0080) != 0) offset = offset | 0XFFFFFE00;

      put_reg(15, get_reg(15, REG_CURRENT) + offset, REG_CURRENT);
                                                   /* get_reg supplies pc + 2 */
/*    fprintf(stderr, "%08X", address + 4 + offset);                          */
      }
    }
  else                                                                 /* SVC */
    {
    offset = op_code & 0X00FF;
                 /* bodge op_code to pass only SVC No. N.B. no copro in Thumb */
    my_system(offset);
    }
  }
}


/*----------------------------------------------------------------------------*/

void thumb_branch1(unsigned int op_code, int exchange)
{
int offset, lr;

lr = get_reg (14, REG_CURRENT);              /* Retrieve first part of offset */
offset = lr + ((op_code & 0X07FF) << 1);

lr = get_reg (15, REG_CURRENT) - 2 + 1;         /* + 1 to indicate Thumb mode */

if (exchange == TRUE)
  {
  cpsr = cpsr & ~TF_MASK;                               /* Change to ARM mode */
  offset = offset & 0XFFFFFFFC;
  }

put_reg(15, offset, REG_CURRENT);
put_reg(14, lr, REG_CURRENT);

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void thumb_branch(unsigned int op_code)
{
int offset;

switch (op_code & 0X1800)
  {
  case 0X0000:                                            /* B -uncond. B(2)  */
    offset = (op_code & 0X07FF) << 1;
    if ((op_code & 0X0400) != 0) offset = offset | 0XFFFFF000; /* sign extend */
    put_reg (15, (get_reg (15, REG_CURRENT) + offset), REG_CURRENT);
    break;

  case 0X0800:                                                         /* BLX */
    if ((op_code & 0X0001) == 0) thumb_branch1 (op_code, TRUE);
    else fprintf(stderr, "Undefined\n");
    break;

  case 0X1000:                                                   /* BL prefix */
    BL_prefix = op_code & 0X07FF;
    offset = BL_prefix << 12;

    if ((BL_prefix & 0X0400) != 0) offset = offset | 0XFF800000; /* Sign ext. */
    offset = get_reg (15, REG_CURRENT) + offset;
    put_reg (14, offset, REG_CURRENT);
    break;

  case 0X1800:                                                          /* BL */
    thumb_branch1 (op_code, FALSE);
    break;
  }

return;
}

/*----------------------------------------------------------------------------*/

/*------------------------------ Charlie's functions--------------------------*/

/*----------------------------------------------------------------------------*/

// Jesus wept.   What was wrong with "read_mem" and "write_mem"?  @@@
// If int < 32 bits the whole lot is broken anyway! @@@


unsigned int getmem32(int number)
{
  number = number % RAMSIZE;
  return memory[(number << 2)]           | memory[(number << 2) + 1] << 8
       | memory[(number << 2) + 2] << 16 | memory[(number << 2) + 3] << 24;
}

void setmem32(int number, unsigned int reg)
{
  number = number & (RAMSIZE - 1);
  memory[(number << 2) + 0] = (reg >> 0)  & 0xff;
  memory[(number << 2) + 1] = (reg >> 8)  & 0xff;
  memory[(number << 2) + 2] = (reg >> 16) & 0xff;
  memory[(number << 2) + 3] = (reg >> 24) & 0xff;
}


/*----------------------------------------------------------------------------*/
/* Terminal support routines                                                  */

void init_buffer(ring_buffer *buffer)
{ buffer->iHead = 0; buffer->iTail = 0; return; }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Measure buffer occupancy                                                   */

int count_buffer(ring_buffer *buffer)
{
int i;
i = buffer->iHead - buffer->iTail;
if (i < 0) i = i + RING_BUF_SIZE;
return i;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean put_buffer(ring_buffer *buffer, unsigned char c)
{
int status;
unsigned int temp;

temp = (buffer->iHead + 1) % RING_BUF_SIZE;

if (temp != buffer->iTail)
  {
  buffer->buffer[temp] = c;
  buffer->iHead = temp;
  status = TRUE;                                                      /* Okay */
  }
else
  status = FALSE;                                              /* Buffer full */

return status;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean get_buffer(ring_buffer *buffer, unsigned char *c)
{
int status;

if (buffer->iTail != buffer->iHead)
  {
  buffer->iTail = (buffer->iTail + 1) % RING_BUF_SIZE;/* Preincrement pointer */
  *c = buffer->buffer[buffer->iTail];
  status = TRUE;                                                      /* Okay */
  }
else
  status = FALSE;                                             /* Nothing read */

return status;
}

/*----------------------------------------------------------------------------*/
/*------------------------------ lap's functions -----------------------------*/
/*----------------------------------------------------------------------------*/
/* set command-line options                                                   */

boolean set_options(int argc, char *argv[], boolean *help)
{
char c;
char *string;

memory_size = MEM_SIZE;
config_filename = NULL;                                 /* Set default values */
*help = FALSE;

#ifdef VSCREEN_SUPPORT
  vscreen_ww = SCR_WIDTH;
  vscreen_hh = SCR_HEIGHT;
  vscreen_ss = SCR_SCALE;
  vscreen_cc = SCR_COL;
  vscreen_ll = 0;                                                     /* LEDs */
  vscreen_kk = SCR_KEY;
	// Ideally, if no key value given, make one up and allocate memory in this prog. @@@
#endif

if (VERBOSE)                                     /* Print out input arguments */
  {                                             /* Done before options parsed */
  int i;
  g_printerr("%s: arguments  %d\n", my_name, argc);
  for (i = 0; i < argc; i++) g_printerr("Arg %2d: '%s'\n", i, argv[i]);
  }

if (argc != 1)
  {
  argv++;                                                     /* Next pointer */

  while ((argc > 1) && ((*argv)[0] == '-'))
    {
    c = (*argv)[1];
    switch (c)
      {
      case 'H':
      case 'h':
        *help = TRUE;
        break;

      case 'D':
      case 'd':
        verbose = TRUE;                                /* For help with Debug */
        break;

      case 'M':
      case 'm':
        {
        int number;
        char *trash;

        if ((*argv)[2] != '\0')             /* If no space follows option ... */
          number = get_number(&((*argv)[2]), &trash);
        else
          {
          if (argc > 2)                          /* ... else, if it exists ...*/
            {
            number = get_number(argv[1], &trash);
            argc--;                          /* ... removing it from the list */
            argv++;
            }
          else
            number = -1;
          }
        if (number >= 0) memory_size = number;
        }
        break;

      case 'S':
      case 's':
        {
        int key, address, size;
        char *trash;

        if ((*argv)[2] != '\0')             /* If no space follows option ... */
          key = get_number(&((*argv)[2]), &trash);
        else
          {
          if (argc > 2)                          /* ... else, if it exists ...*/
            {
            key = get_number(argv[1], &trash);
            argc--;                          /* ... removing it from the list */
            argv++;
            }
          else
            key = -1;
          }

        if (key >= 0)                                       /* Key specified? */
          {
          address = get_number(argv[1], &trash);           /* Try for address */
          if (address >= 0)                             /* Address specified? */
            {
            argc--;                                   /* Remove from the list */
            argv++;
            size = get_number(argv[1], &trash);
            if (size >= 0)                                 /* Size specified? */
              {
              argc--;                                 /* Remove from the list */
              argv++;
              }
            }
          }

        if ((key >= 0) && (address >= 0) && (size >= 0))   /* If valid ...    */
          link_shared_mem(key, address, size);             /* ... add to list */

        }
        break;


#ifdef VSCREEN_SUPPORT
      case 'V':
      case 'v':
        {
        int number;
        char *trash;

        use_vscreen = TRUE;

        if ((*argv)[2] != '\0')             /* If no space follows option ... */
          number = get_number(&((*argv)[2]), &trash);
        else
          {
          if (argc > 2)                          /* ... else, if it exists ...*/
            {
            number = get_number(argv[1], &trash);
            argc--;                          /* ... removing it from the list */
            argv++;
            }
          else
            number = -1;
          }

        if (number >= 0)                                  /* Width specified? */
          {
          vscreen_ww = number;
          vscreen_hh = (number * 3) / 4;                       /* New default */
          number = get_number(argv[1], &trash);
          if (number >= 0)                               /* Height specified? */
            {
            vscreen_hh = number;
            argc--;                                   /* Remove from the list */
            argv++;
            number = get_number(argv[1], &trash);
            if (number >= 0)                              /* Scale specified? */
              {
              vscreen_ss = number;
              argc--;                                 /* Remove from the list */
              argv++;
              number = get_number(argv[1], &trash);
              if (number >= 0)                          /* Colours specified? */
                {
                vscreen_cc = number;
                argc--;                               /* Remove from the list */
                argv++;
                number = get_number(argv[1], &trash);
                if (number >= 0)                     /* No of LEDs specified? */
                  {
                  vscreen_ll = number;
                  argc--;                             /* Remove from the list */
                  argv++;
                  number = get_number(argv[1], &trash);
                  if (number >= 0)            /* Shared memory key specified? */
                    {
                    vscreen_kk = number;
                    argc--;                           /* Remove from the list */
                    argv++;
                    }
                  }
                }
              }
            }
          }
        }
        break;
#endif

      case 'O':
      case 'o':
        string = get_name_string(&argc, &argv);
        if (string != NULL) config_filename = string;
        else
          {
          g_printerr("%s: ERROR - configuration filename missing\n", my_name);
          return FALSE;
          }
        break;

#define XXX 100

      case 'L':
      case 'l':
        string = get_name_string(&argc, &argv);
        if (string != NULL)
          {
          unsigned char s_temp[XXX];        /* Build library parameter string */
          unsigned int i;

          for (i = 0; (string[i] != '\0') && (i < (XXX - 1)); i++)
            s_temp[i] = string[i];
          s_temp[i] = '\0';

          while ((argc > 2) && (*argv[1] != '-'))
            {
            append(s_temp, argv[1], XXX);
            argc--;
            argv++;
            }

          if (!link_so_handler(string, FALSE, s_temp)) /* Uses argv as string */
            return FALSE;				// More error handling?  @@@
          }
        else
          {
          g_printerr("%s: ERROR - library filename missing\n", my_name);
          return FALSE;
          }
        break;

      default:
        g_printerr("%s: WARNING - unknown option -%c\n", my_name, c);
        break;
      }
    argc--;                                    /* Remove parameter from count */
    argv++;                                                   /* Next pointer */
    }
  }

if (config_filename != NULL)
  {
  unsigned int line;

  if ((config_handle = fopen(config_filename, "r")) ==  NULL)
    {
    g_printerr("%s: ERROR - can't open configuration file\n", my_name);
    return FALSE ;
    }
  else
    {
    char place[MAX_CONFIG_LINE+1];
    char option;

    line = 1;
    while (!feof(config_handle))
      {
      input_line(config_handle, place, MAX_CONFIG_LINE);

      string = &place[0];
      while ((*string == ' ') || (*string == '\t')) string++;
      if (*string == '-') string++;
      option = *string;
      if (option != '\0') string++;
      while ((*string == ' ') || (*string == '\t')) string++;
      switch (option)
        {
        case 'L':
        case 'l':
          {
          int i;
          char *p_s;

          i = 0;                               /* Find length of next element */
          while ((string[i]!=' ')&&(string[i]!='\t')&&(string[i]!='\0')) i++;

          if (i > 0)                               /* If there is an item ... */
            {
            p_s = malloc((i+1)*sizeof(char));        /* Allocate space for it */
            p_s[i--] = '\0';                        /* Remembering terminator */
            while (i >= 0) { p_s[i] = string[i]; i--; }        /* Take a copy */
            if (!link_so_handler(p_s, TRUE, string)) /* ... from library name */
              { fclose(config_handle); return FALSE; }	// More error handling?  @@@
            }
          else
            g_printerr("%s: ERROR - library name missing in line %d of "
                       "configuration file \"%s\"\n", my_name, line,
                                                      config_filename);
          }
          break;

        case 'M':
        case 'm':
          {
          int number;
          number = get_number(string, &string);
          if (number >= 0) memory_size = number;
          }
          break;

        case 'S':
        case 's':
          {
          int key, address, size;
          key     = get_number(string, &string);
          address = get_number(string, &string);  /* Will be -1 if key failed */
          size    = get_number(string, &string);                      /* etc. */
          if (size >= 0) link_shared_mem(key, address, size);  /* Add to list */
          }
          break;

#ifdef VSCREEN_SUPPORT
        case 'V':
        case 'v':
          {
          int number;

          use_vscreen = TRUE;
          number = get_number(string, &string);
          if (number >= 0)                                /* Width specified? */
            {
            vscreen_ww = number;
            vscreen_hh = (number * 3) / 4;                     /* New default */
            number = get_number(string, &string);
            if (number >= 0)                             /* Height specified? */
              {
              vscreen_hh = number;
              number = get_number(string, &string);
              if (number >= 0)                            /* Scale specified? */
                {
                vscreen_ss = number;
                number = get_number(string, &string);
                if (number >= 0)                        /* Colours specified? */
                  {
                  vscreen_cc = number;
                  number = get_number(string, &string);
                  if (number >= 0)                   /* No of LEDs specified? */
                    {
                    vscreen_ll = number;
                    number = get_number(string, &string);
                    if (number >= 0)          /* Shared memory key specified? */
                      {
                      vscreen_kk = number;
                      }
                    }
                  }
                }
              }
            }
          }
          break;
#endif

        default:
//        fprintf(stderr, ":%s:\n", string);
          break;
        }
      line++;
      }

    fclose(config_handle);
    }

  }
return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

char *get_name_string(int *p_argc, char **p_argv[])
{
char *string;
if ((**p_argv)[2] != '\0')                  /* If no space follows option ... */
  string = &((**p_argv)[2]);         /* ... start pointer 2 characters in ... */
else
  {
  if (*p_argc > 2)                               /* ... else, if it exists ...*/
    {
    string = (*p_argv)[1];                   /* ... return next parameter ... */
    (*p_argc)--;                             /* ... removing it from the list */
    (*p_argv)++;
    }
  else
    string = NULL;                       /* In case there is no further input */
  }
return string;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int get_number(char *string, char **str_end)
{
int number;

if (string == NULL) number = -1;                               /* Error value */
else
  {
  if (*string == '\0') number = -1;                            /* Error value */
  else
    {
    while ((*string == ' ') || (*string == '\t')) string++;    /* Skip spaces */
    if ((*string < '0') || (*string > '9')) number = -1;       /* Error value */
    else
      {
      number = 0;
      if ((string[0] == '0') && ((string[1] | 0x20) == 'x'))   /* Hex prefix? */
        {
        unsigned char c;
        string += 2;                                       /* Skip hex prefix */
        while (c = *string, ((c >= '0') && (c <= '9'))
                        || (((c | 0x20) >= 'a') && ((c | 0x20) <= 'f')))
          {
          if (c > '9') c = c - 7;
          number = 16 * number + (c & 0xF);
          string++;
          }
        }
      else                                                         /* Decimal */
        {
        while ((*string >= '0') && (*string <= '9'))
          {
          number = 10 * number + (*string & 0xF);
          string++;
          }
        }
      if ((*string | 0x20) == 'k') { number = number << 10; string++; }  /* K */
      else
      if ((*string | 0x20) == 'm') { number = number << 20; string++; }  /* M */
      }
    }
  }

*str_end = string;                  /* Return in case user wants more numbers */

return number;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* 'strcpy' with a dest. buffer limit and a preprended ' '                    */

void append(char *dst, char *src, unsigned int max)
{
unsigned int i, j;

for (i = 0; dst[i] != '\0'; i++);                /* Assumes terminated string */

if (i < (max - 1)) dst[i++] = ' ';
j = 0;
while ((i < (max - 1)) && (src[j] != '\0')) dst[i++]= src[j++];
dst[i] = '\0';
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

gboolean input_line(FILE *file, char *buffer, unsigned int max)
{
int i;
char c;

if (file != NULL)
  {
  i = 0;
  do
    {
    c = getc(file);
    if (!feof(file) && (i <= max - 1)) buffer[i++] = c;
    }
    while ((c != '\n') && (c != '\r') && !feof(file));

  buffer[i] = '\0';                                              /* Terminate */
  if ((i > 0) && ((buffer[i-1] == '\n') || (buffer[i-1] == '\r')))
    buffer[i-1] = '\0';                                           /* Strip LF */

  if (c == '\r') c = getc(file);             /* Strip off any silly DOS-iness */
  if (c != '\n') ungetc(c, file);         /* Yuk! In case there's -just- a CR */

  return TRUE;
  }
else return FALSE;                                          /* File not valid */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean link_so_handler(char *name, boolean f_name, char *qqq)
{
void *lib;
boolean nothing_found;
boolean okay;
constructor p_constructor;
destructor  p_destructor;
svccall   p_svc_handler;
memrcall  p_mem_r_handler;
memwcall  p_mem_w_handler;
coprocall p_copro_handler;
sigcall   p_signal_handler;
so_handler_entry *p_new, *p_temp;

okay = FALSE;
if ((lib = dlopen (name, RTLD_NOW)) == NULL)
  {
  g_printerr("%s: ERROR - can't open library \"%s\"\n", my_name,
                                                        name);
  if (f_name) free(name);
  }
else
  {
  dlerror();                                /* Reset error condition (string) */
  nothing_found = TRUE;
  p_constructor    = dlsym (lib, "constructor");          /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  p_destructor     = dlsym (lib, "destructor");           /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  p_svc_handler    = dlsym (lib, "svc_handler");          /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  p_mem_r_handler  = dlsym (lib, "mem_r_handler");        /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  p_mem_w_handler  = dlsym (lib, "mem_w_handler");        /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  p_copro_handler  = dlsym (lib, "copro_handler");        /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  p_signal_handler = dlsym (lib, "signal_handler");       /* Look for handler */
  if (dlerror() == NULL) nothing_found = FALSE;              /* Found handler */
  /* String from dlerror() not retained; don't expect all symbols everywhere! */

  if (nothing_found)
    {
    dlclose(lib);                                  /* Give up on this library */
    lib = NULL;
    if (f_name) free(name);
    okay = TRUE;                                 /* Okay because no harm done */
    }
  else                                                /* Add handler to chain */
    {
    if (p_constructor == NULL) okay = TRUE;       /* Nothing to go wrong here */
    else okay = p_constructor(name, qqq);                    /* Call constructor */

    if (okay)                       /* Only link in if survived to this point */
      {
      p_new = malloc(sizeof(so_handler_entry));                  /* New entry */
      p_new->constructor    = p_constructor;
      p_new->destructor     = p_destructor;
      p_new->svc_handler    = p_svc_handler;       /* SVC function pointer    */
      p_new->mem_r_handler  = p_mem_r_handler;     /* Memory function pointer */
      p_new->mem_w_handler  = p_mem_w_handler;     /* Memory function pointer */
      p_new->copro_handler  = p_copro_handler;     /* Coprocessor op. pointer */
      p_new->signal_handler = p_signal_handler;    /* Signal handler pointer  */
      p_new->lib            = lib;                 /* Library handle          */
      p_new->has_string     = f_name;              /* Owns some memory        */
      p_new->name           = name;                /* Library name pointer    */
      p_new->next           = NULL;                /* Link to next entry      */

      if (so_handler_list == NULL) so_handler_list = p_new;          /* First */
      else                                               /* Find end of chain */
        {
        p_temp = so_handler_list;
        while (p_temp->next != NULL) p_temp = p_temp->next;
        p_temp->next = p_new;                               /* ... and append */
        }
      }
    }
  }
return okay;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void link_shared_mem(unsigned int key, unsigned int base, unsigned int size)
{
shared_mem_entry *p_new, *p_temp;
unsigned char *addr;
char id[11];                             /* Large enough for an ASCII address */

sprintf(id, "0x%08X", base);                      /* Generate diagnostic clue */
if (setup_shm(my_name, id, size, key, &addr) >= 0)	// Check detachment of fn from vscreen @@@
						// Including #ifdef @@@
  {
  p_new = malloc(sizeof(shared_mem_entry));                      /* New entry */
  p_new->key   = key;
  p_new->base  = base;
  p_new->top   = base + size;
  p_new->size  = size;
  p_new->addr  = addr;
  p_new->owner = FALSE;
  p_new->next  = NULL;

  if (shared_mem_list == NULL) shared_mem_list = p_new;        /* First entry */
  else                                                   /* Find end of chain */
    {
    p_temp = shared_mem_list;
    while (p_temp->next != NULL) p_temp = p_temp->next;
    p_temp->next = p_new;                                   /* ... and append */
    }
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void free_so_handler_list(void)
{
so_handler_entry *p_current, *p_next;

p_current = so_handler_list;                                 /* Start of list */
so_handler_list = NULL;                                      /* Snip off list */

while (p_current != NULL)
  {
  if (p_current->destructor != NULL) p_current->destructor(p_current->name);

  p_next = p_current->next;               /* Get pointer to subsequent record */
  dlclose(p_current->lib);                     /* Remove reference to library */
  if (p_current->has_string) free(p_current->name);     /* Free name if owner */
  free(p_current);                                     /* Free current record */
  p_current = p_next;                                /* Move to next in chain */
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void free_shared_mem_list(void)
{
shared_mem_entry *p_current, *p_next;
char id[11];                                   /* Large enough for an address */

p_current = shared_mem_list;                                 /* Start of list */
shared_mem_list = NULL;                                      /* Snip off list */

while (p_current != NULL)
  {
  p_next = p_current->next;               /* Get pointer to subsequent record */
  sprintf(id, "0x%08X", p_current->base);         /* Generate diagnostic clue */
  release_shm(my_name, id, p_current->addr);     /* Unlink from shared memory */
  free(p_current);                                     /* Free current record */
  p_current = p_next;                                /* Move to next in chain */
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* release the virtual screen shared memory */

int release_shm(char *my_name, char *addr, char *info)
{
if (shmdt(addr) < 0)
  {
  g_printerr("%s: ERROR - cannot release %s shared memory.\n", my_name, info);
  return(-1);
  }
return(0);
}


/*----------------------------------------------------------------------------*/
/* Synchronise with child processes (signal handler)                          */

void sig_handler (int sigNum)
{
int status;
boolean done;
so_handler_entry *p_handler_entry;

if (verbose)
  switch (sigNum)
    {
    case SIGCONT: fprintf(stderr, "SIGCONT\n"); break;
    case SIGINT:  fprintf(stderr, "SIGINT\n");  break;
    case SIGQUIT: fprintf(stderr, "SIGQUIT\n"); break;
    case SIGTERM: fprintf(stderr, "SIGTERM\n"); break;
    case SIGCHLD: fprintf(stderr, "SIGCHLD\n"); break;
    case SIGSEGV: fprintf(stderr, "SIGSEGV\n"); break;
    default:      fprintf(stderr, "Signal %d\n", sigNum); break;
    }

done = FALSE;               /* Indicator that op. has (not) been serviced yet */
p_handler_entry = so_handler_list;               /* List of handler libraries */
while (!done && (p_handler_entry != NULL) && (status != CLIENT_STATE_RESET))
  {                                                 /* Explore list, in order */
  if ((p_handler_entry->signal_handler != NULL)
  && ((p_handler_entry->signal_handler)(sigNum)))
    done = TRUE;
  else p_handler_entry = p_handler_entry->next;
  }

if (done) return;	// Trapping signals is experimental - should never be 'done' @@@

if (sigNum == SIGCONT)
  {
  /* once the screen has sent the SIGCONT signal we can */
  /* restore the signal handling to its default         */
  sig_hndlr.sa_handler = SIG_DFL;
  sigaction(SIGCONT, &sig_hndlr, NULL);
  }
else
  {
  /* SIGCHLD  a child process was closed */
  /* SIGTERM  parent requests jimulator to terminate */
  /* close all resources, terminate child processes and exit */

#ifdef VSCREEN_SUPPORT
  if (use_vscreen) {
    /* release shared memory buffer */
    release_shm(my_name, "", shm_ADDR);
    /* just in case the i2c emulator closed: terminate vscreen */
    kill (screen_PID, SIGTERM);
    waitpid(screen_PID, &status, 0);
    }
#endif

  clean_up_1(my_parent);              /* Ask parent program to terminate.     */
  clean_up_2();                       /* Free dynamically allocated memory.   */
  exit(0);                            /* Returning control proves dangerous!  */
  }                            /* May have destroyed structure whilst in use. */

return;
}

/*                             end of lap's functions                         */
/*----------------------------------------------------------------------------*/

/*                                end of jimulator.c                          */
/*============================================================================*/




// SCR_SZ has become vscreen_size
// SCR_NC has become pixel_bytes
// IMAGE_* should be SCR_*



// Extracted, redundant SWI handling stuff: for reference until deletion
//
//  #ifdef VSCREEN_SUPPORT           /* Map out a set of SVCs for VSCREEN support */
//    if ((use_vscreen != TRUE) || ((op_code & 0X00FFFFFF) < 0x100)
//                              || ((op_code & 0X00FFFFFF) > 0x10F))
//  #endif
//  
//    switch (op_code & 0X00FFFFFF)              /* If no VSCREEN or 'normal' SVC */
//      {
//      case 0:                              /* Output character R0 (to terminal) */
//        put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
//                                      /* Bodge PC so that stall looks `correct' */
//        svc_char_out(get_reg(0, REG_CURRENT) & 0XFF);
//  
//        if (status != CLIENT_STATE_RESET)
//          put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);     /* Correct PC */
//        break;
//  
//      case 1:                             /* Input character R0 (from terminal) */
//        {
//        char c;
//        put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
//                                      /* Bodge PC so that stall looks `correct' */
//        while ((!get_buffer(&terminal0_Rx, &c)) && (status != CLIENT_STATE_RESET))
//          comm(SVC_poll);
//  
//        if (status != CLIENT_STATE_RESET)
//          {
//          put_reg(0, c & 0XFF, REG_CURRENT);
//          put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);     /* Correct PC */
//          }
//        }
//        break;
//  
//      case 2:                                                           /* Halt */
//        status = CLIENT_STATE_BYPROG;
//        break;
//  
//      case 3:                                 /* Print string @R0 (to terminal) */
//        {
//        unsigned int str_ptr;
//        char c;
//  
//        put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
//                                      /* Bodge PC so that stall looks `correct' */
//  
//        str_ptr = get_reg(0, REG_CURRENT);
//        while (((c = read_mem(str_ptr, 1, FALSE, FALSE, MEM_SYSTEM)) != '\0')
//             && (status != CLIENT_STATE_RESET))
//          {
//          svc_char_out(c);                                  /* Returns if reset */
//          str_ptr++;
//          }
//  
//        if (status != CLIENT_STATE_RESET)
//          put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);     /* Correct PC */
//        }
//        break;
//  
//      case 4:                                               /* Decimal print R0 */
//        {
//        int svc_dec_print(unsigned int number)    /* Recursive zero suppression */
//          {
//          int okay;
//  
//          okay = TRUE;
//          if (number > 0)              /* else nothing - suppress leading zeros */
//            {
//            okay = svc_dec_print(number / 10);                /* Recursive call */
//            if (okay) svc_char_out((number % 10) | '0');    /* Returns if reset */
//            }
//          return okay;
//          }
//  
//        unsigned int number;
//        int okay;
//  
//        put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
//                                      /* Bodge PC so that stall looks `correct' */
//  
//        number = get_reg(0, REG_CURRENT);
//        if (number == 0) okay = svc_char_out('0');  /* Don't suppress last zero */
//        else             okay = svc_dec_print(number);      /* Returns if reset */
//  
//        if (status != CLIENT_STATE_RESET)
//          put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);     /* Correct PC */
//        }
//        break;
//  
//      default:
//        if (print_out)
//          fprintf(stderr, "Untrapped SVC call %06X\n", op_code & 0X00FFFFFF);
//        spsr[sup_mode] = cpsr;
//        cpsr = (cpsr & ~MODE_MASK) | sup_mode;
//        cpsr = cpsr & ~TF_MASK;                           /* Always in ARM mode */
//        put_reg (14, get_reg (15, REG_CURRENT) - 4, REG_CURRENT);
//        put_reg (15, 8, REG_CURRENT);
//        break;
//      }
//  
//  #ifdef VSCREEN_SUPPORT
//    else                                        /* Map in VSCREEN support calls */
//    switch (op_code & 0X00FFFFFF)
//      {
//      case 0x100:                     /* Copy frame in memory to virtual screen */
//        {                                                /* Frame address in R0 */
//        unsigned int faddr;
//        ushort       fpix;
//        int          ip;
//        int          hr;
//        int          wr;
//        int          line;
//        int          col;
//  
//        faddr = get_reg(0, REG_CURRENT);
//        ip = 0;
//        for (line = 0; line < IMAGE_HEIGHT; line++)
//          {
//          for (hr = 0; hr < HT_RATIO; hr++)
//            {
//            for (col = 0; col < IMAGE_WIDTH; col++)
//              {
//              fpix = read_mem(faddr, 2, FALSE, FALSE, MEM_DATA);
//              for (wr = 0; wr < WD_RATIO; wr++)
//                {                               /* Copy pixel to virtual screen */
//                shm_ADDR[ip++] = ((fpix >> 10) & 0x1F) << 3;           /* Red   */
//                shm_ADDR[ip++] = ((fpix >>  5) & 0x1F) << 3;           /* Green */
//                shm_ADDR[ip++] = ( fpix        & 0x1F) << 3;           /* Blue  */
//                ip += SCR_NC-IMAGE_NC;
//                }
//              faddr += 2;                                /* Point to next pixel */
//              }
//            faddr -= 2*IMAGE_WIDTH;                     /* Copy same line again */
//            /* point to next line in virtual screen */
//            ip += (SCR_WIDTH-WD_RATIO*IMAGE_WIDTH)*SCR_NC;
//            }
//          faddr += 2*IMAGE_WIDTH;                         /* Point to next line */
//          }
//        shm_ADDR[SCR_SZ] |= 0x01;                 /* Trigger outgoing handshake */
//        }
//        break;
//  
//      case 0x101:                     /* Copy virtual screen to frame in memory */
//        {                                                /* Frame address in R0 */
//        unsigned int faddr;
//        ushort       fpix;
//        int          line;
//        int          col;
//        int          ip;
//  
//        /* get new image into buffer */
//        faddr = get_reg(0, REG_CURRENT);
//        ip = 0;
//        for (line = 0; line < 240; line++)
//          {
//          for (col = 0; col < 320; col++)
//            {
//            fpix  = ((ushort) shm_ADDR[ip++] << 7) & 0x7C00;           /* Red   */
//            fpix |= ((ushort) shm_ADDR[ip++] << 2) & 0x03E0;           /* Green */
//            fpix |= ((ushort) shm_ADDR[ip++] >> 3) & 0x001F;           /* Blue  */
//            write_mem(faddr, fpix, 2, FALSE, MEM_DATA);
//            faddr += 2; /* point to next pixel */
//            /* skip pixels in vs according to ratio */
//            ip += WD_RATIO*SCR_NC-IMAGE_NC;
//            }
//          /* skip lines in vs according to ratio */
//          ip += (HT_RATIO*SCR_WIDTH-WD_RATIO*IMAGE_WIDTH)*SCR_NC;
//          }
//        shm_ADDR[SCR_SZ] &= 0x7F;                 /*  Clear incoming handshake  */
//        }
//        break;
//  
//      case 0x102:                            /* Request a new image from screen */
//        {                                                  /* Request new image */
//        shm_ADDR[SCR_SZ] |= 0x40;
//        /* Bodge PC so that stall looks `correct' */
//        put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
//        /* wait for new image in buffer */
//        while (((shm_ADDR[SCR_SZ] & 0x80) == 0) && (status != CLIENT_STATE_RESET))
//          comm(SVC_poll);                      /* Retain monitor communications */
//        if (status != CLIENT_STATE_RESET)
//          put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);     /* Correct PC */
//        }
//        break;
//  
//      case 0x104:                               /* Write to virtual screen LEDs */
//        {                                                         /* Data in R0 */
//        /* write new data and trigger handshake */
//        shm_ADDR[LED_SHM] = 0x80 | (get_reg(0, REG_CURRENT) & 0x3F);
//        }
//        break;
//  
//      case 0x105:                     /* Copy frame in memory to virtual screen */
//        {     /* R0 frame address                 R1, R2 x, y size of local array
//                 R3, R4 Screen start coordinates, R5 screen step size           */
//        unsigned int faddr;
//        unsigned int image_w;
//        unsigned int image_h;
//        unsigned int screen_x;
//        unsigned int screen_y;
//        unsigned int step_x;
//        unsigned int step_y;
//        ushort       fpix;
//        int          ip;
//        int          hr;
//        int          wr;
//        int          line;
//        int          col;
//  
//        faddr    = get_reg(0, REG_CURRENT);
//        image_w  = get_reg(1, REG_CURRENT);
//        image_h  = get_reg(2, REG_CURRENT);
//        screen_x = get_reg(3, REG_CURRENT) * WD_RATIO;
//        screen_y = get_reg(4, REG_CURRENT) * HT_RATIO;
//        step_x   = get_reg(5, REG_CURRENT) * WD_RATIO;
//        step_y   = get_reg(5, REG_CURRENT) * HT_RATIO;
//  
//        for (line = 0; line < image_h; line++)
//          {
//          for (col = 0; col < image_w; col++)
//            {
//            fpix = read_mem(faddr, 2, FALSE, FALSE, MEM_DATA);
//  /*
//            if (((screen_y + step_y * line) < SCR_HEIGHT)
//              && ((screen_x + step_x * col) < SCR_WIDTH))
//  */
//            {
//            ip = ((screen_y + step_y * line) * SCR_WIDTH
//                + (screen_x + step_x * col)) * SCR_NC;
//  
//            for (hr = 0; hr < step_y; hr++)
//              {
//              if ((screen_y + step_y * line + hr) < SCR_HEIGHT)
//                for (wr = 0; wr < step_x; wr++)
//                  {
//                  if ((screen_x + step_x * col + wr) < SCR_WIDTH)
//                    {                           /* Copy pixel to virtual screen */
//                    shm_ADDR[ip++] = ((fpix >> 10) & 0x1F) << 3;       /* Red   */
//                    shm_ADDR[ip++] = ((fpix >>  5) & 0x1F) << 3;       /* Green */
//                    shm_ADDR[ip++] = ( fpix        & 0x1F) << 3;       /* Blue  */
//  //                ip += SCR_NC-IMAGE_NC;
//                    }
//                  ip += SCR_NC;
//                  }
//                /* Point to next line in virtual screen */
//                ip += (SCR_WIDTH - step_x) * SCR_NC;
//                }
//              }
//            faddr += 2;                                  /* Point to next pixel */
//            }
//          }
//        shm_ADDR[SCR_SZ] |= 0x01;                 /* Trigger outgoing handshake */
//        }
//        break;
//  
//      case 0x106:                     /* Copy virtual screen to frame in memory */
//        {     /* R0 frame address                 R1, R2 x, y size of local array
//                 R3, R4 Screen start coordinates, R5 screen step size           */
//        unsigned int faddr;
//        unsigned int image_w;
//        unsigned int image_h;
//        unsigned int screen_x;
//        unsigned int screen_y;
//        unsigned int step_x;
//        unsigned int step_y;
//        ushort       fpix;
//        int          line;
//        int          col;
//        int          ip;
//  
//        /* Get new image into buffer */
//        faddr    = get_reg(0, REG_CURRENT);
//        image_w  = get_reg(1, REG_CURRENT);
//        image_h  = get_reg(2, REG_CURRENT);
//        screen_x = get_reg(3, REG_CURRENT) * WD_RATIO;
//        screen_y = get_reg(4, REG_CURRENT) * HT_RATIO;
//        step_x   = get_reg(5, REG_CURRENT) * WD_RATIO;
//        step_y   = get_reg(5, REG_CURRENT) * HT_RATIO;
//        for (line = 0; line < image_h; line++)
//          {
//          for (col = 0; col < image_w; col++)
//            {
//            if (((screen_y + step_y * line) < SCR_HEIGHT)
//              && ((screen_x + step_x * col) < SCR_WIDTH))
//              {
//              ip = ((screen_y + step_y * line) * SCR_WIDTH
//                  + (screen_x + step_x * col)) * SCR_NC;
//              fpix  = ((ushort) shm_ADDR[ip++] << 7) & 0x7C00;         /* Red   */
//              fpix |= ((ushort) shm_ADDR[ip++] << 2) & 0x03E0;         /* Green */
//              fpix |= ((ushort) shm_ADDR[ip++] >> 3) & 0x001F;         /* Blue  */
//              }
//            else
//              fpix  = 0x0000;                                          /* Black */
//            write_mem(faddr, fpix, 2, FALSE, MEM_DATA);
//            faddr += 2;                                  /* Point to next pixel */
//            }
//          }
//        shm_ADDR[SCR_SZ] &= 0x7F;                   /* Clear incoming handshake */
//        }
//        break;
//  
//      default:
//        if (print_out)
//          fprintf(stderr, "Untrapped SVC call %06X\n", op_code & 0X00FFFFFF);
//        spsr[sup_mode] = cpsr;
//        cpsr = (cpsr & ~MODE_MASK) | sup_mode;
//        cpsr = cpsr & ~TF_MASK;                           /* Always in ARM mode */
//        put_reg (14, get_reg (15, REG_CURRENT) - 4, REG_CURRENT);
//        put_reg (15, 8, REG_CURRENT);
//        break;
//      }
//  #endif
