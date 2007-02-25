/**------------------------------------------------------------------------**
 * NOTE:
 * It is my intention to only have the bindings/connections in this file,
 * and call the appropriate routines in other files.
 **------------------------------------------------------------------------**/

#include "stdafx.h"

#include "gooey.h"
#include "main.h"
#include "gladebuf.h"

using namespace std;

GladeXML  *main_window;
extern int port;
extern int udp_hax;

void tvMySharestarget_drag_data_received(GtkWidget          *widget,
                                        GdkDragContext     *context,
                                        gint                x,
                                        gint                y,
                                        GtkSelectionData   *data,
                                        guint               info,
                                        guint               time){
  fprintf(stdout,"tvMySharestarget_drag_data_received:\nDATA=%s\n",data->data);
  g_signal_stop_emission_by_name(widget,"drag_data_received");
  }

void btnSendMessage_clicked(GtkWidget *widget, gpointer user_data) {
  string s=gtk_entry_get_text((GtkEntry*) glade_xml_get_widget (main_window, "txtSendMessage"));
  send_msg( s, port );
}

void btnReceive_clicked(GtkWidget *widget, gpointer user_data)
{
  gtk_widget_set_sensitive(widget, false);
  string msg, from;

  if( recv_msg(msg, port, &from) != SOCKET_ERROR) {
    msg = "received: '" + msg + "' from: " + from;
    gtk_entry_set_text((GtkEntry*) glade_xml_get_widget (main_window, "txtLastReceived"), msg.c_str());
  }
  gtk_widget_set_sensitive(widget, true);
}

void btnSetPort_clicked(GtkWidget *widget, gpointer user_data)
{
  int p = atoi(gtk_entry_get_text((GtkEntry*) glade_xml_get_widget (main_window, "txtSetPort")));
  UsePort(p);
}

void btnSetInterface_clicked(GtkWidget *widget, gpointer user_data)
{
  int i = atoi(gtk_entry_get_text((GtkEntry*) glade_xml_get_widget (main_window, "txtInterface")));
  UseInterface(i);
}

void radioProtocol_clicked(GtkWidget *widget, gpointer user_data)
{
  //NOTE: 3vil hax and only verks with 2 toggles :)
  if(!gtk_toggle_button_get_active ((GtkToggleButton*)widget)) {
    UseUDP(!udp_hax);
    fprintf(stderr, "SWITCH: udp_hax=%s\n",udp_hax?"true":"false");
  }
}

uint32 show_gooey()
{
  GtkWidget *widget;

  /* load the interface from the xml buffer */
  main_window = glade_xml_new_from_buffer(gladebuf, gladebufsize, NULL, NULL);

  /* connect the signals in the interface */
  widget = glade_xml_get_widget (main_window, "btnSendMessage");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `btnSendMessage'!\n");
    return -1;
    }
  else {
    g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnSendMessage_clicked), NULL);
    }

  widget = glade_xml_get_widget (main_window, "btnReceive");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `btnReceive'!\n");
    return -1;
    }
  else {
    g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnReceive_clicked), NULL);
    }

  widget = glade_xml_get_widget (main_window, "btnSetPort");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `btnSetPort'!\n");
    return -1;
    }
  else {
    g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnSetPort_clicked), NULL);
    }

  widget = glade_xml_get_widget (main_window, "btnSetInterface");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `btnSetInterface'!\n");
    return -1;
    }
  else {
    g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnSetInterface_clicked), NULL);
    }

  widget = glade_xml_get_widget (main_window, "radioProtocolIPX");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `radioProtocolIPX'!\n");
    return -1;
    }
  else {
    g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (radioProtocol_clicked), NULL);
    }

  widget = glade_xml_get_widget (main_window, "radioProtocolUDP");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `radioProtocolUDP'!\n");
    return -1;
    }
  else {
    g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (radioProtocol_clicked), NULL);
    }
  //Set DnD targets
  widget = glade_xml_get_widget (main_window, "tvMyShares");
  if(widget==NULL) {
    fprintf(stderr,"Error: can not find widget `tvMyShares'!\n");
    return -1;
    }
  else {
    static GtkTargetEntry target_table[] = {{ }};
    gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL,
                      target_table, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(widget);
    gtk_signal_connect (GTK_OBJECT (widget), "drag_data_received",
                        GTK_SIGNAL_FUNC (tvMySharestarget_drag_data_received),
                        NULL);
    }

  /* Have the delete event (window close) end the program */
  widget = glade_xml_get_widget (main_window, "frmMain");
  g_signal_connect (G_OBJECT (widget), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

  /* start the event loop */
  gtk_main ();
  return true;
}
