/******************************************************************************/
/*                Name:           chump.c                                     */
/*                Version:        1.4.0                                       */
/*                Date:           21/6/2006                                   */
/*                (Assembly and) Disassembly functions                        */
/*                                                                            */
/*============================================================================*/


/* This code is property of Charles Brej and is used in this project with his */
/* permission.                                                                */


#include "global.h"
#include "chump.h"
#include "dotparse.h"
#include <glib.h>
#include <string.h>
#include <ctype.h>


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Local prototypes                                                           */

void asm_error_pos(GList *);
unsigned int bitmask(unsigned int);
unsigned int ROR(unsigned int, int, unsigned int);
unsigned int ROL(unsigned int, int, unsigned int);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

DefinitionStack *asm_parser(GList *list)
{
if (TRACE > 5) g_print("asm_parser\n");

if (!SCAN_IS_STRING(SCAN_LIST_FIRST(list)))
  {
  g_print("line:%d position:%d  Expected string to define the disassembly\n",
          SCAN_LIST_FIRST(list)->line, SCAN_LIST_FIRST(list)->position);
  return NULL;
  }
return asm_define(list->next, NULL, SCAN_STRING(SCAN_NODE(list->data)));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Print the position of an error as specified in the passed in `list'        */

void asm_error_pos(GList *list)
{
g_print("line: %d position: %d  ",
       ((PtrSCANNode) list->data)->line,
       ((PtrSCANNode) list->data)->position);
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

DefinitionStack *asm_define(GList *parselist, DefinitionStack *deflist,
                            char *name)
{
DefinitionStack *definitionelement;
DefinitionStack *definitionstack;
DefinitionStack *deflisttemp;
GList *sublist;

if (TRACE > 5) g_print("asm_define\n");

definitionelement = g_new(DefinitionStack, 1);

definitionstack = definitionelement;
definitionelement->next = deflist;
definitionelement->rules = NULL;
definitionelement->string = name;
while (parselist)
  {
  if (SCAN_IS_LIST((PtrSCANNode) parselist->data))
    {
    sublist = ((PtrSCANNode) parselist->data)->body.list;
    if (!sublist)
      {
      asm_error_pos(parselist);
      g_print("Empty list when defining '%s'\n", name);
      return definitionelement;                              //    @@@
      }

    if (!sublist->next)
      {
      asm_error_pos(parselist);
      g_print("List of only one element when defining '%s'\n", name);
      return definitionelement;           //     @@@
      }

    if (SCAN_IS_SYMBOL((PtrSCANNode) sublist->data) &&
      SCAN_IS_STRING((PtrSCANNode) sublist->next->data) &&
      !g_strcasecmp(((PtrSCANNode) sublist->data)->body.string,
                     DEFINITIONSTRING))
      definitionstack = asm_define(sublist->next->next, definitionstack,
                          ((PtrSCANNode) sublist->next->data)->body.string);
    else
      definitionelement->rules = g_list_append(definitionelement->rules,
                                              (gpointer) (asm_rule
                                              (sublist, definitionstack)));
    }


  if (SCAN_IS_SYMBOL((PtrSCANNode) parselist->data))
    {
    deflisttemp = definitionstack;
    while (g_strcasecmp(((PtrSCANNode) parselist->data)->body.string,
                        deflisttemp->string))
      {
      deflisttemp = deflisttemp->next;
      if (!deflisttemp)
        {
        asm_error_pos(parselist);
        g_print("'%s' has not been defined\n",
               ((PtrSCANNode) parselist->data)->body.string);
        return definitionelement;             //     @@@
        }
      }
    definitionelement->rules = g_list_concat(definitionelement->rules,
                                             g_list_copy(deflisttemp->rules));
    }
  parselist = parselist->next;
  }

/* Should free all elements above the definition stack */
deflisttemp = definitionstack;         // Redundant (?)  @@@
while (definitionstack != definitionelement)
  {
  deflisttemp = definitionstack;
  definitionstack = definitionstack->next;
  g_free(deflisttemp);
  }

return definitionelement;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

Rule *asm_rule(GList *parselist, DefinitionStack *deflist)
{
Rule *rule;
RuleElement *belement, *telement;
GList *text;
GList *binary;
DefinitionStack *deflisttemp;
GList *binarytemp;
char *legaltest;
int error;                  /* Boolean, for flagging errors within loops etc. */

if (TRACE > 5) g_print("asm_rule\n");

rule = g_new(Rule, 1);
rule->ascii = NULL;
rule->bit   = NULL;
rule->type  = TRANRULE;
if (SCAN_IS_SYMBOL((PtrSCANNode) parselist->data))
  {                                                           /* start of AAA */
  if (!g_strcasecmp(((PtrSCANNode) parselist->data)->body.string, INTEGERSTRING))
    rule->type = INTRULE;
  if (!g_strcasecmp(((PtrSCANNode) parselist->data)->body.string, RELATIVESTRING))
    rule->type = RELATIVERULE;
  if (!g_strcasecmp(((PtrSCANNode) parselist->data)->body.string, UINTEGERSTRING))
    rule->type = UINTRULE;
  if (!g_strcasecmp(((PtrSCANNode) parselist->data)->body.string, URELATIVESTRING))
    rule->type = URELATIVERULE;       // N.B. g_strcasecmp deprecated @@@

  if (TRANRULE == rule->type)
    {
    asm_error_pos(parselist);
    g_print("Only 'int' and 'relative' are valid rules.\n");
                       // Comment violates abstraction of names   @@@
    }
  else
    {
    parselist = parselist->next;

    if (!SCAN_IS_NUMBER((PtrSCANNode) parselist->data))
      {
      asm_error_pos(parselist);
      g_print("Int and float rules need a size as the first operand.\n");
      }
    else
      {
      rule->bit = g_list_append(rule->bit,
              GINT_TO_POINTER(((PtrSCANNode) parselist->data)->body.number));
      parselist = parselist->next;

      while (parselist != NULL)
        {
        if ((!parselist->next)              // Last in list or data wrong
        || !SCAN_IS_SYMBOL((PtrSCANNode) parselist->data)
        || !SCAN_IS_NUMBER((PtrSCANNode) parselist->next->data))
          {
          asm_error_pos(parselist);
          g_print("Int and float rules need mathematical "
                  "operations in separated pairs e.g.: + 4 * 8\n");
          break;                              //   Quit `while' loop @@@
          }

        rule->ascii = g_list_append(rule->ascii,
                                   ((PtrSCANNode) parselist->data)->body.string);
        rule->bit   = g_list_append(rule->bit,
            GINT_TO_POINTER(((PtrSCANNode) parselist->next->data)->body.number));
        parselist = parselist->next->next;
        }                                  // End of while
      }
    }
  }                                                             /* end of AAA */
