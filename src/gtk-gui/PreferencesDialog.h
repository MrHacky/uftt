#ifndef PREFERENCES_DIALOG_H
	#define PREFERENCES_DIALOG_H
	#include "../UFTTCore.h"
	#include "../UFTTSettings.h"
	#include <boost/bind.hpp>
	#include <boost/signal.hpp>
	#include <gtkmm/alignment.h>
	#include <gtkmm/box.h>
	#include <gtkmm/button.h>
	#include <gtkmm/checkbutton.h>
	#include <gtkmm/dialog.h>
	#include <gtkmm/entry.h>
	#include <gtkmm/spinbutton.h>
	#include <gtkmm/label.h>
	#include <gtkmm/textview.h>

	class UFTTPreferencesDialog : public Gtk::Dialog {
		public:
			UFTTPreferencesDialog(UFTTSettingsRef _settings);
			boost::signal<void(void)> signal_settings_changed;
		protected:
			virtual void on_show();
			virtual bool on_delete_event(GdkEventAny* event);
		private:
			UFTTSettingsRef  settings;
			UFTTSettings     previous_settings;
			Gtk::Entry       username_entry;
			Gtk::CheckButton enable_tray_icon_checkbutton;
			Gtk::CheckButton minimize_on_close_checkbutton;
			Gtk::CheckButton start_in_tray_checkbutton;
			Gtk::CheckButton enable_auto_update_checkbutton;
			Gtk::CheckButton enable_download_resume_checkbutton;
			Gtk::CheckButton enable_global_peer_discovery_checkbutton;
			Gtk::CheckButton enable_auto_clear_tasks_checkbutton;
			Gtk::Adjustment  auto_clear_tasks_spinbutton_adjustment; // note: used by auto_clear_tasks_spinbutton, so ensure it is constructed before auto_clear_tasks_spinbutton
			Gtk::SpinButton  auto_clear_tasks_spinbutton;
			void with_enable_apply_button_do(boost::function<void(void)> f);
			void on_username_entry_changed();
			void on_enable_tray_icon_checkbutton_toggled();
			void on_minimize_on_close_checkbutton_toggled();
			void on_start_in_tray_checkbutton_toggled();
			void on_enable_auto_update_checkbutton_toggled();
			void on_enable_download_resume_checkbutton_toggled();
			void on_enable_global_peer_discovery_checkbutton_toggled();
			void on_control_button_clicked(Gtk::ResponseType response);
			void on_enable_auto_clear_tasks_checkbutton_toggled();
			int  on_auto_clear_tasks_spinbutton_input(double* d);
			bool on_auto_clear_tasks_spinbutton_output();
			void on_auto_clear_tasks_spinbutton_changed();
			void apply_settings();
	};

#endif // PREFERENCES_DIALOG_H
