/******************************************************************************/
/*                Name:           breakview.c                                 */
/*                Version:        1.3.0                                       */
/*                Date:           21/7/2004                                   */
/*                  The interface of BR/WP windows                            */
/*                                                                            */
/*============================================================================*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "breakcalls.h"
#include "breakview.h"
#include "pixmap.h"

/*----------------------------------------------------------------------------*/

// Abstraction functions need improving (and naming properly!) @@@

void break_name_etc(GtkWidget *temp, char *name, GtkWidget *breakwindow)
{
gtk_widget_set_name(temp, name);
gtk_widget_ref(temp);
gtk_object_set_data_full(GTK_OBJECT(breakwindow), name, temp,
                         (GtkDestroyNotify) gtk_widget_unref);
gtk_widget_show(temp);
return;
}

/*----------------------------------------------------------------------------*/

GtkWidget *set_up_breakwindow(char *title, char *name, int x, int y)
{
GtkWidget *window;

window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_widget_set_name(window, name);
gtk_object_set_data(GTK_OBJECT(window), name, window);
gtk_window_set_title(GTK_WINDOW(window), title);
gtk_window_set_default_size(GTK_WINDOW(window), x, y);
gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
gtk_widget_ref(window);		// not sure what - was formerly external @@@

return window;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *set_up_scrolledwindow(GtkWidget *window, char *name)
{
GtkWidget *scrolled_window;

scrolled_window = gtk_scrolled_window_new(NULL, NULL);
break_name_etc(scrolled_window, name, window);
gtk_container_add(GTK_CONTAINER(window), scrolled_window);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
return scrolled_window;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *break_vbox(char *name,GtkWidget *window,GtkWidget *clist,int column)
{
GtkWidget *temp;
temp = gtk_vbox_new(FALSE, 0);
break_name_etc(temp, name, window);
gtk_clist_set_column_widget(GTK_CLIST(clist), column, temp);
return temp;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *break_entry(GtkWidget *breakwindow, GtkWidget *vbox,
                       char *label, char *label_name, char *entry_name)
{
GtkWidget *temp;

temp = gtk_label_new(label);            /* This handle not returned to caller */
break_name_etc(temp, label_name, breakwindow);
gtk_box_pack_start(GTK_BOX(vbox), temp, FALSE, FALSE, 0);

temp = gtk_entry_new();
break_name_etc(temp, entry_name, breakwindow);
gtk_box_pack_start(GTK_BOX(vbox), temp, TRUE, TRUE, 0);

return temp;
}

/*----------------------------------------------------------------------------*/

void break_on_off(GtkWidget *window, GtkWidget *vbox, GtkTooltips *tooltips,
                  GtkWidget **delete_button, GtkWidget **activate_button)
{
*delete_button = gtk_button_new_with_label("Delete");
break_name_etc(*delete_button, "buttondelete", window);
gtk_box_pack_start(GTK_BOX(vbox), *delete_button, FALSE, FALSE, 0);
if (tooltips != NULL)
  gtk_tooltips_set_tip(tooltips, *delete_button, "Delete selected entry", NULL);

*activate_button = gtk_toggle_button_new_with_label("Activate");
break_name_etc(*activate_button, "buttonactivate", window);
gtk_box_pack_start(GTK_BOX(vbox), *activate_button, FALSE, FALSE, 0);

return;
}

/*----------------------------------------------------------------------------*/

GtkWidget *create_breakwindow(char *title, int breakpoint_mask)
{

  GtkWidget *button(char *label, char *name, GtkWidget *breakwindow,
                    GtkWidget *table, int a, int b, int c, int d)
    {
    GtkWidget *temp;

    temp = gtk_check_button_new_with_label(label);
    break_name_etc(temp, name, breakwindow);
    gtk_table_attach(GTK_TABLE(table), temp, a, b, c, d,
                   (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
    return temp;
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  GtkWidget *break_options(GtkWidget *optionaddress)
    {
    GtkWidget *menuitem, *menu;

    menu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_label("<X<");
    gtk_widget_show(menuitem);
    gtk_menu_append(GTK_MENU(menu), menuitem);
    menuitem = gtk_menu_item_new_with_label("Mask");
    gtk_widget_show(menuitem);
    gtk_menu_append(GTK_MENU(menu), menuitem);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(optionaddress), menu);
    return menu;
    }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

GtkWidget *breakwindow;
GtkWidget *scrolledwindow;
GtkWidget *clist;
GtkWidget *vboxnumber;
GtkWidget *buttondelete;
GtkWidget *buttonactivate;
GtkWidget *vboxaddressa;
GtkWidget *entryaddressa;
GtkWidget *optionaddress;
GtkWidget *optionaddress_menu;
GtkWidget *vboxaddressb;
GtkWidget *entryaddressb;
GtkWidget *vboxdataa;
GtkWidget *entrydataa;
GtkWidget *optiondata;
GtkWidget *optiondata_menu;
GtkWidget *vboxdatab;
GtkWidget *entrydatab;
GtkWidget *table;
GtkWidget *bit8;
GtkWidget *bit32;
GtkWidget *bit16;
GtkWidget *bit64;
GtkWidget *checkkernel;
GtkWidget *checkread;
GtkWidget *checkwrite;
GtkWidget *checkuser;

breakwindow    = set_up_breakwindow(title, "breakwindow", 1000, 300);
scrolledwindow = set_up_scrolledwindow(breakwindow, "scrolledwindow");

clist = gtk_clist_new(8);
break_name_etc(clist, "clist", breakwindow);
gtk_container_add(GTK_CONTAINER(scrolledwindow), clist);
gtk_clist_set_column_width (GTK_CLIST (clist), 0,  50);
gtk_clist_set_column_width (GTK_CLIST (clist), 1, 100);
gtk_clist_set_column_width (GTK_CLIST (clist), 2,  65);
gtk_clist_set_column_width (GTK_CLIST (clist), 3, 100);
gtk_clist_set_column_width (GTK_CLIST (clist), 4, 150);
gtk_clist_set_column_width (GTK_CLIST (clist), 5,  65);
gtk_clist_set_column_width (GTK_CLIST (clist), 6, 150);
gtk_clist_set_column_width (GTK_CLIST (clist), 7, 220);
gtk_clist_column_titles_show (GTK_CLIST (clist));

vboxnumber = break_vbox("vboxnumber", breakwindow, clist, 0);     /* Column 0 */
break_on_off(breakwindow, vboxnumber, NULL, &buttondelete, &buttonactivate);

vboxaddressa  = break_vbox("vboxaddressa", breakwindow, clist, 1);/* Column 1 */
entryaddressa = break_entry(breakwindow, vboxaddressa, "Address A",
                           "labeladdressa", "entryaddressa");

optionaddress = gtk_option_menu_new ();                           /* Column 2 */
break_name_etc(optionaddress, "optionaddress", breakwindow);
gtk_clist_set_column_widget (GTK_CLIST (clist), 2, optionaddress);
optionaddress_menu = break_options(optionaddress);

vboxaddressb  = break_vbox("vboxaddressb", breakwindow, clist, 3);/* Column 3 */
entryaddressb = break_entry(breakwindow, vboxaddressb, "Address B",
                          "labeladdressb", "entryaddressb");

vboxdataa  = break_vbox("vboxdataa", breakwindow, clist, 4);      /* Column 4 */
entrydataa = break_entry(breakwindow, vboxdataa, "Data A",
                        "labeldataa", "entrydataa");

optiondata = gtk_option_menu_new();                               /* Column 5 */
break_name_etc(optiondata, "optiondata", breakwindow);
gtk_clist_set_column_widget(GTK_CLIST(clist), 5, optiondata);
optiondata_menu = break_options(optiondata);

vboxdatab  = break_vbox("vboxdatab", breakwindow, clist, 6);      /* Column 6 */
entrydatab = break_entry(breakwindow, vboxdatab, "Data B",
                        "labeldatab", "entrydatab");

table = gtk_table_new(2, 4, FALSE);                               /* Column 7 */
break_name_etc(table, "tablemisc", breakwindow);
gtk_clist_set_column_widget(GTK_CLIST(clist), 7, table);

					// Drive following from a data table @@@
bit8        = button("8bit ",  "bit8",        breakwindow, table, 0, 1, 0, 1);
bit32       = button("32bit",  "bit32",       breakwindow, table, 0, 1, 1, 2);
bit16       = button("16bit",  "bit16",       breakwindow, table, 1, 2, 0, 1);
bit64       = button("64bit",  "bit64",       breakwindow, table, 1, 2, 1, 2);
checkuser   = button("User  ", "checkuser",   breakwindow, table, 2, 3, 0, 1);
checkkernel = button("Kernel", "checkkernel", breakwindow, table, 2, 3, 1, 2);
checkread   = button("Read ",  "checkread",   breakwindow, table, 3, 4, 0, 1);
checkwrite  = button("Write",  "checkwrite",  breakwindow, table, 3, 4, 1, 2);

gtk_signal_connect_object (GTK_OBJECT (breakwindow), "delete_event",
                           GTK_SIGNAL_FUNC (gtk_widget_hide),
                           GTK_OBJECT (breakwindow));
gtk_signal_connect (GTK_OBJECT (clist), "select_row",
                    GTK_SIGNAL_FUNC (breakpoint_select),
                    GINT_TO_POINTER (breakpoint_mask | 0));
gtk_signal_connect (GTK_OBJECT (buttondelete), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_set),
                    GINT_TO_POINTER (breakpoint_mask | 1));
gtk_signal_connect (GTK_OBJECT (buttonactivate), "toggled",
                    GTK_SIGNAL_FUNC (breakpoint_set),
                    GINT_TO_POINTER (breakpoint_mask | 0));
gtk_signal_connect (GTK_OBJECT (entryaddressa), "activate",
                    GTK_SIGNAL_FUNC (breakpoint_enterval),
                    GINT_TO_POINTER (breakpoint_mask | 0));
gtk_signal_connect (GTK_OBJECT (entryaddressb), "activate",
                    GTK_SIGNAL_FUNC (breakpoint_enterval),
                    GINT_TO_POINTER (breakpoint_mask | 1));
gtk_signal_connect (GTK_OBJECT (entrydataa), "activate",
                    GTK_SIGNAL_FUNC (breakpoint_enterval),
                    GINT_TO_POINTER (breakpoint_mask | 2));
gtk_signal_connect (GTK_OBJECT (entrydatab), "activate",
                    GTK_SIGNAL_FUNC (breakpoint_enterval),
                    GINT_TO_POINTER (breakpoint_mask | 3));
gtk_signal_connect (GTK_OBJECT (bit8), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x100));
gtk_signal_connect (GTK_OBJECT (bit32), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x400));
gtk_signal_connect (GTK_OBJECT (bit16), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x200));
gtk_signal_connect (GTK_OBJECT (bit64), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x800));
gtk_signal_connect (GTK_OBJECT (checkkernel), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x40));
gtk_signal_connect (GTK_OBJECT (checkread), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x20));
gtk_signal_connect (GTK_OBJECT (checkwrite), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x10));
gtk_signal_connect (GTK_OBJECT (checkuser), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_toggled),
                    GINT_TO_POINTER (breakpoint_mask | 0x80));

