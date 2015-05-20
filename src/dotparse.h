/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*		Name:		dotparse.h				*/
/*		Version:	1.3.0					*/
/*		Date:		28/6/2004				*/
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


/*
	Lispy file parser
	
	$Id: dotparse.h,v 1.1.1.1 1999/07/15 18:06:52 bardslea Exp $
	Neah killed by Charley and now its caled the dotparser
*/

#ifndef DOTPARSE_H
#define DOTPARSE_H

#include <stdio.h>
#include <glib.h>






/* SCANNode : node node type                                                  */
typedef enum
{ SCANSymbol, SCANString, SCANNumber, SCANList }
SCANType;

typedef struct SCANNode
{
  SCANType type;
  int line;
  int position;
  int list_type;
  union
  {
    char *string;	                          /* String and (or??) Symbol */
    int number;
    GList *list;
  }
  body;
}
SCANNode, *PtrSCANNode;

#define SCAN_NODE(node)   ((PtrSCANNode)(node))
#define SCAN_IS_LIST(node)   ((node) && (node)->type == SCANList)
#define SCAN_IS_SYMBOL(node) ((node) && (node)->type == SCANSymbol)
#define SCAN_IS_STRING(node) ((node) && (node)->type == SCANString)
#define SCAN_IS_NUMBER(node) ((node) && (node)->type == SCANNumber)

#define SCAN_LIST(node)   ((node)->body.list)
#define SCAN_SYMBOL(node) ((node)->body.string)
#define SCAN_STRING(node) ((node)->body.string)
#define SCAN_NUMBER(node) ((node)->body.number)

/* SCAN_LIST_... : take GLists of PtrSCANNodes */
#define SCAN_LIST_FIRST(list) ((PtrSCANNode) ((list) ? (list)->data : NULL))
#define SCAN_LIST_REST(list)                 ((list) ? (list)->next : NULL)
#define SCAN_LIST_SECOND(list) (SCAN_LIST_FIRST(SCAN_LIST_REST(list)))


PtrSCANNode Scantopnode;
int ScanVerbose;



/* ScanNewSCAN{Symbol,...} : node constructors                                */
extern PtrSCANNode ScanNewSCANSymbol (char *symbol, int count);
extern PtrSCANNode ScanNewSCANString (char *string, int count);
extern PtrSCANNode ScanNewSCANNumber (int number);
extern PtrSCANNode ScanNewSCANList (GList * list);

/* ScanDeleteSCANNode : node destructor.                                      */
/* Deallocates `node' and `node->body' members                                */
extern void ScanDeleteSCANNode (PtrSCANNode node);

/* ScanPrintSCANNode : print a list node in a lisp like format onto           */
/*  stream `stream'.   NB. This currently just prints on a single line.       */
extern void ScanPrintSCANNode (FILE * stream, PtrSCANNode node);

/* ScanSCANIsHeaded : returns TRUE if the given node is a list with a head    */
/*  symbol the same as headSymbol.                                            */
/*  eg. SCANIsHeaded ( "(myNode 10 20)", "myNode" ) returns TRUE.             */
extern gboolean ScanSCANIsHeaded (PtrSCANNode node, char *headSymbol);

/* ScanParseSCANNode : parse a SCAN Node out of the currently open scanner,   */
/*  `inAList' should be FALSE if we are not expecting a ')' at the end of     */
/*  a list of items.                                                          */
extern PtrSCANNode ScanParseSCANNode (GScanner * scanner, int inAList);

/* {Open,Close}ScanSCANFile : open/close a file for parsing SCAN Nodes.       */
/* Returns the created scanner                                                */
extern GScanner *ScanOpenSCANFile (char *filename);

/* Open great big string afor parsing. Returns the created scanner            */
extern GScanner *ScanOpenSCANString (char *string);

void ScanCloseSCANFile (GScanner * scanner);

// Find a list begining with this symbol
GList *ScanfindSymbolList (GList * list, char *string, GList * errdef);

// Find a symbol
int ScanfindSymbol (GList * list, char *string, int okdef, int errdef);

// Find symbol number  e.g. (foo 1)
int ScanfindSymbolNumber (GList * list, char *string, int errdef);

// Find symbol string e.g. (foo "bar")
char *ScanfindSymbolString (GList * list, char *string, char *errdef);

// Find a symbol list but return the next element of the original list
GList *ScanfindSymbolListNext (GList * list, char *string, GList * errdef);

// Get the number and advance the list one forward
int ScangetNumberAdvance (GList ** list);

// Get the string and advance the list one forward
char *ScangetStringAdvance (GList ** list);

// Get turn a string list and turn it into a string array
char **ScanStrlist2Strarray (GList * list, int number);

/* Get a string list and turn it into an array of lists containing strings    */
GList **ScanStrlist2Listarray (GList *list, int number);


#endif                                                          /* DOTPARSE_H */

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                           end of dotparse.h                                */
/******************************************************************************/