else                                          /* Presumably SCAN_ISn't_SYMBOL */
  {
  if (parselist->next->next
   || !SCAN_IS_LIST((PtrSCANNode) parselist->data)
   || !SCAN_IS_LIST((PtrSCANNode) parselist->next->data))
    {
    asm_error_pos(parselist);
    g_print("A rule must have two LISTS!\n");
    }
  else
    {
    text   = SCAN_NODE(parselist->data)->body.list;
    binary = SCAN_NODE(parselist->next->data)->body.list;

    while (binary != NULL)
      {
      belement = g_new(RuleElement, 1);
      if (!SCAN_IS_SYMBOL(SCAN_NODE(binary->data)))
        {
        asm_error_pos(binary);
        g_print("A rule binary field must be made from "
                "characters IOXZ or a number starting with '#'\n");
        return rule;                                  //    @@@
        }
      belement->type = TERM;
      belement->string = SCAN_NODE(binary->data)->body.string;
      rule->bit = g_list_append(rule->bit, (gpointer) belement);
      binary = binary->next;
      }

    while (text != NULL)
      {
      telement = g_new(RuleElement, 1);
      rule->ascii = g_list_append(rule->ascii, (gpointer) telement);

      switch (((PtrSCANNode) text->data)->type)
        {
        case SCANString:
          telement->string = SCAN_NODE(text->data)->body.string;
          telement->type = TERM;
          break;

        case SCANList:
          if (!SCAN_NODE(text->data)->body.list)
            {
            asm_error_pos(text);
            g_print("Rule must have text body.\n");
            return rule;                              //    @@@
            }
          else
            {
            if (SCAN_IS_SYMBOL(SCAN_NODE(SCAN_NODE(text->data)->body.list->data)))
              {
              asm_error_pos(text);
              g_print("'%s' should be in quotes.\n",
                     (SCAN_NODE(SCAN_NODE(text->data)->body.list->data))->body.string);
              return rule;                              //    @@@
              }
            else
              {
              if (!SCAN_IS_STRING(SCAN_NODE(SCAN_NODE(text->data)->body.list->data)))
                {
                asm_error_pos(text);
                g_print("no name for inline definition.");
                return rule;                              //    @@@
                }

              telement->string =
                SCAN_NODE(SCAN_NODE(text->data)->body.list->data)->body.string;
              deflisttemp =
                asm_define(SCAN_NODE(text->data)->body.list->next, deflist,
                           telement->string);

              binarytemp = rule->bit;
              while (g_strcasecmp(telement->string,
                          ((RuleElement *) binarytemp->data)->string))
                {
                if (!binarytemp->next)        // No further elements
                  {
                  asm_error_pos(text);
                  g_print("'%s' needs to be in the binary section too.\n",
                     ((PtrSCANNode) text->data)->position, telement->string);
                  return rule;       //    @@@ (Memory allocated (deflisttemp)!)
                  }
                else
                  binarytemp = binarytemp->next;
                }

              ((RuleElement *) binarytemp->data)->type = NONTERM;
              telement->type = NONTERM;

              ((RuleElement *) binarytemp->data)->rules = deflisttemp->rules;
              telement->rules = deflisttemp->rules;

              ((RuleElement *) binarytemp->data)->twin = g_list_index(rule->ascii,
                                                                  telement);
              telement->twin = g_list_position(rule->bit, binarytemp);
              g_free(deflisttemp);
              }
            }
          break;

        case SCANSymbol:
          telement->string = SCAN_NODE(text->data)->body.string;
          deflisttemp = deflist;
          while (g_strcasecmp(telement->string, deflisttemp->string))
            {
            deflisttemp = deflisttemp->next;
            if (!deflisttemp)
              {
              asm_error_pos(text);
              g_print("'%s' has not been defined.\n", telement->string);
              return rule;                              //    @@@
              }
            }

          binarytemp = rule->bit;
          while (g_strcasecmp(telement->string,
                             ((RuleElement *) binarytemp->data)->string))
            {
            binarytemp = binarytemp->next;
            if (!binarytemp)
              {
              asm_error_pos(text);
              g_print("'%s' needs to be in the binary section too.\n",
                      ((PtrSCANNode) text->data)->position, telement->string);
              return rule;                              //    @@@
              }
            }

          ((RuleElement *) binarytemp->data)->type = NONTERM;
          telement->type = NONTERM;
          ((RuleElement *) binarytemp->data)->rules = deflisttemp->rules;
          telement->rules = deflisttemp->rules;
          ((RuleElement *) binarytemp->data)->twin =
                                     g_list_index(rule->ascii, telement);
          telement->twin = g_list_position(rule->bit, binarytemp);
          break;

        default:
          asm_error_pos(text);
          g_print("unexpected thing!\n");
          return rule;                              //    @@@
        }
      text = text->next;
      }

    binary = rule->bit;
    while (binary != NULL)
      {
      if (TERM == ((RuleElement *) binary->data)->type)
        {
        if (*(((RuleElement *) binary->data)->string) == '#')
          ((RuleElement *) binary->data)->string++;   /* Strip '#' if present */

        legaltest = ((RuleElement *) binary->data)->string;

        error = (0 == 1);       /* Will flag illegal letter in following loop */
        while (*legaltest != '\0')                      /* Convert 0/1 to O/I */
          {
          if ('1' == *legaltest) *legaltest = 'I';
          if ('0' == *legaltest) *legaltest = 'O';
          if ('I' != *legaltest && 'O' != *legaltest
           && 'Z' != *legaltest && 'X' != *legaltest) error = (0 == 0);
          legaltest++;
          }

        if (error)       // Dealt with after field converted ...
          {
          asm_error_pos(parselist);
          g_print("This rule's binary field contains '%s' "
                  "which is not a valid binary value "
                  "nor is it in the text field.\n",
                 ((RuleElement *) binary->data)->string); // Any '#' lost
          }

        }
        binary = binary->next;
      }
    }
  }
