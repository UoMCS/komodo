/******************************************************************************/
/*                Name:           breakcalls.c                                */
/*                Version:        1.5.0                                       */
/*                Date:           26/07/2007                                  */
/*                Callbacks concerned with breakpoints                        */
/*                                                                            */
/*============================================================================*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "global.h"

#include "breakcalls.h"
#include "breakview.h"
#include "interface.h"
#include "serial.h"
#include "misc.h"
#include "view.h"
#include "callbacks.h"


int toggleoff = 0;



int read_bkpt_line(gpointer user_data)
{
return breakpoint_line[GPOINTER_TO_INT(user_data) >> 0x14]
                     [(GPOINTER_TO_INT(user_data) >> 0x10) & 0xF];
}

int write_bkpt_line(gpointer user_data, int data)
{
breakpoint_line[GPOINTER_TO_INT(user_data) >> 0x14]
              [(GPOINTER_TO_INT(user_data) >> 0x10) & 0xF] = data;
}


/* Name sensibly when we find out what it does @@@ */
gpointer plugh(gpointer user_data, char *name)
{
return gtk_object_get_data(GTK_OBJECT(
                    view_breakwindow[GPOINTER_TO_INT(user_data) >> 0x14]
                                   [(GPOINTER_TO_INT(user_data) >> 0x10) & 0xF]
                                        ), name);
}


/******************************************************************************/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/* Called when a toggle in the breakpoint window is pressed                   */
/*                                                                            */

void breakpoint_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{
int worda, wordb;
int count;
trap_def trap;

if (TRACE > 5) g_print("breakpoint_toggled\n");

if (toggleoff) return;

board_micro_ping();                                               /* WHY? @@@ */

if (trap_get_status((GPOINTER_TO_INT(user_data) >> 0x10), &worda, &wordb))
  {
  if (VERBOSE) g_print("breakcrash 1 or 2\n");
  return;
  }

if ((worda >> read_bkpt_line(user_data)) & 1)
  {                            /* If breakpoint defined, read trap definition */
  if (!read_trap_defn(GPOINTER_TO_INT (user_data) >> 0x10,
                      read_bkpt_line(user_data),
                     &trap))
    return;

  trap.misc = trap.misc ^ (GPOINTER_TO_INT(user_data) & 0xFFFF);
  if (write_trap_defn(GPOINTER_TO_INT(user_data) >> 0x10, read_bkpt_line(user_data),
                      &trap) == 0)
    return;
  }
else
  {                  /* If breakpoint not definded, just send trap definition */
  trap.misc = ~(GPOINTER_TO_INT (user_data) & 0xFFFF);
  for (count = 0; count < board->memory_ptr_width; count++)
    { trap.addressA[count] = 0x00; trap.addressB[count] = 0xFF; }
  for (count = 0; count < 8; count++)
    { trap.dataA[count] = 0x00;    trap.dataB[count] = 0x00; }

  if (write_trap_defn(GPOINTER_TO_INT(user_data) >> 0x10, read_bkpt_line(user_data),
                      &trap) == 0)
    return;
  }

callback_global_refresh();
breakpoint_refresh((GPOINTER_TO_INT(user_data) >> 0x10) & 3);
breakpoint_select(GTK_CLIST(plugh(user_data, "clist")),
                   read_bkpt_line(user_data), 0, NULL, user_data);

return;
}

/*                                                                            */
/*                                                                            */
/******************************************************************************/



/******************************************************************************/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/* Set breakpoint activation (etc.) status ???                                */
/* Called when changing status of breakpoint ??? @@@                          */

