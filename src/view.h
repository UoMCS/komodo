/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*                Name:           view.h                                */
/*                Version:        1.5.0                                 */
/*                Date:           10/07/2007                            */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef VIEW_H
#define VIEW_H
#include <glib.h>
#include <gtk/gtk.h>
// #include "interface.h"

#define _(String) (String)
#define uchar unsigned char
#define TERMINAL_UPPER_BOUND 20

#define MEM_REP_BYTE             0
#define MEM_REP_2_BYTES          1
#define MEM_REP_4_BYTES          2
#define MEM_REP_8_BYTES          3
#define MEM_REP_16_BYTES         4
#define MEM_REP_32_BYTES         5
#define MEM_REP_HALFWORD         6
#define MEM_REP_2_HALFWORDS      7
#define MEM_REP_4_HALFWORDS      8
#define MEM_REP_8_HALFWORDS      9
#define MEM_REP_16_HALFWORDS    10
#define MEM_REP_WORD            11
#define MEM_REP_2_WORDS         12
#define MEM_REP_4_WORDS         13
#define MEM_REP_8_WORDS         14
#define MEM_REP_ASCII_MAP       15
#define MEM_REP_ASCII_MAP128    16
#define MEM_REP_RULER           -1            /* Instructions to menu builder */
#define MEM_REP_TEMINATOR       -2

//#define LENGTH_OF_MEMORY_WINDOW 36

#define MEM_WINDOW_DEFAULT       0
#define MEM_WINDOW_EXPR          1

#define CURRENT                  0                               /* PSR types */
#define SAVED                    1

#define MAX_ADDRESS_OR_HEX_ENTRY 40
#define DIS_TAB_MAX              24
#define DIS_TAB_INIT             16

#define MEM_ROWS_MAX_INT         40
#define MEM_ROWS_MAX_EXT         64

#define SOURCE_FIELD_COUNT        4
    /* The number of fields which can be input in a single source line import */
#define SOURCE_BYTE_COUNT         4
      /* The total number of bytes which can be input in a single source line */
#define SOURCE_TEXT_LENGTH      100   /* Max length of source line text field */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef enum { MEM_WIN_DUMP, MEM_WIN_SOURCE } mem_win_type;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct win_row_addr_name      /* Record of each row address in window */
{
  unsigned int             address;                    /* Address of this row */
  struct win_row_addr_name *pNext;
}
win_row_addr;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct mem_window_name
{
  mem_win_type type;
  GtkWidget *clist_ptr;                            /* Pointer to the "widget" */
  GtkWidget *address_entry;
  GtkWidget *hex_entry;                                /* the hex blank entry */
  GtkWidget *ascii_entry;                            /* the ASCII blank entry */
  GtkWidget *dis_entry;                        /* the disassembly blank entry */
  GtkWidget *ascii_button;           /* the ASCII toggle button of the window */
  GtkWidget *dis_button;       /* the disassembly toggle button of the window */
  GtkWidget *elf_symbols_button;/*the ELF symbols toggle button of the window */
  GtkWidget *inc_button;/*'increment after entry' toggle button of the window */
  GtkWidget *full_button;  /* List all source lines, not just those with data */
  GtkWidget *scroll;
  GtkWidget *dis_tab;  /* "spin button" that controls the source TAB position */
  int        addr_type;
  char       address_string[MAX_ADDRESS_OR_HEX_ENTRY + 1];       /* +1 for /0 */
  uchar     *address;
  int        count;                                   /* No of rows displayed */
  int        width;                                /* number of bytes per row */
  int        gran;                /* the number of bytes shown in each column */
  GList     *isa;
  win_row_addr *row_addresses;        /* The address of each row as displayed */
// int wait;
// gint timer;
}
mem_window;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct mem_window_entry_name
{
  mem_window                   *mem_data_ptr;
  struct mem_window_entry_name *next;
}
mem_window_entry;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct source_line_name     /* An imported source code (listing) line */
{
  struct source_line_name *pPrev;                            /* Previous line */
  struct source_line_name *pNext;                                /* Next line */
  int                      corrupt;                  /* Flag if value changed */
  int                      nodata;         /* Flag if line has no data fields */
  unsigned int             address;                       /* Address of entry */
  int                      data_size[SOURCE_FIELD_COUNT];  /* Sizes of fields */
  int                      data_value[SOURCE_FIELD_COUNT];     /* Data values */
  char                    *text;                         /* Text, as imported */
}
source_line;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct
{
  source_line *pStart;    /* First line in source (sorted into address order) */
  source_line *pEnd;                                   /* Last line in source */
}
source_file;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct sym_window_entry_name
{
  GtkWidget *clist;                           /* Columns inside symbol window */
  struct sym_window_entry_name *pNext;
}
sym_window_entry;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct reg_window_name
{
  GtkWidget *clist_ptr;
  GtkWidget *address_entry;
  GtkWidget *hex_entry;
  int        regbank_no;
  int        wait;
  gint       timer;
}
reg_window;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct reg_window_entry_name
{
  reg_window                   *reg_data_ptr;
  struct reg_window_entry_name *next;
}
reg_window_entry;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/* Some exported prototypes */
//int         view_updatememwindow(mem_window*);
//int         view_updateregwindow(reg_window*);
mem_window *view_getmemwindowptr(GtkWidget*);
reg_window *view_getregwindowptr(GtkWidget*);
void        view_create_mainwindow(void);
GtkWidget  *view_create_splashwindow(void);
//GtkWidget  *view_create_memorywindow(mem_win_type, int, int, GdkGeometry*);
GtkWidget  *view_create_memorywindow(mem_win_type, int, GdkGeometry*);
GtkWidget  *view_create_symbol_window(GdkGeometry*);
void        view_refresh_symbol_clists(unsigned int, unsigned int);
void        view_remove_symbol_clist(GtkWidget*);
void        view_destroy_memorywindow(mem_window*);
void        print_char_in_terminal(uchar, gpointer);
void        display_status(char*);
void        display_board_state(uchar);
void        view_update_field(GtkCList*, int, int, char*);
char       *view_dis(uchar *, uchar *, int, GList *);




