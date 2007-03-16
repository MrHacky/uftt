/**------------------------------------------------------------------------**
* NOTE:
* It is my intention to only have the bindings/connections in this file,
* and call the appropriate routines in other files.
**------------------------------------------------------------------------**/

#include "stdafx.h"

#include "gooey.h"
#include "yarn.h"
#include "main.h"
#include "gladebuf.h"
#include "sharelister.h"
#include "sharelister_gui.h"
#include "yarn.h"

using namespace std;

GladeXML  *main_window;
extern int port;
extern bool udp_hax;
extern ServerInfo * myServer;
GtkTreeView* sharetreeview;

#ifdef G_OS_WIN32
	#define URI_EXTRA_MUNCH 1
#else
	#define URI_EXTRA_MUNCH 0
#endif

// TODO: move this elsewhere?
template<typename T>
void findandreplace( T* source, const T& find, const T& replace )
{
	size_t j = 0;
	for (;(j = source->find( find, j)) != T::npos;)
	{
		source->replace( j, find.length(), replace );
	}
}

void tvMyShares_target_drag_data_received(GtkWidget          *widget,
																				GdkDragContext     *context,
																				gint                x,
																				gint                y,
																				GtkSelectionData   *data,
																				guint               info,
																				guint               time){
	fprintf(stdout,"tvMyShares_target_drag_data_received:\nDATA=%i:%s\n",info,data->data);
	char *i = (char*)data->data;
	while(*i!=0) {
		char *j = i;
		while ((*i != 0) && (*i != '\r') && (*i != '\n')) ++i;
		if (strncmp(j, "file:///", 8) == 0) {
			string path(j+7+URI_EXTRA_MUNCH, i);
			findandreplace(&path, string("%20"), string(" "));
			/* TODO: other replaces besides (%20 -> ' ') ? */
			ShareInfo* share= new ShareInfo(path);
			myServer->add_share(share);
			add_tree_data(sharetreeview, share);
		}
		while ((*i == '\r') || (*i == '\n')) ++i;
	}

	g_signal_stop_emission_by_name(widget,"drag_data_received"); // Don't know if this is correct, but it makes GTK STFU
}

void btnUpdateServerList_clicked(GtkWidget *widget, gpointer user_data) {
  spawnThread(gatherServers, (void*)NULL);
}

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

void btnSetPort_clicked(GtkWidget *widget, gpointer user_data) {
	int p = atoi(gtk_entry_get_text((GtkEntry*) glade_xml_get_widget (main_window, "txtSetPort")));
	UsePort(p);
}

void btnSetInterface_clicked(GtkWidget *widget, gpointer user_data) {
	int i = atoi(gtk_entry_get_text((GtkEntry*) glade_xml_get_widget (main_window, "txtInterface")));
	UseInterface(i);
}

void radioProtocol_clicked(GtkWidget *widget, gpointer user_data) {
	//NOTE: 3vil hax and only verks with 2 toggles :)
	if(!gtk_toggle_button_get_active ((GtkToggleButton*)widget)) {
		UseUDP(!udp_hax);
		fprintf(stderr, "SWITCH: udp_hax=%s\n",udp_hax?"true":"false");
	}
}

void SpamSpamCallback(uint32 count) {
	static GtkWidget *widget = NULL;
	if(widget==NULL) {
		widget = glade_xml_get_widget (main_window, "txtSpamSpammed");
	}
	if(widget!=NULL) {
		char str[128];
		snprintf(str, 127, "%lu",count);
		gtk_entry_set_text((GtkEntry*)widget, str);
		gtk_widget_queue_draw(widget);  // force a redraw of the widget
	}
}

THREAD SpamSpamThread;
void btnSpamSpam_clicked(GtkWidget *widget, gpointer user_data) {
	extern bool SpamMoreSpam;
	if(!SpamMoreSpam) {
		SpamSpamArgs *Args = new SpamSpamArgs;
		Args->s = "SpamSpam";
		Args->p = SpamSpamCallback;
		SpamSpamThread = spawnThread(SpamSpam, Args);
		gtk_button_set_label((GtkButton*)widget, "Stop Spammming!");
	}
	else {
		//Stop Spamming
		SpamMoreSpam=false;
		gtk_button_set_label((GtkButton*)widget, "Spam Spam!");
	}
}

