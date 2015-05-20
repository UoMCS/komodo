/******************************************************************************/
/*                Name:           view.c                                      */
/*                Version:        1.5.0                                       */
/*                Date:           15/08/2007                                  */
/*                General interface with the graphical front end.             */
/*                                                                            */
/*============================================================================*/
/* Memory size report "memory leak" removed 5/1/04                            */

#include <gdk/gdkkeysyms.h>
#include <string.h>

#include "definitions.h"
#include "global.h"

#if defined(CYCLECOUNT)
#include "cyclecount_ui.h"
#endif

#include "callbacks.h"
#include "view.h"
#include "interface.h"
#include "misc.h"
#include "serial.h"
#include "chump.h"
#include "breakview.h"
#include "breakcalls.h"
#include "config.h"
#include "Pixmaps/tick.xpm"
#include "Pixmaps/mulogo.xpm"
#include "Pixmaps/chump.xpm"
#include "Pixmaps/pkomodotitle.xpm"
#include "Pixmaps/pkomodologo.xpm"
#include "Pixmaps/pkomodoicon.xpm"
#include "Pixmaps/pkomodominiicon.xpm"

/* First attempt to extract window size constants from body of code.          */
#define MAIN_AREA_X  1000
#define MAIN_AREA_Y   580
#define FEATURES_X    400
#define FEATURES_Y    300
#define REG_WIDGET_X  200
#define REG_WIDGET_Y  400
#define FLAG_WIDGET_X 200
#define FLAG_WIDGET_Y  40
#define STATUS_BAR_X  700
#define STATUS_BAR_Y   20

#define  ADDR_ENTRY_MAX_CHAR     40
#define  ADDR_ENTRY_BOX_LENGTH  107
#define   HEX_ENTRY_MAX_CHAR     64
#define   HEX_ENTRY_BOX_LENGTH  200
#define ASCII_ENTRY_MAX_CHAR     30
#define ASCII_ENTRY_BOX_LENGTH   87
#define   DIS_ENTRY_MAX_CHAR    200
#define   DIS_ENTRY_BOX_LENGTH  257

typedef void (*callback_button_go_fn) (GtkButton *button, gpointer steps);
typedef enum {HORIZONTAL, VERTICAL} orientation;

int call_number_to_notebook = 0;
char *symbol_data[] = { "Symbol", "Value", "Type" };

    /* Will keep count of the number of calls to notebook in the display list */

mem_window_entry *view_memwindowlist = NULL;
reg_window_entry *view_regwindowlist = NULL;


const static int zero = 0;            /* Constants - pointers passed to these */
const static int one = 1;
const static int ten = 10;
const static int hundred = 100;
const static int thousand = 1000;
const static int ten_thousand = 10000;
const static int hundred_thousand = 100000;
const static int million = 1000000;

const static int menu_current_mode     = 0;	// These need `generalising' @@@
const static int menu_user_mode        = 1;
const static int menu_supervisor_mode  = 2;
const static int menu_abort_mode       = 3;
const static int menu_undefined_mode   = 4;
const static int menu_irq_mode         = 5;
const static int menu_fiq_mode         = 6;

const static int menu_cpsr_mode        = 0;	// These need `generalising' @@@
const static int menu_spsr_mode        = 1;


/* Local prototypes */
GtkWidget *view_create_filebar(void);
GtkWidget *view_create_featurewindow(void);
GtkWidget *view_create_xfpgasegment(void);
GtkWidget *view_create_mem_topline(mem_window*, GtkWidget*, mem_win_type, int,
                                                GtkWidget*, int);
void       view_mem_display_panel(mem_window*, GtkWidget*, int, mem_win_type);
GtkWidget *view_create_flag_frame(int);
GtkWidget *view_create_aboutwindow(void);
GtkWidget *view_create_prog_ctrl(void);
GtkWidget *view_create_memory_clist(mem_win_type, int, GtkWidget*, int);
GtkWidget *view_create_register_clist(int);
GtkWidget *view_create_console(void);
GtkWidget *view_create_comms(feature*);
GtkWidget *view_parse_list(void);
GtkWidget *view_create_menubar(void);
GtkWidget *view_create_status_bar(void);  /* Creates the status bar at bottom */
GtkWidget *view_create_mov_split(orientation);
GtkWidget *new_box(gboolean, gint, orientation);
GtkWidget *status_message(char*, GtkWidget*, gboolean, gboolean, guint);
GtkWidget *view_separator(GtkMenu*);
void       view_refresh_symbol_clist(GtkWidget*);

GtkWidget *column_label(char*, GtkWidget*, int, int);
GtkWidget *push_button(char*, GtkWidget*, gboolean, gboolean, guint);
GtkWidget *create_submenu_entry(GtkWidget*, char*,
                                callback_button_go_fn, gpointer);
GtkWidget *entry_box(GtkWidget*, int, int, callback_button_go_fn);
                                         /* check "typing" @@@ */

/* Global variables for this file only */
GtkWidget *view_maincontainer;   /* the actual container within the(?) window */
GtkWidget *window_about;                       /* Pointer to the about window */
GtkWidget *window_feature;                   /* Pointer to the feature window */

GtkWidget *view_steplabel; /* the labels that reports the current step number */

GtkWidget *centry_compile;          /* the current file entry for compilation */
//GtkWidget *centry_load;                 /* the current file entry for loading */


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the main KMD main window                                           */
/*                                                                            */

void view_create_mainwindow(void)                       /* Create main window */
{
GtkWidget *top_strip;
GtkWidget *window_body;
GtkWidget *menubar;
GtkWidget *prog_ctrl;
GtkWidget *filebar;
GtkWidget *status_bar;

// GdkGeometry hints;


if (TRACE > 5) g_print("view_create_mainwindow\n");

view_tooltips = gtk_tooltips_new();

view_tick_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                           &view_tick_bitmap, NULL, tick_xpm);
view_mulogo_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                           &view_mulogo_bitmap, NULL,
                                           mulogo_xpm);
view_komodotitle_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                           &view_komodotitle_bitmap, NULL,
                                           komodotitle_xpm);
view_komodoicon_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                           &view_komodoicon_bitmap, NULL,
                                           komodoicon_xpm);
view_chump_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                           &view_chump_bitmap, NULL,
                                           chump_xpm);
view_komodominiicon_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                           &view_komodominiicon_bitmap, NULL,
                                           komodominiicon_xpm);

//   hints.base_width  = 990;
//   hints.base_height = 700;
//   hints.max_height  = 700;
//   hints.max_width   = 990;
//   hints.min_width   = 990;
//   hints.min_height  = 700;

view_mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);         /* Make window */
gtk_window_set_title(GTK_WINDOW(view_mainwindow),
                     PACKAGE " " VERSION " " DATE
                     " (C) Manchester University");
gtk_container_set_border_width(GTK_CONTAINER(view_mainwindow), 2);
// [JNZ] gtk_window_set_default_size(GTK_WINDOW(view_mainwindow), 980, 500);
// [JNZ] gtk_window_set_position(GTK_WINDOW(view_mainwindow), GTK_WIN_POS_CENTER);

//   gtk_window_set_geometry_hints(GTK_WINDOW(view_mainwindow),
//   GTK_WIDGET(view_mainwindow),
//   &hints,
//   GDK_HINT_MAX_SIZE|GDK_HINT_MIN_SIZE|GDK_HINT_BASE_SIZE);

gtk_signal_connect(GTK_OBJECT(view_mainwindow), "destroy",  /* quit if closed */
                   GTK_SIGNAL_FUNC(callback_main_quit), NULL);
gtk_window_set_policy(GTK_WINDOW(view_mainwindow), FALSE, TRUE, FALSE);

view_maincontainer = new_box(FALSE, 3, VERTICAL);
gtk_container_add(GTK_CONTAINER(view_mainwindow), view_maincontainer);

window_about   = view_create_aboutwindow();         /* Create an about window */
window_feature = view_create_featurewindow();     /* Create a features window */
filebar        = view_create_filebar();
prog_ctrl      = view_create_prog_ctrl();
menubar        = view_create_menubar();           /* Depends on the foregoing */
status_bar     = view_create_status_bar();

top_strip = new_box(FALSE, 3, HORIZONTAL); /* Make a strip for menu & buttons */
gtk_container_set_border_width(GTK_CONTAINER(top_strip), 3);

gtk_box_pack_start(GTK_BOX(view_maincontainer), menubar,   FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(view_maincontainer), top_strip, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(top_strip), prog_ctrl, TRUE, TRUE, 0);
                                               /* add the program control bar */
gtk_box_pack_start(GTK_BOX(top_strip), filebar, TRUE, TRUE, 0);
                                                          /* add the file bar */
//  [JNZ `packs' status bar here] @@@


window_body = view_parse_list();  /* Fill main area as specified by ".komodo" */

gtk_widget_set_usize(window_body, MAIN_AREA_X, MAIN_AREA_Y);/* Size main area */

// [JNZ]  gtk_box_pack_start(GTK_BOX(view_maincontainer), window_body, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(view_maincontainer), window_body, TRUE, TRUE, 0);

gtk_box_pack_end(GTK_BOX(view_maincontainer), status_bar, FALSE, FALSE, 0);
                                                             /* Bar at bottom */

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the menu bar                                                       */
/*                                                                            */