return rule;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                            */
/* On entry: bitfield is a pointer to a GList of bits containing the          */
/*             remaining part of the instruction, LSB first                   */
/*           rules defines the ISA description- format below                  */
/*           address is the instruction address (note - normal integer @@@)   */
/*
/*  The `rules' data structure starts with a GList, the data fields of which  */
/* point to "Rule"s.  Each Rule record has a RuleType and two pointers to     */
/* further lists {ascii, bit}.  These further lists are derived from the LHS  */
/* and RHS of the couples which define the assembler/disassembler (.komodo).  */
/* The ordering of these subsequent lists is significant, as the position in  */
/* the list is used in searching.  They are assumed to be in corresponding    */
/* order initially.                                                           */
/* `ascii' and `bit' are GLists.  Their data records are "RuleElement"s.      */
/* These contain a field "twin" which is the list position of the             */
/* corresponding element in the other list.  These are NOT pointers.          */
/* Each element contains its own list of Rules.                               */

Disasm asm_disassemble(GList *bitfield, GList *rules, int address)
{
Disasm ret;                              /* Return value from this invocation */
Disasm error;                   /* Alternative return value if error detected */
GList *binary;
RuleElement *newelement;
unsigned int data;
int found, mismatch;   /* Flags indicating success and (intermediate) failure */

if (TRACE > 5) g_print("asm_disassemble ... Address: %08X\n", address);

error.distext  = &dis_error;           /* Initialise to globally scoped value */
error.bitfield = &dis_error;         // Rather grubby way of error reporting @@@

found = (0 == 1);                                  /* Mark as no result found */
while ((rules != NULL) && !found)
  {
  ret.bitfield = bitfield;

  if (TRANRULE != ((Rule *) rules->data)->type)
    {      /* `keyword' rather than straight translation or another hierarchy */
    int temp_data;		// Temp. bodge to localise data until	@@@
				//   pointer=>int cast problem repaired	@@@
    temp_data = (int) ((Rule *) rules->data)->bit->data;

//    if ((int) ((Rule *) rules->data)->bit->data > g_list_length(bitfield))
//      return error;                              //    @@@
    if (temp_data > g_list_length(bitfield)) return error;		//    @@@
//    binary = g_list_nth(ret.bitfield,
//                        (int) ((Rule *) rules->data)->bit->data - 1);
    binary = g_list_nth(ret.bitfield, temp_data - 1);
                 // check this can't fall off end (if so `binary' == NULL) @@@
                                   // Go to specified position in operation @@@
                                   // ->bit->data is field size ???   @@@
                                   // Always at LS end in given iteration
    if (GPOINTER_TO_INT (binary->data))
      {
      if (UINTRULE == ((Rule *) rules->data)->type
       || URELATIVERULE == ((Rule *) rules->data)->type) data = 1;
        else                                             data = -1; //@@@
      }
    else                                                 data = 0;

    while (binary != ret.bitfield)                 /* While not start of list */
      {         /* Field always at LS end in given invocation (but recursive) */
      binary = binary->prev;
      data = data << 1;
      if (GPOINTER_TO_INT(binary->data) != 0) data |= 1;
      }               /* "data" is the data field assembled from the bit list */

               /* Now apply rules for shifting data, offsets (e.g. PC+8) etc. */
    {                                              /* Evaluate operation list */
    GList *number = g_list_last(((Rule *) rules->data)->bit);     /* Operands */
    GList *opp    = g_list_last(((Rule *) rules->data)->ascii);  /* Operators */
    int imm;                                               /* Current operand */
    char *c;                                              /* Current operator */

    while (opp != NULL)      // Operations remain in list (?)   @@@
      {
      imm = GPOINTER_TO_INT(number->data);                   /* Fetch operand */
      c = (char *) opp->data;             /* Fetch pointer to operator string */
      if ('~' == *c)                                            /* `inverse'? */
        {
        c++;                                                     /* Strip '~' */
        switch (*c)         // May be other error cases missing @@@
          {
          case '-': data = imm - data;
            if ((UINTRULE == ((Rule *) rules->data)->type)
             && ((int) data < 0)) return error;          // Error case @@@
            break;
          case '+': data = imm +  data; break;
          case '/': data = imm /  data; break;
          case '*': data = imm *  data; break;
          case '<': data = imm << data; break;
          case '>': data = imm >> data; break;
          case '@':
            data = ROR(imm, data, (int) g_strtod(++c, NULL));      /* NB. ++c */
            break;          // Is this the correct rotation direction? @@@ BUG ?
          default: break;            //    ^^^^^^^  Even CB doesn't know!  @@@
          }
        }
      else
        {
        switch (*c)         // May be other error cases missing @@@
          {
          case '+': data = data - imm;
            if ((UINTRULE == ((Rule *) rules->data)->type)
             && ((int) data < 0)) return error;          // Error case @@@
              break;
          case '-': data = data +  imm; break;
          case '*': data = data /  imm; break;
          case '/': data = data *  imm; break;
          case '>': data = data << imm; break;
          case '<': data = data >> imm; break;
          case '@':
            data = ROR(data, imm, (int) g_strtod(++c, NULL));      /* NB. ++c */
            break;
          default: break;
          }
        }
      opp    = opp->prev;                                /* Move back up list */
      number = number->prev;
      }                                     // End of while (opp != NULL)
    }                                       /* End of evaluate operation list */

    if ((RELATIVERULE == ((Rule *) rules->data)->type)
    || (URELATIVERULE == ((Rule *) rules->data)->type)) data += address;

//    ret.bitfield = g_list_nth(ret.bitfield,            /* Strip off used bits */
//                             (int) ((Rule *) rules->data)->bit->data);
    ret.bitfield = g_list_nth(ret.bitfield, temp_data);/* Strip off used bits */

    newelement = g_new(RuleElement, 1);                        // Allocates @@@
    newelement->string = g_strdup_printf("%X", data);          // Allocates @@@
    newelement->type = TERM2FREE;                  /* Terminal; non-permanent */
    ret.distext = g_list_append(NULL, newelement);
    found = (0 == 0);
    }                        // if (!TRANRULE)
  else
    {
    binary = g_list_last(((Rule *) rules->data)->bit);
    ret.distext = g_list_copy(((Rule *) rules->data)->ascii);
                    /* Make a copy of the pointer list (not the rule records) */
    mismatch = (0 == 1);                  /* Set if/when patterns don't match */
    do                                /* Repeat ... until (mismatch OR found) */
      {
      if (binary != NULL)    /* If any elements are left in the `binary' list */
        {
        if (NONTERM == ((RuleElement *) binary->data)->type)
          {                                  /* Not a leaf node in rules tree */
          Disasm rep;                            /* Reply from recursive call */
          rep = asm_disassemble(ret.bitfield,               /* Recursive call */
                               ((RuleElement *) binary->data)->rules,
                               address);
          if (&dis_error != rep.bitfield)                         /* No error */
            {
            ret.bitfield = rep.bitfield;
            newelement = g_new(RuleElement, 1);        // Allocates @@@
            newelement->rules = rep.distext;
            newelement->type = NONTERM2FREE;         /* Not a leaf, temporary */
            (g_list_nth(ret.distext,
                       ((RuleElement *) binary->data)->twin))->data =
                                                        (gpointer) newelement;
            }
          else
            mismatch = (0 == 0);        /* Fault in recursive call.  Barf out */
          }
        else                     /* "TERM" type, i.e. leaf node in rules tree */
          {
          char *c;
          c = (char *) ((RuleElement *) binary->data)->string;
          while (*c != '\0') c++;                    /* Move to end of string */

          while (((RuleElement *) binary->data)->string != c)
            {                                    /* Unless start of string ...*/
            c--;                        /*  ... move backwards through string */
            if (ret.bitfield != NULL)       /* Bits remaining to compare with */
              {
              if (((*c == 'I') && (GPOINTER_TO_INT(ret.bitfield->data) == 0))
               || ((*c == 'O') && (GPOINTER_TO_INT(ret.bitfield->data) == 1)))
                mismatch = (0 == 0);
              else
                ret.bitfield = ret.bitfield->next;     /* Step on to next bit */
              }
            else
              mismatch = (0 == 0);                         /* Run out of bits */
            }
          // `mismatch' if (bitstring longer than bitfield)
          // OR (bitstring mismatches leading N elements of bitfield (N>=0))
          }
        binary = binary->prev; /* Wasted if terminating `while', but harmless */
        }
      else
        found = (0 == 0);     /* if (binary)  ... end of binary elements list */
      }
      while (!mismatch && !found);            /* Repeat until status resolved */

    if (!found)              /* If `found' drop through, terminate and return */
      {
      asm_freestringlist(ret.distext);           /* Discard list copied above */
      rules = rules->next;
      }
    }                           // end else
  }                             // end while (rules && !found)

if (found) return ret;
else       return error;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                            */
/* On entry: text points at the input string (remaining)                      */
/*           rules defines the ISA                                            */
/*           address is the target address                                    */
/*           last is a Boolean indicating that this invocation must process   */
/*             the complete input string (`text')                             */
/* Returns: list of `Asm' records, allocated internally                       */

GList *asm_assemble(char *text, GList *rules, int address, int last)
{
Asm *ret;                 // Some working space for building return records
Asm *todo;
GList *todolist = NULL;
GList *retlist = NULL;     // Pointer to list of elements to returm
GList *rep;
RuleElement *newelement;
RuleElement *newelement2;
int error;
int terminate;
unsigned int value;
char *start;
char *c;

if (TRACE > 10) g_print("asm_assemble\n");

ret = g_new(Asm, 1);                            /* Allocate a working element */
ret->binary = NULL;    /* Initialise so that it is safe to check for disposal */
ret->ruletext = NULL;  /* Initialise so that it is safe to check for disposal */

while (rules != NULL)
  {
  ret->text   = text;                  /* Construct record in allocated space */

  if (TRANRULE != ((Rule *) rules->data)->type)
    {
    while (' ' == *ret->text) ret->text++;                    /* Strip spaces */
    if (!isxdigit(*ret->text)) break;                 //   fail @@@
    value = 0;                                             /* Read hex number */
    while (isxdigit(*ret->text))
      {
      if (*ret->text <= '9') value = (value << 4) +  *ret->text - '0';
      else                   value = (value << 4) + (*ret->text & 7) + 9;
      ret->text++;
      }

    if ((RELATIVERULE  == ((Rule *) rules->data)->type)
     || (URELATIVERULE == ((Rule *) rules->data)->type)) value -= address;

    {
    GList *number = ((Rule *) rules->data)->bit->next;
    GList *opp    = ((Rule *) rules->data)->ascii;
    int imm;

    while (opp != NULL)
      {
      c = (char *) opp->data;

      if ('~' == *c)
        {
        imm = value;                                        /* Swap variables */
        value = GPOINTER_TO_INT(number->data);
        c++;                                                      /* Lose '~' */
        }
      else
        imm = GPOINTER_TO_INT(number->data);

      switch (*c)      // No reversed operations here - Investigate @@@
        {              // Error detection (& abort) missing @@@  (CB)
        case '+': value += imm; break;
        case '-': value -= imm; break;
        case '*': value *= imm; break;
        case '/': value /= imm; break;
        case '>': value = value >> imm; break;
        case '<': value = value << imm; break;
        case '@':
          value = ROL(value, imm, (int) g_strtod(++c, NULL));     /* N.B. ++c */
          break;
        default: break;
        }
      opp = opp->next;
      number = number->next;
      }     // end while (opp)
    }       // end GList declarations

    {
    unsigned int temp_data;	// Temp. bodge to localise data until	@@@
				//   pointer=>int cast problem repaired	@@@
    temp_data = (int) ((Rule *) rules->data)->bit->data;

//    if ((value < (1 << (unsigned int) ((Rule *) rules->data)->bit->data))
//     || (RELATIVERULE == ((Rule *) rules->data)->type)
//     || (INTRULE == ((Rule *) rules->data)->type))
    if ((value < (1 << (unsigned int) temp_data))
     || (RELATIVERULE == ((Rule *) rules->data)->type)
     || (INTRULE == ((Rule *) rules->data)->type))
      {
//      start = g_new(char, (int) ((Rule *) rules->data)->bit->data + 1);
//                                                    /* Allocate return string */
//      c = start + (int) ((Rule *) rules->data)->bit->data;
      start = g_new(char, temp_data + 1);           /* Allocate return string */
      c = start + temp_data;
      *c = '\0';/* Plant bit field as 'I's and 'O's into string ... backwards */
      while (start != c--)
        {
        if ((value & 1) != 0) *c = 'I'; else *c = 'O';
        value = value >> 1;
        }
                             // Could range check here (??) (value <> 0) @@@

      newelement = g_new(RuleElement, 1);
      newelement->string = start;
      newelement->type = TERM2FREE;            // Leaf node; non-permanent @@@
      asm_freestringlist(ret->binary);             /* Lose any former entries */
      ret->binary = g_list_append(NULL, newelement);   /*  and start new list */
      if (last) while (' ' == *ret->text) ret->text++;
      if (!(last && *(ret->text)))
        {
        retlist = g_list_append(retlist, ret);    // Add record to return list
        ret = g_new(Asm, 1);                      // Allocate new record
        ret->binary = NULL;
        ret->ruletext = NULL;
        }
      }
    }// end of "temp_data"
    }
  else                                                         /* If TRANRULE */
    {
    ret->ruletext = ((Rule *) rules->data)->ascii;
    asm_freestringlist(ret->binary);               /* Lose any former entries */
    ret->binary = g_list_copy(((Rule *) rules->data)->bit);

    terminate = (0 == 1);                       /* Get out of WHILE condition */
    while (!terminate)
      {
      while (' ' == *(ret->text)) ret->text++;                /* Strip spaces */

      if (ret->ruletext == NULL)        // Meaning ??? @@@ No rules left??
        {
        if (last && (*ret->text != '\0')) // Must complete yet input remains (?)
          {
          asm_freestringlist(ret->binary);   // Frees former list
          }
        else              // can leave some bits for later ??
          {
          retlist = g_list_append(retlist, ret); /* Add record to return list */
          ret = g_new(Asm, 1);                 /* Allocate new scratch record */
          ret->ruletext = NULL;
          }
        ret->binary = NULL;     /* Either list scrapped or new list allocated */
        terminate = (0 == 0);
        }
      else                                           /* ret->ruletext != NULL */
        {
        if (NONTERM == ((RuleElement *) ret->ruletext->data)->type)
          {                               /* Not a leaf node - recursive call */
//          while (' ' == *(ret->text)) ret->text++;
          rep = asm_assemble(ret->text,
                             ((RuleElement *) ret->ruletext->data)->rules,
                              address, last && !ret->ruletext->next);

          error = (rep == NULL); /* Recursive call failed to return a pointer */
          if (!error)
            {
            ret->text = ((Asm *) rep->data)->text;
            newelement = g_new(RuleElement, 1);
            newelement->type = NONTERM2FREE;
            newelement->rules = ((Asm *) rep->data)->binary;

            while ((rep = rep->next) != NULL)      /* Step on at head of loop */
              {                            /*  and continue until end of list */
              newelement2 = g_new(RuleElement, 1);
              newelement2->type = NONTERM2FREE;
              newelement2->rules = ((Asm *) rep->data)->binary;
              todo = rep->data;
              todo->ruletext = ret->ruletext;

              todo->binary   = asm_copystringlist(ret->binary);
              (g_list_nth(todo->binary,
                        ((RuleElement *) todo->ruletext->data)->twin))->data =
                                                        (gpointer) newelement2;

//              (g_list_nth(todo->binary = asm_copystringlist(ret->binary),
//                        ((RuleElement *) todo->ruletext->data)->twin))->data =
//                                                        (gpointer) newelement2;
                //  YUK ! @@@

              todolist = g_list_prepend(todolist, todo);
              }

            (g_list_nth(ret->binary,
              ((RuleElement *) ret->ruletext->data)->twin))->data =
                           (gpointer) newelement;
            }
          }
        else                                            /* Leaf node ("TERM") */
          {
          error = (0 == 1);
          c = (char *) ((RuleElement *) ret->ruletext->data)->string;
//          while (' ' == *ret->text) ret->text++;
          while (' ' == *c) c++; /* Strip spaces (already done for ret->text) */
          if ('\t' == *c)
            {                                   /* \t detected ... look ahead */
            if (('f' == *(c + 1)) 
             && (' ' != *ret->text) && (' ' != *(ret->text - 1)))
              error = (0 == 0);
            }
          else          /* Not TAB; see if strings match as far as end of "c" */
            while (!error && (*c != '\0'))
              {
              if ((*(ret->text) != *c)
                && (!isalpha(*c) || !isalpha(*ret->text)
                  || (*c & 0x1F) != (*ret->text & 0x1F)))
                error =  (0 == 0);
              else
                {          /* Characters equal or (both alphabetic and match) */
                do c++; while (' ' == *c); /* Increment and then strip spaces */
                do ret->text++; while (' ' == *ret->text); /* on both strings */
                }
              }         // WHILE
          }             // ELSE (two elses)

        if (error)       // Error   @@@ HOLE HERE (?) @@@
          {        // String mismatch or fault with TAB (?)  @@@
          if (todolist)
            {
            g_free(ret);
            ret = todolist->data;
            todolist = g_list_remove(todolist, todolist->data);
            }
          else
            terminate = (0 == 0);
          }
        ret->ruletext = ret->ruletext->next;
                                       /* Redundant, but safe, if terminating */
        }
      }            // WHILE (!terminate)
    }              // ELSE
  rules = rules->next;
  }                // Original WHILE

asm_freestringlist(ret->binary);                      /* Lose any wanted list */
g_free(ret);                                     /* Discard scratch workspace */

return retlist;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Interface to line assembler.                                               */
/* On entry: text points at the mnemonic string                               */
/*           rules points at the ISA descriptor                               */
/*           address points at the address char[]                             */
/*           addr_length is the number of bytes in the address                */
/*           max is the size of the return buffer                             */
/*           bytes points at the object code buffer char[]                    */
/*           report is a Boolean enable of message reporting (VERBOSE)        */
/* Returns:  number of bytes produced or error code:  -1 = error              */
/*                                                    -2 = buffer too small   */
/* On exit:  return value chars filled at L.S. end of `bytes[]'               */

int asm_assemble_wrapper(char *text, GList *rules,
                         char *address, unsigned int addr_length,
                         int max, char *bytes, int report)
{
GList *asmlist;                    /* Records of possible return object codes */
Asm *asmed;                  /* Used to indicate the first (preferred) result */
int length;                 /* Number of bytes returned; -ve means error code */

asmlist = asm_assemble(text, rules,
                       view_chararr2int(addr_length, address), 1);
if (asmlist == NULL)
  {
  g_print("\a");                                                     /* Beep! */
  length = 0;                                                /* Nothing found */
  }
else
  {
  if ((asmlist->next != NULL) && report)  // VERBOSE - not available
    g_print("%s ... %d possible object code variations\n", text,
                                                       g_list_length(asmlist));

  asmed = (Asm *) (asmlist->data);           /* Get first (only?) return data */
  if (asmed->binary == &dis_error)                                /* If error */
    {
    g_print("\a");                                                   /* Beep! */
    length = -1;
    if (report) g_print("Assembler error\n");
    }
  else
    {
    uchar *hex;                         /* Pointer to assembler return record */
    int i;

    length = asm_sbprint(asmed->binary, &hex);  /* Convert bit list to char[] */
                                  /* This leaves an allocated record at `hex' */
    if (length > max)                                     /* Buffer too small */
      {
      g_print("\a");                                                 /* Beep! */
      length = -2;
      if (report) g_print("Object code longer than allocated space\n");
      }
    else
      for (i=0; i<length; i++) bytes[i] = hex[i];         /* Copy object code */

    g_free(hex);                  /* Discard array allocated by asm_sbprint() */
    }
  }
return length;                              /* Number of bytes in object code */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Copy a bidirectional GList of pointers to `RuleElement's.                  */
/* If an element is `2FREE' (non-permanent) then copy the element too.        */
/*   If the element is also terminal, copy its string.                        */
/*   If the element is also non-terminal, recursively copy its substructure.  */

GList *asm_copystringlist(GList *list)
{
GList *scan;
RuleElement *newrule;

if (TRACE > 5) g_print("asm_copystringlist\n");

list = g_list_copy(list);                        /* NB Redefinition of `list' */
scan = list;                             /* Elements switched from old to new */
while ((scan != NULL) && (scan != &dis_error))
  {
  switch (((RuleElement *) scan->data)->type)
    {
    case NONTERM2FREE:
      newrule = g_new(RuleElement, 1);                /* Allocate new element */
      newrule->type = NONTERM2FREE;                         /* Duplicate type */
      newrule->string = ((RuleElement *) scan->data)->string; /* Share string */
      newrule->rules = asm_copystringlist(((RuleElement *) scan->data)->rules);
      scan->data = newrule;                                        /* Link in */
      break;

    case TERM2FREE:
      newrule = g_new(RuleElement, 1);                /* Allocate new element */
      newrule->type = TERM2FREE;                            /* Duplicate type */
      newrule->string = g_strdup(((RuleElement *) scan->data)->string); /*Copy*/
      newrule->rules = NULL;
      scan->data = newrule;
      break;

    default:
      break;
    }

  scan = scan->next;
  }
return list;                                       /* Return new list pointer */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void asm_freestringlist(GList *list)
{
GList *scan = list;

if (TRACE > 15) g_print("asm_freestringlist\n");

while ((scan != NULL) && (scan != &dis_error))
  {
  switch (((RuleElement *) scan->data)->type)
    {
    case NONTERM2FREE:
      asm_freestringlist(((RuleElement *) scan->data)->rules);
      g_free(scan->data);
      break;

    case TERM2FREE:
      g_free(((RuleElement *) scan->data)->string);
      g_free(scan->data);
      break;

    case NONTERM:
      break;

    case TERM:
      break;

    default:
      g_print("Trying to `asm_freestringlist' unrecognised type\a\n");
      break;
    }

  scan = scan->next;
  }

g_list_free(list);

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Converts an array of `char' values of size `size' into a GList of bits (?!)*/
/* LSB first                                                                  */

GList *uchararr2glist(int size, uchar *vals)
{
GList *ret;
uchar c;
int i, count;

ret = NULL;
for (i = 0; i < size; i++)
  {
  c = vals[i];
  for (count = 8; count != 0; count--)
    {
    ret = g_list_append(ret, GINT_TO_POINTER(c & 1));
    c >>= 1;
    }
  }
return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void asm_print(GList *list)                         /* Not used within Komodo */
{
if (TRACE > 5) g_print("asm_print\n");

if (&dis_error == list) g_print("Undefined");
else
  while (list)
    {
    if (TERM == ((RuleElement *) list->data)->type
     || TERM2FREE == ((RuleElement *) list->data)->type)
      g_print("%s", ((RuleElement *) list->data)->string);
    else
      asm_print(((RuleElement *) list->data)->rules);
    list = list->next;
  }
return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                            */
/*  This procedure is passed a string and returns a (possibly different)      */
/* string; however the only allocation is made in (leaf) calls to             */
/* "asm_sprint_tabstring", when the old string is discarded internally.       */

char *asm_sprint(GList *list, char *replystring)
{
char *oldreplystring;

if (TRACE > 5) g_print("asm_sprint\n");

while (list != NULL)
  {
  if (TERM == ((RuleElement *) list->data)->type              /* If leaf node */
   || TERM2FREE == ((RuleElement *) list->data)->type)
    {
    oldreplystring = replystring;          /* Remember so it can be discarded */
    replystring = asm_sprint_tabstring(((RuleElement *) list->data)->string,
                                        replystring);
    g_free(oldreplystring);                                        /* Discard */
    }
  else
    replystring = asm_sprint(((RuleElement *) list->data)->rules, replystring);
    /* May change value of `replystring' but waste is discarded by the caller */

  list = list->next;
  }

return replystring;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                            */
/* Returns: a newly allocated string, concatenated from the fragments passed, */
/*            or with calculated padding with spaces.                         */

char *asm_sprint_tabstring(char *tabstring, char *replystring)
{
int value, value2;
char *temp;

if (TRACE > 15) g_print("asm_sprint_tabstring\n");

if ('\t' != *tabstring)                                    /* No specific TAB */
  replystring = g_strconcat(replystring, tabstring, NULL);
else
  {
  value2 = 0;
  if ('f' == *(tabstring + 1))
    {
    if (*(tabstring + 2)) value2 = (int) g_strtod(tabstring + 2, &temp);
    }
  else
    {
    if (*(tabstring + 1)) value2 = (int) g_strtod(tabstring + 1, &temp);
    }

  if (value2 == 0) value = 0;
  else             value = value2 - (strlen(replystring) % value2);
  
  if (value <= 0)
    replystring = g_strconcat(replystring, tabstring, NULL);
  else
    {
    temp = g_new(char, value + 1);         /* Make a string of `value' spaces */
    temp[value] = '\0';
    while (value != 0) temp[--value] = ' ';
  
    replystring = g_strconcat(replystring, temp, NULL);/* Spaces after string */
    g_free(temp);
    }
  }

return replystring;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Converts bit list into char array (???)  ((Looks inefficient @@@))         */
/* On entry: list points to a GList of bit values, LSB first (???)            */
/*           data points to a pointer which is used to return a record        */
/* Returns:  the number of bytes allocated                                    */
/* On exit:  the pointer indicated by `data' points to a newly allocated      */
/*             array of char                                                  */

int asm_sbprint(GList *list, uchar **data)
{
char *text;
int size;
char *c;

if (TRACE > 5) g_print("asm_sbprint\n");

text = asm_sprint(list, g_strdup(""));
             /* Allocates; asm_sprint deallocates or returns string at `text' */
                  /* May deallocate original string, hence needs "g_strdup()" */

size = strlen(text);                               /* Number of bits returned */
c = text + size;                                       /* Point at terminator */
size = (size + 7) >> 3;                            /* Calculate size in bytes */

*data = g_new0(char, size);             // ALLOCATES @@@

for (size = 0; c-- > text; size++)      // N.B. "size" is something different
  if ('X' == *c || 'I' == *c) ((*data)[size >> 3]) |= 1 << (size & 7);

g_free(text);
return (size >> 3);
}

/*----------------------------------------------------------------------------*/
/* Local utility subroutines                                                  */

unsigned int bitmask(unsigned int size)
{                                             /* 2**N - 1  Limited to 32 bits */
if (size >= 32 ) return 0xFFFFFFFF;
else             return (1 << size) - 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* ROR value by imm places in a field of length size                          */
/* Currently fails for imm>31 etc.   @@@                                      */

unsigned int ROR(unsigned int value, int imm, unsigned int size)
{
return ((value >> imm) | (value << (size - imm))) & bitmask(size);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* ROR value by imm places in a field of length size                          */
/* Currently fails for imm>31 etc.   @@@                                      */

unsigned int ROL(unsigned int value, int imm, unsigned int size)
{
return ((value << imm) | (value >> (size - imm))) & bitmask(size);
}

/*                               end of chump.c                               */
/*============================================================================*/
