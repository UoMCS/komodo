/******************************************************************************/
/*                Name:           misc.c                                      */
/*                Version:        1.5.0                                       */
/*                Date:           20/07/2007                                  */
/*                Misc. functions mainly hex and binary conversions           */
/*                                                                            */
/*============================================================================*/


#include "global.h"
#include "interface.h"
#include <glib.h>
#include <ctype.h>
#include "misc.h"
//#include "evaluate.c"                    /* evaluating arithmetic expressions */

#define uchar unsigned char

/*----------------------------------------------------------------------------*/
/* Definitions only used locally                                              */

symbol *symbol_table;            /* A symbol table that is made up of symbols */
int symbol_count;                        /* Number of symbols in symbol table */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Convert integer into hex string                                            */
/* On entry: data holds the value                                             */
/*           size is the number of digits to convert                          */
/*           destination is a pointer the output string position              */
/* Returns:  an updated version of position, incremented by size chars        */

char *view_hex_conv(unsigned int data, int size, char *destination)
{
unsigned int digit;

if (TRACE > 15) g_print("view_hex_conv\n");

while (size > 0)
  {
  size = size - 1;
  digit = ( data >> (4 * size) ) & 0xF;
  if (digit <= 9) *destination = digit + '0';
  else            *destination = digit + 'A' - 10;
  destination++;
  }
return destination;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Convert char array into ascii string - ('.' for non-printing characters)   */
/* On entry: data holds the value                                             */
/*           size is the number of bytes (not digits) to convert              */
/*           destination is a pointer the output string position              */
/* Returns:  an updated version of position, incremented by size chars        */

char *view_ascii_conv(unsigned int data, int size, char *destination)
{
unsigned char c;

if (TRACE > 15) g_print("view_ascii_conv\n");

while (size > 0)
  {
  c = data & 0xFF;
  data = data >> 8;
  if ((c < 0x20) || (c >= 0x7F)) c = '.';
  *destination++ = c;
  size = size - 1;
  }

return destination;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Convert char array into hex string     Big Endian                          */
/* Calls "g_snprint" from "glib"                                              */
/* Allocates its own space, to which it returns a pointer                     */

char *view_chararr2hexstrbe(int count, uchar *values)
{
char *ret = g_new(char, count * 2 + 1);                   /* Allocates memory */
char *ptr = ret;

if (TRACE > 15) g_print("view_chararr2hexstrbe\n");

while (count--)
  {
  g_snprintf(ptr, 3, "%02X", (int) values[count]); /* This also terminates \0 */
  ptr += 2;                                            /* Step string pointer */
  }
return ret;                                             /* Must g_free result */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Convert char array into hex string     Little Endian                       */
/* Calls "g_snprint" from "glib"                                              */
/* Allocates its own space, to which it returns a pointer                     */

char *view_chararr2hexstr(int count, uchar *values)
{
char *ret = g_new(char, count * 2 + 1);                   /* Allocates memory */
char *ptr = ret;

if (TRACE > 15) g_print("view_chararr2hexstr\n");

while (count--)
  {
  g_snprintf(ptr, 3, "%02X", (int) *values);
  ptr += 2;
  values++;                                         /* Pointer steps forwards */
  }
return ret;                                             /* Must g_free result */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Convert character array into printable string                              */
/* Allocates memory for return string                                         */

char *view_chararr2asciistr(int count, uchar *values)
{
char *ret = g_new(char, count + 1);    /* Allocates memory  +1 for terminator */
int index;

if (TRACE > 15) g_print("view_chararr2asciistr\n");

for (index = 0; index < count; index++)                  /* Copy/modify bytes */
  if ((values[index] < ' ') || (values[index] >= 0x7F)) ret[index] = '.';
  else ret[index] = values[index];

ret[index] = '\0';                            /* Append NUL string terminator */

return ret;                                             /* Must g_free result */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* "acc" may be the same as "byte_string"                                     */

void view_chararrAdd(int count, uchar *byte_string, int number, uchar *acc)
{
int index, temp;

if (TRACE > 15) g_print("view_chararrAdd\n");

for (index = 0; index < count; index++)
  {
  temp = byte_string[index] + (number & 0xFF);             /* Add next 8 bits */
  acc[index] = temp & 0xFF;
  number = number >> 8;                                 /* Shift to next byte */
  if (temp >= 0x100) number++;                             /* Propagate carry */
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Add number to byte_string of length count and return result in newly       */
/*  allocated memory                                                          */

uchar *view_chararrAddint(int count, uchar *byte_string, int number)
{
uchar *acc = g_new(uchar, count);               /*bit array + int => bitarray */

if (TRACE > 15) g_print("view_chararrAddint\n");

view_chararrAdd(count, byte_string, number, acc);
return acc;                                             /* Must g_free result */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Adds two input arrays of count bytes                                       */
/* Result returned in newly allocated array of similar type                   */

uchar *view_chararrAddchararr(int count, uchar *value1, uchar *value2)
{
uchar *ret = g_new0(uchar, count);
int index, temp;
int carry = 0;

if (TRACE > 15) g_print("view_chararrAddchararr\n");

for (index = 0; index < count; index++)
  {
  temp = value1[index] + value2[index] + carry;
  ret[index] = temp & 0xFF;
  carry = temp >> 8;
  }
return ret;                                             /* Must g_free result */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int misc_chararr_sub_to_int(int count, uchar *value1, uchar *value2)
{
int ret = 0;                                  /* bit array - bit array => int */

if (TRACE > 15) g_print("misc_chararr_sub_to_int\n");

while (count--) ret = (ret << 8) + (int) value1[count] - (int) value2[count];
return ret;    /* Carry propagated because intermediate -int- can be negative */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

long view_chararrSublong(int count, uchar *value1, long value2)
{
if (TRACE > 15) g_print("view_chararrSublong\n");

while (count--) value2 -= value1[count] << (count << 3);
return value2;                                    /* bit array - long => long */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Accumulate count bytes from memory array (little endian) and return as int */

int view_chararr2int(int count, uchar *input)
{
int ret = 0;                                              /* bit array => int */

if (TRACE > 15) g_print("view_chararr2int\n");

while (count--) ret = (ret << 8) + (input[count] & 0xFF);
return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Strip count bytes from value and place in memory (little endian)           */
/* Allocates own memory space                                                 */
/* Returns pointer to low end of array                                        */

uchar *view_int2chararr(int count, int value)
{
uchar *ret = g_new(uchar, count);                          /* Allocate memory */
uchar *c = ret;

if (TRACE > 15) g_print("view_int2chararr\n");

while (count--)
  {                                                       /* int => bit array */
  *c = value & 0xFF;
  value = value >> 8;
  c++;
  }

return ret;                                             /* Must g_free result */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Copy count characters from source to destination                           */

void view_chararrCpychararr(int count, uchar *source, uchar *destination)
{
if (TRACE > 15) g_print("view_chararrCpychararr\n");

while (count--) destination[count] = source[count];       /* Copy char array */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Evaluate an input string and return a value as an array of bytes           */
/* On entry length_of_result is the size of the resultant array returned at   */
/*  returned_unsigned_string.  passed_string is the string to parse.          */
/* On exit the value is in returned_unsigned_string and the return value      */
/*  indicates the error status.                                               */
/* Returns 1 if okay, 0 if a fault was found                                  */
/*                       array length  input string   pointer to result       */

int view_hexstr2chararr(int length_of_result, char *passed_string,
                        uchar *returned_unsigned_string)
{
unsigned int status;           /* retains the status of expression evaluation */
int result_integer;                               /* the result as an integer */
int i;                                                          /* loop index */
evaluation_data eval_data;

if (TRACE > 10) g_print("view_hexstr2chararr\n");

eval_data.representation = HEXADECIMAL;

status = Evaluate(&passed_string, &result_integer, eval_data);
                                                           /* evaluate string */
for (i = 0; i < (length_of_result); i++)
  returned_unsigned_string[i] = ((result_integer >> (8 * i)) & 0xFF);
                                     /* shift hexadecimal and mask for safety */

for (i = 4; i < length_of_result; i++)
  returned_unsigned_string[i] = 0;        /* BODGE BECAUSE Evaluate IS 32-BIT @@@ */

return (!status);
/* status equals 0 when successful, view_hexstr2chararr returns 1 when successful */
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Compress any spaces from passed string.                                    */
/* Must not move start of string so that deallocation can take place.         */

char *remove_spaces(char *string)
{
int src, dest = 0;

if (TRACE > 15) g_print("remove_spaces\n");

for (src = 0; string[src] != '\0'; src++)      /* Compress spaces from string */
  if (' ' != string[src])
    string[dest++] = string[src];

string[dest] = '\0';                         /* Terminator not copied in loop */

return string;              /* Same as is passed in; syntactically convenient */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Find string in an array of strings and return index of last numbered match */
/* Whole string must match, case insensitive and all input spaces are ignored */
/* Inputs are the string, a pointer to an array of strings and the array size */
/* Returns -1 if not found                                                    */
//								Probably redundant @@
int view_findstrinarr(char *string, char **array, int arrcount)
{
char *mystr;

if (TRACE > 15) g_print("view_findstrinarr\n");

mystr = remove_spaces(g_strdup(string));

while ((arrcount--) && g_strcasecmp(mystr, array[arrcount]));
                                         /* Terminates if/when string matches */
g_free(mystr);

return arrcount;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Find string in an array of lists of strings (as used in register bank).    */
/* Whole string must match, case insensitive and all input spaces are ignored */
/* Inputs are the string, a pointer to an array of lists of strings and       */
/* the array size.                                                            */
/* Returns index of first numbered match or -1 if not found                   */
//					Probably specific to find_reg_name	@@
int view_findstrinlistarr(char *string, GList **array, int arrcount)
{
char *mystr;
int arrindex;
boolean found;
GList *pTemp;

if (TRACE > 15) g_print("view_findstrinlistarr\n");

mystr = remove_spaces(g_strdup(string));     /* Duplicate and compress string */
found = FALSE;

for (arrindex = 0; (arrindex < arrcount); arrindex++)        /* For each list */
  {
  for (pTemp = array[arrindex]; (pTemp != NULL) && !found; pTemp = pTemp->next)
    found = (g_strcasecmp(mystr, (char*) pTemp->data) == 0);
                  /* Chain down each list ; steps past if found, but so what? */
  if (found) break;          /* Grubby but simple way to avoid post-increment */
  }

g_free(mystr);

if (found) return arrindex; else return -1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Find string in a symbol table and return index                             */
/* As above with different record format                                      */

int view_findstrinsymboltable(char *string, symbol *array, int arrcount)
{
char *mystr;

if (TRACE > 15) g_print("view_findstrinsymboltable\n");

mystr = remove_spaces(g_strdup(string));

while ((arrcount--) && g_strcasecmp(mystr, array[arrcount].name));
                                         /* Terminates if/when string matches */
g_free(mystr);

return arrcount;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Search for a value in a register bank and return the name of the first     */
/* register found.  A '+' is appended if other registers have the same value. */
/* Returns: pointer to string of register(+) identifier or NULL if not found  */

char *view_chararrinregbank(int count, uchar * value)
{
int regbank;
int regno;
int byteno;
boolean match;
char *str_ptr;

if (TRACE > 15) g_print("view_chararrinregbank\n");

str_ptr = NULL;                                                    /* Default */

                       /* Only annotate with registers if they are up to date */
for (regbank = board->num_regbanks - 1; regbank >= 0; regbank--)
  if ((board->reg_banks[regbank].pointer)           /* Real register bank ... */
   && (count == board->reg_banks[regbank].width)   /* ... of appropriate size */
   && board->reg_banks[regbank].valid)
    {
    for (regno = 0; regno < board->reg_banks[regbank].number; regno++)
      {                                              /* For each register ... */
      match = TRUE;                  /* Assume match and then prove otherwise */
      for (byteno = board->reg_banks[regbank].width - 1;
          (byteno >= 0) && match; byteno--)     /* Also terminate on mismatch */
        match = (value[byteno] == board->reg_banks[regbank].values
                         [regno * board->reg_banks[regbank].width + byteno]);
      if (match)
        {
        if (str_ptr == NULL)                          /* First register found */
          str_ptr = view_get_reg_name(regno, &(board->reg_banks[regbank]));
        else
          {
          char *temp;

          temp = str_ptr;                        /* Don't lose old string yet */
          str_ptr = g_strconcat(str_ptr, "+", NULL);
                                               /* Concatenate into new string */
          g_free(temp);                                /* and free old string */
          return str_ptr;                   /* Bodge ... early get-out clause */
          }
        }
      }
    }                                           /* ELSE return NULL (default) */

return str_ptr;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/*              These are ARM.C disassemble instructions replaced by LITHP in ASM.C
 * 
 * char* view_dis_arm(uchar* address, uchar* code) // disassemble an ARM instruction
 * {
 *   char* str = g_new0(char, 100);
 *   dis_string_ptr = str;
 *   if(board->cpu_ref==1)
 *           arm_disassemble(view_chararr2int(4,address),view_chararr2int(4,code));
 *   else if(board->cpu_ref==2)
 *           mips_disassemble(str, view_chararr2int(4,code),view_chararr2int(4,address));
 *   return str;
 * }
 *  
 * char* view_dis_thumb(uchar* address, uchar* code) // disassemble an ARM(??) instruction
 * {
 *   char* str = g_new0(char, 100);
 *   dis_string_ptr = str;
 *   if(board->cpu_ref==1)
 *         thumb_disassemble(view_chararr2int(4,address),view_chararr2int(2,code));
 *   return str;
 * }
 */

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*  Disassembles an instruction                                               */
/*  inputs:  count specifies the length                                       */
/*           address specifies the address of the instruction                 */
/*           code specifies the instruction                                   */
/*  returns: the required string                                              */
/*                                                                            */
/*                                                                            */

char *view_dis(uchar * address, uchar * code, int count, GList * isa)
{
Disasm disasm;
char *ret = g_strdup("");
char *temp;
GList *codelist;
int intaddress = view_chararr2int(board->memory_ptr_width, address);

if (TRACE > 15) g_print("view_dis\n");

codelist = uchararr2glist(count, (char *) code);
disasm.bitfield = codelist;
// do     {
disasm = asm_disassemble(disasm.bitfield, isa, intaddress);

if (disasm.distext == &dis_error)
  {
  temp = g_strconcat(ret, "Undefined Instruction", NULL);
  g_free(ret);
  ret = temp;
  disasm.bitfield = NULL;
  }
else
  {
  ret = asm_sprint(disasm.distext, ret);
  asm_freestringlist(disasm.distext);
  }

//        } while (disasm.bitfield);

g_list_free(codelist);
return ret;


/*  char* str;
// These are MIPS.C and ARM.C disassemble instructions replaced by CHUMP in ASM.C  
   if (8 == count) {
   str = g_new0(char, 150);
   dis_string_ptr = str;
   if(board->cpu_ref==1)
   arm_disassemble(view_chararr2int(4,address)  ,view_chararr2int(4,code));
   if(board->cpu_ref==2)
   mips_disassemble(dis_string_ptr, view_chararr2int(4, code),
                                    view_chararr2int(4, address));
   while(dis_string_ptr<str+50) (*(dis_string_ptr++))=' ';
   if(board->cpu_ref==1)
   arm_disassemble(view_chararr2int(4,address)+4,view_chararr2int(4,code+4));
   if(board->cpu_ref==2)
   mips_disassemble(dis_string_ptr, view_chararr2int(4, code),
                                    view_chararr2int(4, address));
   return str;
   }
   if (4 == count) return view_dis_arm  (address, code);
   if (2 == count) return view_dis_thumb(address, code);
   return g_strdup("Unsupported instruction width"); */
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Find the first (default) name of reg in regbank.  Allocates a string.      */
/* On entry: reg is the register number                                       */
/*           regbank is the register bank to search                           */
/* Returns:  allocated string with register name or allocated null string     */

char *view_get_reg_name(int reg, reg_bank *regbank)
{
if ((reg >= 0) && (reg < regbank->number))
  return g_strdup(regbank->names[reg]->data);
else
  return g_strdup("");
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Find the number of a register given (one of) its name(s); -1 if not found  */
/* On entry: name is a register name                                          */
/*           regbank is the register bank to search                           */
/* Returns:  number of register in bank if found; -1 if not found             */

int view_find_reg_name(char *name, reg_bank *regbank)
{
return view_findstrinlistarr(name, regbank->names, regbank->number);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Check state of `button' - returns false (inactive) if button doesn't exist */

int view_check_button(GtkWidget *button)
{
return (button != NULL)
 && (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void misc_init_symbol_table(void)
{
symbol_table = NULL;
symbol_count = 0;
return;
}

/*----------------------------------------------------------------------------*/

void misc_flush_symbol_table(void)
{
symbol *pSym, *pTrash;

pSym = symbol_table;
while (pSym != NULL)                                       /* For all symbols */
  {
  g_free(pSym->name);                                     /* Free name string */
  pTrash = pSym;                                              /* Keep pointer */
  pSym = pSym->pDef;                                               /* Step on */
  g_free(pTrash);                                        /* Free symbol entry */
  }

misc_init_symbol_table();                       /* Reset pointers, count etc. */

return;
}

/*----------------------------------------------------------------------------*/
/* Return number of symbols in table                                          */

int misc_count_symbols(void) { return symbol_count; }

/*----------------------------------------------------------------------------*/

void misc_add_symbol(char *name, long value, symbol_type sym_type)
{
symbol *pSym, *pNew;

pNew = g_new(symbol, 1);
pNew->name     = g_strdup(name);
pNew->value    = value;
pNew->sym_type = sym_type;
pNew->pDef     = NULL;

if (symbol_table == NULL) symbol_table = pNew;
else
  {                                                         /* Append on list */
  for (pSym = symbol_table; pSym->pDef != NULL; pSym = pSym->pDef);
  pSym->pDef = pNew;
  }

symbol_count++;

return;
}

/*----------------------------------------------------------------------------*/
/* Read a value from "symbol_table"                                           */
/* On entry: name points at the symbol name                                   */
/*           value points at the place for the result                         */
/* Returns:  Boolean to indicate symbol found                                 */
/*           value at pointer "value" updated, if found                       */

boolean misc_get_symbol(char *name, int *value)
{
boolean found_flag;
symbol *pSym;

found_flag = FALSE;

for (pSym = symbol_table; pSym != NULL; pSym = pSym->pDef)
  {
  found_flag = (strcasecmp(pSym->name, name) == 0);
  if (found_flag) break;
  }

if (found_flag) *value = (int) pSym->value;

return found_flag;
}

/*----------------------------------------------------------------------------*/
/* Find a symbol with a given value; first defined match (if any) returned    */
/* Only a pointer (alias) is returned - no string is allocated                */
/* On entry: name points to the place to put the result                       */
/*           value points to an array holding the value                       */
/* Returns:  Boolean to indicate symbol with value found                      */
/*           value at pointer "name" updated, if found                        */

boolean misc_find_a_symbol(char **name, char *value)
{
boolean found_flag;
symbol *pSym;

found_flag = FALSE;

for (pSym = symbol_table; pSym != NULL; pSym = pSym->pDef)
  {
  if (((pSym->sym_type == SYM_LOCAL)
    || (pSym->sym_type == SYM_GLOBAL))
   && (view_chararrSublong(board->memory_ptr_width, value, pSym->value) == 0))
    found_flag = TRUE;
  if (found_flag) break;  // Want to rank types, eventually (e.g.  global > local) @@
  }

if (found_flag) *name = pSym->name;

return found_flag;
}

/*----------------------------------------------------------------------------*/
// Temporary functions for abstraction of symbol table @@@

symbol *find_sym(int i)
{
symbol *pSym;

for (pSym = symbol_table; (pSym != NULL) && (i > 0) ; i--, pSym = pSym->pDef);
return pSym;  
}

char       *misc_sym_name (int i) { return find_sym(i)->name;     }
long        misc_sym_value(int i) { return find_sym(i)->value;    }
symbol_type misc_sym_type (int i) { return find_sym(i)->sym_type; }

/*----------------------------------------------------------------------------*/
/******************************************************************************/

/*                                end of misc.c                               */
/*============================================================================*/
