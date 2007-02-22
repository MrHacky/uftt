/**------------------------------------------------------------------------**
 * NOTE:
 * It is my intention to only have the bindings/connections in this file,
 * and call the appropriate routines in other files.
 **------------------------------------------------------------------------**/

#include "stdhdr.h"
#include "types.h"
#include "gooey.h"
#include "main.h"
GladeXML  *main_window;
extern int port;

void btnSendMessage_clicked(GtkWidget *widget, gpointer user_data) {
  string s=gtk_entry_get_text((GtkEntry*) glade_xml_get_widget (main_window, "txtSendMessage"));
  send_msg( s, port );
  }

void btnReceive_clicked(GtkWidget *widget, gpointer user_data) {
  gtk_widget_set_sensitive(widget, false);
  string msg, from;

  if( recv_msg(msg, port, &from) != SOCKET_ERROR) {
    msg = "received: '" + msg + "' from: " + from;
    gtk_entry_set_text((GtkEntry*) glade_xml_get_widget (main_window, "txtLastReceived"), msg.c_str());
    }
  gtk_widget_set_sensitive(widget, true);
  }

uint32 show_gooey() {
  GtkWidget *widget;

  //FIXME: Find a better way to store the .glade file
  main_window = glade_xml_new ("src/uftt.glade", NULL, NULL); /* load the interface * /

  /* connect the signals in the interface */
  /* Have the ok button call the ok_button_clicked callback */
  widget = glade_xml_get_widget (main_window, "btnSendMessage");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnSendMessage_clicked), NULL);

  widget = glade_xml_get_widget (main_window, "btnReceive");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnReceive_clicked), NULL);

  /* Have the delete event (window close) end the program */
  widget = glade_xml_get_widget (main_window, "frmMain");
  g_signal_connect (G_OBJECT (widget), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

  /* start the event loop */
  gtk_main ();
  return 0;
  }