void breakpoint_set(GtkToggleButton * togglebutton, gpointer user_data)
{
int worda, wordb;

if (TRACE > 5) g_print("breakpoint_set\n");

if (toggleoff) return;

board_micro_ping();

if (trap_get_status((GPOINTER_TO_INT(user_data) >> 0x10), &worda, &wordb))
  {
  if (VERBOSE)g_print ("breakcrash 8 or 9\n");
  return;
  }

if (GPOINTER_TO_INT(user_data) & 0xFFFF)
  {
  worda = 0;
  wordb = 1 << read_bkpt_line(user_data);
  }
else
  {
  worda = 1 << read_bkpt_line(user_data);
  wordb = ~wordb & (1 << read_bkpt_line(user_data));
  }
trap_set_status(GPOINTER_TO_INT(user_data) >> 0x10, worda, wordb);
                                                           /* Set trap status */
breakpoint_select(GTK_CLIST(plugh(user_data, "clist")),
                  read_bkpt_line(user_data), 0, NULL, user_data);

breakpoint_refresh((GPOINTER_TO_INT(user_data) >> 0x10) & 3);
callback_global_refresh();

return;
}

/*                                                                            */
/*                                                                            */
/******************************************************************************/




/******************************************************************************/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/* Called a row in the breakpoint window is selected                          */
/*                                                                            */

void breakpoint_select(GtkCList *clist,
                       gint row,
                       gint column,
                       GdkEvent *event,
                       gpointer user_data)
{
int worda, wordb;
char *text;
trap_def trap;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void button(char *name, int value)
  {                                   /* Masks off bottom bit (only) of value */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugh(user_data, name)),
                               value & 1);
  return;
  }

  void buttons(unsigned int value)      /* Attempt at starting some structure */
  {
  button("bit8",        value >>  8);
  button("bit16",       value >>  9);
  button("bit32",       value >> 10);
  button("bit64",       value >> 11);
  button("checkkernel", value >>  6);
  button("checkuser",   value >>  7);
  button("checkwrite",  value >>  4);
  button("checkread",   value >>  5);
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("breakpoint_select\n");

toggleoff = 1;
write_bkpt_line(user_data, row);

board_micro_ping();                                             /* WHY??? @@@ */

if (trap_get_status((GPOINTER_TO_INT(user_data) >> 0x10), &worda, &wordb))
  {
  if (VERBOSE) g_print ("breakcrash 10 or 11\n");
  return;
  }

if ((worda >> read_bkpt_line(user_data)) & 1)
  {
  if (!read_trap_defn(GPOINTER_TO_INT(user_data) >> 0x10, row, &trap))
    return;

  button("buttonactivate", wordb >> read_bkpt_line(user_data));

  text = view_chararr2hexstrbe(board->memory_ptr_width, trap.addressA);
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entryaddressa")), text);
  g_free(text);

  if (GPOINTER_TO_INT(user_data) >> 0x14)
    {
    toggleoff = 0;
    return;
    }

  gtk_option_menu_set_history(GTK_OPTION_MENU(plugh(user_data, "optionaddress")),
                             (trap.misc >> 2) & 1);

  text = view_chararr2hexstrbe (board->memory_ptr_width, trap.addressB);
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entryaddressb")), text);
  g_free(text);

  text = view_chararr2hexstrbe (8, trap.dataA);
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entrydataa")), text);
  g_free(text);

  gtk_option_menu_set_history(GTK_OPTION_MENU(plugh(user_data, "optiondata")),
                              trap.misc & 1);

  text = view_chararr2hexstrbe(8, trap.dataB);
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entrydatab")), text);
  g_free(text);

  buttons(trap.misc);
  }
else
  {
  button("buttonactivate", wordb >> read_bkpt_line(user_data));

  text = g_strnfill(board->memory_ptr_width << 1, '0');
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entryaddressa")), text);
  g_free(text);

  if (GPOINTER_TO_INT(user_data) >> 0x14)
    {
    toggleoff = 0;
    return;
    }

  gtk_option_menu_set_history(GTK_OPTION_MENU(plugh(user_data,
                                           "optionaddress")), 1);

  text = g_strnfill(board->memory_ptr_width << 1, 'F');
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entryaddressb")), text);
  g_free(text);

  text = g_strnfill(16, '0');
  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entrydataa")), text);
  g_free(text);

  gtk_option_menu_set_history(GTK_OPTION_MENU(plugh(user_data, "optiondata")), 1);
  text = g_strnfill(16, '0');

  gtk_entry_set_text(GTK_ENTRY(plugh(user_data, "entrydatab")), text);
  g_free(text);

  buttons(0xFFFFFFFF);
  }

toggleoff = 0;

return;
}

