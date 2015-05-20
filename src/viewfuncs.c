/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*              Name:           viewfuncs.c                             */
/*              Version:        1.5.0                                   */
/*              Date:           20/07/2007                              */
/*                                                                      */
/*              Functions related to view                               */
/*                                                                      */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#include "definitions.h"
#include "global.h"

#include "view.h"
#include "callbacks.h"
#include "breakview.h"
#include "interface.h"
#include "misc.h"
#include "chump.h"

#define MAX_REG_SIZE 8  /* Size of largest printable ASCII field for register */
                                        /* E.g. 8 is enough for up to 64 bits */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Updates a memory window.                                                   */
/* inputs: the pointer to the memory window                                   */
/* returns: 1 when successful                                                 */
/*                                                                            */

int view_updatememwindow(mem_window *memwindowptr)
{
GtkCList *clist = GTK_CLIST(memwindowptr->clist_ptr);
GList *breaks = NULL;
GList *breaksinactive = NULL;

/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Find the start address of the current window.                              */
/*  If there is a valid expression in the input window this will be evaluated */
/*    and kept for future use.                                                */
/*  If the window has been (e.g.) scrolled a default value is used.           */
/* Returns: Pointer to the address (which is kept in *private* space).        */

  uchar *evaluate_window_start_pointer(mem_window *memwindowptr)
  {
  char *recorded_address;
  evaluation_data eval_data;
  int address_entry_status;
  int result_integer;               /* the result of evaluation as an integer */
  char *trash;

  if (memwindowptr->addr_type == MEM_WINDOW_EXPR)
    {                      /* If user expression then re-evaluate start point */
    eval_data.representation = HEXADECIMAL;
    recorded_address = memwindowptr->address_string;  /* Temp. string pointer */
    address_entry_status=Evaluate(&recorded_address,&result_integer,eval_data);
    if (address_entry_status == eval_okay)
      {
      trash = memwindowptr->address;
      memwindowptr->address = view_int2chararr(4, result_integer);
      g_free(trash);                                   /* Lose previous entry */
      }
    else
      memwindowptr->addr_type = MEM_WINDOW_DEFAULT;
    }

  return memwindowptr->address;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*Creates lists of memory addresses that are breakpoints (active and inactive)*/
/* Allocates memory both for list entries and payload as required             */

  boolean make_break_lists()
  {
  unsigned int worda, wordb;
  int temp, temp2;
  uchar *value;
  boolean flag;              /* `flag' indicates that this may go in the list */
  boolean error;

  error = FALSE;

  if (!trap_get_status(0, &worda, &wordb))
    {       /* Have trap status info okay: get breaks and breaksinactive list */
    for (temp = 0; (temp < MAX_NO_OF_TRAPS) && !error; temp++)
      if (((worda >> temp) & 1) != 0)            /* Breakpoint #temp defined? */
        {
        trap_def trap;

        if (read_trap_defn(0, temp, &trap))                        /* if okay */
          {
//          if (((trap.misc & 0x00EF) == 0x00EF) /* All conditions except write */
//           && ((trap.misc & 0x0F00) == 0x0F00)) flag = 1;   /*  and all sizes */
//          else                                  flag = 0;
          flag = (((trap.misc & 0x00EF) == 0x00EF)     /* All conditions except */
               && ((trap.misc & 0x0F00) == 0x0F00));   /*   write and all sizes */

          if (flag)
            for (temp2 = board->memory_ptr_width - 1; temp2 >= 0; temp2--)
              {
              if (trap.addressB[temp2] != 0xFF) flag = FALSE;
                                         /* Address mask <> FFFFFFFF - reject */
/* @@@ WRONG!  SHOULD USE INSTRUCTION LENGTH ONLY FOR DATA @@@ */
              if (trap.dataB[temp2] != 0x00) flag = FALSE;
                                            /* data mask <> 00000000 - reject */
              }

          if (flag)                                      /* if needs painting */
            {
            value = g_new(uchar, board->memory_ptr_width);
            for (temp2 = board->memory_ptr_width - 1; temp2 >= 0; temp2--)
              value[temp2] = trap.addressA[temp2];

            if ((wordb >> temp) & 1) breaks = g_list_append(breaks, value);
            else     breaksinactive = g_list_append(breaksinactive, value);
            }
          }
        else error = TRUE;            /* Read failure causes loop termination */
        }
    }
  else error = TRUE;            /* Didn't read trap status successfully above */

  return !error;                 /* May have allocated some list space anyway */
//  if (error) return 0;           /* May have allocated some list space anyway */
//  else       return 1;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Adds all the relevant memory addresses to the column list, displays        */
/*  register names, symbol names and uses the correct colour highlights       */
/* Generates/renews the `row_addresses' list in the memory window which is an */
/* ordered list of start addresses of displayed rows (used if entry selected) */
// Messy mixture of ints and byte arrays inside @@@

  void add_memory_entries_to_clist(uchar *start_address, unsigned char *memdata)
  {
  typedef enum { normal, faint, highlight, breakpt, special } colouring;

  char *address;
  char *tab_source;

// Probably more buffers could be handled statically @@@
// Wants some masks for addresses < 32 bit for source wrap-around @@@

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    void output_ordinary_data(int row, unsigned int offset, int width,
                              int gran, GList *isa, mem_win_type type)
      {
      int hex_entry;
      char *data, *trash;

      for (hex_entry = 0; hex_entry < (width/gran); hex_entry++)
        {                                               /* Update hex columns */
        data = view_chararr2hexstrbe(gran, &memdata[offset + hex_entry * gran]);
        view_update_field(clist, row, MIN_HEX_ENTRY + hex_entry, data);
        g_free(data);
        }

      data = view_chararr2asciistr(width, &memdata[offset]);
      view_update_field(clist, row, ASCII_ENTRY, data);
      g_free(data);

      data = view_dis(address, &memdata[offset], width, isa);
      if (type == MEM_WIN_SOURCE)                /* Maybe tab out more nicely */
        {
        trash = data;                    /* Keep pointer for disposal process */
        data = g_strconcat(tab_source, data, NULL);
        g_free(trash);
        }
      view_update_field(clist, row, DIS_ENTRY, data);
      g_free(data);

      return;
      }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    void make_ascii(unsigned int offset, int length, char *buffer)
      {
      int i;
      char c;

      for (i = 0; i < length; i++)          /* ASCII copy/convert from memory */
        {
        c = memdata[offset + i];
        if ((c < 0x20) || (c >= 0x7F)) buffer[i] = '.'; else buffer[i] = c;
        }
      buffer[i] = '\0';                                     /* Add terminator */
      return;
      }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    int source_disassemble(source_line *src, unsigned int addr,
                           unsigned int start_addr, int row, int increment,
                           char *ascii)
      {
      unsigned int diff;

//##@@
      if (src != NULL)                       /* If there is a source file ... */
        {
        diff = src->address - addr;            /* How far to next line start? */
        if (diff == 0)            /* Do have a source line, but shan't use it */
          {
          src = src->pNext;                          /* Use the one after ... */
          if (src != NULL) diff = src->address - addr;      /* ... if present */
          else diff = 1000;                           /* Effectively infinity */
          }
        }
      else diff = 1000;                               /* Effectively infinity */
			// Needs some masking if not 32-bit address space @@@@

      if (diff < memwindowptr->width) increment = diff;  /* Next source entry */
      else increment = memwindowptr->width - (addr % memwindowptr->width);
                                                    /* To next word alignment */

      if (increment == memwindowptr->width)   /* Default window settings okay */
        output_ordinary_data(row, addr - start_addr, memwindowptr->width,
                                                     memwindowptr->gran,
                                                     memwindowptr->isa,
                                                     memwindowptr->type);
      else
        {
        char *string, *pStr;
        int i;

        string = g_new(char, 3 * memwindowptr->width);    /* Incl. terminator */
        pStr = string;
        for (i = 0; i < increment; i++)                       /* Byte padding */
          {
          pStr = view_hex_conv(memdata[addr - start_addr + i], 2, pStr);
          *pStr++ = ' ';
          }
        *pStr = '\0';                                       /* Add terminator */

        make_ascii(addr - start_addr, increment, ascii);

        view_update_field(clist, row, MIN_HEX_ENTRY, string);
        view_update_field(clist, row, ASCII_ENTRY, ascii);
        view_update_field(clist, row, DIS_ENTRY, "");
        g_free(string);
        }
      return increment;       /* A hack when routine was extracted from below */
      }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


  char *data, *test, *trash;
  int row, increment, temp;
  colouring colour_option;
  GdkColor *colour;
  GList *templist;
  source_line *src;
  unsigned int addr, start_addr;
  char ascii[SOURCE_BYTE_COUNT + 1];        /* Max. field length + terminator */
  win_row_addr *pWR1, *pWR2;

  int list_full_source;            /* Allows source listing to be abbreviated */
  boolean used_first;/* Flag to say we have already printed first source line */

  list_full_source = view_check_button(memwindowptr->full_button);

  if (memwindowptr->dis_tab != NULL)    /* Reconcile adjuster with global var.*/
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(memwindowptr->dis_tab),
                              source_dis_tabs);

  src = NULL;                              /* Make source record pointer safe */
  used_first == FALSE;
  address = g_new(char,board->memory_ptr_width);/* Allocate CB-style variable */

  for (temp = 0; temp < board->memory_ptr_width; temp++)
    address[temp] = start_address[temp];
  start_addr = view_chararr2int(board->memory_ptr_width, start_address);

  pWR1 = memwindowptr->row_addresses;          /* Dispose of old address list */
  memwindowptr->row_addresses = NULL;
  while (pWR1 != NULL) { pWR2 = pWR1; pWR1 = pWR1->pNext; g_free(pWR2); }

  tab_source = g_new(char, source_dis_tabs + 1);    /* Horribly inefficient @@*/
  for (temp = 0; temp < source_dis_tabs; temp++) tab_source[temp] = ' ';
  tab_source[temp] = '\0';

       /* If looking in source file, skip records before window start address */
  if ((memwindowptr->type == MEM_WIN_SOURCE) && (source.pStart != NULL))
    {
    src = source.pStart;                                 /* Known to be valid */
    while ((src != NULL)
       && ((src->address < start_addr) || (src->nodata && !list_full_source)))
      src = src->pNext;

    if (src == NULL)                    /* We fell off the end; wrap to start */
      {
      src = source.pStart;
      used_first == TRUE;
      if (!list_full_source)                  /* Find a record with some data */
        while ((src != NULL) && src->nodata) src = src->pNext;
      }
    }

  for (row = 0; row < memwindowptr->count; row++)/* Iterate over display rows */
    {
    increment = memwindowptr->width;                     /* Default increment */
    colour_option = normal;                                 /* Default colour */

    addr = view_chararr2int(board->memory_ptr_width, address);
						// address in CONVENIENT form  @@@

    pWR1 = g_new(win_row_addr, 1);    /* Create new Window-Row address record */
    pWR1->address = addr; pWR1->pNext = NULL;   /* IDs row address if clicked */
    if (memwindowptr->row_addresses == NULL) memwindowptr->row_addresses = pWR1;
    else pWR2->pNext = pWR1;                                        /* Append */
    pWR2 = pWR1;                                      /* Last element in list */

    data = view_chararr2hexstrbe(board->memory_ptr_width, address);
                                                  /* create an ADDRESS string */

    /* Possibly replace hex address string with label */
    if (view_check_button(memwindowptr->elf_symbols_button))
                                   /* Whether ELF symbols are to be displayed */
      {
      char *name;                       /* Pointer to returned string (alias) */
      if (misc_find_a_symbol(&name, address)) /* Find symbol: value "address" */
        {                                                     /* If found ... */
        g_free(data);                      /* Replace hex address with symbol */
        data = g_strdup(name);
        }
      }                                       /* end (show_elf_symbols==TRUE) */

    test = view_chararrinregbank(board->memory_ptr_width, address);
  /* Returns a string identifying register(+) pointing to "address" (or NULL) */

    /* Append pointer indication */
    if (test != NULL)          /* At this point this means a reg. points here */
      {
      trash = data;                      /* Keep pointer for disposal process */
      data = g_strconcat(data, " ", test, NULL);
      g_free(trash);              /* Concatenate address/symbol and reg. name */
      g_free(test);
      }

    /* Detect special registers */
    for (temp = 0; temp < SPECIAL_REGISTER_COUNT; temp++)
                                           /* for all special registers known */
      if (special_registers[temp].active     /* if this reg. currently exists */
     && *(special_registers[temp].valid))    /*  ... and it has a valid value */
        if (!misc_chararr_sub_to_int(board->memory_ptr_width,
                                     special_registers[temp].value,
                                     address))
          {        /* If the address is same as the value of the special reg. */
          colour = &special_registers[temp].colour;  /* set to special colour */
          if (special_registers[temp].pixmap)
               /* args below: clist, row, column, text, spacing, bitmap, mask */
            gtk_clist_set_pixtext(clist, row, 0, data, 2,
                                  special_registers[temp].pixmap,
                                  special_registers[temp].bitmap);
          else
            gtk_clist_set_text (clist, row, 0, data);
          colour_option = special;
          }

    /* Address */
    if (colour_option == normal) /* not a special register - (the case above) */
      {
      trash = data;
      data = g_strconcat("  ", data, NULL);
      g_free(trash);                     /* add spaces to align w.r.t. arrows */
      view_update_field(clist, row, ADDRESS_ENTRY, data);
      }
    g_free(data);

    /* Data columns (start at #1) etc.*/
    if ((memwindowptr->type == MEM_WIN_SOURCE) && (src != NULL))
      {                     /* Might be interested in any source file records */
      if (addr == src->address)
        {                                /* Source window - fetch from record */
        int i, j, k;
        boolean corrupt;
        char *pStr;

        corrupt = FALSE;           /* Start by assuming memory matches source */
        k = 0;
        for (i = 0; i < SOURCE_FIELD_COUNT; i++)      /* Source field entries */
          {
          for (j = 0; j < src->data_size[i]; j++)
            if (((src->data_value[i] >> (8 * j)) & 0xFF)
              != memdata[addr - start_addr + k++])
              corrupt = TRUE;
          }

        if (corrupt)                               /* Overwritten source line */
          {                                      /* Highlight and disassemble */
          colour_option = highlight;
          increment = source_disassemble(src, addr, start_addr, row, increment,
                                       ascii);
          }
        else
          {
          increment = 0;                           /* How far should we move? */
          pStr = g_strdup("");                             /* Seed hex string */
          for (i = 0; i < SOURCE_FIELD_COUNT; i++)    /* Source field entries */
            {
            if (src->data_size[i] > 0)
              {
              char spaces[5];            /* Max. one per byte plus terminator */

              data = view_chararr2hexstrbe(src->data_size[i],
                                       &memdata[addr - start_addr + increment]);
              for (j = 0; j<src->data_size[i]; j++) spaces[j] = ' ';/* Spaces */
              spaces[j] = '\0';
              trash = pStr;
              pStr = g_strconcat(pStr, data, spaces, NULL);
              g_free(trash);
              g_free(data);
              }
            increment = increment + src->data_size[i];
            }

          make_ascii(addr - start_addr, increment, ascii);

          view_update_field(clist, row, MIN_HEX_ENTRY, pStr);
          view_update_field(clist, row, ASCII_ENTRY, ascii);
          g_free(pStr);

          view_update_field(clist, row, DIS_ENTRY, src->text);
          }

        do
          {
          if (src->pNext != NULL) src = src->pNext;            /* Move on ... */
          else
            {                                    /* ... wrapping, if required */
            if (!used_first) { src = source.pStart; used_first = TRUE; }
            else               src = NULL;      /* Been here before - give in */
            }
          }
          while ((src != NULL) && src->nodata && !list_full_source);

        }
      else               /* Source file present but no entry for this address */
        {                                        /* Disassemble in faint type */
        if (colour_option == normal) colour_option = faint;
        increment = source_disassemble(src, addr, start_addr, row, increment,
                                       ascii);
        }
      }
    else                                       /* Uninterested in source file */
      {
      if ((memwindowptr->type == MEM_WIN_SOURCE) && (colour_option == normal))
        colour_option = faint;       /* Disassembly in source window is faint */

      output_ordinary_data(row, addr - start_addr, memwindowptr->width,
                                                   memwindowptr->gran,
                                                   memwindowptr->isa,
                                                   memwindowptr->type);
      }

/* Search for breakpoints (inactive) */
    templist = breaksinactive;			// Only does single address type @@@
    while (templist != NULL)
      {
      if (misc_chararr_sub_to_int(board->memory_ptr_width, address,
                                  templist->data) == 0)
        colour_option = faint;
      templist = templist->next;
      }

/* Search for breakpoints (active) */
    templist = breaks;				// Only does single address type @@@
    while (templist != NULL)
      {
      if (misc_chararr_sub_to_int(board->memory_ptr_width, address,
                                  templist->data) == 0)
        colour_option = breakpt;
      templist = templist->next;
      }


    switch (colour_option)
      {
      case normal:    gtk_clist_set_foreground(clist, row,
                       &gtk_widget_get_default_style()->text[GTK_STATE_ACTIVE]);
                      break;
      case faint:     gtk_clist_set_foreground(clist, row, &view_greycolour);
                      break;
      case breakpt:   gtk_clist_set_foreground(clist, row, &view_orangecolour);
                      break;
      case highlight: gtk_clist_set_foreground(clist, row, &view_redcolour);
                      break;
      case special:   gtk_clist_set_foreground(clist, row, colour);
                      break;
      }

    view_chararrAdd(4, address, increment, address);
    }                                                              /* end for */

  g_free(address);                               /* Now finished with address */
  g_free(tab_source);
  return;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Frees up the lists allocated for breakpoints                               */

  void free_list(GList *list)
  {
  GList *temp;

  if (list != NULL)
    {
    temp = list;
    while (temp != NULL) {g_free(temp->data); temp = temp->next;}
    g_list_free(list);
    }
  return;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int count = memwindowptr->count * memwindowptr->width;
                                                /* Number of bytes to display */
uchar *new_address;                        /* Pointer to window start address */
unsigned char *memdata;                     /* Buffer to store fetched memory */
int ret;                                                      /* Error status */

ret = 0;

new_address = evaluate_window_start_pointer(memwindowptr);
                                                 /* Find window start address */
memdata = g_new(unsigned char, count);    /* Allocate buffer for memory image */

if (make_break_lists())                           /* Creates breakpoint lists */
  {
  if (board->wordalign) new_address[0] &= -memwindowptr->width;
                                        /* attempt to get the memory contents */
  if ((board_get_memory(count / memwindowptr->gran, new_address,
                        memdata, memwindowptr->gran))
     || (board_mini_ping()               /* First time failed: try once again */
      && board_get_memory(count / memwindowptr->gran, new_address, memdata,
                         memwindowptr->gran)))
    {
    gtk_clist_freeze(clist);
    add_memory_entries_to_clist(new_address, memdata);    // ###
    gtk_clist_thaw(clist);
    ret = 1;
    }
  }

free_list(breaks);                   /* Deallocate all the created list space */
free_list(breaksinactive);
g_free(memdata);                                             /* Memory buffer */

return ret;                /* one to indicate successful update of the memory */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Updates a register bank display                                            */
/* Inputs: regwindowptr - a pointer to the register bank display record       */
/* returns: 1 when successful                                                 */
/*                                                                            */
/*                                                                            */

int view_updateregwindow(reg_window *regwindowptr)
{
GtkCList *clist;
char *element[] = { "SPSR", "00000000", "????" };/* needed when omitting SPSR */
reg_bank *regbank;

/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Adds all the register to the column list on the screen                     */

  void reg_ASCII(char *source, char *dest, int max)
  {
  int i;
  char c;

  for (i = 0; (i < regbank->width) && (i < max); i++)		// Limited @@@
    {
    if ((source[i] >= ' ') && (source[i] <= '~')) dest[i] = source[i];
    else                                          dest[i] = '.';
    }
  dest[i] = '\0';                                        /* String terminator */

  return;
  }

  void update_reg_clist()
  {
  int count;
  char *data0, *data1, data2[MAX_REG_SIZE+1];          /* "+1" for terminator */

  for (count = 0; count < regbank->number; count++)
    {                                        /* All registers in current bank */
    data0 = view_get_reg_name(count, regbank);

    if (regbank->width != 0)
      data1 = view_chararr2hexstrbe(regbank->width,
                                   &regbank->values[count * regbank->width]);
         /* retrieving values of registers and setting them to be hexadecimal */
    else
      if (regbank->values[count] != 0) data1 = "1"; else data1 = "0";

    reg_ASCII(&regbank->values[count * regbank->width], &data2[0], MAX_REG_SIZE);

    view_update_field(clist, count, 0, data0);     /*Address is static really */
    view_update_field(clist, count, 1, data1);
    view_update_field(clist, count, 2, data2);

    if (regbank->width != 0) g_free(data1);
    g_free(data0);
    }
  return;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Takes care of the dynamic behaviour of the current register bank           */
/*  "spsr_exists" is a global variable which should really be obtained by     */
/*   interrogating display @@@                                                */

  void display_or_omit_spsr_in_current(uchar *displayed_cpsr)
  {
  char ascii[MAX_REG_SIZE+1];

  if (regwindowptr->regbank_no == 0)                  /* Using "current" mode */
    {
    if (((displayed_cpsr[0]&0xF) == 0x0) || ((displayed_cpsr[0]&0xF) == 0xF))
      {               /* If the bottom 4 bits in CPSR is 0xF or 0x0 hide SPSR */
      if (spsr_exists)
        {      /* if we deal with 'current' and the value of CPSR is as above */
        gtk_clist_remove(clist, SPSR);
        spsr_exists = FALSE;                        /* unset flag as expected */
        }
      }
    else                                /* return SPSR to 'current' reg. bank */
      {
      if (!spsr_exists)             /* we don't want to add it more than once */
        {                                /* add SPSR - update data fields too */
        element[1] = view_chararr2hexstrbe(regbank->width,   /* Allocates ... */
                                          &regbank->values[SPSR*regbank->width]);
        reg_ASCII(&regbank->values[SPSR*regbank->width], &ascii[0], MAX_REG_SIZE);
        element[2] = &ascii[0];
        gtk_clist_insert(clist, SPSR, element);
        g_free(element[1]);
        spsr_exists = TRUE;            /* Indicate that SPSR is now displayed */
        }
      }
    }                                                               /* end if */
  return;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  void display_psr(int psr_ID, uchar *displayed_psr)
  {

  void display_psr_button(GtkWidget *button, int state)
    {
    GtkWidget *connid;
    connid = gtk_object_get_data(GTK_OBJECT(button), "connectid");
    gtk_signal_handler_block(GTK_OBJECT(button),   GPOINTER_TO_INT(connid));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), state);
    gtk_signal_handler_unblock(GTK_OBJECT(button), GPOINTER_TO_INT(connid));
    return;
    }


  GtkWidget *psr_menu;
  int i;

  /* The following sets the flagbuttons according to the CPSR or SPSR         */
  for (i=0; i<=3; i++)                            /* For all the flag buttons */
    {
    display_psr_button(flag_button[psr_ID][i], 
                                 (displayed_psr[3] & (1<<(i+4))) != 0);
                                              /* Start at 4th bit of 3rd byte */
    }
  display_psr_button(flag_button[psr_ID][4], 
                              (displayed_psr[0] & 0x80) != 0);      /* I flag */
  display_psr_button(flag_button[psr_ID][5],
                              (displayed_psr[0] & 0x40) != 0);      /* F flag */
#ifdef	THUMB								// BODGE @@@
  display_psr_button(flag_button[psr_ID][6],
                              (displayed_psr[0] & 0x20) != 0);      /* T flag */
#endif

   // Set menu entry according to mode

// Really find handle by some less "global" means  @@@
  if (psr_ID == 0) psr_menu = CPSR_menu_handle; else psr_menu = SPSR_menu_handle;

// Numbers inserted "by hand" below needs "automating"  @@@
  switch (displayed_psr[0] & 0x1F)                   /* inspect bottom 4 bits */
    {                                     /*  and set label according to mode */
    case 0x10: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 0); break;
    case 0x11: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 5); break;
    case 0x12: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 4); break;
    case 0x13: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 1); break;
    case 0x17: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 2); break;
    case 0x1B: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 3); break;
    case 0x1F: gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 6); break;
    default:   gtk_option_menu_set_history(GTK_OPTION_MENU(psr_menu), 7); break;
    }

  return;
  }

