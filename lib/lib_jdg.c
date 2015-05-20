/*----------------------------------------------------------------------------*/
/* This file contains SVC handlers for 'jimulator' support in COMP15111.      */
/*                                                                            */
/* Compile with "gcc -o <library_name> --shared lib_comp15111.c"              */
/*                                                               JDG 15/9/10  */
/*                                    BUGFIX: read_mem interface JDH 16/8/12  */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#include <stdio.h>
#include "states.h"               /* Definitions used for 'status' checks     */
#include "address_spaces.h"       /* Definitions used for REG_CURRENT et alia */

#define TRUE  (0==0)
#define FALSE (0!=0)

typedef int boolean;

/*----------------------------------------------------------------------------*/

boolean svc_handler (unsigned int svc_no, unsigned char *status)
{
boolean svc_handled;
unsigned int x;

svc_handled = TRUE;            /* Assume it is going to be in the list, below */

switch (svc_no)
  {
  case 0:                                /* Output character R0 (to terminal) */
    put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
                                    /* Bodge PC so that stall looks 'correct' */
    svc_char_out(get_reg(0, REG_CURRENT) & 0XFF);

    if (*status != CLIENT_STATE_RESET)
      put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);       /* Correct PC */
    break;

  case 1:                               /* Input character R0 (from terminal) */
    {
    char c;
    put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
                                    /* Bodge PC so that stall looks 'correct' */
    if (svc_char_in(&c))             /* If it returns without being reset ... */
      {
      put_reg(0, c & 0XFF, REG_CURRENT);                        /* Save input */
      put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);       /* Correct PC */
      }
    }
    break;

  case 2:                                                             /* Halt */
    *status = CLIENT_STATE_BYPROG;
    break;

  case 3:                                   /* Print string @R0 (to terminal) */
    {
    unsigned int str_ptr;
    char c;
    boolean abort;                               /* Set by read_mem: ignored. */

    put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
                                    /* Bodge PC so that stall looks `correct' */

    str_ptr = get_reg(0, REG_CURRENT);
    while (((c = read_mem(str_ptr, 1, FALSE, FALSE, MEM_SYSTEM, &abort)) != '\0')
         && (status != CLIENT_STATE_RESET))
      {
      svc_char_out(c);                                    /* Returns if reset */
      str_ptr++;
      }

    if (status != CLIENT_STATE_RESET)
      put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);       /* Correct PC */
    }
    break;

  case 4:                                                 /* Decimal print R0 */
    {
    int svc_dec_print(unsigned int number)      /* Recursive zero suppression */
      {
      int okay;

      okay = TRUE;
      if (number > 0)                /* else nothing - suppress leading zeros */
        {
        okay = svc_dec_print(number / 10);                  /* Recursive call */
        if (okay) svc_char_out((number % 10) | '0');      /* Returns if reset */
        }
      return okay;
      }

    unsigned int number;
    int okay;

    put_reg(15, get_reg(15, REG_CURRENT) - 8, REG_CURRENT);
                                    /* Bodge PC so that stall looks `correct' */

    number = get_reg(0, REG_CURRENT);
    if (number == 0) okay = svc_char_out('0');    /* Don't suppress last zero */
    else             okay = svc_dec_print(number);        /* Returns if reset */

    if (status != CLIENT_STATE_RESET)
      put_reg(15, get_reg(15, REG_CURRENT), REG_CURRENT);       /* Correct PC */
    }
    break;

  default:
    svc_handled = FALSE;     /* Not handled: inform caller there's more to do */
    break;
  }

return svc_handled;

}

/*----------------------------------------------------------------------------*/
