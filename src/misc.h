/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*                Name:           misc.h                                */
/*                Version:        1.5.0                                 */
/*                Date:           20/07/2007                            */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef MISC_H
#define MISC_H
#define uchar unsigned char
#include <glib.h>
#include "interface.h"


typedef enum              /* Internal symbol type; may want extending one day */
{
  SYM_NONE,
  SYM_UNDEFINED,
  SYM_EQUATE,
  SYM_LOCAL,
  SYM_GLOBAL,
  SYM_SECTION,
  SYM_WEAK
}
symbol_type;

typedef struct symbol_name
{
  char       *name;                          /* Pointer to symbol name string */
  long        value;                                        /* Symbol's value */
  symbol_type sym_type;/* Some indication of the symbol's means of definition */
  struct symbol_name *pDef;                            /* Next defined symbol */
//  struct symbol_name *pAlpha;                   /* Alphabetically next symbol */
//  struct symbol_name *pVal;                             /* Next valued symbol */
}
symbol;

typedef enum
{ /* this describes a type of a feature and will be used as member in feature */
  DECIMAL,
  HEXADECIMAL
}
representation_type;

typedef struct
{
  representation_type representation;
}
evaluation_data;


char  *view_hex_conv(unsigned int, int, char*);
char  *view_ascii_conv(unsigned int, int, char*);
char  *view_chararr2hexstrbe(int, uchar*);
char  *view_chararr2hexstr(int, uchar*);
char  *view_chararr2asciistr(int, uchar*);
void   view_chararrAdd(int, uchar*, int, uchar*);
uchar *view_chararrAddint(int, uchar*, int);
uchar *view_chararrAddchararr(int, uchar*, uchar*);
int    view_chararrSubchararr(int, uchar*, uchar*);
long   view_chararrSublong(int, uchar*, long);
int    view_chararr2int(int, uchar*);
uchar *view_int2chararr(int, int);
void   view_chararrCpychararr(int, uchar*, uchar*);
int    view_hexstr2chararr(int, char*, uchar*);
int    view_findstrinarr(char*, char**, int);
int    view_findstrinlistarr(char*, GList**, int);
int    view_findstrinsymboltable(char*, symbol*, int);
char  *view_chararrinregbank(int, uchar*);

// char* view_dis_arm(uchar*, uchar*);
// char* view_dis_thumb(uchar*, uchar*);
char  *view_dis(uchar*, uchar*, int, GList*);

char  *view_get_reg_name(int, reg_bank*);
int    view_find_reg_name(char*, reg_bank*);

int    view_check_button(GtkWidget*);

void    misc_init_symbol_table(void);
void    misc_flush_symbol_table(void);
int     misc_count_symbols(void);
void    misc_add_symbol(char*, long, symbol_type);
boolean misc_get_symbol(char*, int*);
boolean misc_find_a_symbol(char**, char*);

char       *misc_sym_name(int);
long        misc_sym_value(int);
symbol_type misc_sym_type(int);

#endif

/*                                                                      */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
/*                     end of misc.h                                    */
/************************************************************************/