GtkWidget *view_create_menubar(void)
{						// Tidy, order and comment @@@
GtkWidget *menu_bar;
GtkWidget *file_bar;
GtkWidget *actions_bar;
GtkWidget *special_bar;
GtkWidget *mode_bar;
GtkWidget *flags_bar;
GtkWidget *help_bar;
GtkWidget *hbox;
GtkWidget *file_submenu;
GtkWidget *special_submenu;
GtkWidget *actions_submenu;
GtkWidget *mode_submenu;
GtkWidget *help_submenu;
GtkWidget *walk_submenu;
GtkWidget *multi_step_submenu;
GtkWidget *flags_submenu;
GtkWidget *compile;
GtkWidget *load;
  //GtkWidget *browse_compile;
     /* these may be used for the corresponding entries in the menu which are */
  //GtkWidget *browse_load;                               /* currently unused */
GtkWidget *quit;
GtkWidget *features;
GtkWidget *simple_breakpoints;
GtkWidget *breakpoints;
GtkWidget *simple_watchpoints;
GtkWidget *watchpoints;
GtkWidget *run;
GtkWidget *stop;
GtkWidget *single_step;
GtkWidget *multi_step;
GtkWidget *toggle_refresh;
GtkWidget *walk;
GtkWidget *continue_running;
GtkWidget *reset;
GtkWidget *ping;
GtkWidget *about;
                                                     /* Submenu entry handles */
GtkWidget *walk_as_below, *walk_1,  *walk_10,  *walk_100, *walk_1000;
GtkWidget *multi_step_as_below, *multi_step_10,    *multi_step_100;
GtkWidget *multi_step_1000,     *multi_step_10000, *multi_step_100000;
GtkWidget *multi_step_1000000;
GtkWidget *current, *user, *supervisor, *abort, *undefined, *irq, *fiq;
GtkWidget *saved_flags, *current_flags;

GtkWidget *memory_window_entry;      /* The memory window creation menu entry */
GtkWidget *source_window_entry;      /* The source window creation menu entry */
GtkWidget *symbol_window_entry;      /* The symbol window creation menu entry */
GtkWidget *logoitem;        /* The logo in the top right corner of the window */
GtkWidget *separator;      /* Never read so all separator handles dumped here */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *create_submenu_entry2(GtkWidget *submenu, char *string,
                                   gpointer object)
  {
  GtkWidget *handle;

  handle = gtk_menu_item_new_with_label(_(string));
  gtk_widget_show(handle);
  gtk_menu_append(GTK_MENU(submenu), handle);
  gtk_signal_connect_object(GTK_OBJECT(handle), "activate",
                            gtk_widget_show, object);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *create_menubar_entry(char *string, GtkWidget *submenu)
  {
  GtkWidget *handle;

  handle = gtk_menu_item_new_with_label(_(string));
  gtk_widget_show(handle);
  gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), handle);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(handle), submenu);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *create_subsubmenu(GtkWidget *submenu, char *string,
                               GtkWidget *subsubmenu)
  {
  GtkWidget *handle;

  handle = gtk_menu_item_new_with_label(string);
  gtk_menu_item_configure(GTK_MENU_ITEM(handle), 0, TRUE);      /* show arrow */
  gtk_widget_show(handle);
  gtk_menu_append(GTK_MENU(submenu), handle);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(handle), subsubmenu);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void trap_window_big(GtkWidget *handle, int line, int breakpoint_mask)
                                                                /* good name? */
  {                                    /* This should be in breakview.c   @@@ */
  char *startdata[] = { "", "Address A", "<X<", "Address B",
                               "Data A", "<X<", "Data B", "other" };
  GtkWidget *clist = gtk_object_get_data(GTK_OBJECT(handle), "clist");
  int temp;

  gtk_signal_connect(GTK_OBJECT
                     (GTK_MENU_SHELL
                      (gtk_option_menu_get_menu
                       (GTK_OPTION_MENU
                        (gtk_object_get_data
                         (GTK_OBJECT (handle),
                          "optionaddress"))))), "selection-done",
                     GTK_SIGNAL_FUNC (breakpoint_toggled),
                     GINT_TO_POINTER (breakpoint_mask | 0x4));
  gtk_signal_connect(GTK_OBJECT
                     (GTK_MENU_SHELL
                      (gtk_option_menu_get_menu
                       (GTK_OPTION_MENU
                        (gtk_object_get_data
                         (GTK_OBJECT (handle),
                          "optiondata"))))), "selection-done",
                     GTK_SIGNAL_FUNC (breakpoint_toggled),
                     GINT_TO_POINTER (breakpoint_mask | 0x1));

  for (temp = 0; temp < MAX_NO_OF_TRAPS; temp++)
    gtk_clist_append(GTK_CLIST(clist), startdata);
  gtk_widget_set_style(clist, fixed_style);

  breakpoint_select(GTK_CLIST(gtk_object_get_data(GTK_OBJECT(handle), "clist")),
                   line, 0, NULL, NULL);
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void trap_window_small(char *title, int index, int breakpoint_mask)
                                                                /* Good name? */
  {
  GtkWidget *clist;
  int temp;
  char *startdata[] = { "", "Address" };

  view_breakwindow[1][index] = create_simplebreakwindow(title, breakpoint_mask);
  gtk_widget_ref (view_breakwindow[1][index]);

  simple_breakpoints = create_submenu_entry2(special_submenu, title,
                                        GTK_OBJECT(view_breakwindow[1][index]));

  clist = gtk_object_get_data(GTK_OBJECT(view_breakwindow[1][index]), "clist");

  for (temp = 0; temp < MAX_NO_OF_TRAPS; temp++)
    gtk_clist_append(GTK_CLIST(clist), startdata);
  gtk_widget_set_style(clist, fixed_style);

  breakpoint_line[1][index] = 0;
  breakpoint_refresh(index);
  breakpoint_select(GTK_CLIST(gtk_object_get_data(
                        GTK_OBJECT(view_breakwindow[1][index]), "clist")),
                    breakpoint_line[1][index], 0, NULL, NULL);
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("create_menubar\n");

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

walk_submenu = gtk_menu_new();                         /* Create walk submenu */

walk_as_below = create_submenu_entry(walk_submenu,
                                     "Steps as in box                F9",
                                     callback_button_walk,
                                     (gpointer) &view_step_number);

separator = view_separator(GTK_MENU (walk_submenu));

walk_1    = create_submenu_entry(walk_submenu, "1 step at a time",
                                 callback_button_walk, (gpointer) &one);
walk_10   = create_submenu_entry(walk_submenu, "10 steps at a time",
                                 callback_button_walk, (gpointer) &ten);
walk_100  = create_submenu_entry(walk_submenu, "100 steps at a time",
                                 callback_button_walk, (gpointer) &hundred);
walk_1000 = create_submenu_entry(walk_submenu, "1000 steps at a time",
                                 callback_button_walk, (gpointer) &thousand);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

multi_step_submenu  = gtk_menu_new();                   /* multi-step submenu */

multi_step_as_below = create_submenu_entry(multi_step_submenu,
                                           "Steps as in box                F8",
                                           callback_button_start,
                                           (gpointer) &view_step_number);

separator           = view_separator(GTK_MENU (multi_step_submenu));

multi_step_10       = create_submenu_entry(multi_step_submenu, "10 steps",
                           callback_button_start, (gpointer) &ten);
multi_step_100      = create_submenu_entry(multi_step_submenu, "100 steps",
                           callback_button_start, (gpointer) &hundred);
multi_step_1000     = create_submenu_entry(multi_step_submenu, "1000 steps",
                           callback_button_start, (gpointer) &thousand);
multi_step_10000    = create_submenu_entry(multi_step_submenu, "10 000 steps",
                           callback_button_start, (gpointer) &ten_thousand);
multi_step_100000   = create_submenu_entry(multi_step_submenu, "100 000 steps",
                           callback_button_start, (gpointer) &hundred_thousand);
multi_step_1000000  = create_submenu_entry(multi_step_submenu, "1 000 000 steps",
                           callback_button_start, (gpointer) &million);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

file_submenu = gtk_menu_new ();                               /* file submenu */

/* note: the following entries that are left out can be activated and diverted*/
/* to the appropriate callbacks  EH???                                        */

load    = create_submenu_entry(file_submenu, "Load file          ",
                               callback_button_load, centry_load);
compile = create_submenu_entry(file_submenu, "Compile file",
                               callback_button_compile, centry_compile);

separator = view_separator(GTK_MENU(file_submenu));

quit    = create_submenu_entry(file_submenu, "Quit Program",
                               GTK_SIGNAL_FUNC(callback_main_quit), NULL);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

special_submenu = gtk_menu_new();                          /* special submenu */

view_breakwindow[0][0] = create_breakwindow("Breakpoints", 0x00000);

breakpoints = create_submenu_entry2(special_submenu, "Breakpoints",
                                    GTK_OBJECT(view_breakwindow[0][0]));
breakpoint_line[0][0] = 0;                   /* Initially highlighted(?) line */
trap_window_big(view_breakwindow[0][0], breakpoint_line[0][0], 0x00000);

trap_window_small("Simple Breakpoints", 0, 0x100000);

separator = view_separator(GTK_MENU(special_submenu));

view_breakwindow[0][1] = create_breakwindow("Watchpoints", 0x10000);

watchpoints = create_submenu_entry2(special_submenu, "Watchpoints",
                                    GTK_OBJECT(view_breakwindow[0][1]));
breakpoint_line[0][1] = 0;                   /* Initially highlighted(?) line */
trap_window_big(view_breakwindow[0][1], breakpoint_line[0][1], 0x10000);

trap_window_small("Simple Watchpoints", 1, 0x110000);

separator = view_separator(GTK_MENU(special_submenu));

memory_window_entry = create_submenu_entry(special_submenu, "Memory Window",
                       GTK_SIGNAL_FUNC(callback_memory_window_create),
                       (gpointer) MEM_WIN_DUMP);

source_window_entry = create_submenu_entry(special_submenu, "Source Window",
                       GTK_SIGNAL_FUNC(callback_memory_window_create),
                       (gpointer) MEM_WIN_SOURCE);

symbol_window_entry = create_submenu_entry(special_submenu, "Symbol Window",
                       GTK_SIGNAL_FUNC(callback_symbol_window_create),
                       (gpointer) 0);

features = create_submenu_entry2(special_submenu, "Features",
                                 GTK_OBJECT(window_feature));

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

actions_submenu = gtk_menu_new();                          /* actions submenu */

reset = create_submenu_entry(actions_submenu, "Reset",
                GTK_SIGNAL_FUNC(callback_button_reset), NULL);
ping  = create_submenu_entry(actions_submenu, "Ping",
                GTK_SIGNAL_FUNC(callback_button_ping), NULL);
run   = create_submenu_entry(actions_submenu,
                               "Run                                    F5",
                GTK_SIGNAL_FUNC(callback_button_start), (gpointer) &zero);
stop  = create_submenu_entry(actions_submenu,
                               "Stop                                   F6",
                GTK_SIGNAL_FUNC(callback_button_stop), NULL);
continue_running = create_submenu_entry(actions_submenu,
                               "Continue                          F10",
                GTK_SIGNAL_FUNC(callback_button_continue), NULL);
single_step = create_submenu_entry(actions_submenu,
                               "Single-Step                       F7",
                GTK_SIGNAL_FUNC(callback_button_start), (gpointer) &one);

separator = view_separator(GTK_MENU(actions_submenu));

create_subsubmenu(actions_submenu, "Multi-Step", multi_step_submenu);

separator = view_separator(GTK_MENU(actions_submenu));

create_subsubmenu(actions_submenu, "Walk", walk_submenu);

separator = view_separator(GTK_MENU(actions_submenu));

toggle_refresh = create_submenu_entry(actions_submenu, 
                         "Toggle refresh on/off    ",
                          GTK_SIGNAL_FUNC(callback_refresh_toggle_from_menu),
                          NULL);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

mode_submenu = gtk_menu_new();                                /* mode submenu */

/* Names, number of entries etc. are not taken from config. file @@@ */

current    = create_submenu_entry(mode_submenu, "Current             ",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_current_mode);

separator  = view_separator(GTK_MENU(mode_submenu));

user       = create_submenu_entry(mode_submenu, "User/System         ",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_user_mode);
supervisor = create_submenu_entry(mode_submenu, "Supervisor",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_supervisor_mode);
abort      = create_submenu_entry(mode_submenu, "Abort",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_abort_mode);
undefined  = create_submenu_entry(mode_submenu, "Undefined",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_undefined_mode);
irq        = create_submenu_entry(mode_submenu, "IRQ",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_irq_mode);
fiq        = create_submenu_entry(mode_submenu, "FIQ",
                  GTK_SIGNAL_FUNC(callback_mode_selected_from_menu),
                                  (gpointer) &menu_fiq_mode);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

flags_submenu = gtk_menu_new();                              /* flags submenu */

current_flags = create_submenu_entry(flags_submenu,"Current Flags             ",
                     GTK_SIGNAL_FUNC(callback_flags_selected_from_menu),
                                     (gpointer) &menu_cpsr_mode);
saved_flags   = create_submenu_entry(flags_submenu,"Saved Flags",
                     GTK_SIGNAL_FUNC(callback_flags_selected_from_menu),
                                     (gpointer) &menu_spsr_mode);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

help_submenu = gtk_menu_new();                             /* actions submenu */

about = create_submenu_entry2(help_submenu, "About              ",
                              GTK_OBJECT(window_about));

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

hbox = new_box(FALSE, 2, HORIZONTAL); /* Container of menu entries themselves */

menu_bar = gtk_menu_bar_new();                   /* Create menu bar `buttons' */
gtk_menu_bar_set_shadow_type(GTK_MENU_BAR(menu_bar), GTK_SHADOW_ETCHED_IN);
gtk_widget_ref(menu_bar);
gtk_widget_show(menu_bar);

file_bar    = create_menubar_entry("File",       file_submenu);
actions_bar = create_menubar_entry("Actions", actions_submenu);
special_bar = create_menubar_entry("Special", special_submenu);
mode_bar    = create_menubar_entry("Reg. bank",  mode_submenu);
flags_bar   = create_menubar_entry("Flags",     flags_submenu);
help_bar    = create_menubar_entry("Help",       help_submenu);

logoitem=gtk_pixmap_new(view_komodominiicon_pixmap,view_komodominiicon_bitmap);
gtk_widget_show(logoitem);
gtk_widget_ref(logoitem);

gtk_box_pack_start(GTK_BOX(hbox), menu_bar, FALSE, FALSE, 0);
// [JNZ]  gtk_box_set_spacing(GTK_BOX(hbox), 700);
// [JNZ]  gtk_box_pack_start(GTK_BOX(hbox), logoitem, FALSE, FALSE, 0);
gtk_box_pack_end(GTK_BOX(hbox), logoitem, FALSE, FALSE, 0);

return hbox;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the file bar                                                       */
/*                                                                            */

GtkWidget *view_create_filebar(void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *button_compile;
  GtkWidget *button_load;
  GtkWidget *button_findcompile;
  GtkWidget *button_findload;
  GtkWidget *fileselection;
  GtkWidget *dialog_vbox1;
  GtkWidget *label;
  GtkWidget *dialog_action_area1;
  GtkWidget *buttonok;
  GtkWidget *buttoncancel;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *filebar_combo(GtkWidget *action_button,
                           callback_button_go_fn function,
                           GtkWidget *box)
  {
  GtkWidget *combo_handle, *entry_handle;

  combo_handle = gtk_combo_new();
  gtk_widget_ref(combo_handle);
  gtk_widget_show(combo_handle);
  gtk_box_pack_start(GTK_BOX(box), combo_handle, TRUE, TRUE, 0);
  entry_handle = GTK_COMBO(combo_handle)->entry;
  gtk_widget_ref(entry_handle);
  gtk_widget_show(entry_handle);

  gtk_object_set_data(GTK_OBJECT(combo_handle), "list", NULL);
  gtk_signal_connect(GTK_OBJECT(action_button), "clicked", function, entry_handle);
  return entry_handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *filebar_browser(char *string, char *completion,
                             GtkObject *object,
                             GtkWidget *parameter)
  {
  GtkWidget *handle;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;

  handle = gtk_file_selection_new(string);
  gtk_window_set_transient_for(GTK_WINDOW(handle), GTK_WINDOW(view_mainwindow));
                        /* Ensure features window stays on top of main window */
					// Not sure that this is correct choice @@@ 

  gtk_container_set_border_width(GTK_CONTAINER(handle), 10);
  ok_button = GTK_FILE_SELECTION(handle)->ok_button;
  gtk_widget_show(ok_button);
  GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
  cancel_button = GTK_FILE_SELECTION(handle)->cancel_button;
  gtk_widget_show(cancel_button);
  GTK_WIDGET_SET_FLAGS(cancel_button, GTK_CAN_DEFAULT);
  gtk_file_selection_complete(GTK_FILE_SELECTION(handle), completion);


  gtk_signal_connect_object(GTK_OBJECT(handle), "delete_event",
                            GTK_SIGNAL_FUNC(gtk_widget_hide), 
                            GTK_OBJECT(handle)); 
  gtk_signal_connect_object(object, "clicked",
                            GTK_SIGNAL_FUNC(gtk_widget_show),
                            GTK_OBJECT(handle));
  gtk_signal_connect_object(GTK_OBJECT(cancel_button), "clicked",
                            GTK_SIGNAL_FUNC(gtk_widget_hide),
                            GTK_OBJECT(handle));
  gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
                            GTK_SIGNAL_FUNC(gtk_widget_hide),
                            GTK_OBJECT(handle));
  gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
                     GTK_SIGNAL_FUNC(callback_button_ok_file), parameter);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("view_create_filebar\n");

vbox = new_box(FALSE, 2, VERTICAL);
hbox = new_box(FALSE, 4, HORIZONTAL);
gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

button_compile = push_button("Compile ->", hbox, FALSE, FALSE, 4);
centry_compile = filebar_combo(button_compile, callback_button_compile, hbox);

button_findcompile = push_button("Browse...", hbox, FALSE, FALSE, 0);

hbox = new_box(FALSE, 4, HORIZONTAL);

button_load = push_button("Load ->", hbox, FALSE, FALSE, 4);
centry_load = filebar_combo(button_load, callback_button_load, hbox);

button_findload = push_button("Browse...", hbox, FALSE, FALSE, 0);

view_fileerror = gtk_dialog_new();
gtk_object_set_data(GTK_OBJECT(view_fileerror), "view_fileerror",
                     view_fileerror);
gtk_window_set_title(GTK_WINDOW(view_fileerror), _("File load error"));


#ifdef GTK2
  gtk_window_set_type_hint(GTK_WINDOW(view_fileerror),
                            GDK_WINDOW_TYPE_HINT_DIALOG);
#endif

#ifndef GTK2
  GTK_WINDOW(view_fileerror)->type = GTK_WINDOW_DIALOG;
#endif

gtk_window_set_position(GTK_WINDOW(view_fileerror), GTK_WIN_POS_CENTER);
gtk_window_set_policy(GTK_WINDOW(view_fileerror), FALSE, TRUE, TRUE);

dialog_vbox1 = GTK_DIALOG(view_fileerror)->vbox;
gtk_widget_show(dialog_vbox1);

label = status_message("This is not a recognised file type.\n"
                       "Do you wish to load it as a binary file?",
                       dialog_vbox1, TRUE, TRUE, 0);
                             /* This is the contents of the file error window */

view_binary_load_address = gtk_entry_new_with_max_length(64);
gtk_widget_ref(view_binary_load_address);        /* Look at "entry_box" again */
gtk_widget_show(view_binary_load_address);
gtk_box_pack_start(GTK_BOX(dialog_vbox1),view_binary_load_address,TRUE,TRUE,0);
gtk_entry_set_text(GTK_ENTRY(view_binary_load_address), "00000000");

label = status_message("Valid:0", dialog_vbox1, TRUE, TRUE, 0);
                             /* This is the contents of the file error window */

dialog_action_area1 = GTK_DIALOG(view_fileerror)->action_area;
gtk_widget_show(dialog_action_area1);
gtk_container_set_border_width(GTK_CONTAINER(dialog_action_area1), 10);
       /* Action area is the bottom box which holds the OK and Cancel buttons */
buttonok     = push_button("OK",     dialog_action_area1, TRUE, TRUE, 0);
buttoncancel = push_button("Cancel", dialog_action_area1, TRUE, TRUE, 0);

gtk_signal_connect_object(GTK_OBJECT(view_fileerror), "delete_event",
                      /* If window is closed then just hide and don't destroy */
                           GTK_SIGNAL_FUNC(gtk_widget_hide),
                           GTK_OBJECT(view_fileerror));

gtk_signal_connect(GTK_OBJECT(buttonok), "clicked",    /* If ok pressed, load */
                    GTK_SIGNAL_FUNC(callback_load_binary), centry_load);

gtk_signal_connect_object(GTK_OBJECT(buttoncancel), "clicked",
                                               /* If cancel pressed then hide */
                           GTK_SIGNAL_FUNC(gtk_widget_hide),
                           GTK_OBJECT(view_fileerror));

gtk_signal_connect_after(GTK_OBJECT(view_binary_load_address), "changed",
                          GTK_SIGNAL_FUNC(callback_load_binary_address),
                          GTK_OBJECT(label));

gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

                        /* These are the two file selection satellite windows */
fileselection = filebar_browser("Select Source File",
                                ("*" SOURCE_EXT),      /* Concatenate strings */
                                GTK_OBJECT(button_findcompile),
                                centry_compile);

fileselection = filebar_browser("Select Object File",
                                ("*" OBJECT_EXT),      /* Concatenate strings */
                                GTK_OBJECT(button_findload),
                                centry_load);

return vbox;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the features window                                                */
/*                                                                            */

GtkWidget *view_create_featurewindow(void)
{
GtkWidget *window;
GtkWidget *notebook;
GtkWidget *label;
GtkWidget *segment;
int count;
boolean feature_recognised;

if (TRACE > 5) g_print("view_create_featurewindow\n");

window = gtk_window_new(GTK_WINDOW_TOPLEVEL);        /* Build features window */
gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(view_mainwindow));
                        /* Ensure features window stays on top of main window */
					// Not sure that this is correct choice @@@ 

gtk_window_set_default_size(GTK_WINDOW(window), FEATURES_X, FEATURES_Y);
gtk_object_set_data(GTK_OBJECT(window), "window", window);
gtk_window_set_title(GTK_WINDOW(window), _("Features"));

if (board->feature_count != 0)                           /* If features exist */
  {
  notebook = gtk_notebook_new();                 /* to hold multiple features */
  gtk_widget_ref(notebook);
  gtk_object_set_data_full(GTK_OBJECT(window), "notebook", notebook,
                          (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show(notebook);
  gtk_container_add(GTK_CONTAINER(window), notebook);
                                                 /* attach notebook to window */
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  for (count = 0; count < board->feature_count; count++)
    {                                            /* Step through all features */
    feature_recognised = TRUE;                /* Assume feature will be found */
    switch (board->pArray_F[count].type)
      {
      case XILINX_FPGA:
        if (VERBOSE) g_print("%s detected\n", board->pArray_F[count].name);
        segment = view_create_xfpgasegment();
        break;

      case TERMINAL:
        if (VERBOSE) g_print("Terminal detected\n");
        segment = view_create_comms(&board->pArray_F[count]);
        break;

      case COUNTERS:
        if (VERBOSE) g_print("Cycle counters detected\n");
//##        segment = view_create_cyclecount(&board->pArray_F[count]);
        break;

      default:
        if (VERBOSE) g_print("Unknown feature!\n");
        feature_recognised = FALSE;                   /* Feature wasn't found */
        break;
      }

    if (feature_recognised)
      {
      gtk_object_set_data(GTK_OBJECT(segment),"feature",GINT_TO_POINTER(count));
      label = gtk_label_new(board->pArray_F[count].name);
                                                       /* Get name of feature */
      gtk_widget_ref(label);
      gtk_widget_show(label);
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), segment, label);
      }
 /* Put the name of the feature in the tab and add that feature to the window */
    }
  }
else
  {
  label = gtk_label_new("There are no features available");
  gtk_widget_ref(label);
  gtk_widget_show(label);
  gtk_misc_set_padding(GTK_MISC(label), 100, 50);
  gtk_container_add(GTK_CONTAINER(window), label);
  }

gtk_signal_connect_object(GTK_OBJECT(window), "delete_event",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(window));
return window;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* the following creates a feature entry for the FPGA                         */
/*                                                                            */

GtkWidget *view_create_xfpgasegment(void)
{
GtkWidget *container;
GtkWidget *hbox;                          /* Handle for hbox currently in use */
GtkWidget *table;                        /* Handle for table currently in use */
GtkWidget *buttonupdate;
GtkWidget *combo;
GtkWidget *fileentry;
GtkWidget *browse;
GtkWidget *labelfilename;
GtkWidget *labelfiledate;
GtkWidget *labelfiletime;
GtkWidget *buttondownload;
GtkWidget *progressbar;
GtkWidget *labelfpganame;
GtkWidget *labelfpgadate;
GtkWidget *labelfpgatime;
GtkWidget *buttonerase;

GtkWidget *fileselection;
GtkWidget *ok_button;
GtkWidget *cancel_button;

GtkWidget *temp_handle;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *FPGA_hbox(char *string)
  {
  GtkWidget *hbox;

  hbox = new_box(FALSE, 0, HORIZONTAL);
  gtk_object_set_data_full(GTK_OBJECT(container), string, hbox,
                          (GtkDestroyNotify) gtk_widget_unref);
  gtk_box_pack_start(GTK_BOX(container), hbox, FALSE, FALSE, 0);
  return hbox;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *FPGA_button(char *string1, char *string2, GtkWidget *box,
                         callback_button_go_fn function)
  {
  GtkWidget *handle;

  handle = push_button(string1, box, FALSE, FALSE, 0);
  gtk_object_set_data_full(GTK_OBJECT(container), string2, handle,
                           (GtkDestroyNotify) gtk_widget_unref);
  if (function != NULL)
    gtk_signal_connect(GTK_OBJECT(handle), "clicked", function, container);

  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *table_mess(char *string1, char *string2, GtkWidget *table,
                        guint left, guint right, guint top, guint bottom,
                        int justify)
  {
  GtkWidget *handle;

  handle = gtk_label_new(string1);
  gtk_widget_ref(handle);
  gtk_object_set_data_full(GTK_OBJECT(container), string2, handle,
                           (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show(handle);
  gtk_table_attach(GTK_TABLE(table), handle,  left, right, top, bottom,
                   (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  if (justify) gtk_label_set_justify(GTK_LABEL(handle), GTK_JUSTIFY_LEFT);
  
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *create_info_table(char *string, guint rows, guint columns)
  {
  GtkWidget *table;

  table = gtk_table_new(rows, columns, FALSE);
  gtk_widget_ref(table);
  gtk_object_set_data_full(GTK_OBJECT(container), string, table,
                           (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show(table);
  gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
  gtk_table_set_row_spacings(GTK_TABLE(table), 2);
  gtk_table_set_col_spacings(GTK_TABLE(table), 30);

  return table;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("view_create_xfpgasegment\n");

container = new_box(FALSE, 11, VERTICAL);
gtk_object_set_data(GTK_OBJECT(container), "list", NULL);
gtk_container_set_border_width(GTK_CONTAINER(container), 6);

hbox = FPGA_hbox("hbox1");              /* Top box for display/file selection */

buttonupdate = FPGA_button("Update", "buttonupdate", hbox,
                            GTK_SIGNAL_FUNC(callback_fpgaupdate));

combo = gtk_combo_new();
gtk_widget_ref(combo);
gtk_object_set_data_full(GTK_OBJECT(container), "combo", combo,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(combo);
gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);

fileentry = GTK_COMBO(combo)->entry;
gtk_widget_ref(fileentry);
gtk_object_set_data_full(GTK_OBJECT(container), "fileentry", fileentry,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(fileentry);

browse = FPGA_button("Browse...", "browse", hbox, NULL);

hbox = FPGA_hbox("hbox2");               /* Box for file information/download */

table = create_info_table("table", 3, 2);               /* Table for messages */

temp_handle   = table_mess("Name:",      "label27",       table, 0,1,0,1,  TRUE);
labelfilename = table_mess("< none >",   "labelfilename", table, 1,2,0,1, FALSE);
temp_handle   = table_mess("Date:",      "label29",       table, 0,1,1,2,  TRUE);
labelfiledate = table_mess("----/--/--", "labelfiledate", table, 1,2,1,2, FALSE);
temp_handle   = table_mess("Time:",      "label42",       table, 0,1,2,3,  TRUE);
labelfiletime = table_mess("--:--:--",   "labelfiletime", table, 1,2,2,3, FALSE);

buttondownload = FPGA_button("Download", "buttondownload", hbox,
                              GTK_SIGNAL_FUNC(callback_fpgaload));

progressbar = gtk_progress_bar_new();			// Tips??  @@@
gtk_widget_ref(progressbar);
gtk_widget_show(progressbar);
gtk_object_set_data_full(GTK_OBJECT(container), "progressbar",
                         progressbar, (GtkDestroyNotify) gtk_widget_unref);

gtk_box_pack_start(GTK_BOX(container), progressbar, FALSE, FALSE, 0);
gtk_progress_configure(GTK_PROGRESS(progressbar), 0, 0, 100);
gtk_progress_set_show_text(GTK_PROGRESS(progressbar), TRUE);


hbox = FPGA_hbox("hbox3");                             /* Box for FPGA status */

table = create_info_table("table8", 3, 2);

temp_handle   = table_mess("Name:",      "label44",       table, 0,1,0,1,  TRUE);
labelfpganame = table_mess("< reset >",  "labelfpganame", table, 1,2,0,1, FALSE);
temp_handle   = table_mess("Date:",      "label46",       table, 0,1,1,2,  TRUE);
labelfpgadate = table_mess("----/--/--", "labelfpgadate", table, 1,2,1,2, FALSE);
temp_handle   = table_mess("Time:",      "label48",       table, 0,1,2,3,  TRUE);
labelfpgatime = table_mess("--:--:--",   "labelfpgatime", table, 1,2,2,3, FALSE);

buttonerase = FPGA_button("Erase", "buttonerase", hbox,
                              GTK_SIGNAL_FUNC(callback_fpgaerase));

fileselection = gtk_file_selection_new(_("Select FPGA .bit File"));
gtk_container_set_border_width(GTK_CONTAINER(fileselection), 10);
ok_button = GTK_FILE_SELECTION(fileselection)->ok_button;
gtk_widget_show(ok_button);
GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
cancel_button = GTK_FILE_SELECTION(fileselection)->cancel_button;
gtk_widget_show(cancel_button);
GTK_WIDGET_SET_FLAGS(cancel_button, GTK_CAN_DEFAULT);
gtk_file_selection_complete(GTK_FILE_SELECTION(fileselection), ("*"BITFILE_EXT));

gtk_signal_connect_object(GTK_OBJECT(fileselection), "delete_event",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(fileselection));
gtk_signal_connect_object(GTK_OBJECT(browse), "clicked",
                          GTK_SIGNAL_FUNC(gtk_widget_show),
                          GTK_OBJECT(fileselection));
gtk_signal_connect_object(GTK_OBJECT(cancel_button), "clicked",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(fileselection));
gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(fileselection));
gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
                   GTK_SIGNAL_FUNC(callback_button_ok_file), fileentry);
gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
                   GTK_SIGNAL_FUNC(callback_fpgaupdate), container);

return container;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

//#ifdef GTK2
//GtkWidget* view_create_comms(feature *terminal) 
//{
//  GtkWidget *vbox1;
//  GtkWidget *scrolledwindow1;
//  GtkWidget *text1;
//  
//if (TRACE > 5) g_print("view_create_comms\n");
//
//  vbox1 = gtk_vbox_new (FALSE, 0);
//  gtk_widget_ref (vbox1);
//  gtk_widget_show (vbox1);
//
//  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
//  gtk_widget_ref (scrolledwindow1);
//  gtk_widget_show (scrolledwindow1);
//  gtk_box_pack_start (GTK_BOX (vbox1), scrolledwindow1, TRUE, TRUE, 0);
//  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1),
//                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
//
//  text1 = gtk_text_new (NULL, NULL);
//  gtk_widget_ref (text1);
//  gtk_widget_show (text1);
//  gtk_container_add (GTK_CONTAINER (scrolledwindow1), text1);
//
//  console_entry = gtk_entry_new ();
//  gtk_widget_ref (console_entry);
//  gtk_widget_show (console_entry);
//  gtk_box_pack_start (GTK_BOX (vbox1), console_entry, FALSE, FALSE, 0);
//  gtk_signal_connect (GTK_OBJECT(console_entry), "activate",
//                         GTK_SIGNAL_FUNC (callback_comms),terminal);
//  terminal->data.terminal.text=text1;
//  g_timeout_add(POLLING_PERIOD, callback_update_comms, terminal);
//  return vbox1;
//}
//#endif
//
//#ifndef GTK2 


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the a terminal in "features"                                       */
/*                                                                            */

GtkWidget *view_create_comms(feature *terminal)
{
GtkWidget *vbox;
GtkWidget *hbox;
GtkWidget *scrolledwindow;
GtkWidget *text;
GtkWidget *terminal_clear;
GtkWidget *terminal_active;

if (TRACE > 5) g_print("view_create_comms\n");

vbox = new_box(FALSE, 0, VERTICAL);
hbox = new_box(FALSE, 0, HORIZONTAL);

scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
gtk_widget_ref(scrolledwindow);
gtk_widget_show(scrolledwindow);
gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                               GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

text = gtk_text_new(NULL, NULL);
gtk_widget_ref(text);
gtk_widget_show(text);
gtk_tooltips_set_tip(view_tooltips, text, "Type here to input data", NULL);
gtk_widget_set_style(text, fixed_style);
gtk_container_add(GTK_CONTAINER(scrolledwindow), text);

//    console_entry = gtk_entry_new ();
//    gtk_widget_ref (console_entry);
//    gtk_widget_show (console_entry);
//    gtk_box_pack_start (GTK_BOX (vbox), console_entry, FALSE, FALSE, 0);
//    gtk_signal_connect (GTK_OBJECT(console_entry), "activate",
//    GTK_SIGNAL_FUNC (callback_comms),terminal);

terminal_active = gtk_check_button_new_with_label("Active");
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(terminal_active),
                              (terminal->sub_reference_number & 0x0001) != 0);
              /* Start with terminal active/inactive as specified by back-end */
gtk_widget_ref(terminal_active);
gtk_widget_show(terminal_active);
gtk_box_pack_start(GTK_BOX(hbox), terminal_active, FALSE, FALSE, 0);
gtk_tooltips_set_tip(view_tooltips, terminal_active,
                    "Enable/disable traffic", NULL);

terminal_clear = push_button("Clear", hbox, FALSE, FALSE, 0);
gtk_tooltips_set_tip(view_tooltips, terminal_clear, "Clear text pane", NULL);
gtk_signal_connect(GTK_OBJECT(terminal_clear), "clicked",
                   GTK_SIGNAL_FUNC(callback_clear_terminal), terminal);

gtk_widget_set_usize(terminal_clear, 340, -2);
gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

terminal->data.terminal.text = text;     /* set the text in terminal to text */

//printf("ZZ: %08X %08X\n", terminal, text);
gtk_key_snooper_install(callback_terminal_keysnoop, terminal);
                           /* send to callback with pointer to the text area */
g_timeout_add(POLLING_PERIOD, callback_update_comms, terminal);
            /* poll and communicate with the devices every POLLING_PERIOD ms */

if (terminal->dev_number >= TERMINAL_UPPER_BOUND)
  {                     /* we won't allow terminal as 20th feature or higher */
  g_print("Terminal feature found at predefined upper bound.\n");
  exit(1);            // WHY???  @@@
  }
else
  text_in_terminal[terminal->dev_number] = text;
                         /* save text entry, indexed intuitively (?!?! @@@) */

terminal_active_flag[terminal->dev_number] = terminal_active;
return vbox;
}

// #endif

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the splash window (KMD logo)                                       */
/*                                                                            */
// Reportedly "splashwindow" leaks memory somewhere - not -too- serious @@@

GtkWidget *view_create_splashwindow(void)
{
GtkWidget *splashwindow;
GtkWidget *vbox;
GtkWidget *pixmap;
 
if (TRACE > 5) g_print("view_create_splashwindow\n");

view_komodologo_pixmap =
    gdk_pixmap_colormap_create_from_xpm_d(NULL, gdk_colormap_get_system(),
                                          &view_komodologo_bitmap, NULL,
                                          komodologo_xpm);

splashwindow = gtk_window_new(GTK_WINDOW_POPUP);
gtk_window_set_position(GTK_WINDOW(splashwindow), GTK_WIN_POS_CENTER);
gtk_container_set_border_width(GTK_CONTAINER(splashwindow),5);
vbox = new_box(FALSE, 0, VERTICAL);
gtk_container_add(GTK_CONTAINER(splashwindow), vbox);

pixmap = gtk_pixmap_new(view_komodologo_pixmap, view_komodologo_bitmap);
gtk_widget_ref(pixmap);
gtk_widget_show(pixmap);
gtk_box_pack_start(GTK_BOX(vbox), pixmap, FALSE, FALSE, 0);
 
if (interface_type == EMULATOR)
  {
  GtkWidget *message;
  GtkWidget *frame;  

  frame = gtk_frame_new("");
  gtk_widget_ref(frame);
  gtk_widget_show(frame);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  message = gtk_label_new("Emulation Mode");
  gtk_widget_ref(message);
  gtk_widget_show(message);
  gtk_container_add(GTK_CONTAINER(frame), message);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
  } 
  
gtk_widget_show(splashwindow);
gtk_widget_add_events(splashwindow, 1);

while (gtk_events_pending()) gtk_main_iteration();
return splashwindow;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the about window                                                   */
/*                                                                            */

GtkWidget *view_create_aboutwindow(void)
{
GtkWidget *aboutwindow;
GtkWidget *hbox1;
GtkWidget *pixmap1;
GtkWidget *vbox;
GtkWidget *vbox1;
GtkWidget *label1;
GtkWidget *button1;

if (TRACE > 5) g_print("view_create_aboutwindow\n");

aboutwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_object_set_data(GTK_OBJECT(aboutwindow), "aboutwindow", aboutwindow);
gtk_window_set_title(GTK_WINDOW(aboutwindow), _("About"));

vbox = new_box(FALSE, 0, VERTICAL);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "vbox", vbox,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_container_add(GTK_CONTAINER(aboutwindow), vbox);

pixmap1 = gtk_pixmap_new(view_komodotitle_pixmap, view_komodotitle_bitmap);
gtk_widget_ref(pixmap1);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "pixmap1", pixmap1,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(pixmap1);
gtk_box_pack_start(GTK_BOX(vbox), pixmap1, FALSE, FALSE, 0);

hbox1 = new_box(FALSE, 0, HORIZONTAL);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "hbox1", hbox1,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_box_pack_start(GTK_BOX(vbox), hbox1, TRUE, TRUE, 0);
gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);

pixmap1 = gtk_pixmap_new(view_mulogo_pixmap, view_mulogo_bitmap);
gtk_widget_ref(pixmap1);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "pixmap1", pixmap1,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(pixmap1);
gtk_box_pack_start(GTK_BOX(hbox1), pixmap1, FALSE, FALSE, 0);

vbox1 = new_box(FALSE, 0, VERTICAL);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "vbox1", vbox1,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 0);

label1 = gtk_label_new((PACKAGE "\nVersion " VERSION " (" DATE ")\n\n"
        "Copyright " YEAR ":\nUniversity of Manchester\n\n"
        "Authors:\nCharlie Brej\nhttp://www.cs.man.ac.uk/~brejc8\n"
        "Jim Garside\nhttp://www.cs.man.ac.uk/amulet/staff/details/jim.html\n"
        "Roy Schestowitz\nhttp://www2.cs.man.ac.uk/~schestr0\n"
	"John Zaitseff\n"
	"Chris Page\n"
        "\n"
        "Bug reports and feature requests:\n"
        "komodo@cs.man.ac.uk\nWebsite:\n"
        "http://www.cs.man.ac.uk/teaching/electronics/komodo"));

gtk_misc_set_padding(GTK_MISC(label1), 10, 10);
gtk_widget_ref(label1);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "label1", label1,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(label1);
gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, FALSE, 0);

button1 = push_button("Dismiss", vbox1, FALSE, FALSE, 0);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "button1", button1,
                         (GtkDestroyNotify) gtk_widget_unref);

vbox1 = new_box(FALSE, 0, VERTICAL);
gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 0);

label1 = gtk_label_new("Powered By");
gtk_misc_set_padding(GTK_MISC(label1), 10, 10);
gtk_widget_ref(label1);
gtk_widget_show(label1);
gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, FALSE, 0);

pixmap1 = gtk_pixmap_new(view_chump_pixmap, view_chump_bitmap);
gtk_widget_ref(pixmap1);
gtk_object_set_data_full(GTK_OBJECT(aboutwindow), "pixmap1", pixmap1,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(pixmap1);
gtk_box_pack_start(GTK_BOX(vbox1), pixmap1, TRUE, TRUE, 0);

gtk_signal_connect_object(GTK_OBJECT(aboutwindow), "delete_event",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(aboutwindow));
gtk_signal_connect_object(GTK_OBJECT(button1), "clicked",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(aboutwindow));
return aboutwindow;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the control bar (where all the buttons and tick boxes are)         */
/*                                                                            */

GtkWidget *view_create_prog_ctrl(void)
{                                                 // Rationalise @@@
GtkAccelGroup *accel_group = gtk_accel_group_new ();
GtkWidget *hbox;
GtkWidget *vbox;
GtkWidget *button_start;
GtkWidget *button_stop;
GtkWidget *button_single;
GtkWidget *button_multi;
GtkWidget *cbutton_break;
GtkWidget *cbutton_swi;
GtkWidget *cbutton_proc;
GtkWidget *label;
GtkWidget *button_feature;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/* `function' "type" ? @@@ */
  GtkWidget *create_ctrl_push_button(char *string,
                                     callback_button_go_fn function,
                                     gpointer parameter,
                                     guint accelerator,
                                     char *tip)
  {
  GtkWidget *handle;

  handle = push_button(string, hbox, FALSE, FALSE, 0);
  gtk_signal_connect(GTK_OBJECT(handle), "clicked",
                     GTK_SIGNAL_FUNC(function), parameter);
  if (tip[0] != '\0') gtk_tooltips_set_tip(view_tooltips, handle, tip, NULL);
  if (accelerator != 0)
    gtk_widget_add_accelerator(handle, "clicked", accel_group, accelerator, 0,
                               GTK_ACCEL_VISIBLE);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *create_ctrl_toggle_button(char *string,
                                       callback_button_go_fn function,
                                       gpointer parameter,
                                       char *tip,
                                       int active)
  {
  GtkWidget *handle;

  handle = gtk_check_button_new_with_label(string);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(handle), active);
  gtk_widget_ref(handle);
  gtk_widget_show(handle);
  gtk_box_pack_start(GTK_BOX(hbox), handle, FALSE, FALSE, 0);
  gtk_signal_connect(GTK_OBJECT(handle), "toggled", function, parameter);
  if (tip[0] != '\0') gtk_tooltips_set_tip(view_tooltips, handle, tip, NULL);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void create_ctrl_spin_button(int initial, int min, int max,
                               int *parameter, char *tip)
  {
  GtkObject *handle1;
  GtkWidget *handle2;

  handle1 = gtk_adjustment_new(initial, min, max, 1, 1000, 10);
  handle2 = gtk_spin_button_new(GTK_ADJUSTMENT(handle1), 1, 0);
  gtk_widget_ref(handle2);
  gtk_widget_show(handle2);
  gtk_box_pack_start(GTK_BOX(hbox), handle2, TRUE, TRUE, 0);
  gtk_signal_connect(GTK_OBJECT(handle2), "changed",
                     GTK_SIGNAL_FUNC(callback_step_number),
                     parameter);
  gtk_widget_set_usize(handle2, 30, -2);
  gtk_tooltips_set_tip (view_tooltips, handle2, tip, NULL);
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("view_create_prog_ctrl\n");

vbox = new_box(FALSE, 2, VERTICAL);

hbox = new_box(FALSE, 4, HORIZONTAL);

button_start = create_ctrl_push_button("Ping",
                                        GTK_SIGNAL_FUNC(callback_button_ping),
                                        NULL, 0,
                                       "Ping board and refresh display");

button_start = create_ctrl_push_button("Run",
                                       GTK_SIGNAL_FUNC(callback_button_start),
                                      (gpointer) &zero, GDK_F5,
                                      "Start execution [F5]");

button_stop  = create_ctrl_push_button("Stop",
                                        GTK_SIGNAL_FUNC(callback_button_stop),
                                        NULL, GDK_F6,
                                       "Stop execution [F6]");

button_multi = create_ctrl_push_button("Continue",
                                        GTK_SIGNAL_FUNC(callback_button_continue),
                                        NULL, GDK_F10,
                                       "Continue execution [F10]");

button_start = create_ctrl_push_button("Reset",
                                        GTK_SIGNAL_FUNC(callback_button_reset),
                                        NULL, 0,
                                       "Reset client");

button_single= create_ctrl_push_button("Single-Step",
                                        GTK_SIGNAL_FUNC(callback_button_start),
                                       (gpointer) &one, GDK_F7,
                                       "Take one step [F7]");

create_ctrl_spin_button(1, 1, 1000000, &view_step_number,
                       "Change number of steps to take.\n"
                       "Remember to press return after entering a new value");
                                                   /* Max. -was- much greater */

button_multi = create_ctrl_push_button("Multi-Step",
                                        GTK_SIGNAL_FUNC(callback_button_start),
                                       (gpointer) &view_step_number, GDK_F8,
                                       "Take a number of steps [F8]");

create_ctrl_spin_button(1000, 1, 10000, &view_step_freq,
                       "Change period (ms) between steps when walking.\n"
                       "Remember to press return after entering a new value");

button_multi = create_ctrl_push_button("Walk",
                                        GTK_SIGNAL_FUNC(callback_button_walk),
                                       (gpointer) &view_step_number, GDK_F9,
                                       "Repeatedly take a number of steps [F9]");

view_step_number = 1;
view_step_freq = 1000;

cbutton_proc = gtk_toggle_button_new_with_label ("Refresh");
gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbutton_proc), FALSE);
gtk_widget_ref (cbutton_proc);
gtk_widget_show (cbutton_proc);
gtk_box_pack_start (GTK_BOX (hbox), cbutton_proc, FALSE, FALSE, 0);
gtk_signal_connect (GTK_OBJECT (cbutton_proc), "toggled",
                    GTK_SIGNAL_FUNC (callback_refresh_toggle), NULL);
gtk_tooltips_set_tip (view_tooltips, cbutton_proc,
                      "Refresh screen regularly", NULL);
view_refreshbutton = cbutton_proc;

gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

hbox = new_box(FALSE, 4, HORIZONTAL);

button_feature = push_button("Features", hbox, FALSE, FALSE, 0);
gtk_tooltips_set_tip (view_tooltips, button_feature, "Features Available",
                      NULL);
gtk_signal_connect_object (GTK_OBJECT (button_feature), "clicked",
                           gtk_widget_show, GTK_OBJECT (window_feature));

cbutton_break = create_ctrl_toggle_button("Breakpoints",
                                  GTK_SIGNAL_FUNC(callback_start_toggle),
                                  GINT_TO_POINTER(0x10),
                                 "Activate breakpoints",
                                  test_run_flag(RUN_FLAG_BREAK));

cbutton_break = create_ctrl_toggle_button("Watchpoints",
                                  GTK_SIGNAL_FUNC(callback_start_toggle),
                                  GINT_TO_POINTER(0x20),
                                 "Activate watchpoints",
                                  test_run_flag(RUN_FLAG_WATCH));

label = status_message("          Service:", hbox, FALSE, FALSE, 0);

cbutton_swi   = create_ctrl_toggle_button("SWI",
                                  GTK_SIGNAL_FUNC(callback_start_toggle),
                                  GINT_TO_POINTER(0x04),
                                 "Enter software interrupt code",
                                  test_run_flag(RUN_FLAG_SWI));

cbutton_proc  = create_ctrl_toggle_button("BL",
                                  GTK_SIGNAL_FUNC(callback_start_toggle),
                                  GINT_TO_POINTER(0x02),
                                 "Enter sub-procedure code",
                                  test_run_flag(RUN_FLAG_BL));

cbutton_proc  = create_ctrl_toggle_button("Abort",
                                  GTK_SIGNAL_FUNC(callback_start_toggle),
                                  GINT_TO_POINTER(0x08),
                                 "Enter Bad Memory access exception code",
                                  test_run_flag(RUN_FLAG_ABORT));

label = status_message("          Active:", hbox, FALSE, FALSE, 0);

cbutton_proc  = create_ctrl_toggle_button("IRQ",
                                 GTK_SIGNAL_FUNC(callback_rtf_toggle),
                                 GINT_TO_POINTER(0x02),
                                "Activate automatic execution of interrupt code",
                                 FALSE);

cbutton_proc  = create_ctrl_toggle_button("FIQ",
                                 GTK_SIGNAL_FUNC(callback_rtf_toggle),
                                 GINT_TO_POINTER(0x01),
                                "Activate automatic execution of FIQ code",
                                 FALSE);

gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

gtk_window_add_accel_group(GTK_WINDOW(view_mainwindow), accel_group);

return vbox;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/

/* Notes: this could probably link to the front of the list really - easier!  */
/*        Maybe want to keep a pointer/handle so that we can find it again??  */

void link_mem_window_entry(mem_window *memwindow)
{
mem_window_entry *temp_ptr1, *temp_ptr2;


if (TRACE > 5) g_print("link_mem_window_entry\n");

temp_ptr2 = g_new(mem_window_entry, 1);
temp_ptr2->mem_data_ptr = memwindow;
temp_ptr2->next = NULL;

if (view_memwindowlist == NULL) view_memwindowlist = temp_ptr2;  /* 1st entry */
else
  {
  temp_ptr1 = view_memwindowlist;
  while (temp_ptr1->next != NULL) temp_ptr1 = temp_ptr1->next;
                                                      /* Chain to end of list */
  temp_ptr1->next = temp_ptr2;                                 /*  and append */
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *view_create_mem_slidebar(mem_window *memwindow, GtkWidget *hbox)
{
GtkObject *vscale_adj;
GtkWidget *vscale;
GtkWidget *temp_widget;
GtkWidget *vbox1;
GtkWidget *vbox3;
GtkWidget *hbox1;
GtkWidget *topvbox;
GtkWidget *upButton;
GtkWidget *downButton;

char *slidelabeldata[] = {"-1000","-100","-10","0","+10","+100","+1000"};
int i;

if (TRACE > 10) g_print("view_create_mem_slidebar\n");

vscale_adj = gtk_adjustment_new(0, -3.25, 3.25, 3, 3, 0);
vscale = gtk_vscale_new(GTK_ADJUSTMENT(vscale_adj));
gtk_widget_ref(vscale);
gtk_widget_show(vscale);

vbox3 = gtk_vbox_new (FALSE, 0);
gtk_widget_show (vbox3);
gtk_box_pack_start(GTK_BOX(hbox), vbox3, FALSE, FALSE, 0);

upButton = gtk_button_new_with_label("up");
gtk_widget_ref(upButton);
gtk_widget_show(upButton);
gtk_box_pack_start (GTK_BOX (vbox3), upButton, FALSE, FALSE, 0);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox3), hbox1, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (hbox1), vscale, FALSE, TRUE, 0);

  vbox1 = new_box(TRUE, 0, VERTICAL);         /* Separate vbox for scale labels */
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (hbox1), vbox1, TRUE, TRUE, 0);

downButton = gtk_button_new_with_label("down");
gtk_widget_ref(downButton);
gtk_widget_show(downButton);
gtk_box_pack_start (GTK_BOX (vbox3), downButton, FALSE, FALSE, 0);

gtk_scale_set_draw_value(GTK_SCALE(vscale), FALSE);
gtk_scale_set_digits(GTK_SCALE(vscale), 2);
gtk_range_set_update_policy(GTK_RANGE(vscale), GTK_UPDATE_CONTINUOUS);

gtk_signal_connect(GTK_OBJECT(vscale_adj), "value_changed",
                   GTK_SIGNAL_FUNC(callback_memwindow_scrollmove), NULL);
gtk_signal_connect(GTK_OBJECT(vscale), "button-release-event",
                   GTK_SIGNAL_FUNC(callback_memwindow_scrollrelease), NULL);
gtk_signal_connect_after(GTK_OBJECT (vscale), "key-press-event",
                         GTK_SIGNAL_FUNC(callback_memwindow_scrollrelease), NULL);
gtk_signal_connect(GTK_OBJECT (upButton), "clicked",
                   GTK_SIGNAL_FUNC(callback_memwindow_scroll_button_press),
                                  (gpointer) &zero);
gtk_signal_connect(GTK_OBJECT (downButton), "clicked",
                   GTK_SIGNAL_FUNC(callback_memwindow_scroll_button_press),
                                  (gpointer) &one);

gtk_object_set_data(GTK_OBJECT(vscale_adj), "mem_window", memwindow);
gtk_object_set_data(GTK_OBJECT(upButton), "mem_window", memwindow);
gtk_object_set_data(GTK_OBJECT(downButton), "mem_window", memwindow);



for (i = 0; i < 7; i++)                          /* print labels for slidebar */
  temp_widget = status_message(slidelabeldata[i], vbox1, FALSE, FALSE, 0);

return vscale;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *view_create_mem_topline(mem_window *memwindow, GtkWidget *vbox,
                                   mem_win_type type, int external,
                                   GtkWidget *ext_ID, int style)
{
GtkWidget *mem_menu;

GtkWidget *menuitem;
GtkWidget *optionmenu_length;
GtkWidget *optionmenu_isa;
GtkWidget *optionmenu_menu;

GtkWidget *button_memory;
//GtkObject *listsize_adj;
GtkWidget *listsize;
GtkWidget *cbutton;                                    /* Another temp widget */
GtkWidget *temp_widget;         /* Holds temporary or throw-away return value */

DefinitionStack *isa_list;

/* Following two data statements must match */
static int size_menu_code[] = {
            MEM_REP_BYTE,         MEM_REP_2_BYTES,      MEM_REP_4_BYTES,
            MEM_REP_8_BYTES,      MEM_REP_16_BYTES,     MEM_REP_32_BYTES,
            MEM_REP_RULER,
            MEM_REP_HALFWORD,     MEM_REP_2_HALFWORDS,  MEM_REP_4_HALFWORDS,
            MEM_REP_8_HALFWORDS,  MEM_REP_16_HALFWORDS, MEM_REP_RULER,
            MEM_REP_WORD,         MEM_REP_2_WORDS,      MEM_REP_4_WORDS,
            MEM_REP_8_WORDS,      MEM_REP_RULER,
            MEM_REP_ASCII_MAP,    MEM_REP_ASCII_MAP128, MEM_REP_TEMINATOR};
/* The above array is static as only pointers are passed to relevant callback */

char *size_menu_string[] = {"1 Byte",      "2 Bytes",      "4 Bytes",
                            "8 Bytes",     "16 Bytes",     "32 Bytes",   "",
                            "1 Halfword",  "2 Halfwords",  "4 Halfwords",
                            "8 Halfwords", "16 Halfwords", "",
                            "1 Word",      "2 Words",      "4 Words",
                            "8 Words",     "",
                            "ASCII Map64", "ASCII Map128", ""};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *mem_check_button(char *string, GtkWidget *menu,
                              callback_button_go_fn function, gboolean state,
                              char *tip)
  {
  GtkWidget *handle;

  handle = gtk_check_button_new_with_label(string);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(handle), state);
  gtk_widget_ref(handle);
  gtk_widget_show(handle);
  gtk_box_pack_start(GTK_BOX(menu), handle, FALSE, FALSE, 0);
  gtk_signal_connect(GTK_OBJECT(handle), "toggled",
                     GTK_SIGNAL_FUNC(function), NULL);
  gtk_tooltips_set_tip(view_tooltips, handle, tip, NULL);
  return handle;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 5) g_print("view_create_mem_topline\n");

mem_menu = new_box(FALSE, 0, HORIZONTAL);  /* Create top bar of memory window */
gtk_box_pack_start(GTK_BOX(vbox), mem_menu, FALSE, FALSE, 0);

if (type == MEM_WIN_DUMP)
  {
  button_memory = push_button("New Memory Window", mem_menu, FALSE, FALSE, 0);
// gtk_widget_set_usize(button_memory,140,40);
  gtk_signal_connect(GTK_OBJECT(button_memory), "clicked",
                     GTK_SIGNAL_FUNC(callback_memory_window_create),
                     (gpointer) MEM_WIN_DUMP);
  gtk_tooltips_set_tip(view_tooltips, button_memory,
                      "Open a separate memory window", NULL);

  optionmenu_length = gtk_option_menu_new();         /* Format selection menu */
  gtk_widget_ref(optionmenu_length);
  gtk_widget_show(optionmenu_length);
  optionmenu_menu = gtk_menu_new();
  gtk_widget_ref(optionmenu_menu);

  {                                                 /* Build memory size menu */
  int i, j;

  for (i = 0; size_menu_code[i] != MEM_REP_TEMINATOR; i++)
    {
    if (size_menu_code[i] == MEM_REP_RULER)
      temp_widget = view_separator(GTK_MENU(optionmenu_menu));
    else
      temp_widget = create_submenu_entry(optionmenu_menu, size_menu_string[i],
                                   GTK_SIGNAL_FUNC(callback_memwindow_length2),
                                   (gpointer) &size_menu_code[i]);

    if (size_menu_code[i] == style) j = i;         /* Remember required entry */
    }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(optionmenu_length), optionmenu_menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(optionmenu_length), j);
  }

  gtk_tooltips_set_tip(view_tooltips, optionmenu_length,
                      "Choose memory representation format", NULL);
  gtk_box_pack_start(GTK_BOX(mem_menu), optionmenu_length, TRUE, TRUE, 0);
  gtk_object_set_data(GTK_OBJECT(optionmenu_menu), "mem_window", memwindow);
  }
else
  {       /* If source window put address entry box & single step button here */
  GtkWidget *handle, *button_single;   /* See also create_ctrl_push_button @@ */
  memwindow->address_entry = entry_box(mem_menu,
                               ADDR_ENTRY_MAX_CHAR, ADDR_ENTRY_BOX_LENGTH,
                               GTK_SIGNAL_FUNC(callback_memwindow_address));

  handle = push_button("Single-Step", mem_menu, FALSE, FALSE, 0);
  gtk_signal_connect(GTK_OBJECT(handle), "clicked",
                     GTK_SIGNAL_FUNC(callback_button_start), (gpointer) &one);
  gtk_tooltips_set_tip(view_tooltips, handle, "Take one step", NULL);
  }

optionmenu_isa = gtk_option_menu_new();        /* Disassembler selection menu */
gtk_widget_ref(optionmenu_isa);
gtk_widget_show(optionmenu_isa);
optionmenu_menu = gtk_menu_new();
gtk_widget_ref(optionmenu_menu);

isa_list = board->asm_tables;
while (isa_list)
  {
  menuitem = gtk_menu_item_new_with_label(isa_list->string);
  gtk_widget_show(menuitem);
  gtk_menu_prepend(GTK_MENU(optionmenu_menu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                     GTK_SIGNAL_FUNC(callback_memwindow_isa),
                     (gpointer) isa_list->rules);
  memwindow->isa = isa_list->rules;
  isa_list = isa_list->next;
  }

gtk_option_menu_set_menu(GTK_OPTION_MENU(optionmenu_isa), optionmenu_menu);
gtk_box_pack_start(GTK_BOX(mem_menu), optionmenu_isa, TRUE, TRUE, 0);
gtk_object_set_data(GTK_OBJECT(optionmenu_menu), "mem_window", memwindow);

//if (external)                                         /* Rows displayed entry */
//  listsize_adj = gtk_adjustment_new(rows, 0, MEM_ROWS_MAX_EXT, 1, 1, 1);
//else
//  listsize_adj = gtk_adjustment_new(rows, 0, MEM_ROWS_MAX_INT, 1, 1, 1);
//listsize = gtk_spin_button_new(GTK_ADJUSTMENT(listsize_adj), 1, 0);
//gtk_widget_ref(listsize);
//gtk_widget_show(listsize);
//gtk_box_pack_start(GTK_BOX(mem_menu), listsize, TRUE, TRUE, 0);
//gtk_signal_connect(GTK_OBJECT(listsize), "changed",
//                   GTK_SIGNAL_FUNC(callback_memwindow_listsize), 0);

if (type == MEM_WIN_DUMP)        /* Level-of-detail control for source window */
  memwindow->full_button = NULL;
else
  memwindow->full_button = mem_check_button("Full source", mem_menu,
                       GTK_SIGNAL_FUNC(callback_memwindow_full_toggle),
                       FALSE,
                       "Display all source code lines");

memwindow->elf_symbols_button = mem_check_button("Symbols", mem_menu,
                       GTK_SIGNAL_FUNC(callback_memwindow_elf_symbols_toggle),
                       FALSE,
                       "Display/hide ELF symbols");

if (type == MEM_WIN_DUMP)
  {
  memwindow->dis_tab    = NULL;

  memwindow->inc_button = mem_check_button("Addr++", mem_menu,
                       GTK_SIGNAL_FUNC(callback_memwindow_inc_toggle),
                       TRUE,
                       "Increment after entering value");

  memwindow->ascii_button = mem_check_button("ASCII", mem_menu,
                       GTK_SIGNAL_FUNC(callback_memwindow_ascii_toggle),
                       TRUE,
                       "Toggle ASCII column");

  memwindow->dis_button = mem_check_button("Disassembly", mem_menu,
                       GTK_SIGNAL_FUNC(callback_memwindow_dis_toggle),
                       TRUE,
                       "Toggle disassembly column");
  }
else
  {                   /* Create tab adjuster for disassembly in source window */
  GtkObject *tabsize_adj;
  GtkWidget *tabsize;
  GtkWidget *label;
  label = status_message("   Tabs: ", mem_menu, FALSE, FALSE, 0);
  tabsize_adj = gtk_adjustment_new(source_dis_tabs, 0, DIS_TAB_MAX, 1, 1, 1);
  tabsize = gtk_spin_button_new(GTK_ADJUSTMENT(tabsize_adj), 1, 0);
  gtk_widget_ref(tabsize);
  gtk_widget_show(tabsize);
  gtk_box_pack_start(GTK_BOX(mem_menu), tabsize, TRUE, TRUE, 0);
  gtk_signal_connect(GTK_OBJECT(tabsize), "changed",
                     GTK_SIGNAL_FUNC(callback_memwindow_tabsize), 0);
  memwindow->dis_tab            = tabsize;

  memwindow->inc_button         = NULL;         /* Ensure the pointers return */
  memwindow->ascii_button       = NULL;    /* something sensible (`inactive') */
  memwindow->dis_button         = NULL;
  }

if (external)                        /* (Supposedly) not in the `root' window */
  {
  gtk_signal_connect(GTK_OBJECT(ext_ID), "hide", /* Window closed (destroyed) */
                     GTK_SIGNAL_FUNC(callback_memory_window_destroy),
                     (gpointer) memwindow);
                            /* This (apparently) calls gtk_widget_hide anyway */

                                                     /* Make a `close' button */
  button_memory = push_button("  Close  ", mem_menu, FALSE, FALSE, 0);
  gtk_signal_connect_object(GTK_OBJECT(button_memory), "clicked",
                            gtk_widget_hide,
                            GTK_OBJECT(ext_ID));
                     /* The "hide" action then calls the routine linked above */
//  gtk_signal_connect(GTK_OBJECT(button_memory), "clicked",
//                     GTK_SIGNAL_FUNC(callback_memory_window_destroy), ext_ID);

  gtk_tooltips_set_tip(view_tooltips, button_memory, "Close this window", NULL);
  }

return mem_menu;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Make up the display area for the memory                                    */

gboolean test_function(GtkWidget *widget,
                                            GdkEventButton *event,
                                            gpointer user_data)
{
g_print("TEST %d\n",event->button);
}

void view_mem_display_panel(mem_window *memwindow, GtkWidget *vbox,
                            int hex_column_width,  mem_win_type type)
{                                       /* i.e. I don't know what it does yet */
GtkWidget *clist;
GtkWidget *label;
GtkWidget *scrolledwindow;
int i;

if (TRACE > 5) g_print("view_mem_display_panel\n");

scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                               GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
gtk_widget_show(scrolledwindow);
gtk_box_pack_end(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);

clist = gtk_clist_new(TOTAL_ENTRY);       /* Total number of columns possible */
memwindow->clist_ptr = clist;

for (i = 0; i < memwindow->count; i++)                /* Put up each row (??) */
  gtk_clist_append(GTK_CLIST(clist), mem_column_data);
         /* `mem_column_data' declared in global.h and defined in callbacks.c */

gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
gtk_signal_connect(GTK_OBJECT(clist), "select-row",
                   GTK_SIGNAL_FUNC(callback_memwindow_clist),
                   (gpointer) &zero);
gtk_signal_connect(GTK_OBJECT(clist), "size-allocate",
                   GTK_SIGNAL_FUNC(callback_memwindow_size_allocate),
                   (gpointer) &zero);        /* Signal if window size changed */
gtk_signal_connect(GTK_OBJECT(clist), "key-press-event",
                   GTK_SIGNAL_FUNC(callback_memwindow_key_press),
                   (gpointer) &zero); 		   
gtk_signal_connect(GTK_OBJECT(clist), "button-press-event",
                   GTK_SIGNAL_FUNC(callback_memwindow_button_press),
                   (gpointer) &zero);        
gtk_object_set_data(GTK_OBJECT(clist), "mem_window", memwindow);
		   
gtk_widget_show(clist);

gtk_container_add(GTK_CONTAINER(scrolledwindow), clist);

gtk_clist_column_titles_show(GTK_CLIST(clist));

label =   column_label("Address",     clist, ADDRESS_ENTRY, 100);
for (i = MIN_HEX_ENTRY; i <= MAX_HEX_ENTRY; i++)
  label = column_label("Hex",         clist, i, hex_column_width);
label =   column_label("ASCII",       clist, ASCII_ENTRY, -1);
if (type == MEM_WIN_DUMP)
  label = column_label("Disassembly", clist, DIS_ENTRY, -1);
else
  label = column_label("Source/disassembly", clist, DIS_ENTRY, -1);

gtk_widget_set_style(clist, fixed_style);

return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates a large memory window                                              */
/* Returns: Handle for created window                                         */
/*                                                                            */

//GtkWidget *view_create_memorywindow(mem_win_type type, int rows, int style,
GtkWidget *view_create_memorywindow(mem_win_type type, int style,
                                    GdkGeometry *hints)
{
GtkWidget *new_window;
GtkWidget *mem_panel;                                /* The whole memory area */

if (TRACE > 5) g_print("view_create_memorywindow\n");

new_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_default_size(GTK_WINDOW(new_window), MEM_WINDOW_X, MEM_WINDOW_Y);
gtk_window_set_position(GTK_WINDOW(new_window), GTK_WIN_POS_NONE);
gtk_object_set_data(GTK_OBJECT(new_window), "window", new_window);
gtk_window_set_geometry_hints(GTK_WINDOW(new_window),
                              GTK_WIDGET(new_window), hints,
                              GDK_HINT_MAX_SIZE | GDK_HINT_MIN_SIZE |
                              GDK_HINT_BASE_SIZE);

switch (type)
  {
  case MEM_WIN_DUMP:
    gtk_window_set_title(GTK_WINDOW(new_window), _("Memory Window"));
    break;

  case MEM_WIN_SOURCE:
    gtk_window_set_title(GTK_WINDOW(new_window), _("Source Code Window"));
    break;
  }

//mem_panel = view_create_memory_clist(type, TRUE, new_window, rows, style);
mem_panel = view_create_memory_clist(type, TRUE, new_window, style);

gtk_container_add(GTK_CONTAINER(new_window), mem_panel);
gtk_signal_connect_object(GTK_OBJECT(new_window), "delete_event",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(new_window));
gtk_widget_show(new_window);                               /*  and display it */

return(new_window);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates a symbol table window                                              */
/* Returns: Handle for created window                                         */
/*                                                                            */

GtkWidget *view_create_symbol_window(GdkGeometry *hints)
{
GtkWidget *new_window;
GtkWidget *vbox;		// Not sure if all are necessary @@@
GtkWidget *scrolledwindow;
GtkWidget *clist;
GtkWidget *label;
sym_window_entry *new_symwin;
int i, sym_count;

if (TRACE > 5) g_print("view_create_symbol_window\n");

new_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_window_set_default_size(GTK_WINDOW(new_window), MEM_WINDOW_X, MEM_WINDOW_Y);
gtk_window_set_position(GTK_WINDOW(new_window), GTK_WIN_POS_NONE);
gtk_object_set_data(GTK_OBJECT(new_window), "window", new_window);
gtk_window_set_geometry_hints(GTK_WINDOW(new_window),
                              GTK_WIDGET(new_window), hints,
                              GDK_HINT_MAX_SIZE | GDK_HINT_MIN_SIZE |
                              GDK_HINT_BASE_SIZE);

gtk_window_set_title(GTK_WINDOW(new_window), _("Symbol Table"));

vbox = new_box(FALSE, 0, VERTICAL);
gtk_container_add(GTK_CONTAINER(new_window), vbox);
scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                               GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
gtk_widget_show(scrolledwindow);
gtk_box_pack_end(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);

clist = gtk_clist_new(3);                 /* Total number of columns possible */

new_symwin = g_new(sym_window_entry, 1);
new_symwin->clist = clist;
new_symwin->pNext = symbol_windows;                        /* Prepend to list */
symbol_windows = new_symwin;

gtk_widget_set_style(clist, fixed_style);

sym_count = misc_count_symbols();
for (i = 0; i < sym_count; i++)                       /* Put up each row (??) */
  gtk_clist_append(GTK_CLIST(clist), symbol_data);

view_refresh_symbol_clist(clist);                    /* Put values into place */

label = column_label("Symbol", clist, 0, 200);
label = column_label("Value",  clist, 1, 100);
label = column_label("Type",   clist, 2, 100);

gtk_widget_show(clist);
gtk_clist_column_titles_show(GTK_CLIST(clist));
gtk_container_add(GTK_CONTAINER(scrolledwindow), clist);

gtk_signal_connect(GTK_OBJECT(new_window), "hide",/* Window closed (destroyed)*/
                   GTK_SIGNAL_FUNC(callback_symbol_window_destroy),
                   (gpointer) clist);
                            /* This (apparently) calls gtk_widget_hide anyway */
gtk_signal_connect_object(GTK_OBJECT(new_window), "delete_event",
                          GTK_SIGNAL_FUNC(gtk_widget_hide),
                          GTK_OBJECT(new_window));
gtk_widget_show(new_window);                               /*  and display it */

return(new_window);
}

/*----------------------------------------------------------------------------*/

void view_refresh_symbol_clist(GtkWidget *clist)                  /* One list */
{
char value_string[17];							// size @@
char *pChar;
int i, sym_count;

sym_count = misc_count_symbols();
for (i = 0; i < sym_count; i++)                            /* Refresh symbols */
  {
  view_update_field(GTK_CLIST(clist), i, 0, misc_sym_name(i));
  pChar = view_hex_conv((unsigned int) misc_sym_value(i),
                        2 * board->memory_ptr_width, &value_string[0]);
  *pChar = '\0';
  view_update_field(GTK_CLIST(clist), i, 1, value_string);

  switch (misc_sym_type(i))
    {
    case SYM_NONE:      pChar = "Uncertain";    break;
    case SYM_UNDEFINED: pChar = "Undefined";    break;
    case SYM_EQUATE:    pChar = "Value";        break;
    case SYM_LOCAL:     pChar = "Local label";  break;
    case SYM_GLOBAL:    pChar = "Global label"; break;
    case SYM_SECTION:   pChar = "Section";      break;
    case SYM_WEAK:      pChar = "Weak label";   break;
    default:            pChar = "Unknown";      break;
    }
  view_update_field(GTK_CLIST(clist), i, 2, pChar);
  }

return;
}

/*----------------------------------------------------------------------------*/
/* Update all symbol table windows, including adjusting number of rows        */

void view_refresh_symbol_clists(unsigned int old_count, unsigned int new_count)
{
sym_window_entry *p_symwin;
int i, diff;

diff = new_count - old_count;                     /* Change in number of rows */

for (p_symwin = symbol_windows; p_symwin != NULL; p_symwin = p_symwin->pNext)
  {                                                 /* For all symbol windows */
  if (diff >= 0)
    for (i = 0; i < diff; i++)                                /* Add rows ... */
      gtk_clist_insert(GTK_CLIST(p_symwin->clist), old_count + i, symbol_data);
  else
    for (i = 0; i > diff; i--)                          /* ... or remove rows */
      gtk_clist_remove(GTK_CLIST(p_symwin->clist), old_count + i - 1);

  view_refresh_symbol_clist(p_symwin->clist);
  }

return;
}

/*----------------------------------------------------------------------------*/
/* Update a specified symbol table window with the current symbol table       */

void view_remove_symbol_clist(GtkWidget *clist)
{
sym_window_entry *old_symwin, *prev_symwin;

prev_symwin = NULL;                                    /* Means start of list */

for (old_symwin = symbol_windows; old_symwin != NULL;
                                  old_symwin = old_symwin->pNext)
  {                                                 /* For all symbol windows */
  if (old_symwin->clist == clist)
    {
    if (prev_symwin == NULL) symbol_windows = old_symwin->pNext;    /* Unlink */
    else                 prev_symwin->pNext = old_symwin->pNext;
    g_free(old_symwin);
    break;                                                    /* Lazy way out */
    }
  prev_symwin = old_symwin;
  }

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates a memory display panel                                             */
/*                                                                            */

GtkWidget *view_create_memory_clist(mem_win_type type, int external,
//                                    GtkWidget *ext_ID, int rows, int style)
                                    GtkWidget *ext_ID, int style)
{
mem_window *memwindow;    /* Local variable for convenience; copied out below */
GtkWidget *scrolledwindow;
GtkWidget *mem_panel;
GtkWidget *vbox;
GtkWidget *mem_menu;
GtkWidget *entry_stripe;
GtkWidget *entry;
GtkWidget *menuitem;

int i;

if (TRACE > 5) g_print("view_create_memory_clist\n");

memwindow = g_new(mem_window, 1);    /* Allocate new memory window definition */
memwindow->type = type;                            /* Raw dump or source code */
memwindow->row_addresses = NULL;             /* Renewed when window refreshed */

mem_panel = new_box(FALSE, 0, HORIZONTAL);/* Make display area for the window */
gtk_object_set_data(GTK_OBJECT(mem_panel), "mem_window", memwindow);

vbox = new_box(FALSE, 0, VERTICAL);     /* Separate area excluding scroll bar */
gtk_box_pack_start(GTK_BOX(mem_panel), vbox, TRUE, TRUE, 0);

memwindow->scroll = view_create_mem_slidebar(memwindow, mem_panel);

mem_menu = view_create_mem_topline(memwindow,vbox,type,external,ext_ID,style);

memwindow->address = g_new0(uchar, board->memory_ptr_width);   /* N.B. ZEROED */
memwindow->addr_type = MEM_WINDOW_DEFAULT;
memwindow->count = 1;               /* Default number of rows; adjusted later */

if (type == MEM_WIN_DUMP)
  {
  entry_stripe = new_box(FALSE, 0, HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(vbox), entry_stripe, FALSE, FALSE, 0);

  memwindow->address_entry = entry_box(entry_stripe,
                               ADDR_ENTRY_MAX_CHAR, ADDR_ENTRY_BOX_LENGTH,
                               GTK_SIGNAL_FUNC(callback_memwindow_address));

  memwindow->hex_entry     = entry_box(entry_stripe,
                               HEX_ENTRY_MAX_CHAR, HEX_ENTRY_BOX_LENGTH,
                               GTK_SIGNAL_FUNC(callback_memwindow_hex));

  memwindow->ascii_entry   = entry_box(entry_stripe,
                               ASCII_ENTRY_MAX_CHAR, ASCII_ENTRY_BOX_LENGTH,
                               GTK_SIGNAL_FUNC(callback_memwindow_ascii));

  memwindow->dis_entry     = entry_box(entry_stripe,
                               DIS_ENTRY_MAX_CHAR, DIS_ENTRY_BOX_LENGTH,
                               GTK_SIGNAL_FUNC(callback_memwindow_dis));

  entry = status_message("  ", entry_stripe,FALSE,TRUE,0); /* To shorten box? */
  }
else                               /* Don't allow data entry in source window */
  {
  /* address_entry has been placed in top line */
  memwindow->hex_entry   = NULL;
  memwindow->ascii_entry = NULL;
  memwindow->dis_entry   = NULL;
  }

view_mem_display_panel(memwindow, vbox, 200, type);

link_mem_window_entry(memwindow); /* Link window definition into display list */

callback_memwindow_length(memwindow, (gpointer) &style);

return mem_panel;
}

/*----------------------------------------------------------------------------*/
/* Unlink a memory window from the display list and free the allocated memory */
/* On entry: memwindow points to the window's definition                      */

void view_destroy_memorywindow(mem_window *memwindow)
{
mem_window_entry **temp, *temp2;

if (TRACE > 5) g_print("view_destroy_memory_clist\n");

temp = &view_memwindowlist;      /* Point at pointer to list of memory panels */

while ((*temp != NULL) && ((*temp)->mem_data_ptr != memwindow))
  temp = &((*temp)->next);    /* Move so that temp points at pointer to entry */

if (*temp != NULL)            /* Shouldn't be avoidable (unless list corrupt) */
  {                                               /* Unlink from display list */
  temp2 = *temp;                         /* Keep address of defunct entry ... */
  *temp = (*temp)->next;                      /* ... chop entry from list ... */
  g_free(temp2);                                /* ... and kill defunct entry */
  }

g_free(memwindow->address);              /* Tidy up own record of dead window */
g_free(memwindow);

return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates flag frames for CPSR and SPSR                                      */
/*                                                                            */

GtkWidget *view_create_flag_frame(int regbanktype)
{
GtkWidget *frame;                         /* the frame which will be returned */
GtkWidget *hbox;                       /* horizontal box for the flag buttons */
GtkWidget *optionmenu_mode;
GtkWidget *optionmenu_menu;   /* both used to build the menu selection widget */
GtkWidget *menuitem;    /* will hold each menu item in mode selection in turn */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *make_flag(char *name, char *tip, callback_flag_function function,
                       int psr)
  {
  GtkWidget *flag;
  int connid;

  flag = gtk_toggle_button_new_with_label(name);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(flag), FALSE);
  gtk_widget_ref(flag);
  gtk_widget_show(flag);
  gtk_box_pack_start(GTK_BOX(hbox), flag, FALSE, FALSE, 0);
  connid = gtk_signal_connect_after(GTK_OBJECT(flag), "toggled",
                               GTK_SIGNAL_FUNC(function),
                            GINT_TO_POINTER(psr));  /* Value, cast as pointer */
  gtk_object_set_data(GTK_OBJECT(flag), "connectid", GINT_TO_POINTER(connid));
  gtk_tooltips_set_tip(view_tooltips, flag, tip, NULL);

//  {  This gets the button size okay - probably want to set it though
//  GtkRequisition xxx;
//  gtk_widget_size_request(flag, &xxx);
//  printf("Flag: %08X  %08X\n", xxx.width, xxx.height);
//  }

  return flag;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  void make_mode_entry(char *string, int number)
  {
  GtkWidget *menuitem;

  switch (regbanktype)
    {
    case CURRENT:
      menuitem = create_submenu_entry(optionmenu_menu, string,
                                      GTK_SIGNAL_FUNC(callback_mode_selection),
                                      GINT_TO_POINTER(number));
      break;

    case SAVED:
      menuitem = create_submenu_entry(optionmenu_menu, string,
                                GTK_SIGNAL_FUNC(callback_mode_selection_saved),
                                      GINT_TO_POINTER(number));
      break;

    default: break;
    }
  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

reg_window Regwindow;
int i, PSR;

if (TRACE > 5) g_print("view_create_flag_frame\n");

Regwindow.regbank_no = regbanktype + 7;
/* Retrieves the appropriate data of the reg. bank currently dealt with due   */
/*  to primary implementation where flags are register banks.  There is an    */
/*  offset of 7 because Charlie defined flag windows as register windows and  */
/*  their names are stored there.                                             */

frame = gtk_frame_new(board->reg_banks[Regwindow.regbank_no].name);
                                                           /* names the frame */
gtk_widget_ref(frame);
gtk_widget_show(frame);

hbox = new_box(FALSE, 0, HORIZONTAL);
gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
gtk_widget_set_usize(hbox, 5, 5);

gtk_container_add(GTK_CONTAINER(frame), hbox);

switch (regbanktype)
  {
  case CURRENT: i =  0; PSR = CPSR; break;
  case SAVED:   i =  1; PSR = SPSR; break;
  default:      i = -1; break;
  }

if (i >= 0)/* the following will create the buttons which stand for the flags */
  {                              /* Build switches to represent the flag bits */
  flag_button[i][3] = make_flag("N", "Toggle N", callback_flag_pressed, PSR);
  flag_button[i][2] = make_flag("Z", "Toggle Z", callback_flag_pressed, PSR);
  flag_button[i][1] = make_flag("C", "Toggle C", callback_flag_pressed, PSR);
  flag_button[i][0] = make_flag("V", "Toggle V", callback_flag_pressed, PSR);
  flag_button[i][4] = make_flag(" I","Toggle I", callback_flag_pressed, PSR);
  flag_button[i][5] = make_flag("F", "Toggle F", callback_flag_pressed, PSR);
#ifdef	THUMB							// Bodge approach @@@
  flag_button[i][6] = make_flag("T", "Toggle T", callback_flag_pressed, PSR);
#endif
  }                             /* "I" suffers badly from variable width font */

  /*  The following creates the mode selection */

optionmenu_mode = gtk_option_menu_new();
                        /* new option menu to choose to current mode e.g. FIQ */
gtk_widget_show(optionmenu_mode);
optionmenu_menu = gtk_menu_new();
gtk_widget_ref(optionmenu_menu);

            // EXPERIMENTAL GLOBALS ... FIX ### @@@
if (regbanktype == CURRENT) CPSR_menu_handle = optionmenu_mode;
else                        SPSR_menu_handle = optionmenu_mode;

make_mode_entry("User",       1);/* Names are not taken from config. file @@@ */
make_mode_entry("Supervisor", 2);
make_mode_entry("Abort",      3);
make_mode_entry("Undefined",  4);
make_mode_entry("IRQ",        5);
make_mode_entry("FIQ",        6);
make_mode_entry("System",     7);

//menuitem = view_separator(GTK_MENU(optionmenu_menu));            /* Separator */
make_mode_entry("Invalid",   -1);                    /* Display purposes only */

gtk_option_menu_set_menu(GTK_OPTION_MENU(optionmenu_mode), optionmenu_menu);
gtk_option_menu_set_history(GTK_OPTION_MENU(optionmenu_mode), 0);
gtk_tooltips_set_tip(view_tooltips, optionmenu_mode, "Choose mode", NULL);
gtk_box_pack_start(GTK_BOX(hbox), optionmenu_mode, TRUE, TRUE, 0);

if (regbanktype == SAVED) all_flag_windows_have_been_created = TRUE;    /*YUK!*/
gtk_widget_set_usize(frame, FLAG_WIDGET_X, FLAG_WIDGET_Y);

return frame;                  /* return the frame with the appropriate flags */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* creates a register bank frame                                              */
/* inputs: the number of the register bank                                    */
/*                                                                            */

#define REG_NAME_MAX_LEN      6   // Beginning to rationalise `random' numbers!@@@
#define REG_CHAR_WIDTH        7   // So far just some from routine below
#define REG_ADDR_COL_WIDTH   (REG_NAME_MAX_LEN * REG_CHAR_WIDTH)
#define REG_VALUE_CHAR_WIDTH (2 * REG_CHAR_WIDTH)        /* 2 digits per byte */

GtkWidget *view_create_register_clist(int regbanknumber)
{
GtkWidget *scrolledwindow;
GtkWidget *vbox;
GtkWidget *entry_strip;
GtkWidget *frame;
reg_window *regwindow;
reg_window_entry **regwindowlist;

char *startdata[] = { "Reg", "Value (Hex)", "ASCII" };
int temp;
GtkWidget *temp_label;


if (TRACE > 5) g_print("view_create_register_clist\n");

regwindow = g_new(reg_window, 1);              /* Allocate window data record */
regwindowlist = &view_regwindowlist;

regwindow->regbank_no = regbanknumber;
while (NULL != *regwindowlist) regwindowlist = &((*regwindowlist)->next);
*regwindowlist = g_new(reg_window_entry, 1);    /* Allocate new list entry(?) */
(*regwindowlist)->reg_data_ptr = regwindow;   /* Add window to allocated list */
(*regwindowlist)->next = NULL;

frame = gtk_frame_new(board->reg_banks[regwindow->regbank_no].name);
gtk_widget_ref(frame);         /* Create frame to keep everything in (??) @@@ */
gtk_widget_show(frame);
gtk_widget_set_usize(frame, REG_WIDGET_X, REG_WIDGET_Y);

vbox = new_box(FALSE, 0, VERTICAL);
gtk_object_set_data(GTK_OBJECT(vbox), "reg_window", regwindow);
gtk_container_add(GTK_CONTAINER(frame), vbox);

regwindow->wait  = 100;
regwindow->timer = 0;

entry_strip = new_box(FALSE, 0, HORIZONTAL);/* Produce strip with entry boxes */
gtk_box_pack_start(GTK_BOX(vbox), entry_strip, FALSE, FALSE, 0);

                                  /* Note similarities with "entry_box()" @@@ */
regwindow->address_entry = entry_box(entry_strip, REG_NAME_MAX_LEN,
                                                  REG_ADDR_COL_WIDTH + 8, NULL);
// "style" added implicitly @@@

regwindow->hex_entry = entry_box(entry_strip, MAX_ADDRESS_OR_HEX_ENTRY,
                             board->reg_banks[regwindow->regbank_no].width
                           * REG_VALUE_CHAR_WIDTH + 16,
                             GTK_SIGNAL_FUNC(callback_regwindow_hex));

							// ASCII entry box(?) @@@

scrolledwindow = gtk_scrolled_window_new(NULL, NULL);           /* Main panel */
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                               GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
gtk_widget_show(scrolledwindow);
gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);

if (board->reg_banks[regwindow->regbank_no].width > 0)
  regwindow->clist_ptr = gtk_clist_new(3);       /* Three displayable columns */
else
  regwindow->clist_ptr = gtk_clist_new(2);            /*  or only two columns */

gtk_widget_set_style(regwindow->clist_ptr, fixed_style);
for (temp = 0; temp < (board->reg_banks[regwindow->regbank_no]).number; temp++)
  gtk_clist_append(GTK_CLIST(regwindow->clist_ptr), startdata);
gtk_clist_set_selection_mode(GTK_CLIST(regwindow->clist_ptr),
                             GTK_SELECTION_BROWSE);

gtk_signal_connect(GTK_OBJECT(regwindow->clist_ptr), "select-row",
                   GTK_SIGNAL_FUNC(callback_regwindow_clist), NULL);
gtk_widget_show(regwindow->clist_ptr);
gtk_container_add(GTK_CONTAINER(scrolledwindow), regwindow->clist_ptr);

if (board->reg_banks[regwindow->regbank_no].width > 0)
  gtk_clist_column_titles_show(GTK_CLIST(regwindow->clist_ptr));
                                /* Omit titles for width = 0 (bit display(?)) */

temp_label = column_label(startdata[0], regwindow->clist_ptr, 0,
                          REG_ADDR_COL_WIDTH);
temp_label = column_label(startdata[1], regwindow->clist_ptr, 1,
                          board->reg_banks[regwindow->regbank_no].width
                        * REG_VALUE_CHAR_WIDTH + 8);
temp_label = column_label(startdata[2], regwindow->clist_ptr, 2, -1);

view_updateregwindow(regwindow);
if (board->reg_banks[regwindow->regbank_no].pointer)
  board->reg_banks[regwindow->regbank_no].pointer++;

return frame;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* creates console window (the main one at the bottom)                        */
/*                                                                            */

#ifdef GTK2
GtkWidget *view_create_console(void)             /* Create the console window */
{
  GtkWidget *scrolledwindow;
  GtkWidget *view;
  GtkTextBuffer *buffer;


if (TRACE > 5) g_print("view_create_console\n");

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);

  gtk_widget_ref (scrolledwindow);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

  view = gtk_text_view_new ();
  gtk_widget_ref (view);
  gtk_widget_show (view);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
// Fixed font also - when ready to test @@@
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_container_add (GTK_CONTAINER (scrolledwindow), view);
  gtk_text_buffer_set_text (buffer, "Komodo Console Window\n", -1);
  view_console = buffer;

  return scrolledwindow;
}
#endif

#ifndef GTK2
GtkWidget *view_create_console(void)             /* Create the console window */
{
  GtkWidget *scrolledwindow;
  GtkWidget *text;

if (TRACE > 5) g_print("view_create_console\n");

  scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_ref(scrolledwindow);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  text = gtk_text_new (NULL, NULL);
  gtk_text_set_word_wrap (GTK_TEXT (text), TRUE);
  gtk_text_set_editable (GTK_TEXT (text), FALSE);
  gtk_widget_set_style(text, fixed_style);
  gtk_widget_show (text);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), text);
  gtk_text_insert(GTK_TEXT(text),NULL,NULL,NULL,"Komodo Console Window\n",-1);
  view_console = text;
  return scrolledwindow;
}
#endif

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Creates the status bar                                                     */
/*                                                                            */

GtkWidget *view_create_status_bar(void)
{
GtkWidget *status;
GtkWidget *hbox;
GtkWidget *status_bar;
GtkWidget *progresslabel;
GtkWidget *progressbar;
char *memory_string;

if (TRACE > 5) g_print("view_create_status_bar\n");

status_bar = gtk_frame_new ("Status Bar");
gtk_widget_ref (status_bar);
gtk_widget_show (status_bar);

gtk_frame_set_label_align (GTK_FRAME (status_bar), 0.5, 0);
                                                    /* set to middle of frame */
gtk_frame_set_shadow_type (GTK_FRAME (status_bar), GTK_SHADOW_ETCHED_OUT);

hbox = new_box(FALSE, 0, HORIZONTAL);
//gtk_widget_set_usize (hbox, 700, 40);
gtk_widget_set_usize (hbox, STATUS_BAR_X, STATUS_BAR_Y);

if (EMULATOR == interface_type)      //  [JNZ Modded]
  status = status_message("Mode: Emulation", hbox, TRUE, TRUE, 20);
else
  status = status_message("Mode: Serial",    hbox, TRUE, TRUE, 10);

// Much-less-bodged-than-it-was memory report message		@@@
if ((board->pArray_M[0].length[0] == 0x00)
 && (board->pArray_M[0].length[1] == 0x00))
  {                                      /* Multiple of 64K; report in Mbytes */
  float Mbytes;

  Mbytes = board->pArray_M[0].length[3] * 16
         + (float)board->pArray_M[0].length[2] / 16;

  if (Mbytes <= 1.0)
    memory_string = g_strdup_printf("Memory: %6.4f Mbyte",  Mbytes);
  else
    memory_string = g_strdup_printf("Memory: %6.4f Mbytes", Mbytes);
  }
else
  {
  if ((board->pArray_M[0].length[0] == 0x00)
  && ((board->pArray_M[0].length[1] & 0x03) == 0x00))
    {                                               /* Whole number of kbytes */
    unsigned int kbytes;

    kbytes = (board->pArray_M[0].length[3] << 14)
           + (board->pArray_M[0].length[2] << 6)
           + (board->pArray_M[0].length[1] >> 2);

    memory_string = g_strdup_printf("Memory: %d Kbytes", kbytes);
    }
  else
    {
    unsigned int bytes;

    bytes = (board->pArray_M[0].length[3] << 24)
          + (board->pArray_M[0].length[2] << 16)
          + (board->pArray_M[0].length[1] << 8)
          +  board->pArray_M[0].length[0];

    memory_string = g_strdup_printf("Memory: %d bytes", bytes);
    }
  }

status         = status_message(memory_string, hbox,  TRUE,  TRUE, 20);
g_free(memory_string);
status         = status_message("State: ",     hbox, FALSE, FALSE,  2);
view_enqlabel  = status_message(     NULL,     hbox, FALSE, FALSE,  0);
view_steplabel = status_message(     NULL,     hbox,  TRUE,  TRUE, 20);
                                                //  [JNZ Modded]

gtk_box_pack_start(GTK_BOX(hbox), gtk_vseparator_new(), FALSE, FALSE, 0);

progresslabel  = status_message("Load progress:", hbox, FALSE, FALSE, 2);

progressbar = gtk_progress_bar_new ();
gtk_widget_ref (progressbar);
gtk_widget_show (progressbar);
gtk_box_pack_start (GTK_BOX (hbox), progressbar, TRUE, TRUE, 2);
gtk_progress_configure (GTK_PROGRESS (progressbar), 100, 100, 200);

gtk_progress_set_show_text (GTK_PROGRESS (progressbar), TRUE);
view_progressbar = progressbar;

gtk_container_add (GTK_CONTAINER (status_bar), hbox);

//gtk_widget_set_usize(status_bar,900,4);
return status_bar;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Parses the display list from the .komodo file and invokes the              */
/* functions that will construct the body of the main window                  */
/*                                                                            */

/* the following subroutine goes to the already retrieved window display list
  and parses it accordingly.  R within the parsing list stands for register,
                              M for memory list,
                              C for console,
                              F for flag window,
                              ! for vertical line,
                              ~ for horizontal line
                              
  appropriate functions that create to appropriate widget are then called
  and the current display list can be found in the entry window_list
  in the file ./.komodo        */

GtkWidget *view_parse_list(void)                       /* make the subwindows */
{
GtkWidget *ret;
int tempint;

if (TRACE > 5) g_print("view_parse_list\n");

switch (*(view_window_display_list++))
  {
  case '!':
    ret = view_create_mov_split(HORIZONTAL);
    gtk_paned_pack1 (GTK_PANED (ret), view_parse_list (), FALSE, FALSE);
    gtk_paned_pack2 (GTK_PANED (ret), view_parse_list (), FALSE, FALSE);
    return ret;

  case '~':
    ret = view_create_mov_split(VERTICAL);
    gtk_paned_pack1 (GTK_PANED (ret), view_parse_list (), FALSE, FALSE);
    gtk_paned_pack2 (GTK_PANED (ret), view_parse_list (), FALSE, FALSE);
    return ret;

  case '|':
    ret = new_box(FALSE, 4, HORIZONTAL);
    do
      gtk_box_pack_start (GTK_BOX (ret), view_parse_list (), TRUE, TRUE, 0);
      while (',' == *(view_window_display_list++));
    view_window_display_list--;
    return ret;

  case '-':
    ret = new_box(FALSE, 4, VERTICAL);
    do
      gtk_box_pack_start (GTK_BOX (ret), view_parse_list (), TRUE, TRUE, 0);
      while (',' == (*(view_window_display_list++)));
    view_window_display_list--;
    return ret;

  case 'N':
    ret = gtk_notebook_new ();
    if (call_number_to_notebook == 0) register_notebook = GTK_NOTEBOOK(ret);
                /* save reference to this item if it is the first call to 'N' */
    if (call_number_to_notebook == 1) flags_notebook = GTK_NOTEBOOK(ret);
               /* save reference to this item if it is the second call to 'N' */
   /* @@@ WHAT?!?!?! @@@ */

    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (ret), GTK_POS_LEFT);
    gtk_widget_ref (ret);
    gtk_widget_show (ret);
    tempint = 0;
    do
      {
      char *temp = view_window_display_list;
      GtkWidget *label;

      while ('.' != *view_window_display_list) view_window_display_list++;
      *(view_window_display_list++) = '\0';
      gtk_container_add (GTK_CONTAINER (ret), label = view_parse_list ());
      gtk_container_set_border_width (GTK_CONTAINER (label), 4);
      label = gtk_label_new (temp);
      gtk_widget_ref (label);
      gtk_widget_show (label);
      gtk_notebook_set_tab_label(GTK_NOTEBOOK(ret),
                      gtk_notebook_get_nth_page(GTK_NOTEBOOK(ret), tempint++),
                      label);
      }
      while ('.' == (*(view_window_display_list++)));

    view_window_display_list--;
    call_number_to_notebook++;
    gtk_signal_connect_after(GTK_OBJECT(ret), "switch-page",
                             GTK_SIGNAL_FUNC(callback_reg_notebook_change),
                             NULL);         /* Signal -following- page change */
                        /*  so that the flag window can be brought up to date */
    return ret;

					// Needs code for getting source panel @@@
  case 'R':
    return view_create_register_clist (*(view_window_display_list++) - '0');
  case 'M':
    return view_create_memory_clist(MEM_WIN_DUMP,   FALSE, NULL, MEM_REP_WORD);
  case 'K':
    return view_create_memory_clist(MEM_WIN_SOURCE, FALSE, NULL, MEM_REP_WORD);
  case 'F':
    return view_create_flag_frame (CURRENT);
  case 'S':
    return view_create_flag_frame (SAVED);
  case 'C':
    return view_create_console ();
  }

if (VERBOSE) g_print("Write a proper window line!");
           /* an unknown character has been passed and could not be processed */

return NULL;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Prints a character in the given terminal                                   */
/*                                                                            */

void print_char_in_terminal(uchar current_char, gpointer comms)
{
feature *comms_terminal = comms;

  void print_ch(GtkWidget *terminal, char c)
  {
  #ifndef GTK2
  gtk_text_insert(GTK_TEXT(terminal), NULL, NULL, NULL, &c, 1);
  #endif

  #ifdef GTK2
  gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(terminal), &c, 1);
  #endif

  return;
  }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

if (TRACE > 15) g_print("print_char_in_terminal\n");

if ((current_char >= ' ') && (current_char <= '~'))    /* if printable ASCII */
  {
  print_ch(comms_terminal->data.terminal.text, current_char);
  }
else                                                  /* if control character */
  {
  switch (current_char)                                    /* special actions */
    {
    case 0x07: g_print("\a"); break;                             /* ring bell */
    case 0x09:                                                         /* tab */
      gtk_text_insert(GTK_TEXT(comms_terminal->data.terminal.text), NULL,
                      NULL, NULL, "\t", 1);
    case 0x0A:                                                    /* new line */
    case 0x0D:                                                    /* new line */
      gtk_text_insert(GTK_TEXT(comms_terminal->data.terminal.text), NULL,
                      NULL, NULL, "\n", 1);
      break;
    case 0x08:                                                   /* backspace */
    case 0x7F:                                                      /* delete */
      gtk_text_backward_delete(GTK_TEXT(comms_terminal->data.terminal.text), 1);
      break;
    default:                             /* no special action was carried out */
      if (current_char < 0x80)
        {
        print_ch(comms_terminal->data.terminal.text, '^');   /* Print ^<char> */
        print_ch(comms_terminal->data.terminal.text, current_char | 0x40);
        }
      break;
    }
  }
return;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Displays the status (steps life/total steps) given the string              */
/*                                                                            */

void display_status(char *string)
{
gtk_label_set_text(GTK_LABEL(view_steplabel), string);
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Displays the state of the board according to client status byte            */
/*                                                                            */

void display_board_state(uchar client_status)
{
  char *string;

  switch (client_status & CLIENT_STATE_CLASS_MASK)       /* Class of activity */
    {
    case CLIENT_STATE_CLASS_RESET:                            /* Client reset */
      gtk_label_set_text(GTK_LABEL(view_enqlabel), "Reset");
      break;

    case CLIENT_STATE_CLASS_STOPPED:                        /* Client stopped */
      gtk_label_set_text(GTK_LABEL(view_enqlabel), "Stopped");
      switch (client_status)                         /* Check specific reason */
        {
        case CLIENT_STATE_STOPPED:
          gtk_label_set_text(GTK_LABEL(view_enqlabel), "Stopped");
          break;
        case CLIENT_STATE_BREAKPOINT:
          gtk_label_set_text(GTK_LABEL(view_enqlabel), "Breakpoint");
          break;
        case CLIENT_STATE_WATCHPOINT:
          gtk_label_set_text(GTK_LABEL(view_enqlabel), "Watchpoint");
          break;
        case CLIENT_STATE_MEMFAULT:
          gtk_label_set_text(GTK_LABEL(view_enqlabel), "Page Fault");
          break;
        case CLIENT_STATE_BYPROG:
          gtk_label_set_text(GTK_LABEL(view_enqlabel), "User Stop");
          break;
        default:
          gtk_label_set_text(GTK_LABEL(view_enqlabel), "Stopped");
          break;
        }
      break;

    case CLIENT_STATE_CLASS_RUNNING:                        /* Client running */
      gtk_label_set_text(GTK_LABEL(view_enqlabel), "Running");
      break;

    case CLIENT_STATE_CLASS_ERROR:        /* Client error */
      //    string = g_new(char, (client_status & 0x3f)+1);
                                                        /* WHAT THE HELL... ? */
      //    string[client_status & 0x3F] = '\0';
                                                         /* THIS IS UNDEFINED */
      //    if ((client_status & 0x3F) == board_getchararray(
      //                                    client_status & 0x3f,string))
      //      gtk_label_set_text(GTK_LABEL(view_enqlabel), string);
      //    g_free(string);

      gtk_label_set_text(GTK_LABEL(view_enqlabel), "Error");
      break;

    }                                // end switch
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/* Wants renaming @@@ */

GtkWidget *status_message(char *string, GtkWidget *child,
                          gboolean expand, gboolean fill, guint padding)
{
GtkWidget *handle;

handle = gtk_label_new(string);
gtk_widget_ref(handle);
gtk_widget_show(handle);
gtk_box_pack_start(GTK_BOX(child), handle, expand, fill, padding);
return handle;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Make a movable split; direction is horizontal or vertical                  */
/*                                                                            */

GtkWidget *view_create_mov_split(orientation direction)
{
GtkWidget *view_mov_split;

if (direction == HORIZONTAL) view_mov_split = gtk_hpaned_new();
else                         view_mov_split = gtk_vpaned_new();
gtk_widget_show(view_mov_split);
gtk_paned_set_gutter_size(GTK_PANED(view_mov_split), 10);
return view_mov_split;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *new_box(gboolean homogeneous, gint spacing, orientation way)
{                        /* "distribute"               */
GtkWidget *box;

if (way == HORIZONTAL) box = gtk_hbox_new(homogeneous, spacing);
else                   box = gtk_vbox_new(homogeneous, spacing);
gtk_widget_ref(box);
gtk_widget_show(box);
return box;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Make a `ruler' in a menu                                                   */

GtkWidget *view_separator(GtkMenu *menu)
{
GtkWidget *handle;

handle = gtk_menu_item_new();
gtk_widget_show(handle);
gtk_menu_append(menu, handle);

return handle;                /* Probably needn't return anything - CHECK @@@ */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *column_label(char *string, GtkWidget *clist, int number, int width)
{
GtkWidget *label;

label = gtk_label_new(string);
gtk_widget_show(label);
gtk_clist_set_column_widget(GTK_CLIST(clist), number, label);
if (width > 0) gtk_clist_set_column_width(GTK_CLIST(clist), number, width);

return label;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *push_button(char *string, GtkWidget *box,
                       gboolean expand, gboolean fill, guint padding)
{
GtkWidget *handle;

handle = gtk_button_new_with_label(string);
gtk_widget_ref(handle);
gtk_widget_show(handle);
gtk_box_pack_start(GTK_BOX(box), handle, expand, fill, padding);
return handle;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *entry_box(GtkWidget *box, int max, int length,
                     callback_button_go_fn function)    /* check "typing" @@@ */
{
GtkWidget *handle;

handle = gtk_entry_new_with_max_length(max);
gtk_widget_ref(handle);
gtk_widget_show(handle);
gtk_box_pack_start(GTK_BOX(box), handle, FALSE, TRUE, 0);
gtk_widget_set_usize(handle, length, -2);
gtk_widget_set_style(handle, fixed_style);
if (function != NULL)
  gtk_signal_connect(GTK_OBJECT(handle), "activate", function, NULL);

return handle;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *create_submenu_entry(GtkWidget *submenu,
                                char *string,
/* check "typing" again @@@ */  callback_button_go_fn function,
                                gpointer constant)
{
GtkWidget *handle;

handle = gtk_menu_item_new_with_label(string);
gtk_widget_show(handle);
gtk_menu_append(GTK_MENU(submenu), handle);
gtk_signal_connect(GTK_OBJECT(handle), "activate",
                   GTK_SIGNAL_FUNC(function), constant);
return handle;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/******************************************************************************/


/*                                end of view.c                               */
/*============================================================================*/
