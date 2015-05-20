/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*                Name:           callbacks.h                           */
/*                Version:        1.5.0                                 */
/*                Date:           20/07/2007                            */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef CALLBACKS_H
#define CALLBACKS_H
#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include "definitions.h"
#include "view.h"

#define MEMORY_WINDOW   0    /* used when updating either memory or registers */
#define REGISTER_WINDOW 1    /* used when updating either memory or registers */


#define uchar unsigned char
#define REFRESH_PERIOD 300 /* The interval between each refresh on the screen */
                          /* i.e. how often the values on board are inspected */
#define POLLING_PERIOD 50
#define MAX_LENGTH 32    /* maximum length of message returned from the board */


#define FPGA_LOAD_PKT_LENGTH 0x100        /* POSITION */
#define FPGA_LOAD_BLK_LENGTH 0x100 /*Must be multiple of FPGA_LOAD_PKT_LENGTH */

#define FPGA_HEADER_BUFFER 0x80              /* Large enough for any header?  */

/* Exported prototypes only  (@@@ eventually)                                 */
void callback_refresh_toggle_from_menu(gpointer);
void callback_memwindow_elf_symbols_toggle(GtkToggleButton*, gpointer);
void callback_mode_selected_from_menu(GtkWidget*, gpointer);
void callback_flags_selected_from_menu(GtkWidget*, gpointer);

/* User has selected new mode to be chosen in the register bank notebook      */
void callback_clear_terminal(GtkButton*, gpointer);
void callback_button_load(GtkButton*, gpointer);
void callback_load_binary_address(GtkEntry*, gpointer);
void callback_load_binary(GtkButton*, gpointer);
void callback_button_compile(GtkButton*, gpointer);
void callback_button_ok_file(GtkButton*, gpointer);
void callback_fpgaerase(GtkButton*, gpointer);
                                     /* Called in features when erase pressed */
void callback_fpgaload(GtkButton*, gpointer);
                                  /* Called in features when download pressed */
void callback_fpgaupdate(GtkButton*, gpointer);
                                    /* Called in features when update pressed */

void callback_button_start(GtkButton*, gpointer);
void callback_button_stop(GtkButton*, gpointer);
void callback_button_walk(GtkButton*, gpointer);
void callback_button_continue(GtkButton*, gpointer);
void callback_button_ping(GtkButton*, gpointer);
void callback_start_toggle(GtkToggleButton*, gpointer);
void callback_rtf_toggle(GtkToggleButton*, gpointer);
void callback_refresh_toggle(GtkToggleButton*, gpointer);
void callback_button_reset(GtkButton*, gpointer);
void callback_refresh_selection(int, int, int);
void callback_global_refresh(void);
void callback_step_number(GtkEditable*, gpointer);
void callback_memwindow_length(mem_window*, gpointer);
void callback_memwindow_length2(GtkMenuItem*, gpointer);
void callback_memwindow_isa(GtkMenuItem*, gpointer);
void callback_memwindow_size_allocate(GtkCList*, gpointer);
//void callback_memwindow_listsize(GtkEditable*, gpointer);
void callback_memwindow_tabsize(GtkEditable*, gpointer);
//void callback_memwindow_no(GtkMenuItem*, gpointer);
void callback_memwindow_ascii_toggle(GtkToggleButton*, gpointer);
void callback_memwindow_dis_toggle(GtkToggleButton*, gpointer);
void callback_memwindow_inc_toggle(GtkToggleButton*, gpointer);
void callback_memwindow_full_toggle(GtkToggleButton*, gpointer);
void callback_memwindow_address(GtkEditable*, gpointer);
void callback_memwindow_hex(GtkEditable*, gpointer);
void callback_memwindow_ascii(GtkEditable*, gpointer);
void callback_memwindow_dis(GtkEditable*, gpointer);
void callback_memwindow_clist(GtkCList*, gint, gint, GdkEventButton*, gpointer);
                          /* called when user selects row from a memory frame */
//void callback_memory_window_row_selected (GtkCList*, gint, gint,
//                                          GdkEventButton*, gpointer);
//                           /* called when user selects row from memory window */

void callback_memwindow_key_press(GtkWidget *widget,
                                            GdkEventKey *event,
                                            gpointer user_data);
					    
void callback_memwindow_button_press(GtkWidget *widget,
                                            GdkEventButton *event,
                                            gpointer user_data);

void callback_memwindow_scroll_button_press(GtkWidget *widget,
                                            gpointer user_data);					    

void callback_memwindow_scrollmove(GtkAdjustment*, gpointer);
gboolean callback_memwindow_scrollrelease(GtkWidget*, gpointer, gpointer);
void callback_regwindow_hex(GtkEditable*, gpointer);
void callback_regwindow_clist(GtkCList*, gint, gint, GdkEventButton*, gpointer);

void callback_flag_pressed(GtkToggleButton*, gpointer);
typedef void (*callback_flag_function) (GtkToggleButton*, gpointer);

void callback_mode_selection(GtkMenuItem*, gpointer);
void callback_mode_selection_saved(GtkMenuItem*, gpointer);
void callback_reg_notebook_change(GtkButton*, gpointer);

/* The following is not to be used as a callback function, but to set memory  */
/*   window value width                                                       */
//void callback_set_memory_window_size(GtkMenuItem *menuitem, gpointer newvalue);

gint callback_terminal_keysnoop(GtkWidget*, GdkEventKey*, gpointer);
/* deals with events of keypresses */

gint callback_update_comms(gpointer);
gint callback_updateall(gpointer);

gint callback_console_update(gpointer, gint, GdkInputCondition);
void callback_comms(GtkEditable*, gpointer);
void callback_main_quit(gpointer, gpointer);

int compile_communication[2];
//FILE *view_console_port;

void callback_memory_window_create(gpointer, gpointer);
void callback_symbol_window_create(gpointer, gpointer);
void callback_memory_window_destroy(gpointer, gpointer);
void callback_symbol_window_destroy(gpointer, gpointer);

#endif

/*                                                                            */
/*----------------------------------------------------------------------------*/
/*                     end of callbacks.h                                     */
/******************************************************************************/