return breakwindow;
}

/*----------------------------------------------------------------------------*/

GtkWidget *create_simplebreakwindow(char *title, int breakpoint_mask)
{
GtkWidget *breakwindow;
GtkWidget *scrolledwindow;
GtkWidget *clist;
GtkWidget *vboxnumber;
GtkWidget *buttondelete;
GtkWidget *buttonactivate;
GtkWidget *vboxaddress;
GtkWidget *entryaddress;
GtkTooltips *tooltips;

tooltips = gtk_tooltips_new();

breakwindow    = set_up_breakwindow(title, "simplebreakwindow", 300, 200);
scrolledwindow = set_up_scrolledwindow(breakwindow, "scrolledwindow");

clist = gtk_clist_new (2);
break_name_etc(clist, "clist", breakwindow);
gtk_container_add (GTK_CONTAINER (scrolledwindow), clist);
gtk_clist_set_column_width (GTK_CLIST (clist), 0, 48);	// is "50" above @@
gtk_clist_set_column_width (GTK_CLIST (clist), 1, 80);
gtk_clist_column_titles_show (GTK_CLIST (clist));

vboxnumber = break_vbox("vboxnumber", breakwindow, clist, 0);
break_on_off(breakwindow,vboxnumber,tooltips,&buttondelete,&buttonactivate);

vboxaddress  = break_vbox("vboxaddress", breakwindow, clist, 1);
entryaddress = break_entry(breakwindow, vboxaddress,
                          "Address",  "labeladdress", "entryaddressa");

gtk_signal_connect_object (GTK_OBJECT (breakwindow), "delete_event",
                           GTK_SIGNAL_FUNC (gtk_widget_hide),
                           GTK_OBJECT (breakwindow));
gtk_signal_connect (GTK_OBJECT (clist), "select_row",
                    GTK_SIGNAL_FUNC (breakpoint_select),
                    GINT_TO_POINTER (breakpoint_mask));
gtk_signal_connect (GTK_OBJECT (buttondelete), "clicked",
                    GTK_SIGNAL_FUNC (breakpoint_set),
                    GINT_TO_POINTER (breakpoint_mask | 1));
gtk_signal_connect (GTK_OBJECT (buttonactivate), "toggled",
                    GTK_SIGNAL_FUNC (breakpoint_set),
                    GINT_TO_POINTER (breakpoint_mask | 0));
gtk_signal_connect (GTK_OBJECT (entryaddress), "activate",
                    GTK_SIGNAL_FUNC (breakpoint_enterval),
                    GINT_TO_POINTER (breakpoint_mask | 0));

gtk_object_set_data (GTK_OBJECT (breakwindow), "tooltips", tooltips);

return breakwindow;
}

/*----------------------------------------------------------------------------*/

/*                             end of breakview.c                             */
/*============================================================================*/