/*                                                                            */
/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int ret;

if (TRACE > 5) g_print("view_updateregwindow\n");

ret = 1;

if (regwindowptr->regbank_no == gtk_notebook_get_current_page(register_notebook))
  {                             /* Only do anything if this is the front page */
             /* Only works with single register window - handle is global @@@ */
  if ((board_get_regbank(regwindowptr->regbank_no))      /* get register bank */
   || (board_mini_ping() && board_get_regbank(regwindowptr->regbank_no)))
    {                                                        /*  successfully */
    clist = GTK_CLIST(regwindowptr->clist_ptr);
    gtk_clist_freeze(clist);

    regbank = &board->reg_banks[regwindowptr->regbank_no];
                                 /* the current register bank being refreshed */

    update_reg_clist();

    if (strcmp(board->cpu_name, "ARM") == 0) /* Following only applies to ARM */
      display_or_omit_spsr_in_current(&regbank->values[CPSR * regbank->width]);

    if (all_flag_windows_have_been_created)
      {  /* we only want to refresh those after the widgets have been created */
      display_psr(0, &regbank->values[CPSR * regbank->width]);
      display_psr(1, &regbank->values[SPSR * regbank->width]);
      }
    gtk_clist_thaw(clist);
    }
  else ret = 0;                                  /* Error in client interface */
  }

