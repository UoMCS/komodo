/******************************************************************************/
/*                Name:           evaluate.c                                  */
/*                Version:        1.5.0                                       */
/*                Date:           19/07/2007                                  */
/*                Provides the function Evaluate() which evaluates            */
/*                  expressions in KMD                                        */
/*                                                                            */
/*============================================================================*/


#include <stdio.h>
#include "misc.h"                                   /* Needed for datastructs */

#include "definitions.h"                        /* Definitions & enumerations */

unsigned int Evaluate(char **string, int *value, evaluation_data eval_data);
int     Get_variable(char **input, int *value, int *unary, int *bracket,
                      evaluation_data eval_data);
char   *strip_hex_prefix(char *temp);
int     check_hex(char character);
int     check_decimal(char character);
int     Get_operator(char **input, int *operator, int *priority);
void    Strip_spaces(char **input);
boolean check_existence_in_symbol_table(char *ident);
int     find_value_in_symbol_table(char *ident);
boolean check_identifier_first_char(char input);
boolean identifier_breaker(char input);


#define mathstack_size 20
                     /* Enumerations used for both unary and binary operators */
#define plus                 0
#define minus                1
#define not                  2
#define multiply             3
#define divide               4
#define end                  5
#define closebr              6
#define left_shift           7
#define right_shift          8
#define and                  9
#define or                  10
#define xor                 11

/*----------------------------------------------------------------------------*/
/* Evaluate - modulo current word length                                      */
/* On entry: **string points to a pointer to the input string                 */
/*            *value points to the location for the result value              */
/*             eval_data carries aux. data (currently just radix info.)       */
/* On exit:  the pointer at **string is adjusted to the end of the expression */
/*           the value at *value contains the result, assuming no error       */
/*           the return value is the error status                             */

unsigned int Evaluate(char **string, int *value, evaluation_data eval_data)
{
int math_stack[mathstack_size];
unsigned int math_SP, error;

  void Eval_inner(int priority, int *value)
  {                                        /* Main function shares stack etc. */
  int done;
  int operator, operand, unary, bracket;

  done = FALSE;                                      /* Termination indicator */

  math_stack[math_SP] = priority;
  math_SP = math_SP + 1;                              /* Stack `start' marker */

  while (!done)
    {
    if ((error=Get_variable(string,&operand,&unary,&bracket,eval_data))==eval_okay)
      {
      if (bracket) Eval_inner(1, &operand);               /* May return error */
      if (error == eval_okay)
        {
        switch (unary)               /* Can now apply unary to returned value */
          {
          case plus:                      break;
          case minus: operand = -operand; break;
          case not:   operand = ~operand; break;
          }

        if ((error = Get_operator(string, &operator, &priority)) == eval_okay)
          {
          while ((priority <= math_stack[math_SP - 1])
                          && (math_stack[math_SP - 1] > 1))
            { /* If priority decreasing and previous a real operator, OPERATE */
            switch (math_stack[math_SP - 2])
              {
              case plus:
                operand = math_stack[math_SP - 3] + operand;
                break;
              case minus:
                operand = math_stack[math_SP - 3] - operand;
                break;
              case multiply:
                operand = math_stack[math_SP - 3] * operand;
                break;
              case divide:
                operand = math_stack[math_SP - 3] / operand;
                break;
              case left_shift:
                operand = math_stack[math_SP - 3] << operand;
                break;
              case right_shift:
                operand = math_stack[math_SP - 3] >> operand;
                break;
              case and:
                operand = math_stack[math_SP - 3] & operand;
                break;
              case or:
                operand = math_stack[math_SP - 3] | operand;
                break;
              case xor:
                operand = math_stack[math_SP - 3] ^ operand;
                break;

              default:
                break;
              }
            math_SP = math_SP - 3;
            }
          done = (priority <= 1);               /* Next operator a ")" or end */

          if (!done)
            {                                  /* Priority must be increasing */
            if ((math_SP + 3) <= mathstack_size)                      /* PUSH */
              {
              math_stack[math_SP] = operand;
              math_stack[math_SP + 1] = operator;
              math_stack[math_SP + 2] = priority;
              math_SP = math_SP + 3;
              }
            else
              error = eval_mathstack_limit;           /* Don't overflow stack */
            }
          else
            {                      /* Now bracketed by terminators.  Matched? */
            if (priority == math_stack[math_SP - 1]) math_SP = math_SP - 1;
            else if (priority == 0) error = eval_not_closebr;       /* Errors */
            else                     error = eval_not_openbr;
            }
          }
        }
      }
    if (error != 0) done = TRUE;          /* Terminate on error whatever else */
    }

  *value = operand;

  return;
  }


error = eval_okay;             /* "Evaluate" initialised and called from here */
math_SP = 0;
Eval_inner(0, value);                /* Potentially recursive evaluation code */

return error;
}