/*                                                                            */
/*                                                                            */
/******************************************************************************/


/******************************************************************************/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/* Called when a breakpoint window refresh is required                        */
/*                                                                            */

void breakpoint_refresh(int brwc)
{
int worda, wordb;
int count;
int column;
char *text;
trap_def trap;
GtkCList *clist;

  void xyzzy(int xxx)        /* Names await knowledge of the function :-( @@@ */
  {
  clist = GTK_CLIST(gtk_object_get_data
                      (GTK_OBJECT(view_breakwindow[xxx][brwc]), "clist"));

  if ((wordb >> count) & 1)
    {
    gtk_clist_set_pixmap(clist, count, 0, view_tick_pixmap, view_tick_bitmap);
    gtk_clist_set_foreground(clist, count,
                      &gtk_widget_get_default_style()->text[GTK_STATE_ACTIVE]);
    }
  else
    {
    gtk_clist_set_text(clist, count, 0, "");
    gtk_clist_set_foreground(clist, count, &view_greycolour);
    }

  text = view_chararr2hexstrbe(board->memory_ptr_width, trap.addressA);
  gtk_clist_set_text(clist, count, 1, text);
  g_free(text);

  return;
  }


  void zyxxy(int xxx)  /* As above @@@ */
  {
  clist = GTK_CLIST(gtk_object_get_data(
                      GTK_OBJECT(view_breakwindow[xxx][brwc]), "clist"));
  gtk_clist_set_text(clist, count, 0, "Deleted");
  gtk_clist_set_foreground(clist, count,
        &gtk_widget_get_default_style()->text[GTK_STATE_ACTIVE]);
                                               /* set back to original colour */
  if (!(wordb >> count) & 1)
    for (column = 31; column >= count; column--) gtk_clist_remove(clist, column);
  return;
  }


if (TRACE > 5) g_print("breakpoint_refresh: class %d\n", brwc);

clist = GTK_CLIST(gtk_object_get_data
                 (GTK_OBJECT(view_breakwindow[0][brwc]), "clist"));
gtk_clist_freeze(clist);
               /* Does what?  Not always undone (e.g. conditional returns @@@ */

if (trap_get_status(brwc, &worda, &wordb))
  {
  if (VERBOSE) g_print("breakcrash 17 or 18\n");
  return;
  }

for (count = 0; count < 32; count++)
  {
  if ((worda >> count) & 1)
    {
    board_micro_ping();
/* NOTE: the mask later applied to (brwc<<2) ---- ( & 0xC) did not exist in the past; see ver. 1.2.8 */
    if (!read_trap_defn(brwc, count, &trap)) return;

    xyzzy(1);                     /* May have side effects (e.g. on "clist" ) */
    xyzzy(0);

    if ((trap.misc & 0x4) != 0) text = "Mask";
    else                        text = "<= X <=";
    gtk_clist_set_text(clist, count, 2, text);

    text = view_chararr2hexstrbe(board->memory_ptr_width, trap.addressB);
    gtk_clist_set_text(clist, count, 3, text);
    g_free(text);

    text = view_chararr2hexstrbe(8, trap.dataA);
    gtk_clist_set_text(clist, count, 4, text);
    g_free(text);

    if ((trap.misc & 0x1) != 0) text = "Mask";
    else                        text = "<= X <=";
    gtk_clist_set_text(clist, count, 5, text);

    text = view_chararr2hexstrbe(8, trap.dataB);
    gtk_clist_set_text(clist, count, 6, text);
    g_free(text);

    {
    char *liltext[9] = { "", "", "", "", "", "", "", "", "" };

    liltext[8] = NULL;

    if ((trap.misc & 0xF00) && (trap.misc & 0xC0) && (trap.misc & 0x30))
      {
      if (0xF00 != (trap.misc & 0xF00))               /* If not all sizes ... */
        {
        if (trap.misc & 0x100) liltext[0] = "8b ";
        if (trap.misc & 0x200) liltext[1] = "16b ";
        if (trap.misc & 0x400) liltext[2] = "32b ";
        if (trap.misc & 0x800) liltext[3] = "64b ";
        }
      if (0xC0 != (trap.misc & 0xC0))                /* If not both modes ... */
        {
        if (trap.misc & 0x80) liltext[4] = "user ";
        if (trap.misc & 0x40) liltext[5] = "kernel ";
        }
      if (0x30 != (trap.misc & 0x30))
        {
        if (trap.misc & 0x20) liltext[6] = "read ";
        if (trap.misc & 0x10) liltext[7] = "write ";
        }
      }
    else
      liltext[0] = "DISABLED";

    text = g_strjoinv ("", liltext);
    gtk_clist_set_text (clist, count, 7, text);
    g_free(text);
    }
    }
  else
    {
    zyxxy(1);   /* Names awaited @@@ */
    gtk_clist_set_text(clist, count, 1, "");
    zyxxy(0);
    for (column=1;column<8;column++) gtk_clist_set_text(clist,count,column,"");
    }
  }
gtk_clist_thaw(clist);

return;
}

