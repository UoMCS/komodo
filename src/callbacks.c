/******************************************************************************/
/*                Name:           callbacks.c                                 */
/*                Version:        1.5.0                                       */
/*                Date:           26/07/2007                                  */
/*                Functions called on actions from the GTK interface          */
/*                                                                            */
/*============================================================================*/
/*                                                                            */
/*  All functions linked to buttons, menu entries etc. (the majority of the   */
/*  functions herein) enter with two parameters.  The first is the handle of  */
/*  the cause for the call, the second is a user parameter which is attached  */
/*  with the call.  The parameter is really a pointer to a structure but may  */
/*  simply be cast as an integer instead.                                     */


#include <glib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <gdk/gdkkeysyms.h>                  /* Definitions of `special' keys */

#include "definitions.h"
#include "global.h"

#include "callbacks.h"
#include "breakcalls.h"
#include "interface.h"
#include "serial.h"
#include "misc.h"
#include "view.h"
#include "chump.h"

#define LOAD_BUFFER_LENGTH 80 /* Batch size for bytes in ELF (or binary) load */
#define MAX_OBJECT_LENGTH  16  /* Max number of bytes accepted in object code */

//typedef enum		 enum causes compiler error!!
//  { FILE_NONE, FILE_ERROR, FILE_UNKNOWN, FILE_KMD, FILE_ELF, FILE_BIN }
//ftype;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct section_list_name      /* Record for listing ELF section info. */
  {
  struct section_list_name  *pNext;
  unsigned int               number;
  unsigned int               name;
  unsigned int               type;
  unsigned int               flags;
  unsigned int               addr;
  unsigned int               offset;
  unsigned int               size;
  unsigned int               link;
  unsigned int               info;
  unsigned int               align;
  unsigned int               entsize;
  unsigned int               count;
  }
  elf_section_list;

const SECTION_LIST_SIZE = sizeof(elf_section_list);

#define ELF_SYMTAB     2
#define ELF_DYNSYM    11

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define FILE_NONE    0
#define FILE_ERROR   1
#define FILE_UNKNOWN 2
#define FILE_KMD     3
#define FILE_ELF     4
#define FILE_BIN     5

/* Non-exported prototypes                                                    */

int     ID_file_type(char*);                      /* Identify input file type */
void    load_data(gpointer, int, char*);      /* Load data of designated type */
boolean read_elf(char*);
void elf_header(FILE*, unsigned int*, unsigned int*, unsigned int*);
void elf_list_sections(FILE*, unsigned int, unsigned int, elf_section_list**);
unsigned int elf_section_load(FILE*, elf_section_list*,
                              unsigned int, unsigned int);
void    elf_read_sym_table(FILE*, elf_section_list*, elf_section_list*);
void    elf_delete_section_list(elf_section_list**);
unsigned int get_item(FILE*, int);
void    read_binary(char*, uchar*);
void    maybe_step_addr(mem_window*, uchar*, int);
void    scroll_display(mem_window*, uchar*);
void    memwindow_step(mem_window*, int);
void    set_refresh(boolean, int);
void    set_fpga_text(char*, gpointer, uchar*);
boolean FPGA_header(uchar*, gpointer, unsigned int*, int);
void    callback_memory_refresh(void);
void    callback_register_refresh(void);
void    callback_mode_selection_all(int, int);
/* Called from(callback_mode_selection, callback_mode_selection_saved}        */
/*   to generalise them.                                                      */
void    maintain_filename_list(gpointer*, char*, int);
void    flush_sauce(void);
boolean check_elf(char*);
boolean check_sauce(char*);
boolean read_sauce(char*);

gint callback_walk(gpointer);

/* Non-exported variables                                                     */

gint view_global_refresh_timer;
                          /* Handle of the `walk' timer (NULL == inactive) ?? */
boolean board_running = FALSE; /* Host thinks so - check for client agreement */
boolean was_walking   = FALSE;

char *mem_column_data[] =        /* Global; shared with view.c (see global.h) */
    { "Address", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex",
                 "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex",
                 "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex",
                 "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex", "Hex",
                 "ASCII", "Diss" };           /* Must be "TOTAL_ENTRY" fields */


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Called when load button pressed and loads a file                          */
/*  entry is the file entry                                                   */
/*                                                                            */