/*----------------------------------------------------------------------------*/
/* Get a quantity from the front of the passed ASCII input string, stripping  */
/* the value in the process.                                                  */
/* On entry: **input points to a pointer to the input string                  */
/*            *value points to the location for the result value              */
/*            *unary points to the location for the result's unary indicator  */
/*            *bracket points to the location of a Boolean "found '(' signal  */
/* On exit:  the pointer at **string is adjusted to the end of the variable   */
/*           the value at *value contains the result, assuming no error       */
/*           the value at *unary contains a unary code, assuming no error     */
/*           the value at *bracket contains a "'(' found instead" indicator   */
/*           the return value is the error status                             */

/* Currently only gets hex values */

int Get_variable(char **input, int *value, int *unary, int *bracket,
                 evaluation_data eval_data)
{
int status;
char *temp;

status = eval_no_operand;                            /* In case nothing found */
temp = *input;                               /* String pointer within routine */
Strip_spaces(&temp);

*unary   = plus;                                                   /* Default */
*bracket = FALSE;                                    /* Default - no brackets */
*value   = 0;

/* Deal with unary operators */
if      (*temp == '+') {                 (temp)++; Strip_spaces(&temp); }
else if (*temp == '-') { *unary = minus; (temp)++; Strip_spaces(&temp); }
else if (*temp == '~') { *unary = not;   (temp)++; Strip_spaces(&temp); }

if (*temp == '(')
  {                                         /* Open brackets instead of value */
  *bracket = TRUE;
  (temp)++;                                                   /* Skip bracket */
  status = eval_okay;                                         /* Legal syntax */
  }
else
  {
  if (check_identifier_first_char(*temp))               /* Try for identifier */
    {
    int i = 0;
    int string_pos = 0;
    char ident[64];

    while ((!identifier_breaker(temp[i])) && (i<63))   /* Count is a hack @@@ */
      {                                         /* accumulate identifier name */
      ident[string_pos] = temp[i];
      string_pos++;
      i++;
      }
    ident[string_pos] = '\0';
    if (check_existence_in_symbol_table(ident))           /* see if it exists */
      {
      *value = find_value_in_symbol_table(ident);            /* get its value */
      status = eval_okay;
      temp += i;
      }
    else                                              /* if it does not exist */
      {
      status = eval_no_identifier;
      }
    }
  }

if (status != eval_okay)                      /* Try for a number - no prefix */
  {
  switch (eval_data.representation)          /* check representation required */
    {
    case DECIMAL:
      {                               /* Try for a decimal number - no prefix */
      int x;
      while (x = check_decimal(*temp), x >= 0)            /* Accumulate value */
        {*value = (*value * 10) + x;  (temp)++;  status = eval_okay;}
      }
      break;                             /* "status" flags something acquired */
    case HEXADECIMAL:
      {
      int x;
      temp = strip_hex_prefix(temp);
      while (x = check_hex(*temp), x >= 0)                /* Accumulate value */
        {*value = (*value << 4) + x;  (temp)++;  status = eval_okay;}
      }
      break;                             /* "status" flags something acquired */

    default: break;
    }
  }

if (status == eval_okay) *input = temp;   /* Move input pointer if successful */
return status;                                           /* Return error code */
}

