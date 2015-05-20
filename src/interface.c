/******************************************************************************/
/*                Name:           interface.c                                 */
/*                Version:        1.5.0                                       */
/*                Date:           10/8/2007                                   */
/*                The interface between the GUI and the client                */
/*                                                                            */
/*============================================================================*/

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

//#include "definitions.h"
#include "global.h"

#include "interface.h"
#include "callbacks.h"
#include "breakcalls.h"
#include "serial.h"
// #include "view.h"
#include "dotparse.h"
#include "chump.h"
#include "config.h"

#define KMD_DIR  "KMD_HOME"         /* Which environment variable to look for */
#define COMPILE_SCRIPT "$"KMD_DIR"/kmd_compile"/* Default compile script name */

char *dotkomodo =                // This is very dirty
#include "dotkomodo.string"
 ;

#ifndef SETUP_DIR
#define SETUP_DIR g_get_home_dir()
#endif

#ifndef EMULATOR_PROG
#define EMULATOR_PROG  "jimulator"
#endif

#ifndef USE_INTERNAL
#define USE_INTERNAL 0
#endif

#define COMMANDLINE_CONTINUE   0    /* Return values from command line parser */
#define COMMANDLINE_TERMINATE  1    /* Probably should be enumerated type @@@ */
#define COMMANDLINE_ERROR      2


/* Non-exported prototypes                                                    */
void init_global_vars(void);
void init_locks(void);
void get_cpu_info_from_dotfile(target_system*);
                /* set all the CPU variables below from the dot file ???  @@@ */
target_system* board_wotru(void);                         /* Check board info */
void backend_dispose(target_system*);
void backend_display(target_system*);

int commandline(int argc, char *argv[]);   /* Process command line parameters */
void setup(void);                                          /* Set up function */

void dead_child(int);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Non-exported variables                                                     */

GdkFont *fixed_font;

//int board_regbanks_gran;                 /* granularity of the register banks */	// #!#!#

int board_emulation_communication_from[2];
int board_emulation_communication_to[2];

uchar board_runflags;           /* "run" command byte - modified as necessary */

char *own_path;                /* The default directory to look for things in */
unsigned int own_path_length; /* True length default directory path inc. '\0' */
char *emulator_prog;                      /* The name of the emulator program */

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called from wotRu? function and                                            */
/* uses backend->cpu_ref from the board to initialise some values             */
/*                                                                            */

