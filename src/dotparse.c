/******************************************************************************/
/*                Name:           dotparse.c				      */
/*                Version:        1.3.0					      */
/*                Date:           24/6/2006				      */
/*		  Parses the Lispy ".komodo" file       	              */
/*									      */
/*============================================================================*/

/*
	`dotparse.c'
	Lispy file parser Stolen from Andrew
	This code belongs to Andrew Bardsley and is used and amended with his permission
	$Id: dotparse.c,v 1.1.1.1 1999/07/15 18:06:52 bardslea Exp $
*/

#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <glib.h>
#include "dotparse.h"

int ScanVerbose = FALSE;
int lastline = 0;

/* ScanNewSCAN{Symbol,...} : node constructors */
PtrSCANNode
ScanNewSCANSymbol (char *symbol, int count)
{
  PtrSCANNode node = (PtrSCANNode) g_malloc (sizeof (SCANNode));
  char *newSymbol;

  if (count == -1)
    count = strlen (symbol);
  newSymbol = (char *) g_malloc (1 + count);
  strncpy (newSymbol, symbol, count);
  newSymbol[count] = '\0';
  if (ScanVerbose)
    g_print ("Symbol:%s ", newSymbol);
  node->type = SCANSymbol;
  node->body.string = newSymbol;

  return node;
}


PtrSCANNode ScanNewSCANString(char *string, int count)
{
PtrSCANNode node;

if (ScanVerbose) g_print ("String");

node = ScanNewSCANSymbol(string, count);
node->type = SCANString;

if (ScanVerbose) g_print ("String");

return node;
}


PtrSCANNode ScanNewSCANNumber(int number)
{
PtrSCANNode node = (PtrSCANNode) g_malloc (sizeof (SCANNode));

node->type = SCANNumber;
node->body.number = number;

if (ScanVerbose) g_print ("Number:%d ", number);

return node;
}


PtrSCANNode ScanNewSCANList(GList * list)
{
PtrSCANNode node = (PtrSCANNode) g_malloc (sizeof (SCANNode));

node->type = SCANList;
node->body.list = list;

return node;
}


/* DeleteSCANNode : node destructor. Deallocates `node' and `node->body' members */
void ScanDeleteSCANNode(PtrSCANNode node)
{
if (!node) return;

switch (node->type)
  {
  case SCANNumber: break;

  case SCANSymbol:
  case SCANString: g_free(node->body.string); break;

  case SCANList:
    {
    GList *listIter = node->body.list;

    while (listIter)
      {
      ScanDeleteSCANNode ((PtrSCANNode) listIter->data);
      listIter = listIter->next;
      }
    g_list_free (node->body.list);
    }
    break;
  }

g_free(node);
}


/* PrintParamObject : print a list node in a lisp like format onto stream `stream'.
	NB. This currently just prints on a single line. */
void ScanPrintSCANNode(FILE *stream, PtrSCANNode node)
{
if (!node) fprintf (stream, "NULL");
else
  switch (node->type)
    {
    case SCANNumber: fprintf (stream, "%d",     node->body.number); break;
    case SCANString: fprintf (stream, "\"%s\"", node->body.string); break;
    case SCANSymbol: fprintf (stream, "%s",     node->body.string); break;
    case SCANList:
      {
      GList *listIter = node->body.list;

      fprintf (stream, "(");

      while (listIter)
        {
        ScanPrintSCANNode (stream, listIter->data);
        listIter = listIter->next;
        if (listIter) fprintf (stream, " ");
        }

      fprintf (stream, ")");
      }
      break;
    }
}



/* ParseSCANNode : parse a SCAN Node out of the currently open scanner,       */
/* `inAList' should be FALSE if we are not expecting a ')' at the end of      */
/*  a list of items.                                                          */