/* Exported variables */
char *view_window_display_list;   /* Display list string derived from .komodo */

mem_window_entry *view_memwindowlist;    /* List of memory windows/containers */
reg_window_entry *view_regwindowlist;         /* As above, for register banks */
source_file       source;    /* Pointers to the loaded source file (or NULLs) */
sym_window_entry *symbol_windows;

int view_step_number;   /* Initial number of steps taken when step is pressed */
int view_step_freq;     /* Initial value in the second box next to multi-step */

gpointer view_console;           /* Pointer to the text in the string console */
gpointer view_comms;

/* Definitions of pixmaps */
GdkPixmap *view_chump_pixmap;
GdkBitmap *view_chump_bitmap;
GdkPixmap *view_tick_pixmap;
GdkBitmap *view_tick_bitmap;
GdkPixmap *view_mulogo_pixmap;
GdkBitmap *view_mulogo_bitmap;
GdkPixmap *view_komodologo_pixmap;
GdkBitmap *view_komodologo_bitmap;
GdkPixmap *view_komodotitle_pixmap;
GdkBitmap *view_komodotitle_bitmap;
GdkPixmap *view_komodoicon_pixmap;
GdkBitmap *view_komodoicon_bitmap;
GdkPixmap *view_komodominiicon_pixmap;
GdkBitmap *view_komodominiicon_bitmap;

GtkTooltips *view_tooltips;  /* one to hold the tooltips of all items in turn */

// GtkWidget *console_entry; /* the entry within the console/terminal is now global */

GtkWidget *view_mainwindow;                  /* refers to our main GTK window */
GtkWidget *view_progressbar;         /* the progress bar widget which is used */
                     /* to make other local progress bar global by assignment */
GtkWidget *view_enqlabel;/* Label for current state of the board e.g. stopped */
GtkWidget *view_cpsr_reg_bank;                     /* handle for flag display */
GtkWidget *view_spsr_reg_bank;                     /* handle for flag display */
GtkWidget *view_breakwindow[2][4];
GtkWidget *view_fileerror;                    /* a widget for error messages, */
                          /*  will be assigned to a new gtk dialogue at times */
GtkWidget *view_refreshbutton;     /* the widget of the toggle button refresh */
GtkWidget *view_binary_load_address;  /* the address of a binary being loaded */

GtkWidget *text_in_terminal[TERMINAL_UPPER_BOUND];
GtkWidget *terminal_active_flag[TERMINAL_UPPER_BOUND];

GtkWidget *centry_load;                 /* the current file entry for loading */


/* Caters for up to TERMINAL_UPPER_BOUND features where the highest serial    */
/*   number for a console is 20                                               */

GtkNotebook *register_notebook;     /* pointer to the register banks notebook */
GtkNotebook *flags_notebook;                 /* pointer to the flags notebook */

boolean spsr_exists;/* Indicates whether SPSR register is currently displayed */
int all_flag_windows_have_been_created;
int source_dis_tabs;                   /* Inset for source window disassembly */

#endif

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                           end of view.h                                    */
/******************************************************************************/