void ReceiveSpamCallback(uint32 count) {
	static GtkWidget *widget = NULL;
	if(widget==NULL) {
		widget = glade_xml_get_widget (main_window, "txtSpamReceived");
	}
	if(widget!=NULL) {
		char str[128];
		snprintf(str, 127, "%lu",count);
		gtk_entry_set_text((GtkEntry*)widget, str);
		gtk_widget_queue_draw(widget);  // force a redraw of the widget
	}
}

THREAD ReceiveSpamThread;
void btnReceiveSpam_clicked(GtkWidget *widget, gpointer user_data) {
	extern bool ReceiveMoreSpam;
	if(!ReceiveMoreSpam) {
		SpamSpamArgs *Args = new SpamSpamArgs;
		Args->s = ""; //not used
		Args->p = ReceiveSpamCallback;
		//ReceiveSpamThread = spawnThread((int* (WINAPI*)(x*))ReceiveSpam, (x*)Args);
		gtk_button_set_label((GtkButton*)widget, "Stop Receiving!");
	}
	else {
		//Stop Spamming
		ReceiveMoreSpam=false;
		gtk_button_set_label((GtkButton*)widget, "Receive Spam!");
	}
}

uint32 show_gooey() {
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

	widget = glade_xml_get_widget (main_window, "btnSpamSpam");
	if(widget==NULL) {
		fprintf(stderr,"Error: can not find widget `btnSpamSpam'!\n");
		return -1;
	}
	else {
		g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnSpamSpam_clicked), NULL);
	}

	widget = glade_xml_get_widget (main_window, "btnReceiveSpam");
	if(widget==NULL) {
		fprintf(stderr,"Error: can not find widget `btnReceiveSpam'!\n");
		return -1;
	}
	else {
		g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnReceiveSpam_clicked), NULL);
	}

	widget = glade_xml_get_widget (main_window, "btnUpdateServerList");
	if(widget==NULL) {
		fprintf(stderr,"Error: can not find widget `btnUpdateServerList'!\n");
		return -1;
	}
	else {
		g_signal_connect (G_OBJECT (widget), "pressed", G_CALLBACK (btnUpdateServerList_clicked), NULL);
	}

	//Set DnD targets
	widget = glade_xml_get_widget (main_window, "tvMyShares");
	if(widget==NULL) {
		fprintf(stderr,"Error: can not find widget `tvMyShares'!\n");
		return -1;
	}
	else {
		static const GtkTargetEntry target_table[] = { {} };
		gtk_drag_dest_set(widget, \
											GTK_DEST_DEFAULT_ALL,\
											target_table, \
											0, \
											GDK_ACTION_MOVE //FIXME: possibly (KDE vs Gnome): (GdkDragAction)(GDK_ACTION_COPY | GDK_ACTION_MOVE)
											);
		gtk_drag_dest_add_uri_targets(widget);
		gtk_signal_connect (GTK_OBJECT (widget),
												"drag_data_received",
												GTK_SIGNAL_FUNC (tvMyShares_target_drag_data_received),
												NULL);
		init_tree_view( (GtkTreeView*)widget);
		sharetreeview = (GtkTreeView*)widget;
	}

	/* Have the delete event (window close) end the program */
	widget = glade_xml_get_widget (main_window, "frmMain");
	g_signal_connect (G_OBJECT (widget), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

// DEBUG
			ShareInfo* share= new ShareInfo("/home/dafox/Desktop");
			myServer->add_share(share);
			add_tree_data(sharetreeview, share);



	/* start the event loop. TODO: Do this in the background? */
	//spawnThread(gtk_main(), NULL); ?
	gtk_main ();
	return true;
}