void get_cpu_info_from_dotfile(target_system *backend)
{
GList *toplist = Scantopnode->body.list;
GList *cpulist;                /* scans through the init file to find the CPU */
GList *regbankslist;                          /* a list of all register banks */
GList *regbanklist;       /* a list of the registers of a given register bank */
GList *asmlist;               /* a list of the possible assemblies e.g. Thumb */
int count = 0;
char *name;
int flag;
int special;
int offset;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  int detect_cpu(target_system *backend)
  {
  int okay;

  do
    {  /* This will loop until all entries of CPU were scanned or match found */
    cpulist = ScanfindSymbolList(toplist, "cpu", NULL);
             /* These two will allow cpu list to step through all CPU entries */
    toplist = ScanfindSymbolListNext(toplist, "cpu", NULL);
                                                /* Retains the next CPU entry */
    okay = (NULL != cpulist);               /* Note if no CPU entry was found */
    }
    while (okay
         && (ScangetNumberAdvance(&cpulist) != backend->cpu_ref
          || ScangetNumberAdvance(&cpulist) <  backend->cpu_subref
          || ScangetNumberAdvance(&cpulist) >  backend->cpu_subref));
                   /* Searching for the CPU info using the dotparse functions */

  if (okay)
    {
    backend->cpu_name = ScangetStringAdvance(&cpulist);
                                           /* Getting CPU name from the entry */
    if (VERBOSE) g_print("Detected        : %s\n", backend->cpu_name);
                                              /* Can be redirected to console */
    }
  else
    if (VERBOSE)
      g_print("Could not find a match for the CPU within .komodo file.\n");

  return okay;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void get_info_from_cpu(target_system *backend)
  {
  backend->memory_ptr_width = ScanfindSymbolNumber(cpulist,"memory-ptr-width",4);
  backend->wordalign        = ScanfindSymbolNumber(cpulist,"wordalign",0);
  view_window_display_list  = ScanfindSymbolString(cpulist,"window-list","M");
                                                      /* Get the display list */
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void get_registers_info(target_system *backend)
  {
//  int board_main_regbank;

  regbankslist = ScanfindSymbolList(cpulist, "regbanks", NULL);
//  board_main_regbank = ScangetNumberAdvance(&regbankslist);
  backend->regbanks_gran =
                  ScanfindSymbolNumber(regbankslist, "regbank-granularity", 1);

  backend->num_regbanks = 1;            /* Initialise count of register banks */
  regbanklist = regbankslist;
  while (NULL!=(regbanklist=ScanfindSymbolListNext(regbanklist,"regbank",NULL)))
    backend->num_regbanks++;           /* Accumulate number of register banks */

  if (VERBOSE) g_print("Regbanks No.    : %d\n", backend->num_regbanks);
                                                            /* Register banks */

  backend->reg_banks = g_new(reg_bank, backend->num_regbanks);
                                     /* Allocate space for the register banks */

  for (count = 0; count < backend->num_regbanks; count++)
    {                                        /* For all listed register banks */
    regbanklist  = ScanfindSymbolList(regbankslist, "regbank", NULL);
    regbankslist = ScanfindSymbolListNext(regbankslist, "regbank", NULL);
                             /* see cpulist and toplist above for parallelism */
    backend->reg_banks[count].name   = ScangetStringAdvance(&regbanklist);
                                                        /* Register bank name */
    backend->reg_banks[count].number = ScangetNumberAdvance(&regbanklist);
                                                        /* Register bank size */
    backend->reg_banks[count].width  = ScangetNumberAdvance(&regbanklist);
                                              /* Register bank width in bytes */
    backend->reg_banks[count].offset = ScangetNumberAdvance(&regbanklist);
                                                      /* Register bank offset */
    backend->reg_banks[count].names  =                      /* Register names */
        ScanStrlist2Listarray(ScanfindSymbolList(regbanklist, "names", NULL),
                             backend->reg_banks[count].number);
    backend->reg_banks[count].values =
        g_new(uchar,
            MAX(1,backend->reg_banks[count].width)*backend->reg_banks[count].number);
                   /* allocate the space necessary to store all the registers */
                          /* given the width in bytes and number of registers */
    backend->reg_banks[count].pointer=ScanfindSymbol(regbanklist,"pointers",1,0);

    backend->reg_banks[count].valid  = FALSE;      /* Values not yet obtained */

    if (backend->reg_banks[count].pointer)
      {
      for (special = 0; special < SPECIAL_REGISTER_COUNT; special++)
                                 /* perform a check for all special registers */
        if (0 <= (offset = ScanfindSymbolNumber(regbanklist,
                                         special_registers[special].name, -1)))
                                /* If the special register exists in the list */
          {
          special_registers[special].valid  = &(backend->reg_banks[count].valid);
                                                    /* Point at validity flag */
          special_registers[special].active = TRUE;
                            /* Activate the special register in the structure */
          special_registers[special].value = backend->reg_banks[count].values
                                  + offset * backend->reg_banks[count].width;
                                 /* put value of special registers separately */
          gdk_colormap_alloc_color(gdk_colormap_get_system(),
                                   &special_registers[special].colour, 0, 1);
                                    /* allocate colours for special registers */
          if (special_registers[special].pixmap_data != NULL)
                                          /* assign a pixmap where applicable */
          special_registers[special].pixmap =
            gdk_pixmap_colormap_create_from_xpm_d(NULL,
                                        gdk_colormap_get_system(),
                                        &special_registers[special].bitmap,
                                        NULL,
                                        special_registers[special].pixmap_data);
          }
      }
    }
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  char *get_compile_script(char *default_name)
  {
  char *name;

  name = ScanfindSymbolString(cpulist, "compile-script", "");

  if ((*name != '\0') && (!use_internal || (getenv(KMD_DIR) != NULL)))
    return name;  /* Name exists and $<KMD_DIR> is set if using internal file */
  else
    return default_name;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  DefinitionStack* get_ISA_description(DefinitionStack *tables)
  {                /* Searching for the ISA info using the dotparse functions */
  asmlist = ScanfindSymbolList(cpulist, "isa", NULL); /* find ISA description */
  while (asmlist != NULL)        /* while more ISA descriptions are available */
    {
    name = ScangetStringAdvance(&asmlist);                 /* advance to body */
    toplist = Scantopnode->body.list;
    do                                            /* repeat until match found */
      {
      cpulist = ScanfindSymbolList(    toplist, "isa", NULL);
      toplist = ScanfindSymbolListNext(toplist, "isa", NULL);
      if (cpulist == NULL)                     /* reached the end of the list */
        {
        if (VERBOSE) g_print("Could not find %s ISA description\n", name);
        exit(1);         //   @@@
        }
      }
      while (g_strcasecmp(ScangetStringAdvance(&cpulist), name) != 0);

    if (VERBOSE) g_print("ISA definition  : %s\n", name);
                                            /* declare the match for ISA def. */
    tables = asm_define(cpulist, tables, name);          /* Record that match */
    }
  return tables;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  int get_features_info(target_system *backend)
  {           /* Searching for the features info using the dotparse functions */
  boolean feature_IDed;            /* Note if at least one feature identified */

  feature_IDed = FALSE;            /* Note if at least one feature identified */

  for (count = 0; count < backend->feature_count; count++)
    {                                /* For all features that have been found */
    toplist = Scantopnode->body.list;
    do
      {
      cpulist = ScanfindSymbolList(    toplist, "feature", NULL);
      toplist = ScanfindSymbolListNext(toplist, "feature", NULL);
                                              /* progress in features listing */
      if (cpulist != NULL)                     /* if end of least not reached */
        {
        int ref_no     = ScangetNumberAdvance(&cpulist); /* get reference no. */
        int sub_ref_no = ScangetNumberAdvance(&cpulist); /* and sub-reference */

        if ((ref_no == backend->pArray_F[count].reference_number)
    && ((sub_ref_no == backend->pArray_F[count].sub_reference_number)
      || sub_ref_no == 0xFFFF))      /* if match found where FFFF means 'all' */
          break;
        }

      }
      while (cpulist != NULL);              /* while still more to go through */

    if (cpulist != 0) name = ScangetStringAdvance(&cpulist);
                                                   /* get name if found match */
    else name = "unknown";                       /* name it unknown otherwise */

    backend->pArray_F[count].type = UNKNOWN;               /* Default setting */

    if (!g_strcasecmp("xilinx-fpga", name))
      {                          /* if name=xilinx-fpga set some trivial data */
      feature_IDed = TRUE;
      flag = TRUE;                 /* "flag" says if a feature was identified */
      backend->pArray_F[count].type = XILINX_FPGA;
      backend->pArray_F[count].data.xilinx_fpga.filestring
                      = ScanfindSymbolString(cpulist, "XFPGA-filestring", "");
      }
    else
      if (!g_strcasecmp("terminal", name))
        {                          /* if name==terminal set some trivial data */
        feature_IDed = TRUE;
        flag = TRUE;
        backend->pArray_F[count].type = TERMINAL;
        }
      else
        flag = FALSE;

    /* flag is a Boolean saying if a match was found for the name */
    if (VERBOSE && !flag)
      g_print("Could not find feature description reference no:%d "
              "subreference no:%d\n",
               backend->pArray_F[count].reference_number,
               backend->pArray_F[count].sub_reference_number); /* report user */

    if (cpulist != 0)
      backend->pArray_F[count].name = ScanfindSymbolString(cpulist, "name",
                                "Add a name of the device in the .komodo file");
    else
      backend->pArray_F[count].name = "unknown";  /* setting the feature name */
    }                                                              /* end for */

  return feature_IDed;      /* Return TRUE if at least one feature identified */
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  char *environmental_expansion(char *pIn)
  {
  char buffer[200];		// Beastly hack @@@
  char temp[200];
  char *pOut, *pTemp;

  pOut = &buffer[0];

  while (*pIn != '\0')                                   /* Scan input string */
    {
    if (*pIn != '$') *pOut++ = *pIn++;   /* If not shell environment variable */
    else
      {
      pIn++;                                                 /* Step over '$' */
      pTemp = &temp[0];
      while ((*pIn != '/') && (*pIn != '\0'))  /* Extract string to delimiter */
        *pTemp++ = *pIn++;
      *pTemp = '\0';                            /* Terminate temporary string */

      pTemp = getenv(&temp[0]);          /* Expand shell environment variable */
      if (pTemp!=NULL) while (*pTemp!='\0') *pOut++ = *pTemp++;  /* => output */
      else if (*pIn == '/') pIn++;             /* Skip '/' if `getenv' failed */
     }
    }

  *pOut = '\0';                                     /* Terminate final string */

  return g_strdup(buffer);           /* Return copy in newly allocated buffer */
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("get_cpu_info_from_dotfile\n");

if (!detect_cpu(backend)) exit(1);     // @@@ structuring (only) begun ...
compile_script = get_compile_script(COMPILE_SCRIPT);
                                  /* Find if a particular script is specified */
				// This SHOULD be on a per-architecture basis @@@
compile_script = environmental_expansion(compile_script);
				// Old "compile_script" MAY need disposal (??)  @@@
get_info_from_cpu(backend);
get_registers_info(backend);
backend->asm_tables = get_ISA_description(NULL);

if (!get_features_info(backend)) backend->feature_count = 0;
     /* If no feature identified pretend that there are no features received. */
     /* This changes the way the feature window is drawn.                     */

return;
}                                         /* End of get_cpu_info_from_dotfile */

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* See if/what backend is connected                                           */
/* Returns: board version if read successfully, else -1                       */

int get_client_version(void)
{
unsigned char reply[4];                                       /* Reply buffer */
int version;

if ((board_sendchar(BR_PING) == 1) && (board_getchararray(4, reply) == 4)
           && (reply[0] == 'O') && (reply[1] == 'K')) /* Ensure reply is "OK" */

  version = (reply[2] - '0') * 10 + reply[3] - '0'; /* Extract decimal number */
else
  version = -1;                                         /* Some form of error */

return version;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Checks if the board is still available and is the same version as before   */
/* Returns: TRUE if contact established                                       */
/* board_version is -2 if this function is called for the first time.         */
/*                                                                            */

#define PING_NUMBER_NOPS 100

boolean board_mini_ping(void)
{
unsigned char reply[4];
int version;

if (TRACE > 5) g_print("board_mini_ping\n");
if (VERBOSE) g_print("Mini Ping\n");

version = get_client_version();

if (version < 0)                                     /* Tested for and failed */
  {                            /* Retry after flushing communications buffers */
  int count;
  unsigned char reply;

//  while (gtk_events_pending()) gtk_main_iteration();
                                          /* update display and do user stuff */ 
  for (count = 0; count < PING_NUMBER_NOPS; count++);
    board_sendchar(BR_NOP);    /* Keep sending NOPs to clear out the FIFO out */

  while (board_getchar(&reply))                          /* Flush out FIFO in */
    if (VERBOSE)
      g_print("Throwing away unexpected data in input buffer: %x\n", reply);

  version = get_client_version();                               /* Retry ping */
  }

if (version >= 0)
  {
  boolean match;

  if (board_version == -2) board_version = version;/* 1st time record version */
  match = (version == board_version);

  if (!match)
    {        /* Check board version and return "TRUE" if version is as before */
    if (board_version == -1)
      {   /* Recovering from unavailability really need new features etc. @@@ */
g_print("Board version recovered to: %d\n", version);
      board_version = version;
      }
    else
      {
      target_system *new_fella;

      if (VERBOSE) g_print("Different board reconnected!\n");
g_print("Board changed from: %d to %d\n", board_version, version);
//  Could take action to try to reestablish correct operation.
      new_fella = board_wotru();		// #!#!#

      if (new_fella != NULL)                  /* else `match' must stay false */
        {
        backend_display(new_fella);				// Debug print  @@@
        match = (new_fella->cpu_ref == board->cpu_ref);

        backend_dispose(new_fella);
        }
      }
    }

  return match;
  }
else
  return FALSE;                             /* Couldn't (re)establish contact */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* See if a valid client is responding; if not try harder.                    */
/*                                                                            */

boolean board_micro_ping(void)
{
boolean okay;

if (TRACE > 15) g_print("board_micro_ping\n");

okay = (get_client_version() >= 0);		// #!# Slight change #!#

if (!okay) okay = board_mini_ping();        /* If not visible, try to recover */

return okay;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Find the current execution state of the client                             */
/* Returns: client response if stopped, running, error, else "BROKEN"         */

int board_enq(unsigned int *steps_left)
{                              // Does more than you think -- needs subdivision @@@
uchar client_status;
unsigned steps_total;
char *string;

if (TRACE > 5) g_print("board_enq\n");

board_sendchar(BR_WOT_U_DO);                           /* Send status enquiry */
if (board_getchar(&client_status) != 1)    /* Not got one char - as expected? */
  {
  board_micro_ping();                          /* On error try to resync once */
  board_sendchar(BR_WOT_U_DO);                               /* Enquire again */
    //  Dubious value @@@
    // also "CLIENT_STATE_BROKEN" could be sent from back end ... not defined!
  if (board_getchar(&client_status) != 1) client_status = CLIENT_STATE_BROKEN;
  }

if ((client_status == CLIENT_STATE_BROKEN)
 || (4 != board_getbN(steps_left, 4))                      /* Steps remaining */
 || (4 != board_getbN(&steps_total, 4)))                 /* Steps since reset */
  client_status = CLIENT_STATE_BROKEN;
else
  {
  // enquires the board for the current step count and displays that within
  // the GTK interface(sets its value) by calling display_board_state
  if (*steps_left != 0) string=g_strdup_printf("  Steps left: %u",*steps_left);
  else                  string=g_strdup_printf("  Total steps: %u",steps_total);
  display_status(string);                     /* Update step count on display */
  g_free(string);

  display_board_state(client_status);
  switch (client_status & CLIENT_STATE_CLASS_MASK)       /* Class of activity */
    {
    case CLIENT_STATE_CLASS_RESET:                            /* Client reset */
      callback_global_refresh();                               /* refresh all */
      breakpoint_refresh(0);           /* ONLY DO THIS IF UNEXPECTED (??) @@@ */
      breakpoint_refresh(1);
      client_status = CLIENT_STATE_BROKEN;
      break;

    case CLIENT_STATE_CLASS_STOPPED:                        /* Client stopped */
    case CLIENT_STATE_CLASS_RUNNING:                        /* Client running */
    case CLIENT_STATE_CLASS_ERROR:                            /* Client error */
      break;                         /* Return state as recovered from client */
    }
  }

return client_status;
}

/* end board_enq */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Gets the board information at the start                                    */
/* Returns: pointer to target_system block or NULL if failed                  */

target_system* board_wotru(void)
{
int lengthofmessage;
uchar *message;
int i;                                       /* Index to the message returned */
int count;
int okay;
target_system *backend;

/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  void get_board_info_from_message(target_system *backend)
  {
  backend->cpu_ref     = message[i++];
  backend->cpu_subref  = message[i++];
  backend->cpu_subref |= message[i++] << 8;
  return;
  }

/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  void get_features_from_message(target_system *backend)
  {
  backend->feature_count = message[i++];                /* Number of features */
  if (VERBOSE) g_print("No. Of Features : %d\n", backend->feature_count);

  backend->pArray_F = g_new(feature, backend->feature_count);
                                  /* Create a space for the array of features */

  for (count = 0; count < backend->feature_count; count++)
    {
    backend->pArray_F[count].dev_number = count;
                                 /* The number used to reference this feature */
    backend->pArray_F[count].reference_number     = message[i++];
                                    /* Record ref. number and sub ref. number */
    backend->pArray_F[count].sub_reference_number  = message[i++];
    backend->pArray_F[count].sub_reference_number |= message[i++] << 8;
    }
  return;
  }

/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  void get_memory_info_from_message(target_system *backend)
  {
  backend->memory_count = message[i++];            /* Note number of segments */
  backend->pArray_M = g_new(memory_segment, backend->memory_count);
                                        /* Allocate memory for recording them */

  for (count = 0; count < backend->memory_count; count++)
    {                                     /* Step through all memory segments */
    backend->pArray_M[count].start =       /* Record the start of the segment */
          g_memdup(&message[i], backend->memory_ptr_width * sizeof(uchar));
    i += backend->memory_ptr_width;               /* Step through the message */
    backend->pArray_M[count].length =           /*  and now record the length */
          g_memdup(&message[i], backend->memory_ptr_width * sizeof(uchar));
    i += backend->memory_ptr_width;
                     /* Collection data regarding the memory from the message */
    }
  return;
  }

/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

if (TRACE > 5) g_print("board_wotru\n");

i = 0;                                       /* Index to the message returned */
backend = NULL;

okay = (board_version >= 0) && (1 == board_sendchar(BR_WOT_R_U))
                            && (2 == board_getbN(&lengthofmessage, 2));
             /* Check client believed present and responding to interrogation */

if (!okay) board_version = -1;                 /* Set board to be unavailable */
else
  {
  message = g_new(uchar, lengthofmessage);/* Allocate temp buffer for message */
  okay = (lengthofmessage == board_getchararray(lengthofmessage, message));
                                           /* Message must be expected length */

  if (okay)
    {
    backend = g_new(target_system, 1);       /* Create new backend definition */
    backend->cpu_ref       = 0;
    backend->feature_count = 0;
    backend->memory_count  = 0;
    backend->pArray_F = NULL;
    backend->pArray_M = NULL;

    get_board_info_from_message(backend);        /* Get information about CPU */
    get_features_from_message(backend);     /* Get information about features */
    get_cpu_info_from_dotfile(backend);
    get_memory_info_from_message(backend);   /* Record info. about memory map */
    }                        // Display memory available as status?? @@@
  else backend = NULL;                                       /* If not `okay' */

  g_free(message);                                       /* Free temp. buffer */
  }

return backend;                           /* Return record or NULL if failure */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Free the space occupied by a target_system record                          */
/* Hopefully thorough but some questions over shared stings, etc. @@@@        */

void backend_dispose(target_system *backend)
{
int i;

if (backend->pArray_F != NULL) g_free(backend->pArray_F);
if (backend->pArray_M != NULL) g_free(backend->pArray_M);
for (i = 0; i < backend->num_regbanks; i++) g_free(backend->reg_banks[i].values);
g_free(backend->reg_banks);
g_free(backend);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Just for debugging purposes  (not complete, either)                        */

void backend_display(target_system *backend)
{
int i, j;

g_print("CPU: %02X - %04X (%s)\n", backend->cpu_ref, backend->cpu_subref, backend->cpu_name); 
for (i = 0; i < backend->memory_count; i++)
  {
  g_print("memory base: ");
  for (j = backend->memory_ptr_width - 1; j >=0 ; j--)
    g_print("%02X", backend->pArray_M[i].start[j]);
  g_print("  length ");
  for (j = backend->memory_ptr_width - 1; j >=0 ; j--)
    g_print("%02X", backend->pArray_M[i].length[j]);
  g_print("\n");
  }
for (i = 0; i < backend->feature_count; i++)
  {
  g_print("feature:     %02X - %04X (%s) #%d\n", backend->pArray_F[i].reference_number,
                                                 backend->pArray_F[i].sub_reference_number,
                                                 backend->pArray_F[i].name,
                                                 backend->pArray_F[i].dev_number);
  }

return;
}

/******************************************************************************/

int read_registers(unsigned int address, unsigned char *data,
                   unsigned int width,   unsigned int count)
{
return (1 == board_sendchar(BR_GET_REG))
                          /* if unable to send a BR_GET_REG byte to the board */
    && (4 == board_sendbN(address, 4))
                                /* send the address of the requested register */
    && (2 == board_sendbN(count, 2))
            /* send 1 - number of register values following and return status */
    && (count*width == board_getchararray(count*width, data));
                         /*  and the reply from the board is of expected size */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int write_register(unsigned int address, unsigned char *data, unsigned int width)
{
return (1 == board_sendchar(BR_SET_REG))
                            /* if able to send a BR_SET_REG byte to the board */
    && (4 == board_sendbN(address, 4))
                                /* send the address of the requested register */
    && (2 == board_sendbN(1, 2))
                              /* send 1 - number of register values following */
    && (width == board_sendchararray(width, data));
                               /* send the value and return the success state */
}


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Updates a given register bank on the screen given its number               */
/* Returns 1 if successful, 0 otherwise                                       */
/*                                                                            */

int board_get_regbank(int regbanknumber)
{
unsigned char *message;/* register bank values are kept in the regbank struct */
int length_message;
int gran;
int okay;      /* Used to indicate comms. protcol still functioning correctly */


if (TRACE > 5) g_print("board_get_regbank\n");

okay = (board_version >= 0);

if (okay)
  {
  if (board->reg_banks[regbanknumber].width > 0)
    {                                        /* if regbank is not a bit field */
    okay = read_registers(board->reg_banks[regbanknumber].offset,
                          board->reg_banks[regbanknumber].values,
                          board->reg_banks[regbanknumber].width,
                          board->reg_banks[regbanknumber].number);

    board->reg_banks[regbanknumber].valid = TRUE;/* Indicate value may be used */
    }
  else
    {                                      /* Get regbank if it's a bit field */
      /* complex messy stuff to do with bit field - ask Charlie  @@@    */

    gran = board->regbanks_gran << 3;
    length_message = (((board->reg_banks[regbanknumber].number +
                        board->reg_banks[regbanknumber].offset - 1) / gran) -
                       (board->reg_banks[regbanknumber].offset / gran) + 1);

    message = g_new(unsigned char, length_message);         /* Allocate space */

    okay = read_registers(board->reg_banks[regbanknumber].offset / gran,
                          message,
                          board->regbanks_gran,
                          length_message);

    if (okay)
      {                       // This needs some cleaning-up! @@@
      int counter;

      if (VERBOSE) g_print("getting registerbank %d at offset %x size %d\n",
                            regbanknumber,
                            board->reg_banks[regbanknumber].offset>>3,
                            board->reg_banks[regbanknumber].number);

      length_message = board->reg_banks[regbanknumber].number;
      gran = board->reg_banks[regbanknumber].offset % gran;

      for (counter=0;counter<board->reg_banks[regbanknumber].number;counter++)
        board->reg_banks[regbanknumber].values[counter] =
                  message[(counter + gran) >> 3] >> ((counter + gran) & 7) & 1;
      }
    g_free(message);
    }
  }

if (!okay) board_version = -1;
return okay;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sets a register to a given value.                                          */
/* Inputs: register number which is the number of the register                */
/*         regbanknumber which is the number of the register bank             */
/*        *value which is a pointer to the array of bytes that stands for the */
/*         value to be set                                                    */
/* Returns: 1 for success, 0 for failure                                      */
/*                                                                            */
/*                                                                            */

int board_set_register(int regbanknumber, int registernumber,
                       unsigned char *value)
{
int count;
uchar *cpuregvalue;

if (TRACE > 5) g_print("board_set_register\n");

if (!(board_version >= 0)) return 0;       /* If the board is not there return failure */

count = board->reg_banks[regbanknumber].width;
                     /* save the width of the register to be updated in bytes */

if (count > 0)                                          /* if not a bit field */
  {
  if (!write_register(board->reg_banks[regbanknumber].offset+registernumber,
                             value, count))
    board_version = -1;
  }
else
  {                          /* If register is a bit field, read-modify-write */
  count = (board->reg_banks[regbanknumber].offset + registernumber);
                     /* save the width of the register to be updated in bytes */

  cpuregvalue = g_new(uchar, board->regbanks_gran);

  if (!read_registers((count >> 3) / board->regbanks_gran, cpuregvalue,
                                     board->regbanks_gran, 1))
    board_version -1;

  if ((board_version >= 0))
    {
    if (*value != 0)                                     /* Set addressed bit */
      cpuregvalue[(count >> 3) % board->regbanks_gran] |=   1 << (count & 7);
    else                                               /* Clear addressed bit */
      cpuregvalue[(count >> 3) % board->regbanks_gran] &= ~(1 << (count & 7));

    if (!write_register((count >> 3) / board->regbanks_gran,
                                 cpuregvalue, board->regbanks_gran))
      board_version -1;
    }
  g_free(cpuregvalue);
  }                                             /* if register is a bit field */

return (board_version >= 0);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

/* Look-up function to encode length of memory transfer                       */
/* Only four legal sizes at present                                           */

unsigned int board_translate_memsize(int size)
{
switch (size)
  {
  case 1:  return 0;
  case 2:  return 1;
  case 4:  return 2;
  case 8:  return 3;
  default: return 0;
  }
}


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Gets a memory value of a given address                                     */
/* Inputs: count - number of bytes                                            */
/*         address - pointer to the address in bytes                          */
/*         dest   - pointer to where the value will be stored                 */
/*         size   - width of current memory in bytes                          */
/* Returns: TRUE for success, FALSE for failure                               */
/*                                                                            */

boolean board_get_memory(int count, unsigned char *address, unsigned char *dest,
                         int size)
{
int bytecount = count * size;

if (TRACE > 5) g_print("board_get_memory\n");

//if (VERBOSE) g_print("Getting Memory values from board\n");

if ((board_version >= 0))     // Scoping is strange! @@@
  {
  if ((1 != board_sendchar(BR_GET_MEM | board_translate_memsize(size)))
   || (board->memory_ptr_width != board_sendchararray(board->memory_ptr_width,
                                                     address))
                                          /* send the address given its width */
   || (2 != board_sendbN(count, 2))
                   /* send the width (in bytes) of the memory value requested */
   || (bytecount != board_getchararray(bytecount, dest)))
    board_version = -1;         /* Fail if any part of the transmission fails */
  }

return (board_version >= 0);      /* Copy of global indicates success/failure */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sets a memory value of a given address to a new value                      */
/* Inputs: count   - number of elements                                       */
/*         address - pointer to the address in bytes                          */
/*         source  - pointer to the new value to be stored                    */
/*         size    - width of current memory in bytes                         */
/* Returns: TRUE for success, FALSE for failure                               */
/*                                                                            */

boolean board_set_memory(int count, unsigned char *address, unsigned char *source,
                         int size)
{
int bytecount = count * size;

if (TRACE > 5) g_print("board_set_memory\n");

if ((board_version >= 0))     // Scoping is strange! @@@
  {
  if ((1 != board_sendchar(BR_SET_MEM | board_translate_memsize(size)))
   || (board->memory_ptr_width != board_sendchararray(board->memory_ptr_width,
                                                     address))/* send address */
   || (2 != board_sendbN(count, 2))                             /* send width */
   || (bytecount != board_sendchararray(bytecount, source)))    /* send value */
    board_version = -1;
  }

return (board_version >= 0);      /* Copy of global indicates success/failure */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Initialises the locks                                                      */
/*                                                                            */

void init_locks(void)
{
if (TRACE > 5) g_print("init_locks\n");

serial_lock  = FREE;    /* initialises the lock on the serial line to be free */
fpga_lock    = FREE;     /* initialises the lock on the FPGA loads to be free */
compile_lock = FREE;    /* initialises the lock on the compilation to be free */
load_lock    = FREE;     /* initialises the lock on the ELF loader to be free */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


void init_global_vars(void)
{

spsr_exists = TRUE;  /*indicates whether SPSR register is currently displayed */
all_flag_windows_have_been_created = FALSE;

return;
}

/******************************************************************************/
/*                                                                            */
/*                                                                            */
/*                       *****  WORLD STARTS HERE *****                       */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Main function                                                              */
/* Inputs: command line argument                                              */
/*                                                                            */

int main(int argc, char *argv[])
{
GtkWidget *splashwindow;                              /* splash window handle */

TRACE = 0;                                            /* JDG   bodged for now */

if (TRACE > 5) g_print("\nmain\n");

  {
  /* own_path is an attempt at pointing to the binary directory.  It will use */
  /* KMD_DIR if defined, otherwise resorting to the same place as the binary. */
  uchar *pChar;
  own_path = malloc(200);				// @@@@ sort out length @@@

  if ((pChar = getenv(KMD_DIR)) != NULL)
    {                                   /* Has a defined directory to look in */
    strcpy(own_path, pChar);
    for (pChar = own_path; *pChar != '\0'; pChar++);/* Find end of string ... */
//    if ((pChar != own_path) && (pChar[-1] != '/'))  /* If there isn't a slash */
      { pChar[0] = '/'; pChar[1] = '\0'; }                     /*  append one */
    }
  else
    {
    realpath(argv[0], own_path);       /* Get (presumed) own path into buffer */
    for (pChar = own_path; *pChar != '\0'; pChar++);/* Find end of string ... */
    while (*pChar != '/') pChar--;            /* ... cut off last element ... */
    pChar[1] = '\0';                                    /* ... and terminate. */
    }
  own_path_length = strlen(own_path) + 1;              /* Includes terminator */
  }

init_global_vars();
board_version = -2;                                  /* Code for untested for */
board_runflags = RUN_FLAG_INIT;
VERBOSE = VERBOSE_D;
rcfile = g_strconcat(SETUP_DIR, "/.komodo", NULL);
                                              /* sets the path to the rc file */

/* Global initialisation being collected here until it can be sorted properly */
(special_registers[0]).name        = "PURPLE"; /* Just a name for association */
(special_registers[0]).value       = NULL;
(special_registers[0]).colour      = (GdkColor) {0, 0x8000, 0x0000, 0x8000};
(special_registers[0]).pixmap_data = sppixmap_xpm;
(special_registers[0]).pixmap      = NULL;
(special_registers[0]).bitmap      = NULL;
(special_registers[0]).active      = FALSE;  /* JDG  Expanded as example @@@  */

special_registers[1] = (special_reg)
  {"BLUE", NULL,  {0, 0x0000, 0x0000, 0xFFFF}, sppixmap_xpm, NULL, NULL, FALSE};

special_registers[2] = (special_reg)
  {"GREEN", NULL, {0, 0x0000, 0xC000, 0x2000}, pcpixmap_xpm, NULL, NULL, FALSE};

source.pStart   = NULL;                              /* No source file loaded */
source.pEnd     = NULL;
source_dis_tabs = DIS_TAB_INIT;                        /* Set up tab position */
symbol_windows  = NULL;                            /* No symbol table windows */

emulator_PID = 0;
emulator_prog = malloc(own_path_length + strlen(EMULATOR_PROG));  /* Allocate */
strcat(strcpy(emulator_prog, own_path), EMULATOR_PROG);
                               /* Initialise the default name of the emulator */

use_internal = USE_INTERNAL;

switch (commandline(argc, argv))                     /* process the arguments */
  {
  case COMMANDLINE_CONTINUE:           break;
  case COMMANDLINE_TERMINATE: exit(0); break;
  case COMMANDLINE_ERROR:     exit(1); break;
  default: fprintf(stderr, "Command parser failed\n"); exit(1); break;
  }

gtk_init(&argc, &argv);                    /* Initialise GTK (user interface) */
init_locks();                           /* Initialise the locks to free state */
splashwindow = view_create_splashwindow();       /* Display the splash window */

view_greycolour   = (GdkColor) { 1, 0xC000, 0xC000, 0xA000 };
view_redcolour    = (GdkColor) { 1, 0xFFFF, 0x0000, 0x0000 };
view_bluecolour   = (GdkColor) { 1, 0x0000, 0x0000, 0xFFFF };
view_orangecolour = (GdkColor) { 1, 0xF000, 0x9000, 0x0000 };
gdk_colormap_alloc_color(gdk_colormap_get_system(), &view_greycolour,   0, 1);
gdk_colormap_alloc_color(gdk_colormap_get_system(), &view_redcolour,    0, 1);
gdk_colormap_alloc_color(gdk_colormap_get_system(), &view_bluecolour,   0, 1);
gdk_colormap_alloc_color(gdk_colormap_get_system(), &view_orangecolour, 0, 1);
/* gdk_colormap_alloc_color(gdk_colormap_get_system(), ...  -after- gtk_init  */

setup();                                  /* Do some necessary initialisation */

//{     Just looking at the memory map, as supplied ...
//int i, j;
//printf("Client has %d memory mapped area(s)\n", board_num_mem_segments);
//for (i=0; i<board_num_mem_segments; i++)
//  {
//  printf("Segment %d, start ", i);
//  for (j=3; j>=0; j--) printf("%02X", *(board_mem_segments[i].start+j));
//  printf(", length ");
//  for (j=3; j>=0; j--) printf("%02X", *(board_mem_segments[i].length+j));
//  printf("\n");
//  }
//}

gtk_widget_hide(splashwindow);                      /* Hide the splash window */
while (gtk_events_pending()) gtk_main_iteration();     /* Free up event queue */
gtk_widget_destroy(splashwindow);                /* Destroy the splash screen */
      /* The splashwidow has some memory which needs freeing - BUT WHAT?? @@@ */
gtk_main();                                            /* Give control to GTK */

return 0;                             /* Should never be invoked in principle */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Displays the help message                                                  */
/*                                                                            */

void usage_message()
{
g_print("\n " PACKAGE " Version " VERSION " (" DATE ")\n");
g_print(" Copyright " YEAR " University of Manchester \n");
g_print(" Authors: Charlie Brej, Roy Schestowitz, Jim Garside, John Zaitseff, "
         "Chris Page\n");
g_print(" Bug reports and feature requests: komodo@cs.man.ac.uk\n");
g_print(" Website: http://www.cs.man.ac.uk/teaching/electronics/komodo\n");
g_print("\n");
g_print(" Usage: kmd [options]\n");
g_print("\n");
g_print("   -?              display this help message\n");
g_print("   -c              create the default .komodo file in %s\n",
         SETUP_DIR);
g_print("   -e [emulator]   run on an emulator program Default: %s\n",
         EMULATOR_PROG);
g_print("   -h\n");
g_print("  --help           display this help message\n");
g_print("   -i              use standard .komodo file compiled into program\n");
g_print("   -k komodofile   use specific .komodo file Default: %s/.komodo\n",
         SETUP_DIR);
g_print("   -n              run with back end over a network port\n");
g_print("   -q              be quiet\n");
g_print("   -s [port]       run through serial port Default:" PORTNAME "\n");
g_print("   -t gtkrc        change GTK+ theme by specifying a different"
                           " gtkrc file\n");
g_print("   -v              be verbose\n");
g_print("\n");
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* command line arguments are processed here                                  */
/* Inputs: command line argument                                              */
/* Returns: subsequent behaviour code number                                  */

int commandline(int argc, char *argv[])
{
int count = 1;
int comp;
FILE *fd;
boolean create = FALSE;                            /* flag for create .komodo */
char *gtkrc[]  = { NULL, NULL };
char *commandstrings[] =
   {"--help","-h","-?","-e","-s","-c","-k","-i","-t","-v","-q","-d","-n",NULL };
/*      0      1    2    3    4    5    6    7    8    9   10   11   12   13  */
int error;

if (TRACE > 5) g_print("commandline\n");

interface_type = SERIAL;
portname       = PORTNAME;

error = 0;         /* Used to flag problems in following loop: 0 means "none" */

while ((argv[count] != NULL) && (error == 0))       /* while arguments remain */
  {
  if ('-' != *argv[count])          /* check that they are prefixed by a dash */
    {
    g_print("Unknown option %s\n\n", argv[count]);
    error = 1;
    }
  else
    {
    comp = 0;                           /* initiliase commandstring[] pointer */
    while ((commandstrings[comp] != NULL)
         && g_strcasecmp(commandstrings[comp], argv[count]))
      comp++;       /* step through list until parameter matches or list ends */

    switch (comp)                                 /* analyze the match is any */
      {
      case 0:
      case 1:
      case 2: error = 1; break;                                       /* Help */

      case 3:                                                  /* emulator -e */
        interface_type = EMULATOR;
        if ((argv[count + 1]) && ('-' != *argv[count + 1]))
          emulator_prog = argv[++count];   /* Change pointer to next argument */
           /* This leaves the old string as garbage; else if >1 "-e" included */
                    /*  could start free-ing argv space.  Not worth trapping. */
        break;

      case 4:                                                     /*serial -s */
        interface_type = SERIAL;
        if ((argv[count + 1]) && ('-' != *argv[count + 1]))
          portname = argv[++count];
                  /* set the port name according to the argument that follows */
        break;

      case 5: create = TRUE; break;                 /* Create .komodo file -c */

      case 6:                                                 /* dotkomodo -k */
        use_internal = FALSE;
        if ((argv[count + 1]) && ('-' != *argv[count + 1]))
          rcfile = argv[++count];      /* Retrieve the new `.komodo' filename */
        break;

      case 7: use_internal = TRUE; break;                     /* dotkomodo -i */

      case 8:                                                     /* Theme -t */
        if ((argv[count + 1]) && ('-' != *argv[count + 1]))
                                           /* check validity of next argument */
          {
          gtkrc[0] = argv[++count];                  /* step to next argument */
          gtk_rc_set_default_files (gtkrc);                      /* set theme */
          }
        break;

      case 9:  VERBOSE = TRUE;  break;                          /* Verbose -v */
      case 10: VERBOSE = FALSE; break;                            /* Quiet -q */
      case 11: TRACE = 20; break;   // Should take a number, really  @@@
      case 12: interface_type = NETWORK; break;                 /* Network -n */

      default:
        g_print("Command parameter error!\n");              /* No match found */
        error = 1;
      }                                                         /* end switch */
    count++;                                   /* step to next user parameter */
    }                                                             /* end else */
  }                                                              /* end while */

if (create && (error == 0))             /* if ".komodo" file is to be created */
  {       /* This shouldn't be in here, it should be after the return @@@     */
  fd = fopen(rcfile, "w");               /* open the rc file to be written to */
  if (fd == NULL)                                   /* if error opening it... */
    {
    g_print("can't open .komodo file\n");
    error = 2;                                      /* quit with error status */
    }
  else
    {
    fprintf(fd, "%s", dotkomodo);    /* copy the contents of dotkomodo string */
    fclose(fd);                                             /* close the file */
    g_print("%s/.komodo set to default\n", SETUP_DIR);
    error = 3;                                     /* Not really an error @@@ */
    }                /* a new file has been created with defaults from rcfile */
  }

switch (error)                                          /* Select return code */
  {
  case 1: usage_message(); return COMMANDLINE_TERMINATE;/* Unrecognised option*/
  case 2: return COMMANDLINE_ERROR;           /* Failed to write .komodo file */
  case 3: return COMMANDLINE_TERMINATE;    /* Successfully wrote .komodo file */
  default: return COMMANDLINE_CONTINUE;                   /* Continue normally*/
  }
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Setting up the application. A subroutine called from main()                */
/*                                                                            */

void setup()
{
GScanner *scanner;                          /* used to parse the .komodo file */
int status;

/*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

  int open_dot_komodo()
  {
  if (use_internal)                               /* If internal file is used */
    scanner = ScanOpenSCANString(dotkomodo);             /* Use internal file */
  else
    scanner = ScanOpenSCANFile(rcfile);                       /* Open .komodo */

  if (!scanner)                            /* Check the state of file scanner */
    {
    g_print("Cannot find set up file `.komodo'\n");
    g_print("If you would like to create one then start komodo with -c flag\n");
//    exit(1);
    use_internal = TRUE;                         /* Use internal file instead */
    scanner = ScanOpenSCANString(dotkomodo);
    }

  Scantopnode = ScanParseSCANNode(scanner, FALSE);       /* Scan .komodo file */
  ScanCloseSCANFile(scanner);         /* The scanner can safely be closed now */
  if (Scantopnode)                       /* Check if the parse was successful */
    {
    if (VERBOSE) g_print(".komodo init    : PASSED\n");
    return 1;                                                      /* Success */
    }
  else
    {
    if (VERBOSE) g_print(".komodo init    : FAILED\n.komodo file error\n");
    return 0;
    }
  }

  /*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

  void set_style()                                     /* some style settings */
  {
  fixed_font = gdk_font_load("-*-lucidatypewriter-medium-*-*-*-*-120-*-*-*-*-*");
                                                            /* get fixed font */

fixed_font = NULL;	// Hack for LJ (31/3/05)	@@@@

  if (NULL == fixed_font) fixed_font = gdk_font_load("fixed");
                                      /* if no good font get pants fixed font */
  fixed_style = gtk_widget_get_default_style();
  fixed_style = gtk_style_copy(fixed_style);

  #ifdef GTK2
  gtk_style_set_font(fixed_style, fixed_font);
  #endif

  #ifndef GTK2
  fixed_style->font = fixed_font;               /* create style with new font */
  #endif
  }

  /*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

  void init_emulation()
  {
  pipe(board_emulation_communication_from);
  read_pipe = board_emulation_communication_from[0];
  pipe(board_emulation_communication_to);      /* 'man pipe' for more details */
  write_pipe = board_emulation_communication_to[1];
                                       /* set up the pipes to allow emulation */

  emulator_PID = fork();
  signal(SIGCHLD, dead_child);  /* Register procedure called on child's death */
  if (emulator_PID == 0)                                   /* If daughter ... */
    {
    close(1);
    dup2(board_emulation_communication_from[1], 1);
    close(0);
    dup2(board_emulation_communication_to[0], 0);
    execlp(emulator_prog, emulator_prog, NULL);
    exit(0);
    }
  else if (VERBOSE) g_print("Emulating with : %s\n", "komodo_emulate");
  }


  /*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

  void init_serial()
  {
  status = serial_setup(1);        /* Attempt to set up the serial connection */
  read_pipe  = serial_FD;                           /* Set up the reading ... */
  write_pipe = serial_FD;                /*  and writing pipes to serial port */

  if (VERBOSE) g_print("Serial Setup    : %d\n", status);/* setup serial port */
  if (!status) exit(1);               /* Problem setting up serial connection */
  }

  /*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

#define NETWORK_PORT  1337

  void init_network()
  {
  int peer1;
 
  struct sockaddr_in sockaddr;
 
  peer1 = socket (AF_INET, SOCK_STREAM, 0);
  if (peer1 == -1) g_print("Error creating socket.\n");
 
  sockaddr.sin_family      = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sockaddr.sin_port        = htons(NETWORK_PORT); 
 
  fcntl (peer1, F_SETFL, O_NONBLOCK);

  connect (peer1, (struct sockaddr *) &sockaddr, sizeof (sockaddr));

  read_pipe  = peer1;
  write_pipe = peer1;

  if (VERBOSE) g_print("Network setup\n");                   /* Setup network */
  return; 
  }
 
  /*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

  void start_gtk()
  {
  unsigned int dummy;

  view_create_mainwindow();                                  /* Create window */

  pipe(compile_communication);     /* set up the pipe so that compiler comms. */
                                 /* can later be viewed in the console window */
  gdk_input_add(compile_communication[0], GDK_INPUT_READ,
                (GdkInputFunction) callback_console_update, NULL);
  callback_global_refresh();                /* receive information from board */
  g_timeout_add(REFRESH_PERIOD, callback_updateall, NULL);
                /* add a function to read from back-end terminal to the timer */
  board_enq(&dummy);                          /* check board state (Why? @@@) */
  gtk_widget_show(view_mainwindow);
      /* display main window after it has been fully created and accommodated */
  gdk_window_set_icon(view_mainwindow->window, NULL,
                      view_komodoicon_pixmap, view_komodoicon_bitmap);
                                                              /* set its icon */
  }


  /*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***/

if (TRACE > 5) g_print("setup\n");

misc_init_symbol_table();

if (open_dot_komodo())                    /* get the .komodo input, else fail */
  {
  set_style();

  switch (interface_type)
    {
    case SERIAL:   init_serial();    break;
    case EMULATOR: init_emulation(); break;
    case NETWORK:  init_network();   break;
    default: g_print("Back end type not recognised in setup\n"); break;
    }

  status = board_mini_ping();      /* Attempt to establish contact with board */

  if (!status)         /* if no board was present, emulator should be invoked */
    {
//  init_emulation();
//  interface_type = EMULATOR;
//  close(serial_FD);                               /* close serial if exists */
//  board_mini_ping();
//  g_print("\nUnable to establish connection with the board."
//          " Emulation invoked.\n\n");

    g_print("\nUnable to establish connection with remote system."
            "\nPlease check connections, power, etc.\n");
    g_print("To use the emulator, invoke with the \"-e\" option\n");
    exit(1);                                    // @@@
    }

  if (VERBOSE) g_print("Ping            : okay\n");   /* Alive if reaches here */

  board = board_wotru();             /* Find what's connected at the back end */
						// Returns allocated structure @@@

  if (VERBOSE) g_print("Board version   : %d\n", status);
//  if (!status) exit(1);                         // @@@ Already did this

  start_gtk();                                            /* Invoke GTK start */

  }

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/***************************/
/** Breakpoints functions **/
/***************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Reads trap status                                                          */
/*                                                                            */

boolean trap_get_status(unsigned int trap_type, unsigned int *wordA,
                        unsigned int *wordB)
{
boolean error;

if (TRACE > 5) g_print("trap_get_status\n");

board_sendchar(BR_BP_GET | ((trap_type << 2) & 0xC));/* get breakpoint packet */

error = ((board_getbN(wordA, 4) != 4) || (board_getbN(wordB, 4) != 4));
                                                  /* Record word a and word b */
if (error) board_mini_ping();             // Return value ignored @@@

//  if ((board_getbN(wordA, 4) == 4) && (board_getbN(wordB, 4) == 4))
//    error = (0 == 1);                           /* record word a and word b */
//  else
//    {
//    board_mini_ping();             // Return value ignored @@@
//    error = (0 == 0);
//    }

return error;                                          /* return error status */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sets trap status                                                           */
/*                                                                            */

void trap_set_status(unsigned int trap_type, unsigned int wordA,
                     unsigned int wordB)
{
if (TRACE > 5) g_print("trap_set_status\n");

board_sendchar(BR_BP_SET | ((trap_type << 2) & 0xC));/* set breakpoint packet */
board_sendbN(wordA, 4);                                        /* send word a */
board_sendbN(wordB, 4);                                        /* send word b */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Reads a trap definition for use by the breakpoints callbacks               */
/*                                                                            */

boolean read_trap_defn(unsigned int trap_type, unsigned int trap_number,
                       trap_def *trap_defn)
{
if (TRACE > 5) g_print("read_trap_defn  type %d, number %d\n",
                        trap_type & 3, trap_number);

board_sendchar(BR_BP_READ | ((trap_type << 2) & 0xC));/*send trap-read packet */
board_sendchar(trap_number);             /* send the number of the definition */

if ((2 != board_getbN(&((*trap_defn).misc), 2))/*get the trap misc properties */
      || (board->memory_ptr_width != board_getchararray(board->memory_ptr_width,
                                                      (*trap_defn).addressA))
      || (board->memory_ptr_width != board_getchararray(board->memory_ptr_width,
                                                      (*trap_defn).addressB))
      || (8 != board_getchararray (8, (*trap_defn).dataA))
      || (8 != board_getchararray (8, (*trap_defn).dataB)))
                                              /* get address a&b and data a&b */
  {
  if (VERBOSE)
    g_print("breakpoint crash when attempting to read trap definition\n");
  return FALSE;
  }
else
  return TRUE;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Writes a trap definition on behalf of the breakpoints callbacks            */
/*                                                                            */

boolean write_trap_defn(unsigned int trap_type, unsigned int trap_number,
                        trap_def * trap_defn)
{
if (TRACE > 5) g_print("write_trap_defn  type %d, number %d\n",
                        trap_type & 3, trap_number);

board_sendchar(BR_BP_WRITE | ((trap_type << 2) & 0xC));
                                                    /* send trap-write packet */
board_sendchar(trap_number);             /* send the number of the definition */
board_sendbN(((*trap_defn).misc), 2);        /* send the trap misc properties */
board_sendchararray(board->memory_ptr_width, (*trap_defn).addressA);
board_sendchararray(board->memory_ptr_width, (*trap_defn).addressB);
board_sendchararray(8, (*trap_defn).dataA);
board_sendchararray(8, (*trap_defn).dataB);  /* send address a&b and data a&b */
return TRUE;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/*****************************/
/** Board control functions **/
/*****************************/

/* Some simple ones first                                                     */

void stop_board(void)     { board_sendchar(BR_STOP); }  /* Stop client signal */

void continue_board(void) { board_sendchar(BR_CONTINUE);}  /* Continue client */

void reset_board(void)    { board_sendchar(BR_RESET); }       /* Reset client */


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Called when service to interrupts is enabled or disabled                  */
/*  user_data is 0x2 if called from IRQ                                       */
/*  user_data is 0x1 if called from FIQ                                       */
/*  toggle_state is Boolean containing the state of the toggle button pressed */

void set_interrupt_service(int toggle_state, int user_data)
{
uchar rtf;                               /* used to store response from board */

if (TRACE > 5) g_print("set_interrupt_service\n");

board_sendchar(BR_RTF_GET);
if (!board_getchar(&rtf))                          /* if no response received */
if (!board_mini_ping() || !board_getchar(&rtf))
                                       /* try again and abort if unsuccessful */
  return;            // @@@

if (toggle_state) rtf |= user_data;  /* if the toggle button is now activated */
                         /* set the appropriate service (in a bitwise manner) */
else rtf &= ~user_data;  /*  else rtf is set to unset the appropriate service */

board_sendchar(BR_RTF_SET);                                    /* send header */
board_sendchar(rtf);                                          /* send new rtf */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Signals the board to start/run from steps                                 */
/*  Returns: TRUE to indicate (apparent) success                              */
/*                                                                            */

boolean run_board(int *steps)
{
int board_state;                     /* Stores the current state of the board */
unsigned int dummy;

if (TRACE > 5) g_print("run_board\n");

board_state = board_enq(&dummy);
if (board_state == CLIENT_STATE_RUNNING_SWI    /* Might need modification @@@ */
 || board_state == CLIENT_STATE_RUNNING
 || board_state == CLIENT_STATE_STEPPING
 || board_state == CLIENT_STATE_MEMFAULT
 || board_state == CLIENT_STATE_BUSY)
  return FALSE;                                               /* Return error */

board_sendchar(BR_START | board_runflags);
                             /* Send definition at start e.g. breakpoints on? */
board_sendbN(*steps, 4);                                   /* Send step count */
return TRUE;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

// Alter these to derive "runflags" as required rather than storing them(?) @@@
// Probably needs vector of flag handles @@@

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Signals the board to walk from steps                                      */
/*                                                                            */

void walk_board(int steps, int mask)
{
board_sendchar(BR_START | board_runflags | mask);
board_sendbN(steps, 4);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Adjusts the start command when the toggle buttons are pressed.            */
/*  Bits fields are: <5> watchpoints <4> breakpoints <3> abort                */
/*                   <2> SWI         <1> BL          <0> break on 1st         */
/*                                                                            */

void change_start_command(int button_state, int user_data)
{
if (button_state) board_runflags = board_runflags |   user_data;
else              board_runflags = board_runflags & (~user_data);

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean test_run_flag(unsigned int mask) { return (board_runflags&mask) != 0; }

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Reads data from the back-end terminal and prints characters accordingly   */
/*                                                                            */

void read_string_terminal(gpointer comms)
{
char string[256];  /* (large enough) string to get the message from the board */
char current_char;               /* and one for storing chars of string above */
uchar length;
int i;                                                          /* loop index */
feature *comms_terminal = comms;

if (TRACE > 15) g_print("read_string_terminal\n");

if (serial_lock == FREE)
  {         /* if the serial line is not used by updateall - just in case...  */
  serial_lock = TERMINAL_UPDATE;                               /* acquire lock */
  do
    {
    board_sendchar(BR_FR_READ);                   /* send appropriate command */
    board_sendchar(comms_terminal->dev_number); /* send the terminal number(?)*/
    board_sendchar(MAX_LENGTH);/* send the maximum length allowed in response */
    board_getchar(&length);                          /* get length of message */
    if (length != 0) // non-zero received from board/* if not an empty packet */
      {
      board_getchararray(length, string);/* get the message recorded in string*/
      for (i = 0; i < length; i++)
        print_char_in_terminal(string[i], comms);  /* print the Ith character */
      }
    }                                                               /* end do */
    while (255 == length);
        /* if there could be another packet to follow the current one, repeat */
  serial_lock = FREE;                                            /* free lock */
  }                                                                 /* end if */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sends an ASCII value to the terminal given                                 */
/*                                                                            */

//void send_key_to_terminal(int key_pressed, gpointer comms)
void send_key_to_terminal(int key_pressed, int feature)
{
uchar response_from_board;
//feature *comms_terminal = comms;
                           /* record the feature from which the callback came */

if (TRACE > 5) g_print("send_key_to_terminal\n");

board_sendchar(BR_FR_WRITE);                                /* begins a write */
//board_sendchar(comms_terminal->dev_number);         /* tells where to send it */
board_sendchar(feature);                            /* tells where to send it */
board_sendchar(1);                                           /* send length 1 */
board_sendchar(key_pressed);           /* send the message - 1 char currently */
board_getchar(&response_from_board);
                      /* store the currently unused response from board - ACK */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sends packets when downloading in features                                 */
/*                                                                            */

char download_send_packet(int featureentry, unsigned int pkt_length,
                          uchar *position)
{
char ch;

if (TRACE > 5) g_print("download_send_packet\n");

board_sendchar(BR_FR_SEND);                                /* Download packet */
board_sendchar(featureentry);                                   /* Feature No */
board_sendchar(pkt_length & 0xFF);                /* Packet length (256 -> 0) */
board_sendchararray(pkt_length, position);                            /* Data */
board_getchar(&ch);                                            /* get the ACK */

return ch;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Sends the header of a download in features                                 */
/*                                                                            */

char download_send_header(int featureentry, int size)
{
char ch;

if (TRACE > 5) g_print("download_send_header\n");

board_sendchar(BR_FR_FILE);                                /* Download header */
board_sendchar(featureentry);                                   /* Feature No */
board_sendbN(size, 4);                                         /* File length */
board_getchar(&ch);                                            /* get the ACK */

return ch;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/
/* Invoked when a child process has died.                                     */

void dead_child(int arg)
{
if (TRACE > 2) g_print("Child process died\n");
return;
}


/*                              end of interface.c                            */
/*============================================================================*/