if ((ret == 1)                      // @@@ BODGE This routine currently relied
 && (regwindowptr->regbank_no != 0))// upon to update symbols for evaluator too
  {
  if (!board_get_regbank(0)                    /* get `current' register bank */
   && (board_mini_ping() && board_get_regbank(0)))
    ret = 0;
  }

return ret;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get the memory window pointer of a widget                                  */
/* (climb up the tree to get the parent who is a mem_window)                  */
/* inputs: the widget                                                         */
/* returns: the memory window pointer                                         */
/*                                                                            */

mem_window *view_getmemwindowptr(GtkWidget *widget)
{
mem_window *mem_window_ptr = NULL;                  /* initialise the pointer */

while (NULL == mem_window_ptr)                         /* step while not NULL */
  {
  mem_window_ptr = gtk_object_get_data(GTK_OBJECT(widget), "mem_window");
                                                  /* assignment to the parent */
  widget = (GTK_WIDGET(widget))->parent;
  }

return mem_window_ptr;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get the register window pointer of a widget                                */
/* (climb up the tree to get the parent who is a reg_window)                  */
/* inputs: the widget                                                         */
/* returns: the register window pointer                                       */
/*                                                                            */

reg_window *view_getregwindowptr(GtkWidget *widget)
{
reg_window *reg_window_ptr = NULL;                  /* initialise the pointer */

while (NULL == reg_window_ptr)                         /* step while not NULL */
  {
  reg_window_ptr = gtk_object_get_data(GTK_OBJECT(widget), "reg_window");
                                                  /* assignment to the parent */
  widget = (GTK_WIDGET(widget))->parent;
  }
return reg_window_ptr;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void view_update_field(GtkCList *clist, int row, int column, char *text)
{
char *test;

if (gtk_clist_get_text(clist, row, column, &test) == 0)
  gtk_clist_set_text(clist, row, column, text);         /* could not get text */
else                                         /* only update if it has changed */
  if (strcmp(text, test) != 0) gtk_clist_set_text(clist, row, column, text);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/*                                                                      */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
/*                     end of viewfuncs.c                               */
/************************************************************************/