void callback_button_load(GtkButton *button, gpointer entry)
{                  /*        Load button       Text entry    */

if (TRACE > 5) g_print("callback_button_load\n");

load_data(entry, FILE_UNKNOWN, NULL);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when the address in the "binary load address" pop-up is changed.    */
/*                                                                            */

void callback_load_binary_address(GtkEntry *entry, gpointer label)
{
uchar *address;
char *text;

if (TRACE > 5) g_print("callback_load_binary_address\n");

address = g_new0(uchar, board->memory_ptr_width);
text    = gtk_editable_get_chars(GTK_EDITABLE(view_binary_load_address), 0, -1);

if (!view_hexstr2chararr(board->memory_ptr_width, text, address))
  {                                                 /* get address from entry */
  gtk_label_set_text(label, "Not Valid!");                /* Not a hex number */
  }
else
  {
  g_free(text);                                         /* Re-use pointer :-( */
  text = view_chararr2hexstrbe(board->memory_ptr_width, address);
  gtk_label_set_text(label, text);                /* Reprint number on pop-up */
  }

g_free(text);
g_free(address);
return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Loads a binary file at specified address.                                 */
/*  Called from binary `pop up' button.                                       */

void callback_load_binary(GtkButton *button, gpointer entry)
{
uchar *address;
char  *text;

if (TRACE > 5) g_print("callback_load_binary\n");

address  = g_new0(uchar, board->memory_ptr_width);
text = gtk_editable_get_chars(GTK_EDITABLE(view_binary_load_address), 0, -1);

if (view_hexstr2chararr(board->memory_ptr_width, text, address))
  {
  load_data(entry, FILE_BIN, address);
  gtk_widget_hide(view_fileerror);
  }

g_free(text);
g_free(address);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

int ID_file_type(char *filename)
{
int file_type;
struct stat status;

if (stat(filename, &status) != 0) file_type = FILE_ERROR;        /* Not found */
else if (check_sauce(filename))   file_type = FILE_KMD;
else if (check_elf(filename))     file_type = FILE_ELF;
else                              file_type = FILE_UNKNOWN;

return file_type;
}

/*----------------------------------------------------------------------------*/
/* `file_type' and `address' specified for pure binary loads                  */

void load_data(gpointer entry, int file_type, char *address)
{
unsigned int old_symbol_count;
char *filename;
int error;                    // Enumerate @@@

if (load_lock == FREE)                  /* if nothing else is already loading */
  {
  load_lock = LOAD;                                               /* Set lock */

  error = -1;
  filename = gtk_entry_get_text(GTK_ENTRY(entry)); /* Get pointer to filename */

  if (file_type == FILE_UNKNOWN) file_type = ID_file_type(filename);

  old_symbol_count = misc_count_symbols(); /* Remember how many `rows' we had */
  if ((file_type==FILE_KMD) || (file_type==FILE_ELF) || (file_type==FILE_BIN))
    {                              /* Okay to overwrite, so flush old records */
    flush_sauce();
    misc_flush_symbol_table();
    if (VERBOSE) g_print("Loading: %s\n", filename);
    }

  switch (file_type)
    {
    case FILE_KMD: if (read_sauce(filename))       error = 0; break;
    case FILE_BIN: read_binary(filename, address); error = 0; break;	//@@
    case FILE_ELF: if (!read_elf(filename))        error = 0; break;
    case FILE_UNKNOWN: gtk_widget_show(view_fileerror); break;/* Offer binary */
    default: g_print("\a"); break;   /* Simple indication of "File not found" */
    }

  if (error == 0) maintain_filename_list(entry, filename, TRUE);   /* If okay */
  callback_memory_refresh();                                   /* Refresh all */
  view_refresh_symbol_clists(old_symbol_count, misc_count_symbols());
                                           /* Update any symbol table windows */

  load_lock = FREE;                                           /* release lock */
  }                                // end locking

return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Remove all source file records                                            */
/*                                                                            */

void flush_sauce(void)
{
source_line *pOld, *pTrash;

if (TRACE > 5) g_print("flush_sauce\n");

pOld = source.pStart;
source.pStart = NULL;
source.pEnd   = NULL;			// Backward links never used in anger @@@

while (pOld != NULL)
  {
  if (pOld->text != NULL) g_free(pOld->text);
  pTrash = pOld;
  pOld = pOld->pNext;
  g_free(pTrash);
  }
return;
}

/*----------------------------------------------------------------------------*/
/* See if we recognise this file `type'                                       */

boolean check_elf(char *filename)
{
FILE *fPtr;
boolean result;

result = FALSE;

if ((fPtr = fopen(filename, "r")) != NULL)
  {
  result = (getc(fPtr) == 0x7F) && (getc(fPtr) == 'E')
        && (getc(fPtr) == 'L')  && (getc(fPtr) == 'F');
   fclose(fPtr);
  }

return result;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean check_sauce(char *filename)
{
FILE *fPtr;
boolean result;

result = FALSE;

if ((fPtr = fopen(filename, "r")) != NULL)
  {
  result = (getc(fPtr) == 'K') && (getc(fPtr) == 'M') && (getc(fPtr) == 'D');
  fclose(fPtr);
  }

return result;
}

/*----------------------------------------------------------------------------*/

boolean read_elf(char *filename)
{
int error;                    // Enumerate @@@
FILE *fp;
elf_section_list *elf_sections, *pTemp, *pTemp2;
unsigned int sect_offset, sect_count, start_address;
unsigned int total, loaded;                             /* Progress bar stuff */

error = 0; 
if ((fp = fopen(filename, "r")) == NULL)                      /* Read file in */
  fprintf(stderr, "can't open %s\n", filename);
else
  {
  elf_header(fp, &sect_offset, &sect_count, &start_address);
  elf_list_sections(fp, sect_offset, sect_count, &elf_sections);

  total  = 0;
  loaded = 0;
  for (pTemp = elf_sections; pTemp != NULL; pTemp = pTemp->pNext)
    if ((pTemp->flags & 2) != 0)                        /* Loadable attribute */
      total = total + pTemp->size;                  /* Measure length to load */

  for (pTemp = elf_sections; pTemp != NULL; pTemp = pTemp->pNext)
    if ((pTemp->flags & 2) != 0)                        /* Loadable attribute */
      loaded = elf_section_load(fp, pTemp, loaded, total);
                                                   /* Chain down section list */

  for (pTemp = elf_sections; pTemp != NULL; pTemp = pTemp->pNext)
    if ((pTemp->type == ELF_SYMTAB) || (pTemp->type == ELF_DYNSYM))
      {                                                       /* Symbol table */
      for (pTemp2 = elf_sections;
          (pTemp2 != NULL) && (pTemp2->number != pTemp->link);
           pTemp2 = pTemp2->pNext);        /* Point pTemp2 at strings section */
      if (pTemp2 != NULL)                     /* The names (strings) do exist */
        {
        elf_read_sym_table(fp, pTemp, pTemp2);
        }
      }

  elf_delete_section_list(&elf_sections);
  }

return (error != 0);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void elf_header(FILE *fHandle, unsigned int *sect_offset,
                               unsigned int *sect_count,
                               unsigned int *entry)
{
unsigned int fPos;

fPos = ftell(fHandle);                                 /* Record where we are */

fseek(fHandle, 0x18, SEEK_SET); *entry       = get_item(fHandle, 4);
fseek(fHandle, 0x20, SEEK_SET); *sect_offset = get_item(fHandle, 4);
fseek(fHandle, 0x30, SEEK_SET); *sect_count  = get_item(fHandle, 2);

fseek(fHandle, fPos, SEEK_SET);                  /* Restore original position */

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Return list of sections in ELF file                                        */

void elf_list_sections(FILE *fHandle, unsigned int offset, unsigned int count,
                                 elf_section_list **elf_sections)
{
unsigned int i;
elf_section_list *pLast, *pNew;

*elf_sections = NULL;                              /* Initialise list pointer */
pLast         = *elf_sections;

fseek(fHandle, offset, SEEK_SET);                             /* Strip ident. */

for (i = 0; i < count; i++)
  {
  pNew = (elf_section_list*) malloc(SECTION_LIST_SIZE);     /* Make new entry */
  pNew->pNext  = NULL;
  pNew->number = i;

  if (pLast == NULL) *elf_sections = pNew;                  /* Link new entry */
  else             pLast->pNext = pNew;
  pLast = pNew;

  pNew->name    = get_item(fHandle, 4);             /* Offset to section name */
  pNew->type    = get_item(fHandle, 4);
  pNew->flags   = get_item(fHandle, 4);
  pNew->addr    = get_item(fHandle, 4);
  pNew->offset  = get_item(fHandle, 4);
  pNew->size    = get_item(fHandle, 4);
  pNew->link    = get_item(fHandle, 4);
  pNew->info    = get_item(fHandle, 4);
  pNew->align   = get_item(fHandle, 4);
  pNew->entsize = get_item(fHandle, 4);

  if (pNew->entsize == 0) pNew->count = 0;
  else                    pNew->count = pNew->size / pNew->entsize;
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

unsigned int elf_section_load(FILE *fHandle, elf_section_list *section,
                              unsigned int loaded,  unsigned int total)
{
unsigned int fPos, i, j, block_len;
unsigned char byte, *base_addr;
unsigned char addr[MAX_WORD_LENGTH];
unsigned char buf[LOAD_BUFFER_LENGTH];

fPos = ftell(fHandle);
fseek(fHandle, section->offset, SEEK_SET);               /* Find data in file */

base_addr = view_int2chararr(board->memory_ptr_width, section->addr);

i = 0;                                                 /* Position in section */
while (i < section->size)
  {
  block_len = MIN(section->size - i, LOAD_BUFFER_LENGTH);
  for (j = 0; j < block_len; j++) buf[j] = get_item(fHandle, 1);    /* Buffer */
  view_chararrAdd(board->memory_ptr_width, base_addr, i, addr);       /* Yuk! */
  board_set_memory(block_len, addr, buf, 1);          /* Send block to client */
  i = i + block_len;                                               /* Move on */
  loaded = loaded + block_len;                   /* Just for progress monitor */

  gtk_progress_bar_update(GTK_PROGRESS_BAR(view_progressbar),
                               (float) loaded / total);
  while (gtk_events_pending()) gtk_main_iteration();  /* Update display, etc. */
  }

fseek(fHandle, fPos, SEEK_SET);                  /* Restore original position */
return loaded;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define SYM_NAME_MAX  30

void elf_read_sym_table(FILE *fp, elf_section_list *pSection,
                                  elf_section_list *pStrings)
{
int i, k;
boolean acceptable;
unsigned int fPos, offset, value, size, info, section;
char buffer[SYM_NAME_MAX + 1];
symbol_type internal_type;

fseek(fp, pSection->offset, SEEK_SET);

for (i = 0; i < pSection->count; i++)
  {
  offset = get_item(fp, 4);

  fPos = ftell(fp);                                    /* Record where we are */
  fseek(fp, pStrings->offset + offset, SEEK_SET);       /* Go find the string */
  k = 0;
  acceptable = TRUE;
  do
    {
    buffer[k] = get_item(fp, 1);
    if ((buffer[k] != '\0') &&
      ((buffer[k] < '0') || ((buffer[k] > '9') && (buffer[k] < 'A'))
    || ((buffer[k] > 'Z') && (buffer[k] < 'a') && (buffer[k] != '_'))
     || (buffer[k] > 'z')))     /* Filter out `illegal' (e.g. system) strings */
      acceptable = FALSE;
    k++;
    }
    while ((k < SYM_NAME_MAX) && (buffer[k-1] != '\0'));
  buffer[SYM_NAME_MAX] = '\0';                       /* Guaranteed terminator */

  if (buffer[0] =='\0') acceptable = FALSE;            /* Filter null strings */

  fseek(fp, fPos, SEEK_SET);              /* Return to original file position */

  value   = get_item(fp, 4);
  size    = get_item(fp, 4);
  info    = get_item(fp, 1);
  get_item(fp, 1);                                            /* Unused field */
  section = get_item(fp, 2);

  switch (info & 0xF0)
    {
    case 0x00: internal_type = SYM_LOCAL;  break;
    case 0x10: internal_type = SYM_GLOBAL; break;
    case 0x20: internal_type = SYM_WEAK;   break;
    default:   internal_type = SYM_NONE;   break;
    }
  if (acceptable) misc_add_symbol(buffer, value, internal_type);
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void elf_delete_section_list(elf_section_list **pList)
{
elf_section_list *pNext, *pThis;

pNext = *pList;                                              /* Point to list */
*pList = NULL;                                               /* Chop off list */

while (pNext != NULL)                                  /* While records exist */
  {
  pThis = pNext;                                            /* Keep a pointer */
  pNext = pNext->pNext;                                /* Move down the chain */
  free(pThis);                                       /* Free record behind us */
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get an item of `size' bytes from the current position in `fHandle'         */

unsigned int get_item(FILE *fHandle, int size)
{
unsigned int i, j;

for (i = 0, j = 0; i < size; i++) { j = j | fgetc(fHandle) << (8 * i); }

return j;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void read_binary(char *filename, uchar *address)
{
int file;
int rep;
uchar buf[LOAD_BUFFER_LENGTH];
struct stat status;
int sofar;	// Can get rid of this, but need to use different file ops. @@@
boolean error;

sofar = 0;
error = FALSE;
stat(filename, &status);                    /* Find length (for progress bar) */

file = open(filename, O_RDONLY);

while ((rep = read(file, buf, LOAD_BUFFER_LENGTH)) && !error)
  {
  sofar += rep;
  board_set_memory(rep, address, buf, 1);
  if (!board_micro_ping())
    {                              /* if errored with microping then do again */
    board_set_memory(rep, address, buf, 1);
    error = error | !board_micro_ping();
    }

  if (!error)
    {
    view_chararrAdd(board->memory_ptr_width, address, rep, address);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(view_progressbar),
                               (float) sofar / status.st_size);
                                                       /* Update progress bar */

    while (gtk_events_pending()) gtk_main_iteration();
    }                                       /* Update display and do user stuff */
  }

close(file);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

boolean read_sauce(char *filename)
{

  int get_number(FILE *fHandle, char *pC, unsigned int *number)
    {                         /* Read hex number and return #bytes (2^N) also */
    int digit, value, j, k;

    while ((*pC == ' ') || (*pC == '\t')) *pC = getc(fHandle); /* Skip spaces */

    value = 0;
    j = 0;
    while ((digit = check_hex(*pC)) >= 0)
      {
      value = (value << 4) | digit;                       /* Accumulate digit */
      j = j + 1;                               /* Count hex characters parsed */
      *pC = getc(fHandle);                              /* Get next character */
      }               /* Exit at first non-hex character (which is discarded) */

    j = (j + 1) / 2;                            /* Round digit count to bytes */

    if (j == 0) k = 0;                        /* Round bytes up to power of 2 */
    else
      {
      *number = value;                                /* Only if number found */
      if (j > 4) k = 4;				// Currently clips at 32-bit @@@
      else for (k = 1; k < j; k = k << 1);                  /* Round j to 2^N */
      }
    return k;
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  char get_symbol(FILE *fHandle)
    {                  /* Returns last character read (not really important!) */
    #define SYM_BUF_SIZE  64               /* Limit on symbol length imported */
    int allowed(char c) { return (c == '_') || (c >= '0') && (c <= '9')
                          || ((c & 0xDF) >= 'A') && ((c & 0xDF) <= 'Z'); }

    char c, buffer[SYM_BUF_SIZE];
    int i, value, defined;
    symbol_type sym_type;

    do c = getc(fHandle); while ((c == ' ') || (c == '\t'));  /* Strip spaces */

    for (i = 0; allowed(c) && i < (SYM_BUF_SIZE - 1); i++)     /* Copy string */
      { buffer[i] = c; c = getc(fHandle); }
    buffer[i] = '\0';                                            /* Terminate */

    while (allowed(c)) c = getc(fHandle);   /* Discard characters after limit */

    value = 0;          /* A tidy looking default - not functionally relevant */
    defined = get_number(fHandle, &c, &value) > 0;

    while ((c == ' ') || (c == '\t')) c = getc(fHandle);

    if (!defined) sym_type = SYM_UNDEFINED;                  /* Value missing */
    else
      switch (c)				// Bodge - just first character @@
        {
        case 'G': sym_type = SYM_GLOBAL;    break;
        case 'L': sym_type = SYM_LOCAL;     break;
        case 'U': sym_type = SYM_UNDEFINED; break;
        case 'V': sym_type = SYM_EQUATE;    break;
        default:  sym_type = SYM_NONE;      break;
        }
    misc_add_symbol(buffer, value, sym_type);                   /* Set symbol */
    return c;                                 /* ... and discard rest of line */
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

FILE *sauce;
unsigned int address, old_address;
unsigned int d_size[SOURCE_FIELD_COUNT], d_value[SOURCE_FIELD_COUNT];
int i, j, m, digit;
boolean flag, has_old_addr;
int byte_total, text_length;
char c, buffer[SOURCE_TEXT_LENGTH + 1];                 /* + 1 for terminator */
source_line *pNew, *pTemp1, *pTemp2;
struct stat status;

if (TRACE > 5) g_print("read_sauce\n");

if ((sauce = fopen(filename, "r")) == NULL)
  {
  printf("Sauce error!\n");
  return FALSE;
  }
else
  {
  has_old_addr = FALSE;                          /* Don't know where we start */

  stat(filename, &status);

  while (!feof(sauce))                            /* Repeat until end of file */
    {
    address = 0;					// Really needed? @@
    flag = FALSE;                             /* Haven't found an address yet */

    c = getc(sauce);
    if (c == ':')  /* If the first character is a colon, read a symbol record */
      {
      get_symbol(sauce);
      has_old_addr = FALSE;                          /* Don't retain position */
      }
    else                                         /* Read a source line record */
      {
      for (j = 0; j < SOURCE_FIELD_COUNT; j++) {d_size[j] = 0; d_value[j] = 0;}
      byte_total = 0;

      flag = get_number(sauce, &c, &address) != 0;       /* Some digits read? */
      if (flag)                                         /* Read a new address */
        {                        /* If we got an address, try for data fields */
        if (c == ':') c = getc(sauce);                          /* Skip colon */

        for (j = 0; j < SOURCE_FIELD_COUNT; j++)       /* Loop on data fields */
          {
          d_size[j] = get_number(sauce, &c, &d_value[j]);
          if (d_size[j] == 0) break;                 /* Quit if nothing found */
          byte_total = byte_total + d_size[j];                 /* Total input */
          }    /* Repeated a number of times or until `illegal' character met */

        old_address = address + byte_total;       /* Predicted -next- address */
        has_old_addr = TRUE;
        }
      else                                         /* Address field not found */
        if (has_old_addr)                         /* Maybe something useable? */
          {                                             /* Can't contain data */
          address = old_address;                     /* Use predicted address */
          flag = TRUE;                          /* Note we do have an address */
          }

      if (flag)                           /* We have a record with an address */
        {
        while ((c != ';') && (c != '\n') && !feof(sauce)) c = getc(sauce);
        if (c == ';')                            /* Check for field separator */
          {
          c = getc(sauce);
          if (c == ' ') c = getc(sauce);             /* Skip formatting space */

          text_length = 0;                  /* Measure (& buffer) source line */
          while ((c!='\n') && !feof(sauce) && (text_length<SOURCE_TEXT_LENGTH))
            {                          /* Everything to end of line (or clip) */
            buffer[text_length++] = c;
            c = getc(sauce);
            }
          buffer[text_length++] = '\0';  /* text_length now length incl. '\0' */

          pNew = g_new(source_line, 1);                  /* Create new record */
          pNew->address = address;
          pNew->corrupt = FALSE;

          byte_total = 0;				// Again! ... inefficient @@@
          for (j = 0; j < SOURCE_FIELD_COUNT; j++)
            {
            pNew->data_size[j]  = d_size[j];             /* Bytes, not digits */
            pNew->data_value[j] = d_value[j];

            if ((pNew->data_size[j] > 0)
            && ((pNew->data_size[j] + byte_total) <= SOURCE_BYTE_COUNT))
              {					// @@ clips memory load - debatable
              unsigned char addr[4], data[4];
              for (i = 0; i < 4; i++)
                addr[i] = ((address + byte_total) >> (8 * i)) & 0xFF;
              for (i = 0; i < pNew->data_size[j]; i++)
                data[i] = (pNew->data_value[j] >> (8 * i)) & 0xFF;

              board_set_memory(1, addr, data, pNew->data_size[j]);
				// Ignore Boolean error return value for now @@@@@
              }

            byte_total = byte_total + pNew->data_size[j];
            pNew->nodata = (byte_total == 0);  /* Mark record if `blank' line */
            }

//        while (byte_total > SOURCE_BYTE_COUNT)
          if (byte_total > SOURCE_BYTE_COUNT)	// @@ clips source record - essential
            {
            m = 0;
            for (j = 0; j < SOURCE_FIELD_COUNT; j++)
              {
              m = m + pNew->data_size[j];
              if (m <= SOURCE_BYTE_COUNT)
                {
                d_size[j] = 0;
                byte_total = byte_total - pNew->data_size[j];
                }
              else
                {
                d_size[j] = pNew->data_size[j];		// Bytes, not digits
                pNew->data_size[j] = 0;
                }
              }
printf("OVERFLOW %d %d %d %d  %d\n", d_size[0], d_size[1],	// FIX ME! @@@
                                     d_size[2], d_size[3], byte_total);
// Extend with some more records here? @@@ (plant in memory, above,  also)
            }

          pNew->text = g_new(char, text_length);       /* Copy text to buffer */
          for (j = 0; j < text_length; j++) pNew->text[j] = buffer[j];

          pTemp1 = source.pStart;/* Place new record in address ordered list, */
          pTemp2 = NULL;    /*  behind any earlier records with same address. */

          while ((pTemp1 != NULL) && (address >= pTemp1->address))
            {
            pTemp2 = pTemp1;
            pTemp1 = pTemp1->pNext;
            }

          pNew->pNext = pTemp1;
          pNew->pPrev = pTemp2;
          if (pTemp1 != NULL) pTemp1->pPrev = pNew;
          else                source.pEnd   = pNew;
          if (pTemp2 != NULL) pTemp2->pNext = pNew;
          else                source.pStart = pNew;
          }
        }                                                      /* Source line */
      }
    while ((c != '\n') && !feof(sauce)) c = getc(sauce);  /* Next line anyway */

    gtk_progress_bar_update(GTK_PROGRESS_BAR(view_progressbar),
                             (float) ftell(sauce) / status.st_size);
                                                       /* Update progress bar */

//    while (gtk_events_pending()) gtk_main_iteration();
//                                          /* Update display and do user stuff */
//
//    gtk_progress_bar_update(GTK_PROGRESS_BAR(view_progressbar), 1);
//                                                       /* Update progress bar */
    }

  fclose(sauce);
  }
return TRUE;						// Return error flag  @@@
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Compiles a file when compile is pressed                                   */
/*                                                                            */

void callback_button_compile(GtkButton *button, gpointer entry)
{
GList *list;
char *filename;

if (TRACE > 5) g_print("callback_button_compile\n");

if (compile_lock == FREE)                       /* Check if already compiling */
  {							// Busy-wait ?? @@@@
  compile_lock = COMPILE;                                         /* set lock */
  filename = gtk_entry_get_text(GTK_ENTRY(entry));  /* Get ptr; no allocation */
  if (filename[0] != '\0')
    {
    if (!fork())                                           /* If daughter ... */
      {
      close(1);
      dup2(compile_communication[1], 1);
      close(2);
      dup2(compile_communication[1], 2);
      close(0);
      if (VERBOSE) g_print("Compiling: %s\n", filename);
      setpgid(0, 0);
      execlp(compile_script,compile_script,filename,board->cpu_name,"0",NULL);
      _exit(0);
      }
    else
      {                                                      /* If parent ... */
      int i;
      char buffer[200];
      char *pTemp1, *pTemp2;

      maintain_filename_list(entry, filename, TRUE);     /* Log filename used */

      pTemp1 = g_strdup(filename);         /* Translate filename to load name */
      if (pTemp1 != NULL)
        {
        for (i=strlen(pTemp1); (i>0)&&(pTemp1[i]!='/')&&(pTemp1[i]!='.'); i--);
                            /* Reverse search for start of extension - if any */
        if (pTemp1[i] == '.') pTemp1[i] = '\0';/* Trim off any ".*" extension */

        pTemp2 = g_strconcat(pTemp1, OBJECT_EXT, NULL);		// @@@ re-extend properly @@@
        gtk_entry_set_text(GTK_ENTRY(centry_load), pTemp2);
        g_free(pTemp2);
        }
      g_free(pTemp1);
      }
    }
  else g_print("\a");                                   /* Warning of failure */
  compile_lock = FREE;                                        /* free up lock */
  }
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Confirms a file choice.                                                    */
/*                                                                            */

void callback_button_ok_file(GtkButton *button, gpointer entry)
{                                /* on fileentry ok button send to text entry */
char *text;

if (TRACE > 5) g_print("callback_button_ok_file\n");

text = gtk_file_selection_get_filename(GTK_FILE_SELECTION
                                  (GTK_WIDGET(button)->parent->parent->parent));
gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);          /* clear entry */
gtk_entry_append_text(GTK_ENTRY(entry), text);             /* insert new text */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void set_fpga_text(char *name, gpointer container, uchar *data)
{
gtk_label_set_text((GTK_LABEL(gtk_object_get_data(
                     GTK_OBJECT(container), name))), data);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Check output Xilinx bit file header                                        */
/* Returns TRUE if no errors found                                            */

boolean FPGA_header(uchar *buffer, gpointer container, unsigned int *size, int load)
{
int position;
char *name;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("FPGA_header\n");

if (buffer[0] != 0) return (0 == 1);                        /* WHAT? WHY? @@@ */

position = 1;                                 /* Pointer to length of ???     */
position = position + buffer[position] + 5;                 /* Skip ??? field */
name = &buffer[position + 1];                       /* Pointer to design name */
position = position + buffer[position] + 3;  /* Skip design name field et al. */

/* position now points to the length of the device identifier string (I think)*/
/* The identifier string starts at the next address                           */
if (g_strcasecmp(board->pArray_F[
          GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(container), "feature"))
                                  ].data.xilinx_fpga.filestring,
                 &buffer[position + 1]) != 0)
        /* Compare device identifiers */
  return (0 == 1);                                /* Zero means strings match */

/* if (VERBOSE) g_print("%d\n", rep); rep was No. bytes read from file */

if (load) set_fpga_text("labelfpganame", container, name);     /* Design name */
else      set_fpga_text("labelfilename", container, name);

position = position + buffer[position] + 3;
/* Skip device identifier string field et al. */

if (load) set_fpga_text("labelfpgadate",container,&buffer[position+1]); /*date*/
else      set_fpga_text("labelfiledate",container,&buffer[position+1]);

position = position + buffer[position] + 3;    /* Skip date string field etc. */

if (load) set_fpga_text("labelfpgatime",container,&buffer[position+1]); /*time*/
else      set_fpga_text("labelfiletime",container,&buffer[position+1]);

position = position + buffer[position] + 2;    /* Skip time string field etc. */

*size = (buffer[position] << 24) + (buffer[position + 1] << 16)
  + (buffer[position + 2] << 8) + buffer[position + 3] + position + 4;
/* Total length of file */
return (0 == 0);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Reset selected FPGA by sending `zero length' header                        */

void callback_fpgaerase(GtkButton *button, gpointer container)
{
if (fpga_lock == FREE)                             /* to avoid multiple loads */
  {
  int featureentry;
  char ch;

  if (TRACE > 5) g_print("callback_fpgaerase\n");
  fpga_lock = FPGA_LOAD;                                          /* Set lock */

  featureentry = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(container),
                                               "feature")); /* Which feature? */

  ch = download_send_header(featureentry, 0);       /* Ignore what reply says */

  set_fpga_text("labelfpganame", container, "< reset >");      /* Design name */
  set_fpga_text("labelfpgadate", container, "----/--/--");
  set_fpga_text("labelfpgatime", container, "--:--:--");

  fpga_lock = FREE;                                           /* Release lock */
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Complete download sequence to selected FPGA                                */

void callback_fpgaload(GtkButton *button, gpointer container)
{
if (fpga_lock == FREE)                             /* to avoid multiple loads */
  {
  char *filename;
  GList *list;
  FILE *file;
  unsigned int length_read, position, pkt_length;
  unsigned int size, originalsize;
  char ch;
  int first_block, terminate, error;
  int featureentry;
  uchar data[FPGA_LOAD_BLK_LENGTH];          /* the data buffer of a download */

  if (TRACE > 5) g_print("callback_fpgaload\n");

  error = 0;                                             /* Indicates anomaly */
  originalsize = 0;                              /* Default, in case of error */

  filename = gtk_entry_get_text(GTK_ENTRY             /* No memory allocation */
                                (gtk_object_get_data
                                 (GTK_OBJECT(container), "fileentry")));

  fpga_lock = FPGA_LOAD;                                          /* Set lock */
  featureentry = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(container),
                                                    "feature"));
  if ((filename[0] != '\0') && ((file = fopen(filename, "r")) != NULL))
    {
    first_block = (0 == 0);
    terminate   = (0 == 1);

    while (!terminate                                              /* TEST@@@ */
       && ((length_read = fread(data, 1, FPGA_LOAD_BLK_LENGTH, file)) > 0))
      {
      if (first_block)             /* Check device compatibility; send header */
        {
        if ((length_read==FPGA_LOAD_BLK_LENGTH)/* First block must be full(?!)*/
          && FPGA_header(data, container, &size, 0 == 0))
          {
          if (VERBOSE) g_print("size:%x\n", size);
          originalsize = size;
          ch = download_send_header(featureentry, size);
          if ('A' != ch) terminate = (0 == 0);                /* Ack. failure */
          first_block = (0 == 1);
          }
        else
          {
          terminate = (0 == 0);                         /* Some sort of error */
          error = 1;                          /* So won't save filename below */
          }
        }

      for (position = 0; (position < length_read) && !terminate;
                          position = position + FPGA_LOAD_PKT_LENGTH)
        {
        pkt_length = MIN(size, FPGA_LOAD_PKT_LENGTH);
        if (pkt_length != 0)               /* "size" is no of bytes remaining */
          {
          ch = download_send_packet(featureentry, pkt_length, &data[position]);
          if ('A' != ch) terminate = (0 == 0);            /* Ack. from client */
          size = size - pkt_length;
          }
        if (size == 0) terminate = (0 == 0);       /* Should terminate anyway */
        }

      /* Intersperse loading with other services */
      if (!board_micro_ping()) terminate = (0 == 0);
             /* if errored with microping then do again (NOT MY COMMENT! @@@) */

      if (originalsize != 0)                              /* In case of error */
        gtk_progress_bar_update(GTK_PROGRESS_BAR(
                                gtk_object_get_data(GTK_OBJECT(container),
                                                    "progressbar")),
                                                     1-(float)size/originalsize);
                                                       /* update progress bar */

      while (gtk_events_pending()) gtk_main_iteration();
                                          /* update display and do user stuff */
      }

    if (error == 0)                                   /* i.e. No errors above */
      maintain_filename_list(container, filename, FALSE);
    // Have seen popdown get `small' after second call (different path).
    // Undiagnosed    @@@

//    list = gtk_object_get_data(GTK_OBJECT(container), "list");
//                                 /* append filename to list of past filenames */
//
//    if (!g_list_find_custom(list, filename, (GCompareFunc) g_strcasecmp))
//      list = g_list_prepend(list, g_strdup(filename));
//
//    gtk_object_set_data(GTK_OBJECT(container), "list", list);
//    gtk_combo_set_popdown_strings(GTK_COMBO(gtk_object_get_data
//                                    (GTK_OBJECT(container), "combo")), list);

    fclose(file);
    }
  fpga_lock = FREE;                                           /* Release lock */
  }                                // end locking

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Load button pressed                                                        */
/* Read the header information from a Xilinx bit file                         */
/* Display some of it (?)                                                     */
/*                                                                            */

void callback_fpgaupdate(GtkButton *button, gpointer container)
{
char *filename;
uchar data[FPGA_HEADER_BUFFER];
FILE *file;
unsigned int size;

if (TRACE > 5) g_print("callback_fpgaupdate\n");

/* Get filename */
filename = gtk_entry_get_text(GTK_ENTRY(gtk_object_get_data( /* No allocation */
                              GTK_OBJECT(container), "fileentry")));

if (*filename != '\0')                                    /* If real filename */
  {
  file = fopen (filename, "r");                                  /* Open file */
  if (file != NULL)
    {
    if (fread(data, 1, FPGA_HEADER_BUFFER, file) == FPGA_HEADER_BUFFER)
       /* Get first few bytes from file; quit if not enough bytes read (WHY?) */
      FPGA_header(data, container, &size, 0 == 1);
    fclose(file);
    }
  }
return;
}

/*----------------------------------------------------------------------------*/
/* Set the state of the client display "Refresh" function                     */

void set_refresh(boolean running, int timer_ref)
{
if (TRACE > 15) g_print("set_refresh\n");

if (view_global_refresh_timer != 0)
  gtk_timeout_remove(view_global_refresh_timer);
view_global_refresh_timer = timer_ref;
board_running = running;                          /* Internal record of state */
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view_refreshbutton), running);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Start button pressed - either from panel or from menu entry                */

void callback_button_start(GtkButton *button, gpointer steps)
{
if (TRACE > 5) g_print("callback_button_start\n");

was_walking = FALSE;
if (run_board(steps)) set_refresh(TRUE, 0);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when stop is chosen in the cases as above                           */

void callback_button_stop(GtkButton *button, gpointer ignore)
{
if (TRACE > 5) g_print("callback_button_stop\n");

stop_board();
gtk_label_set_text(GTK_LABEL(view_enqlabel), "Stopped");
set_refresh(FALSE, 0);                                /* unset refresh button */
callback_refresh_selection(TRUE, TRUE, TRUE);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when continue is chosen in the cases as in 'start'                  */

void callback_button_continue(GtkButton *button, gpointer ignore)
{
unsigned int steps_left;

if (TRACE > 5) g_print("callback_button_continue\n");

if (was_walking)
  {
  if (board_enq(&steps_left) == CLIENT_STATE_STOPPED) /* Check why we stopped */
    {
    if (steps_left > 1) steps_left--;// @@@ HACK: don't know where extra 1 is from
    walk_board(steps_left, 1);           /* walk on - may break on first step */
    }
  else
    continue_board();

  gtk_label_set_text(GTK_LABEL(view_enqlabel), "Walking");
  set_refresh(FALSE, g_timeout_add(view_step_freq, callback_walk,
             (gpointer) &view_step_number));
  callback_global_refresh();                               /* refresh display */
  }
else
  {
  continue_board();
  set_refresh(TRUE, 0);
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void callback_start_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
int button_state;

if (TRACE > 5) g_print("callback_start_toggle\n");

change_start_command(gtk_toggle_button_get_active(togglebutton),
                     GPOINTER_TO_INT(user_data));
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Called when service to interrupts is enabled or disabled                  */
/*  user_data is 0x2 if called from IRQ                                       */
/*  user_data is 0x1 if called from FIQ                                       */
/*                                                                            */

// Could take some more flags @@@

void callback_rtf_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
if (TRACE > 5) g_print("callback_rtf_toggle\n");

set_interrupt_service(gtk_toggle_button_get_active(togglebutton),
                      GPOINTER_TO_INT(user_data));
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Currently does nothing, but the state of the toggle button is inspected   */
/*  in other functions to enable the right behaviour                          */
/*                                                                            */

void callback_refresh_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
if (TRACE > 5) g_print("callback_refresh_toggle\n");
return;                      /* If turned off should we do screen update? @@@ */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* called when toggle refresh selected from menu entry                        */

void callback_refresh_toggle_from_menu(gpointer ignore)
{
if (TRACE > 5) g_print("callback_refresh_toggle_from_menu\n");

gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view_refreshbutton),
                            !gtk_toggle_button_get_active
                            (GTK_TOGGLE_BUTTON(view_refreshbutton)));
                                     /* toggle by switching to opposite state */
return;
}
/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Called when the ping button is pressed or its entry in the menu is        */
/*  activated                                                                 */
/*                                                                            */

void callback_button_ping(GtkButton *ignore, gpointer ignore2)
{
unsigned int dummy;

if (TRACE > 5) g_print("callback_button_ping\n");

if (board_mini_ping())                                      /* Ping the board */
  {
  board_enq(&dummy);        /* Get and display the current state of the board */
  callback_global_refresh();         /* Refresh all memory and register lists */
  }
else
  if (VERBOSE) g_print("Naughty change of backend system!\n");		// @@@

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* reset button pressed or menu entry activated                               */

void callback_button_reset(GtkButton *ignore, gpointer ignore2)
{
unsigned int dummy;

if (TRACE > 5) g_print("callback_button_reset\n");

reset_board();
board_micro_ping();
set_refresh(FALSE, 0);                                /* Unset refresh button */
board_micro_ping();                                        /* Why TWICE?? @@@ */
board_enq(&dummy);                          /* Update client behaviour report */

callback_global_refresh();                                     /* Refresh all */
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Refresh display of indicated window types                                 */
/*                                                                            */

void callback_refresh_selection(int memory, int registers, int status)
{
unsigned int dummy;

if (TRACE > 3) g_print("callback_refresh_selection\n");

if (registers) callback_register_refresh();
if (memory)    callback_memory_refresh();
if (status)    board_enq(&dummy);                    // over-heavy (?) @@@
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Refresh all the memory lists and register banks                           */
/*                                                                            */

void callback_global_refresh(void)
{
if (TRACE > 3) g_print("callback_global_refresh\n");
callback_refresh_selection(TRUE, TRUE, FALSE);
return;
}

/*----------------------------------------------------------------------------*/
/*  Refresh all the memory lists one by one                                   */
/*                                                                            */

void callback_memory_refresh(void)
{
mem_window_entry *mlist;

if (TRACE > 5) g_print("callback_memory_refresh\n");

mlist = view_memwindowlist;                     /* Get list of memory windows */
while (NULL != mlist)
  {
  view_updatememwindow(mlist->mem_data_ptr);                      /* Refresh  */
  mlist = mlist->next;                                     /*  and step ahead */
  }

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Refresh all the register lists                                             */
/*                                                                            */

void callback_register_refresh(void)
{
reg_window_entry *rlist = view_regwindowlist;   /* get list of register banks */
int regbank;                                     /* index to hold the current */

if (TRACE > 5) g_print("callback_register_refresh\n");

for (regbank = 0; regbank < board->num_regbanks; regbank++)  // WHY? @@@ HERE?!?
  if (1 == board->reg_banks[regbank].pointer)                /* if pointer==1 */
    if (!board_get_regbank(regbank))            /* if unable to get reg. bank */
      {
      if (!board_mini_ping() || !board_get_regbank(regbank)) return;
                               /* if seems like access is lost, abort refresh */
      }

while (NULL != rlist)
  {
  view_updateregwindow(rlist->reg_data_ptr);                      /* Refresh  */
  rlist = rlist->next;                                     /*  and step ahead */
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Called when the user changes the step number for multistep                */
/*                                                                            */

void callback_step_number(GtkEditable *editable, gpointer value)
{
if (TRACE > 5) g_print("callback_step_number\n");

*((int*) value) = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editable));
                                      /* Change the step number for multistep */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* A new representation for memory has been picked and new settings need to   */
/* be taken into consideration                                                */
/* The first function is just a wrapper                                       */

void callback_memwindow_length2(GtkMenuItem *menuitem, gpointer p_newvalue)
{
callback_memwindow_length(view_getmemwindowptr(GTK_WIDGET(menuitem)),p_newvalue);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void callback_memwindow_length(mem_window *memwin, gpointer p_newvalue)
                                            /* The line width and granularity */
{
typedef enum {OFF, ON, ASIS} button_state;

int temp, newvalue;
int max_hex_column;


  void waggle_buttons(button_state ascii, button_state disassembly)
  {                                    /* This relies on C-style Booleans :-( */
  if (memwin->ascii_button != NULL)
    switch (ascii)
      {
      case OFF:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(memwin->ascii_button),FALSE);
        break;
      case ON:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(memwin->ascii_button), TRUE);
        break;
      default: break;                                  /* Leave setting "As is" */
      }

  if (memwin->dis_button != NULL)
    switch (disassembly)
      {
      case OFF:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(memwin->dis_button),FALSE);
        break;
      case ON:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(memwin->dis_button), TRUE);
        break;
      default: break;                                  /* Leave setting "As is" */
      }

  return;
  }

if (TRACE > 5) g_print("callback_memwindow_length\n");

/***** The ASCII entry and dis. will have to check state of column visibility */
/*  and callback the ASCII and dis. if true                                   */

newvalue = *(int*)p_newvalue;                        /* Isn't C syntax great? */

switch (newvalue)
  {
  /* Following cases bind a new values to reflect the representation and make */
  /*  sure the appropriate callbacks are invoked w.r.t. ASCII and disassembly */

  case MEM_REP_BYTE:
    memwin->width = 1;
    memwin->gran  = 1;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_2_BYTES:
    memwin->width = 2;
    memwin->gran  = 1;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_4_BYTES:
    memwin->width = 4;
    memwin->gran  = 1;
    waggle_buttons(ON, ASIS);
    break;
  case MEM_REP_8_BYTES:
    memwin->width = 8;
    memwin->gran  = 1;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_16_BYTES:
    memwin->width = 16;
    memwin->gran  = 1;
    waggle_buttons(OFF, OFF);
    break;
  case MEM_REP_32_BYTES:
    memwin->width = 32;
    memwin->gran  = 1;
    waggle_buttons(OFF, OFF);
    break;
  case MEM_REP_HALFWORD:
    memwin->width = 2;
    memwin->gran  = 2;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_2_HALFWORDS:
    memwin->width = 4;
    memwin->gran  = 2;
    waggle_buttons(ON, ASIS);
    break;
  case MEM_REP_4_HALFWORDS:
    memwin->width = 8;
    memwin->gran  = 2;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_8_HALFWORDS:
    memwin->width = 16;
    memwin->gran  = 2;
    waggle_buttons(OFF, OFF);
    break;
  case MEM_REP_16_HALFWORDS:
    memwin->width = 32;
    memwin->gran  = 2;
    waggle_buttons(OFF, OFF);
    break;
  case MEM_REP_WORD:
    memwin->width = 4;
    memwin->gran  = 4;
    waggle_buttons(ON, ON);
    break;
  case MEM_REP_2_WORDS:
    memwin->width = 8;
    memwin->gran  = 4;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_4_WORDS:
    memwin->width = 16;
    memwin->gran  = 4;
    waggle_buttons(OFF, OFF);
    break;
  case MEM_REP_8_WORDS:
    memwin->width = 32;
    memwin->gran  = 4;
    waggle_buttons(OFF, OFF);
    break;
  case MEM_REP_ASCII_MAP:
    memwin->width = 64;
    memwin->gran  = 1;
    waggle_buttons(ON, OFF);
    break;
  case MEM_REP_ASCII_MAP128:
    memwin->width = 128;
    memwin->gran  = 1;
    waggle_buttons(ON, OFF);
    break;
  default:
    if (VERBOSE) g_print("Error: non-existent memory representation.\n");
    memwin->width = 4;
    memwin->gran  = 4;
    waggle_buttons(ON, ON);
    break;
  }

max_hex_column = memwin->width / memwin->gran;

gtk_clist_freeze(GTK_CLIST(memwin->clist_ptr));
                                            /* Freeze the current column list */

if ((newvalue == MEM_REP_ASCII_MAP) || (newvalue == MEM_REP_ASCII_MAP128))
                               /* Abnormal settings for this exceptional case */
  {
  for (temp = MAX_HEX_ENTRY; temp >= MIN_HEX_ENTRY; temp--)
                                                    /* All hex to be disabled */
    gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr), temp, FALSE);
  if (memwin->hex_entry != NULL) gtk_widget_hide(memwin->hex_entry);
                                                        /* Hide hex entry box */
  if (memwin->ascii_entry != NULL)
    gtk_widget_set_usize(memwin->ascii_entry,450,-2);/* Set ASCII column size */
  }
else                                             /* Normal hex representation */
  {
  if (memwin->hex_entry != NULL) gtk_widget_show(memwin->hex_entry);
                    /* Display hex entry box in case of return from ASCII map */

  for (temp = MAX_HEX_ENTRY; temp >= MIN_HEX_ENTRY; temp--)
    if (temp > max_hex_column)
      gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr), temp,FALSE);
                            /* Disable all columns that need not be displayed */
    else
      {  /* From top hex column e.g. 4 for 4th hex, where 0 is address column */
      gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr), temp, TRUE);
                                  /* Make hex columns visible where necessary */
      gtk_clist_set_column_width(GTK_CLIST(memwin->clist_ptr), temp,
                            (memwin->width*25)/(max_hex_column)-7);
                                                      /*  and set their width */
      }

  if (memwin->hex_entry != NULL)                 /* Set size of hex entry box */
    gtk_widget_set_usize(memwin->hex_entry, (memwin->width * 25), -2);

  gtk_clist_set_column_width(GTK_CLIST(memwin->clist_ptr), ASCII_ENTRY,
                             memwin->width * 10); /* Set width of ASCII entry */
  if (memwin->ascii_entry != NULL)             /* Set size of ASCII entry box */
    gtk_widget_set_usize(memwin->ascii_entry, (memwin->width * 10) + 7, -2);
  }

gtk_clist_thaw(GTK_CLIST(memwin->clist_ptr));
                                  /* Display the list as it was newly defined */
view_updatememwindow(memwin);                /* update the contents displayed */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* A new ISA description has been chosen from the selection menu e.g MIPS     */
/*                                                                            */

void callback_memwindow_isa(GtkMenuItem *menuitem, gpointer newvalue)
{
mem_window *memwin;               /* will be pointer to current memory window */

if (TRACE > 5) g_print("callback_memwindow_isa\n");

memwin = view_getmemwindowptr(GTK_WIDGET(menuitem));
memwin->isa = (GList *) newvalue;                       /* save the new value */
view_updatememwindow(memwin);  /* update the memory window that is pointed to */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Change number of rows - a new value has now been selected                  */
/* to show more memory entries                                                */

void callback_memwindow_size_allocate(GtkCList *clist, gpointer ignore)
{
int new_number_of_rows;
mem_window *memwin;               /* Will be pointer to current memory window */

if (TRACE > 5) g_print("callback_memwindow_size_allocate\n");
memwin = view_getmemwindowptr(GTK_WIDGET(clist));

                                            /* bit of calculation required... */
new_number_of_rows = (clist->clist_window_height / clist->row_height) - 1;
						// This is too many in large window @@@

gtk_clist_freeze(GTK_CLIST(memwin->clist_ptr));
                               /* Freeze the current state of the memory list */

while (memwin->count < new_number_of_rows)            /* add rows as required */
  gtk_clist_insert(GTK_CLIST(memwin->clist_ptr),++memwin->count,mem_column_data);

while (memwin->count > new_number_of_rows)     /*  or remove rows as required */
  gtk_clist_remove(GTK_CLIST(memwin->clist_ptr), --memwin->count);

view_updatememwindow(memwin);                            /* update the window */
gtk_clist_thaw(GTK_CLIST(memwin->clist_ptr));      /* refresh the clist state */

return;
}

 
//void callback_memwindow_listsize(GtkEditable *editable, gpointer ignore)
//{
//int new_number_of_rows;
//mem_window *memwin;               /* Will be pointer to current memory window */
//
//if (TRACE > 5) g_print("callback_memwindow_listsize\n");
//
//new_number_of_rows = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editable));
//                                                   /* Retrieve number of rows */
//memwin = view_getmemwindowptr(GTK_WIDGET(editable));
//   /* Get a pointer to the memory window currently requiring new num. of rows */
//gtk_clist_freeze(GTK_CLIST(memwin->clist_ptr));
//                               /* Freeze the current state of the memory list */
//
//while (memwin->count < new_number_of_rows)            /* add rows as required */
//  gtk_clist_insert(GTK_CLIST(memwin->clist_ptr),++memwin->count,mem_column_data);
////  gtk_clist_insert(GTK_CLIST(memwin->clist_ptr), ++memwin->count, startdata);
//
//while (memwin->count > new_number_of_rows)     /*  or remove rows as required */
//  gtk_clist_remove(GTK_CLIST(memwin->clist_ptr), --memwin->count);
//
//view_updatememwindow(memwin);                            /* update the window */
//gtk_clist_thaw(GTK_CLIST(memwin->clist_ptr));      /* refresh the clist state */
//}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Change number spaces tabbed when disassembling in source window            */
/* to show more memory entries                                                */

void callback_memwindow_tabsize(GtkEditable *editable, gpointer ignore)
{

if (TRACE > 5) g_print("callback_memwindow_tabsize\n");

source_dis_tabs = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editable));
                                                   /* Retrieve number of rows */
if (source_dis_tabs < 0) source_dis_tabs = 0;                /* Shouldn't be! */
source_dis_tabs = MIN(source_dis_tabs, DIS_TAB_MAX);

callback_memory_refresh(); /* Update all memory windows as variable is global */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when check-box for ELF symbols is pressed                           */
/*                                                                            */

void callback_memwindow_elf_symbols_toggle(GtkToggleButton *tbutton,
                                           gpointer ignore)
{
mem_window *memwin;

if (TRACE > 5) g_print("callback_memwindow_elf_symbols_toggle\n");

memwin = view_getmemwindowptr(GTK_WIDGET(tbutton));
                                          /* pointer to current memory window */
view_updatememwindow(memwin);
   /* update the memory window in case symbols should be retrieved or not now */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Display/hide ASCII column                                                  */
/* Called when relevant toggle buttons are pressed                            */

void callback_memwindow_ascii_toggle(GtkToggleButton *tbutton, gpointer parameter)
{
mem_window *memwin;

if (TRACE > 5) g_print("callback_memwindow_ascii_toggle\n");

memwin = view_getmemwindowptr(GTK_WIDGET(tbutton));
                                          /* pointer to current memory window */

if (gtk_toggle_button_get_active(tbutton))              /* currently inactive */
  {
       //  gtk_widget_show(parameter);
       // Change to this later (if poss.) then incorporate next proc too @@@
  if (memwin->ascii_entry != NULL) gtk_widget_show(memwin->ascii_entry);
  gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr),
                                  ASCII_ENTRY, TRUE);
  }
else                                                       /* already active */
  {
  if (memwin->ascii_entry != NULL) gtk_widget_hide(memwin->ascii_entry);
  gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr),
                                  ASCII_ENTRY, FALSE);
  }
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Display/hide Disassembly column                                            */
/* Called when relevant toggle buttons are pressed                            */

void callback_memwindow_dis_toggle(GtkToggleButton *tbutton, gpointer ignore)
{
mem_window *memwin;

if (TRACE > 5) g_print("callback_memwindow_dis_toggle\n");

memwin = view_getmemwindowptr(GTK_WIDGET(tbutton));
/* pointer to current memory window */

if (gtk_toggle_button_get_active(tbutton))              /* currently inactive */
  {
  gtk_widget_show(memwin->dis_entry);		// Protection not needed ??@@@@
  gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr),
                                  DIS_ENTRY, TRUE);
  }
else                                                        /* already active */
  {
  gtk_widget_hide(memwin->dis_entry);
  gtk_clist_set_column_visibility(GTK_CLIST(memwin->clist_ptr),
                                  DIS_ENTRY, FALSE);
  }
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when memory address increment button state is changed.              */

void callback_memwindow_inc_toggle(GtkToggleButton *tbutton, gpointer ignore)
{
if (TRACE > 5) g_print("callback_memwindow_inc_toggle\n");
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when full source button state is changed.                           */

void callback_memwindow_full_toggle(GtkToggleButton *tbutton, gpointer ignore)
{

if (TRACE > 5) g_print("callback_memwindow_full_toggle\n");

view_updatememwindow(view_getmemwindowptr(GTK_WIDGET(tbutton))); /* & refresh */

return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a new address is entered in a memory frame                     */
/* Inputs: editable which is the address entry                                */
/*                                                                            */

void callback_memwindow_address(GtkEditable *editable, gpointer ignore)
{
mem_window *memwin;
char *text;

if (TRACE > 5) g_print("callback_memwindow_address\n");

memwin = view_getmemwindowptr(GTK_WIDGET(editable));    /* New address passed */
text = gtk_editable_get_chars(editable, 0, -1);
strcpy(memwin->address_string, text);                    /* Keep our own copy */
memwin->addr_type = MEM_WINDOW_EXPR;                  /* Note something typed */
g_free(text);
view_updatememwindow(memwin);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get "address"ed element onto display                                       */
/* inputs: the address to according to which the scroll is carried out and    */
/* the memory window pointer                                                  */
/*                                                                            */

void scroll_display(mem_window *memwin, uchar *address)
{
int next_addr_offset;

if (TRACE > 5) g_print("scroll_display\n");

next_addr_offset = misc_chararr_sub_to_int(board->memory_ptr_width, address,
                                           memwin->address);
/* Distance from top of window */

if (next_addr_offset < 0)                                 /* If above display */
  {
  next_addr_offset = 0;
  view_chararrCpychararr(board->memory_ptr_width, address, memwin->address);
  memwin->addr_type = MEM_WINDOW_DEFAULT;
  }
else
  if (next_addr_offset >= memwin->count * memwin->width)
    {                      /* if passing end of window then scroll one foward */
    next_addr_offset = (memwin->count * memwin->width) - memwin->width;
    view_chararrAdd(board->memory_ptr_width, address, -next_addr_offset,
                    memwin->address);
    /* Scroll display */
    memwin->addr_type = MEM_WINDOW_DEFAULT;
    }
/* ALIGNMENT ?? @@@                    */

gtk_clist_select_row(GTK_CLIST(memwin->clist_ptr),
                     next_addr_offset / memwin->width,
                    (next_addr_offset % memwin->width) / memwin->gran + 1);
          /* highlight the appropriate row now  " + 1" to skip address column */
return;
}

/*----------------------------------------------------------------------------*/
/* Local utility routine                                                      */

void maybe_step_addr(mem_window *memwin, uchar *address, int increment)
{
if (view_check_button(memwin->inc_button))
  {                                               /* Optionally, step address */
  view_chararrAdd(board->memory_ptr_width, address, increment, address);
  scroll_display(memwin, address);                   /* Rejustify if required */
  }

return;
}

/*----------------------------------------------------------------------------*/

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a new value is entered in the hex entry                        */
/* inputs: the hex entry                                                      */
/*                                                                            */

void callback_memwindow_hex(GtkEditable *editable, gpointer ignore)
{
mem_window *memwin;
char *address_string;
uchar *win_start_address;
uchar *address;
char *hex_string;
uchar *hex;

if (TRACE > 5) g_print("callback_memwindow_hex\n");

memwin = view_getmemwindowptr(GTK_WIDGET(editable));
address = g_new(uchar, board->memory_ptr_width);
address_string=gtk_editable_get_chars(GTK_EDITABLE(memwin->address_entry),0,-1);

if (view_hexstr2chararr(board->memory_ptr_width, address_string, address))
                                                    /* Get address from entry */
  {
  if (board->wordalign) address[0] &= -(memwin->gran);          /* Word align */
  hex = g_new(uchar, memwin->gran);
  hex_string = gtk_editable_get_chars(GTK_EDITABLE(memwin->hex_entry), 0, -1);

  if (view_hexstr2chararr(memwin->gran, hex_string, hex))
    {
    board_set_memory(1, address, hex, memwin->gran);
    maybe_step_addr(memwin, address, memwin->gran);
    gtk_entry_select_region(GTK_ENTRY(memwin->hex_entry), 0, -1);
                                                      /* Select all of region */
    callback_memory_refresh();                  /* Refresh all memory windows */
    }
  g_free(hex_string);
  g_free(hex);
  }
g_free(address_string);
g_free(address);

while (gtk_events_pending()) gtk_main_iteration();          /* Update display */
                                                             /* NEEDED?? @@@  */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Takes ASCII from editable and displays hex in the relevant region          */
/*                                                                            */

void callback_memwindow_ascii(GtkEditable *editable, gpointer ignore)
{
mem_window *memwin;
char *ascii_from_entry, *temp;
int ascii_entry_size;
uchar address[board->memory_ptr_width];
                                  /* unsigned to hold to dest. memory address */

if (TRACE > 5) g_print("callback_memwindow_ascii\n");

memwin = view_getmemwindowptr(GTK_WIDGET(editable));

if (memwin->ascii_entry != NULL)                          /* else just return */
  {
  ascii_from_entry = 
    gtk_editable_get_chars(GTK_EDITABLE(memwin->ascii_entry), 0, -1);
                                        /* get ASCII entry - allocates memory */

  ascii_entry_size = 0;
  while (ascii_from_entry[ascii_entry_size] != '\0') ascii_entry_size++;
                                                /* Find length of ASCII entry */

  temp = gtk_editable_get_chars(GTK_EDITABLE(memwin->address_entry), 0, -1);
  view_hexstr2chararr(4, temp, address);                    /* memory address */
  g_free(temp);

  board_set_memory(ascii_entry_size,address,ascii_from_entry,1);/* Set memory */
  maybe_step_addr(memwin, address, ascii_entry_size);
  gtk_entry_select_region(GTK_ENTRY(memwin->ascii_entry), 0, -1);
                                                     /* Select all of region  */

// @@@ "gtk_entry_select_region"(?) may fail to select correct address
//      at >15 bytes (sort of ... use wide ASCII display to try) @@@

  callback_memory_refresh();
  g_free(ascii_from_entry);
  }

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a line of assembler text is entered.                           */
/*                                                                            */

void callback_memwindow_dis(GtkEditable *editable, gpointer ignore)
{
mem_window *memwin;
char *address_entry, *mnemonic;
uchar *address;                                  /* Address as array of bytes */
char buffer[MAX_OBJECT_LENGTH];                    /* Buffer for object codes */
int length;                              /* Size of object code or error code */

if (TRACE > 5) g_print("callback_memwindow_dis\n");

memwin = view_getmemwindowptr(GTK_WIDGET(editable));
                                           /* Find relevant window descriptor */
address_entry=gtk_editable_get_chars(GTK_EDITABLE(memwin->address_entry),0,-1);
address = g_new(uchar, board->memory_ptr_width);

if (view_hexstr2chararr(board->memory_ptr_width, address_entry, address))
                                                    /* Get address from entry */
  {
  if (board->wordalign) address[0] &= -(memwin->gran);  /* Align, if required */

  mnemonic = gtk_editable_get_chars(GTK_EDITABLE(memwin->dis_entry), 0, -1);
                                                              /* Get mnemonic */

  length = asm_assemble_wrapper(mnemonic, memwin->isa,
                                address, board->memory_ptr_width,
                                MAX_OBJECT_LENGTH, buffer, VERBOSE);

  if (length < 0)
    {
    g_print("\a");                                          /* Any error code */
    if (length == -2) g_print("Object too long\n");  // Should be #DEFINED @@@
    }
  else
    if (length == memwin->gran)      /* If assembler returned correct # bytes */
      {                     // Won't work for variable length instructions @@@ 
      board_set_memory(1, address, buffer, memwin->gran);       /* Set memory */
      maybe_step_addr(memwin, address, memwin->gran);
      gtk_editable_select_region(GTK_EDITABLE(memwin->dis_entry), 0, -1);
                                                      /* Select all of region */

      if (gtk_events_pending() > 2)
        while (gtk_events_pending()) gtk_main_iteration();  /* Update display */
      else
        callback_memory_refresh();                             /* Refresh all */
      }

  g_free(mnemonic);
  }
g_free(address);
g_free(address_entry);
return;
}


//{
//mem_window *memwin;
//char *address_entry, *mnemonic;
////int row;
//uchar *address; /* Address as array of bytes (inconsistent - passed as int)@@@*/
//Asm *asmed;                  /* Used to indicate the first (preferred) result */
//GList *asmlist;                    /* Records of possible return object codes */
//
//if (TRACE > 5) g_print("callback_memwindow_dis\n");
//
//memwin = view_getmemwindowptr(GTK_WIDGET(editable));
//                                           /* Find relevant window descriptor */
//address_entry =
//  gtk_editable_get_chars(GTK_EDITABLE(memwin->address_entry), 0, -1);
//address = g_new(uchar, board->memory_ptr_width);
//
//if (view_hexstr2chararr(board->memory_ptr_width, address_entry, address))
//                                                    /* Get address from entry */
//  {
//  if (board->wordalign) address[0] &= -(memwin->gran);  /* Align, if required */
//
//  mnemonic = gtk_editable_get_chars(GTK_EDITABLE(memwin->dis_entry), 0, -1);
//                                                              /* Get mnemonic */
//
//  asmlist = asm_assemble(mnemonic, memwin->isa,
//                         view_chararr2int(board->memory_ptr_width, address), 1);
//
//  if (asmlist != NULL)
//    {
//    asmed = (Asm *) (asmlist->data);         /* Get first (only?) return data */
//    if (asmed->binary != &dis_error)                          /* If not error */
//      {
//      uchar *hex;                       /* Pointer to assembler return record */
//      int temp;
//
//      temp = asm_sbprint(asmed->binary, &hex);  /* Convert bit list to char[] */
//                                  /* This leaves an allocated record at `hex' */
//      if ((asmlist->next != NULL) && VERBOSE)
//        g_print("Multiple possible assemblies, size:%d", g_list_length(asmlist));
//
//      while (asmlist != NULL)                     /* Free returned lists etc. */
//        {
//        asm_freestringlist(((Asm *) asmlist->data)->binary);
//        g_free(asmlist->data);
//        asmlist = g_list_remove(asmlist, asmlist->data);
//        }
//
//      if (memwin->gran == temp)      /* If assembler returned correct # bytes */
//        {
//        board_set_memory(1, address, hex, memwin->gran);        /* Set memory */
//        view_chararrAdd(board->memory_ptr_width, address, memwin->gran, address);
//                                                              /* Step address */
//        scroll_display(memwin, address);             /* Rejustify if required */
//        gtk_editable_select_region(GTK_EDITABLE(memwin->dis_entry), 0, -1);
//                                                      /* Select all of region */
//
//        if (gtk_events_pending() > 2)
//          while (gtk_events_pending()) gtk_main_iteration();/* Update display */
//        else
//          callback_memory_refresh();                           /* Refresh all */
//        }
//      g_free(hex);                /* Discard array allocated by asm_sbprint() */
//      }
//    }
//  g_free(mnemonic);
//  }
//g_free(address);
//g_free(address_entry);
//return;
//}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

/* additions to help for scrolling of memwindow by ajr 09/08/06               */

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Steps memory window up or down by a number of lines.                       */
/*                                                                            */

void memwindow_step(mem_window *memwin, int lines)
{
if (TRACE > 10) g_print("memwindow_step\n");

memwin->addr_type = MEM_WINDOW_DEFAULT;		// Dunno! @@@

view_chararrAdd(board->memory_ptr_width, memwin->address,
                lines * memwin->width,  memwin->address);
view_updatememwindow(memwin);                                      /* Refresh */

return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a keyboard key is pressed over  memory window                  */
/* used to scroll accordingly    (by one step or 10 for page keys)            */
/*                                                                            */
// Isn't quite right; interacts with "blue bar" stuff @@@@

void callback_memwindow_key_press(GtkWidget *widget,
                                            GdkEventKey *event,
                                            gpointer user_data)
{
switch(event->keyval)
  {
  case GDK_Down:                                           /* Arrow downwards */
    memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"),  1);
    break;

  case GDK_Up:                                               /* Arrow upwards */
    memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"), -1);
    break;

  case GDK_Page_Down:                                       /* Page downwards */
    memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"), 10);
	// 10 should probably be #defined or derived from window length @@@@
    break;

  case GDK_Page_Up:                                           /* Page upwards */
    memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"),-10);
	// 10 should probably be #defined or derived from window length @@@@
    break;

  default: break;
  }
return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when mouse button is pressed over  memory window                    */
/* used to scroll accordingly    (by one step)                                */
/*                                                                            */
void callback_memwindow_button_press(GtkWidget *widget,
                                            GdkEventButton *event,
                                            gpointer user_data)
{
switch(event->button)
  {
  case 4:                                /* Mouse scroll wheel scroll upwards */
    memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"), -1);  
    break;

  case 5:                              /* Mouse scroll wheel scroll downwards */
    memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"),  1);  
    break;

  default: break;
  }

return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when either the up or down buttons are clicked to scroll memory     */
/* accordingly    (by one step)                                               */
/*                                                                            */
void callback_memwindow_scroll_button_press(GtkWidget *widget,
                                            gpointer user_data)
{
int step;

if (TRACE > 5) g_print("callback_memwindow_scroll_button_press\n");

if ( *((int*) user_data) == 0) step = -1; else step = 1;    /* Find direction */

memwindow_step(gtk_object_get_data(GTK_OBJECT(widget), "mem_window"), step);

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when the adjustment widget is manipulated and scrolls memory        */
/* accordingly                                                                */
/*                                                                            */

void callback_memwindow_scrollmove(GtkAdjustment *adjustment, gpointer ignore2)
{
mem_window *memwin;

if (TRACE > 5) g_print("callback_memwindow_scrollmove\n");

memwin = gtk_object_get_data(GTK_OBJECT(adjustment), "mem_window");
memwin->addr_type = MEM_WINDOW_DEFAULT;		// Needed here ??? @@@

/* Inform memory update that updating the start address is not necessary      */
if (ABS(adjustment->value) < 0.05)
  {
  view_updatememwindow(memwin);
  }
else
  {
  uchar temp[MAX_WORD_LENGTH];
  int lines;

  if (adjustment->value < 0)         /* Determine address decrement/increment */
    lines = -((int) pow(16, -adjustment->value));       /* Logarithmic scroll */
  else
    lines =  ((int) pow(16,  adjustment->value));

     /* Window is scrolled, but address is not altered until slider released. */
  view_chararrCpychararr(board->memory_ptr_width, memwin->address, temp);
  memwindow_step(memwin, lines);
  view_chararrCpychararr(board->memory_ptr_width, temp, memwin->address);
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
                                                     /* Let go off scroll bar */

gboolean callback_memwindow_scrollrelease(GtkWidget *scroll,
                                          gpointer ignore1,
                                          gpointer ignore2)
{
int distance;
mem_window *memwin;

if (TRACE > 5) g_print("callback_memwindow_scrollrelease\n");

memwin = view_getmemwindowptr(GTK_WIDGET(scroll));

if (ABS(gtk_range_get_adjustment(GTK_RANGE(scroll))->value) >= 0.1)
  {
  if (gtk_range_get_adjustment(GTK_RANGE(scroll))->value < 0)
    distance=-((int)pow(16,-gtk_range_get_adjustment(GTK_RANGE(scroll))->value))
                  * memwin->width;
  else
    distance= ((int)pow(16, gtk_range_get_adjustment(GTK_RANGE(scroll))->value))
                  * memwin->width;

  view_chararrAdd(board->memory_ptr_width, memwin->address, distance,
                  memwin->address);           /* Commit to the address change */

  gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(scroll)), 0);
                                                      /* Move scroll bar back */
  view_updatememwindow(memwin);
  }
return FALSE;                                /* Signifying what exactly?  @@@ */
}
/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* a line in the column list representing the memory was pressed              */

void callback_memwindow_clist(GtkCList *clist, gint row, gint column,
                              GdkEventButton *event, gpointer memory_type)
{					// Where are all the parameters from?  @@@
mem_window *memwin;
uchar address[16];                     /* Large enough for 128 bit address @@ */
uchar *memdata;

  void callback_memwindow_set_hex()
  {
  char *address_text;
  if (memwin->hex_entry != NULL)
    {
    address_text = view_chararr2hexstrbe(memwin->gran,memdata);/*Set hex entry*/
    gtk_entry_set_text(GTK_ENTRY(memwin->hex_entry), address_text);
    g_free(address_text);
    }
  return;
  }

  void callback_memwindow_set_ASCII()
  {
  char *address_text;
  if (memwin->ascii_entry != NULL)
    {
    address_text=view_chararr2asciistr(memwin->gran,memdata);/*Set ASCII entry*/
    gtk_entry_set_text(GTK_ENTRY(memwin->ascii_entry), address_text);
    g_free(address_text);
    }
  return;
  }

  void callback_memwindow_set_dis()
  {
  char *address_text;
  if (memwin->dis_entry != NULL)
    {
    address_text = view_dis(address, memdata, memwin->gran, memwin->isa);
                                                             /* set dis entry */
    gtk_entry_set_text(GTK_ENTRY(memwin->dis_entry), address_text);
    g_free(address_text);
    }
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

char *address_text;
uchar *new_address;
boolean error;

if (TRACE > 5) g_print("callback_memwindow_clist\n");

error = FALSE;
memwin = view_getmemwindowptr(GTK_WIDGET(clist));
new_address = memwin->address;

if (board->wordalign) new_address[0] &= -memwin->width;         /* word align */

{                                                   /* Look up source address */
win_row_addr *pWR;
int temp, i;
unsigned int aaa;

if ((column < MIN_HEX_ENTRY) || (column > MAX_HEX_ENTRY)) temp = 0;
else temp = column - MIN_HEX_ENTRY;           /* Column number if hex, else 0 */

pWR = memwin->row_addresses;
for (i = 0; i < row; i++) if (pWR != NULL) pWR = pWR->pNext;   /* `row' times */
aaa = pWR->address + temp * memwin->gran;         /* Address of clicked entry */

for (i = 0; i < board->memory_ptr_width; i++) address[i] = (aaa >> (8*i)) & 0xFF;
					// Unpack address to byte array   @@@@
}


if (event)
  if (GDK_2BUTTON_PRESS == event->type)
    {
    unsigned int worda, wordb;

//  if (trap_get_status(0, &worda, &wordb)) error = TRUE;         /* if error */
    error = error | trap_get_status(0, &worda, &wordb);

    if (!error)
      {
      int brkpt_no, temp;
      int breakpoint_found = FALSE;

      for (brkpt_no = 0; brkpt_no < MAX_NO_OF_TRAPS; brkpt_no++)
        if (((worda >> brkpt_no) & 1) != 0)  /* Defined? (active or inactive) */
          {
          trap_def trap;                 /* Space to store fetched definition */

          error = !read_trap_defn(0, brkpt_no, &trap);
                             /* read_trap_defn() returns `true' if successful */
          if (!error
          && (!misc_chararr_sub_to_int(board->memory_ptr_width, address,
                                       trap.addressA)))
            {                                          /* Addresses match (?) */
            breakpoint_found = TRUE;
            trap_set_status(0, 0, 1 << brkpt_no);
            }
          }

      if (breakpoint_found)          /* breakpoint(s) matched and deleted ??? */
        {
        breakpoint_refresh(0);
        callback_memory_refresh();                             /* refresh all */
        }
      else                                    /* See if we can set breakpoint */
        {
        temp = (~worda) & wordb;              /* Undefined => possible choice */
        if (temp != 0)                             /* If any free entries ... */
          {                                        /* Define (set) breakpoint */
          trap_def trap;
          int count;

          for (brkpt_no = 0; (((temp >> brkpt_no) & 1) == 0); brkpt_no++);
                  /* Choose free(?) number */
          trap.misc = -1;                       /* Really two byte parameters */
                /* Should send 2*words address then two*double words data @@@ */
          view_chararrCpychararr(8, address, trap.addressA);
          for (count = 0; count < board->memory_ptr_width; count++)
            trap.addressB[count] = 0xFF;
          for (count = 0; count < board->memory_ptr_width * 2; count++)
            { trap.dataA[count] = 0x00; trap.dataB[count] = 0x00; }
          write_trap_defn(0, brkpt_no, &trap);
          breakpoint_refresh(0);
          callback_memory_refresh();                           /* refresh all */
          }
        }
      }                              // end if not error
    }                                // end if two buttons pressed

/* the following will fill the appropriate fields upon a single mouse press   */

memdata = g_new(uchar, memwin->gran);

address_text = view_chararr2hexstrbe(board->memory_ptr_width, address);
gtk_entry_set_text(GTK_ENTRY(memwin->address_entry), address_text);
g_free(address_text);

if (!board_get_memory(1, address, memdata, memwin->gran))
  {                                          /* get value into the text entry */
    if (!board_mini_ping()
        || !board_get_memory(1, address, memdata, memwin->gran))
      {                /* if that didn't work then don't panic just try again */
      }
  }
/* sorry but board is lost */
callback_memwindow_set_hex();

if (memory_type == GPOINTER_TO_INT(0x0))                      /* memory frame */
  {
  callback_memwindow_set_ASCII();
  callback_memwindow_set_dis();
  }

g_free(memdata);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* The following is called when a new value has been put into a register      */

void callback_regwindow_hex(GtkEditable *editable, gpointer ignore)
{
reg_window *regwin;
char *text;
uchar *hex;
int count;

if (TRACE > 5) g_print("callback_regwindow_hex\n");

regwin = view_getregwindowptr(GTK_WIDGET(editable)); /* Which bank are we in? */
text = gtk_editable_get_chars(GTK_EDITABLE(regwin->address_entry), 0, -1);

count = view_find_reg_name(text, &(board->reg_banks[regwin->regbank_no]));
                                                       /* find which register */
g_free(text);

if (count >= 0)
  {
  hex = g_new(uchar, MAX(1, board->reg_banks[regwin->regbank_no].width));
                                                  /* make space for the value */
  text = gtk_editable_get_chars(GTK_EDITABLE(regwin->hex_entry), 0, -1);
                                                                 /* get chars */
  if (view_hexstr2chararr(MAX(1, board->reg_banks[regwin->regbank_no].width),
                          text, hex))                       /* convert to bin */
    {
    board_set_register(regwin->regbank_no, count, hex);       /* set register */
    callback_global_refresh();                                 /* refresh all */
    }
  g_free(text);
  g_free(hex);
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a row is pressed within any of the register banks column lists */
/*                                                                            */

void callback_regwindow_clist(GtkCList *clist, gint row, gint column,
                              GdkEventButton *event, gpointer ignore)
{
reg_window *regwin = view_getregwindowptr(GTK_WIDGET(clist));
char *name, *value;

if (TRACE > 5) g_print("callback_regwindow_clist\n");

if (board->reg_banks[regwin->regbank_no].width)         /* Get register value */
  value = view_chararr2hexstrbe(board->reg_banks[regwin->regbank_no].width,
                                 &board->reg_banks[regwin->regbank_no].
                                 values[board->reg_banks[regwin->regbank_no].
                                        width * row]);
else                                                   /* if flag then 1 or 0 */
  if (board->reg_banks[regwin->regbank_no].values[row]) value = "1";
  else                                                  value = "0";

gtk_entry_set_text(GTK_ENTRY(regwin->address_entry),
                view_get_reg_name(row,&(board->reg_banks[regwin->regbank_no])));

gtk_entry_set_text(GTK_ENTRY(regwin->hex_entry), value);

if (board->reg_banks[regwin->regbank_no].width) g_free(value);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* walk button pressed or menu entry activated                                */

void callback_button_walk(GtkButton *button, gpointer steps)
{
if (TRACE > 5) g_print("callback_button_walk\n");

was_walking = TRUE;                            /* Note, in case of "continue" */
walk_board(*((int *) steps), 0);       /* walk board - no break on first step */
callback_global_refresh();                                 /* refresh display */
gtk_label_set_text(GTK_LABEL(view_enqlabel), "Walking");  /* set status label */
set_refresh(FALSE, g_timeout_add(view_step_freq, callback_walk, steps));
/*  callback_walk is set to be called with arg. steps every view_step_freq ms */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

gint callback_walk(gpointer steps)
{
int client_status;
unsigned int dummy;

if (TRACE > 5) g_print("callback_walk\n");

client_status = board_enq(&dummy);

if ((client_status & CLIENT_STATE_CLASS_MASK) != CLIENT_STATE_CLASS_RUNNING)
  {                                                    /* If not already busy */
  if (CLIENT_STATE_STOPPED == client_status)      /* If stopped after step(s) */
    {                                                        /*  then step on */
    walk_board(*((int *) steps), 1);       /* walk board - may break 1st step */
    gtk_label_set_text(GTK_LABEL(view_enqlabel), "Walking");
    }
  else
    {                /*  else stopped for other reason, so don't walk further */
    set_refresh(FALSE, 0);                                    /* Stop walking */
    }

  callback_global_refresh();

  while (gtk_events_pending()) gtk_main_iteration();
                                          /* update display and do user stuff */
  }

return 1;                                  /* Never returns anything else @@@ */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

//
///* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
///* the following 2 functions are callbacks for the console feature          */
//
///* This specific one used to send messages to the back-end terminal more    */
///* than one character at a time                         */
//
//void callback_comms(GtkEditable* entry, gpointer comms_console)
//{
//char* tosend = gtk_entry_get_text(GTK_ENTRY(entry));
//                                                 /* get what needs to be sent */
//int length = strlen(tosend);                                    /* set length */
//int index = 0;
//int sub;
//feature* comms = comms_console;            /* reference to the console window */
//char accepted;
//
//if (-1 < board_version)	// Altered meaning; also encodes presence 19/7/07
// {             /* board version is currently 0, but this needs to be changed */
//  while (length>index)
//    {
//    board_sendchar(BR_FR_WRITE);                            /* begins a write */
//    board_sendchar(comms->dev_number);              /* tells where to send it */
//    sub = MIN(length - index, 255);             /* length of message in bytes */
//    board_sendchar(sub);                                  /* send that length */
//    board_sendchararray(sub, tosend + index);             /* send the message */
//    board_getchar(&accepted);                       /* find what was accepted */
//    index += sub;                                      /* step within message */
//    }                                 /* NB This version ignores overruns @@@ */
// }
//
//gtk_entry_set_text(GTK_ENTRY(entry), "");
//return;
//}
//
///* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
//


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Updates the terminal given its reference and returns 1 to remain on the    */
/* scheduler that invokes it every POLLING_PERIOD ms                          */
/*                                                                            */

gint callback_update_comms(gpointer comms)
{
feature *comms_terminal = comms;

if (TRACE > 15) g_print("callback_update_comms\n");

if (gtk_toggle_button_get_active
    (GTK_TOGGLE_BUTTON(terminal_active_flag[comms_terminal->dev_number])))
  read_string_terminal(comms);
return 1;                                           /* Why return anything ?? */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* wrapper function to go from timer interrupts                               */

gint callback_updateall(gpointer ignore)
{
unsigned int dummy;

if (TRACE > 10) g_print("callback_updateall\n");    /* Don't trace too easily */

//board_micro_ping();  /* @@@ NOTE: seems to be redundant, causes double update */

if (serial_lock == FREE)
  {
  serial_lock = MEM_AND_REG_UPDATE;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view_refreshbutton)))
    {                                            /* If refreshing the display */
    if (board_running
    && (CLIENT_STATE_CLASS_RUNNING !=
                                 (board_enq(&dummy) & CLIENT_STATE_CLASS_MASK)))
      {                            /* Client has stopped running autonomously */
      board_running = FALSE;                                     /* Take note */
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view_refreshbutton), FALSE);
                                                           /* Disable refresh */
      }

    callback_global_refresh();
    while (gtk_events_pending()) gtk_main_iteration();
                                           /*update display and do user stuff */
    }
  serial_lock = FREE;        /* NB MOVED @@@ */
  }

return 1;   /* Meaning ??? @@@ */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* update either a register bank or a memory frame                            */

// gint callback_update_memreg_window(int window_type, gpointer windowptr)
// {
// if (CLIENT_STATE_CLASS_RUNNING != (board_enq () & CLIENT_STATE_CLASS_MASK))
//                                     /* if the client is not currently running */
//   if (view_global_refresh_timer && board_running)
//     {
//     gtk_timeout_remove(view_global_refresh_timer);
//     view_global_refresh_timer = FALSE;
//     board_running = TRUE;
//     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view_refreshbutton), FALSE);
//     }
// 
// if (window_type == MEMORY_WINDOW)
//   view_updatememwindow((mem_window *) windowptr);
// else
//   view_updateregwindow((reg_window *) windowptr);            /* Only two types */
// 
// while (gtk_events_pending()) gtk_main_iteration ();
//                                             /*update display and do user stuff */
// return 1;   /* Meaning ??? @@@ */
// }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Updates the console window                                                 */
/*                                                                            */

gint callback_console_update(gpointer data, gint source,
                             GdkInputCondition condition)
{
char getbuff[20];
int got;

if (TRACE > 5) g_print("callback_console_update\n");

got = read(compile_communication[0], getbuff, 20);
#ifndef GTK2
gtk_text_insert(GTK_TEXT(view_console), NULL, NULL, NULL, getbuff, got);
#endif

#ifdef GTK2
gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(view_console), getbuff, got);
#endif
return 1;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* The following function will set or unset a flag i.e. change some bits in   */
/* the CPSR (as a response to flag press)                                     */
/*                                                                            */

void callback_flag_pressed(GtkToggleButton *togglebutton, gpointer psr_index)
{
unsigned char *psr_ptr, new_flags, new_status;
reg_bank *regbank;
int displayed_registers;
int nflag_boolean, zflag_boolean, cflag_boolean, vflag_boolean;
int psr, i, j;


if (TRACE > 5) g_print("callback_flag_pressed\n");

displayed_registers = gtk_notebook_get_current_page(register_notebook);
                                              /* Mapping function too ??? @@@ */
regbank = &board->reg_banks[displayed_registers];    /* Get current reg. bank */

psr = GPOINTER_TO_INT(psr_index);             /* Really does nothing but copy */
psr_ptr = &regbank->values[psr * regbank->width];
                                              /* get PSR (CPSR=R16, SPSR=R17) */
                       // Saved copy rather than refetching :-(    @@@

if (psr == CPSR) i = 0; else i = 1;
new_flags  = 0;

for (j = 0; j <= 3; j++)                         /* Encode button states NZCV */
  new_flags |=
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(flag_button[i][j]))<<(j+4);

new_status =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(flag_button[i][4])) << 7
    | gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(flag_button[i][5])) << 6;
//  | gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(flag_button[i][6])) << 5;

#ifdef	THUMB						// Bodge approach @@@
new_status |=
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(flag_button[i][6])) << 5;
#endif

if ((((new_flags  ^ psr_ptr[3]) & 0xF0) != 0)      /* Encode button states IF */
 || (((new_status ^ psr_ptr[0]) & 0xE0) != 0))      /*  something has changed */
  {
  int state;                        /* Temporary */

  psr_ptr[3] = (psr_ptr[3] & 0x0F) | new_flags;
  psr_ptr[0] = (psr_ptr[0] & 0x1F) | new_status;

//  state = board_enq() & CLIENT_STATE_CLASS_MASK;
//
//  if (state != CLIENT_STATE_CLASS_RUNNING)
     /* only set the register if the board is not running to avoid dirty read */
    {

    // can lose variable if no mapping needed @@@
    board_set_register(displayed_registers, psr, psr_ptr);

    callback_refresh_selection(TRUE, TRUE, FALSE);  // Not flag window ???  @@@
    }
  }
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* The following is called when the user made a selection of current mode     */
/* CPSR will get a new value depending on the mode chosen                     */
/*                                                                            */

// Code PSR ID into "newmode" too? @@@  Then lose both these entry points! @@@

void callback_mode_selection(GtkMenuItem *menuitem, gpointer newmode)
{
if (TRACE > 5) g_print("callback_mode_selection\n");

callback_mode_selection_all(GPOINTER_TO_INT(newmode), CPSR);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* The following is called when the user made a selection of current mode     */
/* SPSR will get a new value depending on the mode chosen                     */
/*                                                                            */
/*                                                                            */

void callback_mode_selection_saved(GtkMenuItem *menuitem, gpointer newmode)
{
if (TRACE > 5) g_print("callback_mode_selection_saved\n");

callback_mode_selection_all(GPOINTER_TO_INT(newmode), SPSR);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Generalises the above                                                      */
/*                                                                            */

void callback_mode_selection_all(int newmode, int PSR)
{
int regbank_no;
reg_bank *regbank;
uchar *displayed_psr;


if (TRACE > 5) g_print("callback_mode_selection_all\n");

regbank_no = gtk_notebook_get_current_page(register_notebook);
regbank = &board->reg_banks[regbank_no];

displayed_psr = &regbank->values[PSR * regbank->width];
/* get value of PSR in 'current' */
switch (newmode)                      /* set to right mode according to input */
  {
  case 1: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x10; break;
  case 2: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x13; break;
  case 3: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x17; break;
  case 4: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x1B; break;
  case 5: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x12; break;
  case 6: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x11; break;
  case 7: displayed_psr[0] = (displayed_psr[0] & 0xE0) | 0x1F; break;
  case -1: break;                                /* Used if "invalid" is sent */
  default:
    if (VERBOSE) g_print("Errorneous input to callback_mode_selection()\n");
    break;
  }
board_set_register(regbank_no, PSR, displayed_psr);
callback_global_refresh();
                    /* Certainly overkill here (not breakpoints at least) @@@ */

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/
/* Function called when the displayed page of the register notebook changes   */

void callback_reg_notebook_change(GtkButton *button, gpointer nothing)
{
int regbank;

if (TRACE > 5) g_print("callback_reg_notebook_change\n");

regbank = gtk_notebook_get_current_page(register_notebook);

callback_register_refresh();   // CRUDE ... for now @@@

return;
}


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Traps a keypress and sends it to the terminal when appropriate             */
/*                                                                            */

gint callback_terminal_keysnoop(GtkWidget *grab_widget, GdkEventKey *event,
                                gpointer comms)
{
uint key_pressed;  /* to save the value of the key that invoked this function */
feature *terminal;           /* record feature from which callback came */
int i, j;

if (TRACE > 5) g_print("callback_terminal_keysnoop\n");

terminal = comms;            /* record feature from which callback came */

{
//printf("QQ: %08X %08X %08X\n", grab_widget, event, comms);

j = -1;
for (i = 0; i < board->feature_count; i++) // Bodgy, dubious, but finds feature No. @@@
  if ((board->pArray_F[i].type == TERMINAL)
   && (board->pArray_F[i].data.terminal.text == grab_widget)) j = i;

}

//for (i = 0; i < 5; i++) printf("TT: %d, %08X %d\n", i, text_in_terminal[i], terminal_active_flag[j]);

//if ((grab_widget == text_in_terminal[terminal->dev_number]))
if (j >= 0)
  {        /* only when called from the window defined and character is valid */
  if (gtk_toggle_button_get_active(
//     GTK_TOGGLE_BUTTON(terminal_active_flag[terminal->dev_number])))
     GTK_TOGGLE_BUTTON(terminal_active_flag[j])))
    {
//    key_pressed = (event->keyval) & 0xFF;
    key_pressed = event->keyval;
    switch (key_pressed)   /* Translate key codes if necessary and understood */
      {
      case GDK_Return:
      case GDK_KP_Enter:    key_pressed = '\n'; break;
      case GDK_BackSpace:   key_pressed = '\b'; break;
      case GDK_Tab:         key_pressed = '\t'; break;
      case GDK_Escape:      key_pressed = 0x1B; break;
      case GDK_KP_0:
      case GDK_KP_1:
      case GDK_KP_2:
      case GDK_KP_3:
      case GDK_KP_4:
      case GDK_KP_5:
      case GDK_KP_6:
      case GDK_KP_7:
      case GDK_KP_8:
      case GDK_KP_9:        key_pressed = key_pressed - GDK_KP_0 + '0'; break;
      case GDK_KP_Add:      key_pressed = '+'; break;
      case GDK_KP_Subtract: key_pressed = '-'; break;
      case GDK_KP_Multiply: key_pressed = '*'; break;
      case GDK_KP_Divide:   key_pressed = '/'; break;
      case GDK_KP_Decimal:  key_pressed = '.'; break;
      default:
//        if ((key_pressed < ' ') || (key_pressed > 0x7F)) key_pressed = '\a';
        break;                                     /* Illegal character = BEL */
      }

//    send_key_to_terminal(key_pressed, comms);
    if (((key_pressed >= ' ') && (key_pressed <= 0x7F))
      || (key_pressed == '\n')
      || (key_pressed == '\b')
      || (key_pressed == '\t')
      || (key_pressed == '\a')
   )
      send_key_to_terminal(key_pressed, j);
    }
  }
else
  gtk_propagate_event(grab_widget, (gpointer) event);
return 1;                                   // Meaning? @@@
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when clear terminal is requested                                    */
/*                                                                            */

void callback_clear_terminal(GtkButton *ignore, gpointer term)
{
int text_length;
feature *terminal;              /* Pointer to the terminal - could be omitted */

if (TRACE > 5) g_print("callback_clear_terminal\n");

terminal = (feature*) term;                     /* Purely here to fix `types' */

text_length = gtk_text_get_length(GTK_TEXT(terminal->data.terminal.text));
                                                /* Get the length of the text */
gtk_text_backward_delete(GTK_TEXT(terminal->data.terminal.text), text_length);
                              /*  and use that length to erase the whole text */
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when the user has selected new mode to be chosen in the register    */
/*  bank GTKNotebook                                                          */

void callback_mode_selected_from_menu(GtkWidget *ignore, gpointer mode_number)
{
if (TRACE > 5) g_print("callback_mode_selected_from_menu\n");

gtk_notebook_set_page(register_notebook, *(int*)mode_number);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when the user has selected new mode to be chosen in the flags       */
/*  GTKNotebook                                                               */

void callback_flags_selected_from_menu(GtkWidget *ignore, gpointer mode_number)
{
if (TRACE > 5) g_print("callback_flags_selected_from_menu\n");

gtk_notebook_set_page(flags_notebook, *(int*)mode_number);
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* The main exit routine. Close any open connections and/or files here        */

void callback_main_quit(gpointer ignore, gpointer ignore2)
{
if (TRACE > 5) g_print("callback_main_quit\n");

if (emulator_PID != 0) kill(emulator_PID, SIGKILL);
                             /* If emulator in use shut down emulator process */

exit(1);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a new memory window is required                                */
/* type is {MEM_WIN_DUMP, MEM_WIN_SOURCE} and specifies memory window format  */

void callback_memory_window_create(gpointer ignore, gpointer type)
{
GtkWidget *window_ptr;
GdkGeometry hints;

if (TRACE > 5) g_print("callback_memory_window_create\n");

if (type == MEM_WIN_DUMP)                    /* size properties of the window */
  {
  hints.base_width  = MEM_WINDOW_BASE_WIDTH;
  hints.base_height = MEM_WINDOW_BASE_HEIGHT;
  hints.max_width   = MEM_WINDOW_MAX_WIDTH;
  hints.max_height  = MEM_WINDOW_MAX_HEIGHT;
  hints.min_width   = MEM_WINDOW_MIN_WIDTH;
  hints.min_height  = MEM_WINDOW_MIN_HEIGHT;
  }
else
  {
  hints.base_width  = SOURCE_WINDOW_BASE_WIDTH;
  hints.base_height = SOURCE_WINDOW_BASE_HEIGHT;
  hints.max_width   = SOURCE_WINDOW_MAX_WIDTH;
  hints.max_height  = SOURCE_WINDOW_MAX_HEIGHT;
  hints.min_width   = SOURCE_WINDOW_MIN_WIDTH;
  hints.min_height  = SOURCE_WINDOW_MIN_HEIGHT;
  }

hints.width_inc   = 1;
hints.height_inc  = 1;
hints.min_aspect  = 10.0;
hints.max_aspect  = 0.10;
//hints.win_gravity = GDK_GRAVITY_STATIC;

window_ptr = view_create_memorywindow((mem_win_type) type,
//                                       LENGTH_OF_MEMORY_WINDOW,
                                       (type == MEM_WIN_DUMP)
                                         ? MEM_REP_8_WORDS
                                         : MEM_REP_WORD,
                                       &hints);
                                         /* Linked to display list internally */

callback_global_refresh();    // @@@ Surely just this one ?
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a new symbol window is required                                */

void callback_symbol_window_create(gpointer ignore, gpointer ignore2)
{
GtkWidget *window_ptr;
GdkGeometry hints;

if (TRACE > 5) g_print("callback_symbol_window_create\n");

hints.base_width  = SYMBOL_WINDOW_BASE_WIDTH;
hints.base_height = SYMBOL_WINDOW_BASE_HEIGHT;
hints.max_width   = SYMBOL_WINDOW_MAX_WIDTH;
hints.max_height  = SYMBOL_WINDOW_MAX_HEIGHT;
hints.min_width   = SYMBOL_WINDOW_MIN_WIDTH;
hints.min_height  = SYMBOL_WINDOW_MIN_HEIGHT;
hints.width_inc   = 1;
hints.height_inc  = 1;
hints.min_aspect  = 10.0;
hints.max_aspect  = 0.10;
//hints.win_gravity = GDK_GRAVITY_STATIC;

window_ptr = view_create_symbol_window(&hints);

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a memory window is destroyed                                   */
/* On entry: top_window points to the graphical window to destroy             */
/*           window points at the window's descriptor                         */

void callback_memory_window_destroy(gpointer top_window, gpointer window)
{
if (TRACE > 5) g_print("callback_memory_window_destroy\n");

gtk_widget_destroy(top_window);                       /* Destroy the graphics */
view_destroy_memorywindow((mem_window*) window);      /* Unlink & free memory */
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Called when a symbol window is destroyed                                   */
/* On entry: top_window points to the graphical window to destroy             */
/*           window points at the window's descriptor                         */

void callback_symbol_window_destroy(gpointer top_window, gpointer clist)
{
if (TRACE > 5) g_print("callback_memory_window_destroy\n");

view_remove_symbol_clist(clist);    /* Remove entry from symbols display list */

//gtk_widget_destroy(top_window);                       /* Destroy the graphics */

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/
/* Find the list of passed filenames for `entry'; if `filename' is present    */
/* move it to the front, else add it at the front.  In the latter case, space */
/* is allocated and left in the list.                                         */
/* The `parent' flag alters the "combo" used. Not understood at this time @@@ */

void maintain_filename_list(gpointer *entry, char *filename, int parent)
{
GList *list, *element;

list = gtk_object_get_data(GTK_OBJECT(entry), "list");
                                 /* Append filename to list of past filenames */

element = g_list_find_custom(list, filename, (GCompareFunc) g_strcasecmp);

if (element == NULL)                   /* Not found in list of previous names */
  list = g_list_prepend(list, g_strdup(filename));     /* Allocates new entry */
else                         /* Found - move latest filename to front of list */
  list = g_list_concat(element, g_list_remove_link(list, element));

gtk_object_set_data(GTK_OBJECT(entry), "list", list);

if (parent)
  gtk_combo_set_popdown_strings(GTK_COMBO(GTK_WIDGET(entry)->parent), list);
else
  gtk_combo_set_popdown_strings(GTK_COMBO(gtk_object_get_data
                               (GTK_OBJECT(entry), "combo")), list);

//gtk_entry_set_text(GTK_ENTRY(entry), filename);           // Better??? @@@
                                                       // Redundant!!??? @@@
return;
}

/*                           end of callbacks.c                               */
/*============================================================================*/