/*----------------------------------------------------------------------------*/
/* Check if the passed character can start an identifier                      */
/* On entry: input is the character under test                                */
/* On exit:  the return value is true if the character can start an           */
/*           identifier, false otherwise                                      */

boolean check_identifier_first_char(char input)
{
return (((input >= 'A') && (input <= 'Z')) || ((input >= 'a') && (input <= 'z'))
      || (input == '_') || (input == '$')  ||  (input == '#') || (input == '.'));
/*          SOME MORE LEGAL SYMBOLS                          */

}

/*----------------------------------------------------------------------------*/
/* Check if the passed character breaks an identifier character string        */
/* On entry: input is the character under test                                */
/* On exit:  the return value is true if the character breaks an              */
/* identifier, false otherwise                                                */

boolean identifier_breaker(char input)
{
return !(check_identifier_first_char(input)
         || (input >= '0' && input <= '9'));
// this needs to be extended if new operations are added!
}

/*----------------------------------------------------------------------------*/
/* Check if the passed identifier exists in the symbol table                  */
/* On entry: ident is the identifier under test                               */
/* On exit:  the return value is true if the identifier exists in the symbol  */
/* table, false otherwise                                                     */

boolean check_existence_in_symbol_table(char *ident)
{
int dummy;
boolean found_flag;

reg_bank *regbank = &board->reg_banks[0];        /* get current register bank */

found_flag = (view_find_reg_name(ident, regbank) >= 0);   /* True if register */

if (!found_flag)                   /* try symbols only if no register matched */
  found_flag = misc_get_symbol(ident, &dummy);

return found_flag;
}

/*----------------------------------------------------------------------------*/
/* Returns the value of an identifier based on the symbol table               */
/* On entry: ident is the identifier to be searched                           */
/* On exit:  the return value is the value of this identifier                 */

int find_value_in_symbol_table(char *ident)
{
int i;
int value, found_flag;
reg_bank *regbank = &board->reg_banks[0];        /* get current register bank */

i = view_find_reg_name(ident, regbank);
found_flag = (i >= 0);

if (found_flag)
  return view_chararr2int(regbank->width, &regbank->values[i * regbank->width]);

value = 0;                    /* Default (BODGE) "should never happen in KMD" */

found_flag = misc_get_symbol(ident, &value);

if (!found_flag && VERBOSE)
  g_print("The identifier could not be found. Defaulting to %08X.\n", value);

return value;
}

/*----------------------------------------------------------------------------*/
/* Examine passed string and strip off hex prefix if found. Accepted prefices */
/* are {"&", "0x", "0X"}.  Spaces are not tolerated.                          */
/* On entry: temp points to next character                                    */
/* On exit:  the return value is a pointer into the passed string beyond the  */
/*             prefix (if any).                                               */

char *strip_hex_prefix(char *temp)
{
if       (*temp == '&') return temp+1;
else if ((*temp == '0') && ((*(temp+1) & 0xDF) == 'X')) return temp+2;
else return temp;
}

/*----------------------------------------------------------------------------*/
/* Check if the passed character represents a legal hex digit.                */
/* On entry: character is the character under test                            */
/* On exit:  the return value is the hex value or -1 if not a legal hex digit */

int check_hex(char character)
{
if      ((character >= '0') && (character <= '9')) return character - '0';
else if ((character >= 'A') && (character <= 'F')) return character - 'A' + 10;
else if ((character >= 'a') && (character <= 'f')) return character - 'a' + 10;
//else if ((character == 'x') || (character == 'X') || (character == '&')) return 0;
                 // Ignore prefix characters AT ANY POINT   @@@ (for JNZ)
else return -1;
}

/*----------------------------------------------------------------------------*/
/* Check if the passed character represents a legal decimal digit.            */
/* On entry: character is the character under test                            */
/* On exit:  the return value is the decimal value or -1 if not a legal hex   */
/* digit                                                                      */

int check_decimal(char character)
{
if ((character >= '0') && (character <= '9')) return character - '0';
else return -1;
}

