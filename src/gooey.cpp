/**------------------------------------------------------------------------**
 * NOTE:
 * It is my intention to only have the bindings/connections in this file,
 * and call the appropriate routines in other files.
 **------------------------------------------------------------------------**/

#include "stdhdr.h"
#include "types.h"
#include "gooey.h"

void btnQuit_clicked(GtkWidget *widget, gpointer user_data) {
  fprintf (stdout, "Blaat.\n");
  gtk_main_quit ();
  }

uint32 show_gooey() {
  GladeXML  *main_window;
  GtkWidget *widget;

  //FIXME: Find a better way to store the .glade file
  main_window = glade_xml_new ("src/uftt.glade", NULL, NULL); /* load the interface * /

  /* connect the signals in the interface */
  /* Have the ok button call the ok_button_clicked callback */
  widget = glade_xml_get_widget (main_window, "btnQuit");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (btnQuit_clicked), NULL);

  /* Have the delete event (window close) end the program */
  widget = glade_xml_get_widget (main_window, "frmMain");
  g_signal_connect (G_OBJECT (widget), "delete_event", G_CALLBACK (gtk_main_quit), NULL);

  /* start the event loop */
  gtk_main ();
  return 0;
  }