PtrSCANNode ScanParseSCANNode(GScanner *scanner, int inAList)
{
GTokenType tokenType;
GList *list = NULL;
PtrSCANNode ret;
int line     = g_scanner_cur_line(scanner);
int position = g_scanner_cur_position(scanner);
int listtype;
char left[] = { 0, '(', '[', '{' };
char right[] = { 0, ')', ']', '}' };

do
  {
  tokenType = g_scanner_get_next_token(scanner);
  listtype = 1;
  switch (tokenType)
    {
    case G_TOKEN_EOF:
      if (inAList)
        fprintf (stderr, "\nParseSCANNode: premature end of file in list "
                         "started on line %d character %d.\n"
                         "Suspected unclosed loop on line %d\n"
                         "Confused but pressing on\n\a",
                          line, position, lastline);
      return NULL;                                                     /* EOF */

    case G_TOKEN_INT:                                              /* Integer */
      {
      PtrSCANNode num =
            ScanNewSCANNumber (g_scanner_cur_value (scanner).v_int);

      num->line     = g_scanner_cur_line(scanner);
      num->position = g_scanner_cur_position(scanner);
      if (inAList) list = g_list_append (list, (gpointer) num);
      else         return num;
      }
      break;

    case G_TOKEN_IDENTIFIER:                           /* Identifier (Symbol) */
      {
      PtrSCANNode ident =
            ScanNewSCANSymbol (g_scanner_cur_value (scanner).v_identifier, -1);

      ident->line     = g_scanner_cur_line(scanner);
      ident->position = g_scanner_cur_position(scanner);
      if (inAList) list = g_list_append (list, (gpointer) ident);
      else         return ident;
      }
      break;

    case G_TOKEN_STRING:                                            /* String */
      {
      PtrSCANNode str =
            ScanNewSCANString (g_scanner_cur_value (scanner).v_identifier, -1);

      str->line = g_scanner_cur_line (scanner);
      str->position = g_scanner_cur_position (scanner);
      if (inAList) list = g_list_append (list, (gpointer) str);
      else         return str;
      }
      break;

    case G_TOKEN_LEFT_CURLY:
      listtype++;

    case G_TOKEN_LEFT_BRACE:
      listtype++;

    case G_TOKEN_LEFT_PAREN:
      if (ScanVerbose) g_print ("( ");
      if (inAList)
        list = g_list_append (list, ScanParseSCANNode (scanner, listtype));
      else
        {
        ret = ScanParseSCANNode (scanner, TRUE);
        if (G_TOKEN_EOF != g_scanner_get_next_token (scanner))
          g_print("\nParseSCANNode: Extra characters at end of file found "
                  "line %d character %d\nConfused but pressing on\n\a",
                   g_scanner_cur_line(scanner), g_scanner_cur_position(scanner));
        return ret;
        }
        break;

    case G_TOKEN_RIGHT_CURLY:
      listtype++;

    case G_TOKEN_RIGHT_BRACE:
      listtype++;

    case G_TOKEN_RIGHT_PAREN:
      if (ScanVerbose) g_print (" )\n");
      if (inAList)
        {
        if (inAList != listtype)
          fprintf(stderr, "\nEntered list on a %c on line %d character %d\n"
                          "But left the list with a %c on line %d character %d\n"
                          "Confused but pressing on\n\a",
                           left[inAList], line, position, right[listtype],
                           g_scanner_cur_line(scanner),
                           g_scanner_cur_position(scanner));
        ret = ScanNewSCANList (list);
        ret->line      = line;
        ret->position  = position;
        ret->list_type = listtype;
        lastline = line;
        return ret;
        }
      else
        fprintf (stderr, "\nParseSCANNode: unexpected ')' found "
                         "line %d character %d\nConfused but pressing on\n\a",
                          g_scanner_cur_line(scanner),
                          g_scanner_cur_position(scanner));
      break;

    default:
      fprintf(stderr, "\nParseSCANNode: unexpected token "
                      "on line %d character %d\nConfused but pressing on\n\a",
                       g_scanner_cur_line(scanner),
                       g_scanner_cur_position(scanner));
      return NULL;
      break;
    }
  }
  while(inAList);

return NULL;
}