/*                                                                            */
/*                                                                            */
/******************************************************************************/


/******************************************************************************/
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/* Called when a value/address is entered in the window                       */
/*                                                                            */

void breakpoint_enterval(GtkEditable *editable, gpointer user_data)
{
trap_def trap;
int worda, wordb, count;
char *text;
uchar *hexval;
boolean error;     /* Hack to remove conditional returns; should be removable */

if (TRACE > 5) g_print("breakpoint_enterval\n");

error  = FALSE;
text   = gtk_editable_get_chars(editable, 0, -1);
hexval = g_new(uchar, board->memory_ptr_width);


if (trap_get_status((GPOINTER_TO_INT(user_data) >> 0x10), &worda, &wordb))
  {
  if (VERBOSE) g_print("breakcrash 24 or 25\n");
  }
else
  {
  if (!view_hexstr2chararr(MAX(board->memory_ptr_width, 8), text, hexval))
    {                                                 /* Get value from entry */
    board_mini_ping();
    if (VERBOSE)
      g_print("breakcrash 26. NOTE: could just be an invalid hex entry e.g. 321'S\n");
    }
  else
    {
    if ((worda >> read_bkpt_line(user_data)) & 1)
      {                        /* If breakpoint defined, read trap definition */
      if (!read_trap_defn(GPOINTER_TO_INT(user_data) >> 0x10,
                           read_bkpt_line(user_data), &trap))
        error = TRUE;
      }
    else
      {
      trap.misc = 0x00000FFF;             /* All accesses enabled, mask modes */
      for (count = 0; count < board->memory_ptr_width; count++)
        { trap.addressA[count] = 0x00; trap.addressB[count] = 0xFF; }
      for (count = 0; count < 8; count++)
        { trap.dataA[count] = 0x00;    trap.dataB[count] = 0x00; }
      }

    if (!error)
      {
      switch (GPOINTER_TO_INT(user_data) & 0xFFFF)
        {
        case 0: view_chararrCpychararr(8, hexval, trap.addressA); break;
        case 1: view_chararrCpychararr(8, hexval, trap.addressB); break;
        case 2: view_chararrCpychararr(8, hexval, trap.dataA);    break;
        case 3: view_chararrCpychararr(8, hexval, trap.dataB);    break;
        }

      if (write_trap_defn(GPOINTER_TO_INT(user_data) >> 0x10,
                          read_bkpt_line(user_data), &trap) != 0)
        {
        breakpoint_refresh((GPOINTER_TO_INT(user_data) >> 0x10) & 3);
        callback_refresh_selection(TRUE, FALSE, FALSE);/* Just memory windows */
        }
      }
    }
  }
g_free(hexval);
g_free(text);

return;
}

/*                                                                            */
/*                                                                            */
/******************************************************************************/


/*                              end of breakcalls.c                           */
/*============================================================================*/