/*----------------------------------------------------------------------------*/
/* Get an operator from the front of the passed ASCII input string, stripping */
/* it in the process.  Returns the token and the priority.                    */
/* On entry: **input points to a pointer to the input string                  */
/*            *operator points to the location for the operator code          */
/*            *priority points to the location for the priority code          */
/*                   0 is the lowest priority and is reserved for terminators */
/*                   priority 1 is reserved for brackets                      */
/* On exit:  the pointer at **string is adjusted to beyond the operator       */
/*           the value at *operator contains the operator code                */
/*           the value at *priority contains the operator priority            */
/*           the return value is the error status                             */

int Get_operator(char **input, int *operator, int *priority)
{
int status;
char *temp;

  int check_terminator(char c)            /* Test for valid end of expression */
  {
  switch (c)
    {                                        /* Add other terminators in here */
    case '\0':
    case '\n': return TRUE;  break;
    default:   return FALSE; break;
    }
  }


temp = *input;                               /* String pointer within routine */
Strip_spaces(&temp);
status = eval_no_operator;
                   /* in case no operator was found, this will be the default */
if (check_terminator(*temp)) {*operator = end;                status=eval_okay;}
else if (*temp == '+')       {*operator = plus;     (temp)++; status=eval_okay;}
else if (*temp == '-')       {*operator = minus;    (temp)++; status=eval_okay;}
else if (*temp == '*')       {*operator = multiply; (temp)++; status=eval_okay;}
else if (*temp == '/')       {*operator = divide;   (temp)++; status=eval_okay;}
else if (*temp == ')')       {*operator = closebr;  (temp)++; status=eval_okay;}
else if (*temp == '|')       {*operator = or;       (temp)++; status=eval_okay;}
else if (*temp == '&')       {*operator = and;      (temp)++; status=eval_okay;}
else if (*temp == '^')       {*operator = xor;      (temp)++; status=eval_okay;}
else if ((*temp == '<') && (*(temp + 1) == '<'))
                             {*operator=left_shift; (temp) += 2; status=eval_okay;}
else if ((*temp == '>') && (*(temp + 1) == '>'))
                             {*operator=right_shift;(temp) += 2; status=eval_okay;}
else if (((*temp&0xDF)=='A')&&(((*(temp+1))&0xDF)=='N')&&(((*(temp+2))&0xDF)=='D'))
                             {*operator = and;      (temp) += 3; status=eval_okay;}
else if (((*temp&0xDF)=='O')&&(((*(temp+1))&0xDF)=='R'))
                             {*operator = or;       (temp) += 2; status=eval_okay;}
else if (((*temp&0xDF)=='X')&&(((*(temp+1))&0xDF)=='O')&&(((*(temp+2))&0xDF)=='R'))
                             {*operator = xor;      (temp) += 3; status=eval_okay;}
else if (((*temp&0xDF)=='E')&&(((*(temp+1))&0xDF)=='O')&&(((*(temp+2))&0xDF)=='R'))
                             {*operator = xor;      (temp) += 3; status=eval_okay;}


switch (*operator)                                      /* Priority "look up" */
  {                                     /* The first two priorities are fixed */
  case end:         *priority = 0; break;
  case closebr:     *priority = 1; break;
  case plus:        *priority = 2; break;
  case minus:       *priority = 2; break;
  case multiply:    *priority = 3; break;
  case divide:      *priority = 3; break;
  case left_shift:  *priority = 6; break;
  case right_shift: *priority = 6; break;
  case and:         *priority = 5; break;
  case or:          *priority = 4; break;
  case xor:         *priority = 4; break;
  }

if (status == eval_okay) *input = temp;   /* Move input pointer if successful */
return status;                                           /* Return error code */
}

/*----------------------------------------------------------------------------*/

void Strip_spaces(char **input)
{
while ((**input == ' ') || (**input == '\t')) (*input)++;
return;
}

/*----------------------------------------------------------------------------*/

/*                                end of evaluate.c                           */
/*============================================================================*/