/* {Open,Close}SCANFile : open/close a file for parsing SCAN Nodes.           */
/* Returns the created scanner                                                */
GScanner *ScanOpenSCANFile(char *filename)
{
  int fd = open (filename, O_RDONLY);
  GScannerConfig config;
  GScanner *listScanner = NULL;

  config.cset_skip_characters = " \t\n";
  config.cset_identifier_first =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_+-/*#@~";
  config.cset_identifier_nth =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+-/*#@~";
  config.cpair_comment_single  = ";\n";
  config.case_sensitive        = 1;
  config.skip_comment_multi    = 1;
  config.skip_comment_single   = 1;
  config.scan_comment_multi    = 0;
  config.scan_identifier       = 1;
  config.scan_identifier_1char = 1;
  config.scan_identifier_NULL  = 1;
  config.scan_symbols          = 1;
  config.scan_binary           = 1;
  config.scan_octal            = 1;
  config.scan_float            = 1;
  config.scan_hex              = 1;
  config.scan_hex_dollar       = 0;
  config.scan_string_sq        = 0;
  config.scan_string_dq        = 1;
  config.numbers_2_int         = 1;
  config.int_2_float           = 0;
  config.identifier_2_string   = 0;
  config.char_2_token          = 1;
  config.symbol_2_token        = 0;
  config.scope_0_fallback      = 1;

  if (fd >= 0)
    {
    listScanner = g_scanner_new (&config);
    g_scanner_input_file (listScanner, fd);
    }

  return listScanner;
}

/* Open great big string afor parsing. Returns the created scanner */
GScanner *
ScanOpenSCANString (char *string)
{
  GScannerConfig config;
  GScanner *listScanner = NULL;

  config.cset_skip_characters = " \t\n";
  config.cset_identifier_first =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_+-/*#@~";
  config.cset_identifier_nth =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+-/*#@~";
  config.cpair_comment_single  = ";\n";
  config.case_sensitive        = 1;
  config.skip_comment_multi    = 1;
  config.skip_comment_single   = 1;
  config.scan_comment_multi    = 0;
  config.scan_identifier       = 1;
  config.scan_identifier_1char = 1;
  config.scan_identifier_NULL  = 1;
  config.scan_symbols          = 1;
  config.scan_binary           = 1;
  config.scan_octal            = 1;
  config.scan_float            = 1;
  config.scan_hex              = 1;
  config.scan_hex_dollar       = 0;
  config.scan_string_sq        = 0;
  config.scan_string_dq        = 1;
  config.numbers_2_int         = 1;
  config.int_2_float           = 0;
  config.identifier_2_string   = 0;
  config.char_2_token          = 1;
  config.symbol_2_token        = 0;
  config.scope_0_fallback      = 1;

  listScanner = g_scanner_new (&config);
  g_scanner_input_text (listScanner, string, strlen (string));

  return listScanner;
}


void ScanCloseSCANFile (GScanner * scanner)
{ g_scanner_destroy (scanner); }


/*----------------------------------------------------------------------------*/
/* "Find a list begining with this symbol"                                    */
/* Search for `string' (case insensitive) in `list' as long as as ?????       */
/* If not found return `errdef'                                               */

GList *ScanfindSymbolList(GList * list, char *string, GList *errdef)
{
while (list != NULL)
  {
  if ((SCANList   == ((PtrSCANNode) list->data)->type)
   && (SCANSymbol == ((PtrSCANNode) ((PtrSCANNode) list->data)->body.list->data)->type)
   && !g_strcasecmp (((PtrSCANNode) ((PtrSCANNode) list->data)->body.list->data)->body.string, string))
    return ((PtrSCANNode) list->data)->body.list->next;

  list = list->next;
  }
return errdef;
}

/*----------------------------------------------------------------------------*/

/* Dot file functions*/			// Meaning ???

/*----------------------------------------------------------------------------*/
/* "Find a symbol"                                                            */
/* Search for `string' (case insensitive) in `list' as long as as ?????       */
/* If not found return `errdef'                                               */

int ScanfindSymbol(GList *list, char *string, int okdef, int errdef)
{
while (list != NULL)
  {
  if ((SCANList   == ((PtrSCANNode) list->data)->type)
   && (SCANSymbol == ((PtrSCANNode) ((PtrSCANNode) list->data)->body.list->data)->type)
   && !g_strcasecmp (((PtrSCANNode) ((PtrSCANNode) list->data)->body.list->data)->body.string, string))
    return okdef;

  list = list->next;
  }

return errdef;
}

/*----------------------------------------------------------------------------*/
/* "Find symbol number  e.g. (foo 1)"                                         */
/* Identify `string' & numeric double and return the number                   */
/* Returns `errdef' if not found                                     ??       */

int ScanfindSymbolNumber(GList *list, char *string, int errdef)
{
PtrSCANNode elem;

while (list != NULL)
  {
  elem = SCAN_LIST_FIRST (list);

  if (SCAN_IS_LIST (elem))
    {
    GList *subList = SCAN_LIST (elem);
    PtrSCANNode first  = SCAN_LIST_FIRST  (subList);
    PtrSCANNode second = SCAN_LIST_SECOND (subList);

    if (SCAN_IS_SYMBOL (first) && !g_strcasecmp (SCAN_SYMBOL (first), string)
                               && SCAN_IS_NUMBER (second))
      return SCAN_NUMBER (second);
    }
  list = list->next;
  }

return errdef;
}

/*----------------------------------------------------------------------------*/
/* "Find symbol string e.g. (foo "bar")                                       */
/* Identify `string' & string double and return the (later) string            */
/* Returns `errdef' if not found                                     ??       */

char *ScanfindSymbolString(GList *list, char *string, char *errdef)
{
PtrSCANNode elem;

while (list != NULL)
  {
  elem = SCAN_LIST_FIRST (list);
  if (SCAN_IS_LIST (elem))
    {
    GList *subList = SCAN_LIST (elem);
    PtrSCANNode first  = SCAN_LIST_FIRST  (subList);
    PtrSCANNode second = SCAN_LIST_SECOND (subList);

    if (SCAN_IS_SYMBOL (first) && !g_strcasecmp (SCAN_SYMBOL (first), string)
                               && SCAN_IS_STRING (second))
      return SCAN_STRING (second);
    }
  list = list->next;
  }

return errdef;
}

/*----------------------------------------------------------------------------*/

GList *ScanfindSymbolListNext(GList *list, char *string, GList *errdef)
{
while (list)
  {
  if ((SCANList   == ((PtrSCANNode) list->data)->type)
   && (SCANSymbol == ((PtrSCANNode) ((PtrSCANNode) list->data)->body.list->data)->type)
   && !g_strcasecmp (((PtrSCANNode) ((PtrSCANNode) list->data)->body.list->data)->body.string, string))
    return list->next;

   list = list->next;
  }

return errdef;
}

/*----------------------------------------------------------------------------*/

int ScangetNumberAdvance(GList **pList)
{
int val;

val = 0;                                             /* Return value on error */

//if (!*pList)
if (*pList == NULL) g_print ("expected number!but end\n");
else
  {
  if (SCANNumber != ((PtrSCANNode) (*pList)->data)->type)
    g_print ("expected number!\n");
  else
    {
    val = ((PtrSCANNode) (*pList)->data)->body.number;
    *pList = (*pList)->next;
    }
  }

return val;
}

/*----------------------------------------------------------------------------*/

char *ScangetStringAdvance(GList **pList)
{
char *val;

val = 0;                                             /* Return value on error */

//if (!*pList)
if (*pList == NULL) g_print ("expected string!but end\n");
else
  {
  if (SCANString != ((PtrSCANNode) (*pList)->data)->type)
    g_print ("expected string!\n");
  else
    {
    val = ((PtrSCANNode) (*pList)->data)->body.string;
    *pList = (*pList)->next;
    }
  }

return val;
}

/*----------------------------------------------------------------------------*/

char **ScanStrlist2Strarray(GList *list, int number)
{
char **strings = g_new(char *, number);
int count;

for (count = 0; count < number; count++)
  {
  if (list == NULL)
    {
    fprintf(stderr, "expected array of strings size %d but got %d\n",
                     number, count);
    for (number = 0; number < count; number++)
    fprintf(stderr, "%s,", strings[number]);
      fprintf(stderr, "\nstunned at the stupidity but pressing on\n");
    return strings;
    }

  if (SCANString == ((PtrSCANNode) list->data)->type)
    strings[count] = ((PtrSCANNode) list->data)->body.string;

  list = list->next;
  }

return strings;
}

/*----------------------------------------------------------------------------*/
/* `list' defines a list of (nominally) `number' elements                     */
/* Returns pointer to allocated array (size `number') of pointers to GLists   */

GList **ScanStrlist2Listarray(GList *list, int number)
{
GList **regs;
int count;

regs = g_new(GList*, number); /* Allocate array of pointers; one per register */

for (count = 0; count < number; count++)
  {
  if (list == NULL)
    {
    fprintf(stderr, "expected array of strings/lists size %d but got %d\n",
                     number, count);
    for (number = 0; number < count; number++)
    fprintf(stderr, "%s,", (char*) regs[number]->data);
      fprintf(stderr, "\nstunned at the stupidity but pressing on\n");
    return regs;
    }

  if (SCAN_IS_STRING(SCAN_LIST_FIRST(list)))
    regs[count] = g_list_append(NULL, SCAN_STRING(SCAN_LIST_FIRST(list)));

  if (SCAN_IS_LIST(SCAN_LIST_FIRST(list)))
    {
    GList *names;

    regs[count] = NULL;
    names = SCAN_LIST(SCAN_LIST_FIRST(list));

    while (names != NULL)		// Can this tolerate no names ??? @@
      {
      regs[count] = g_list_append(regs[count], SCAN_STRING(SCAN_LIST_FIRST(names)));
      names = SCAN_LIST_REST(names);
      }
    }

  list = list->next;
  }

return regs;
}

/*                            end of dotparse.c                               */
/*============================================================================*/
