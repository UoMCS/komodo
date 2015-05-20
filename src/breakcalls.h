/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*		Name:		breakcalls.h				*/
/*		Version:	1.3.0					*/
/*		Date:		14/1/2004				*/
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#include <gtk/gtk.h>

//int breakpoint_mask;
int breakpoint_line[2][4];

void breakpoint_toggled (GtkToggleButton * togglebutton, gpointer user_data);
void breakpoint_set (GtkToggleButton * togglebutton, gpointer user_data);

void breakpoint_select (GtkCList * clist, gint row, gint column,
			GdkEvent * event, gpointer user_data);

void breakpoint_refresh (int);

void breakpoint_enterval (GtkEditable * editable, gpointer user_data);

/*									*/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 	*/
/*                     end of breakcalls.h				*/
/************************************************************************/
