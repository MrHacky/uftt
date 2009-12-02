#include "PreferencesDialog.h"
#include <boost/function.hpp>
#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <iostream>
#include <pangomm/attrlist.h>

using namespace std;

UFTTPreferencesDialog::UFTTPreferencesDialog(UFTTSettingsRef _settings) 
: settings(_settings),
  enable_global_peer_discovery_checkbutton("_Announce shares over the internet", true),
  enable_download_resume_checkbutton("_Resume partial downloads", true),
  enable_tray_icon_checkbutton("Enable system _tray icon", true),
  minimize_on_close_checkbutton("_Minimize to tray on close", true),
  start_in_tray_checkbutton("_Start in tray", true)
{
	set_modal(true);
	set_title("Preferences");
	set_default_size(320, 384);
	set_position(Gtk::WIN_POS_CENTER_ON_PARENT);

	Pango::FontDescription bold_font_description;
	bold_font_description.set_weight(Pango::WEIGHT_BOLD);
	Pango::AttrFontDesc afd(Pango::Attribute::create_attr_font_desc(bold_font_description));
	afd.set_desc(bold_font_description);
	Pango::AttrList attr_list;
	attr_list.insert(afd);
	
	Gtk::Label*     label;
	Gtk::Alignment* alignment;
	Gtk::VBox*      categories;
	Gtk::VBox*      category;
	Gtk::VBox*      options;
	Gtk::HBox*      option;

	categories = Gtk::manage(new Gtk::VBox());
	categories->set_spacing(12);
	category = Gtk::manage(new Gtk::VBox());
	categories->add(*category);
	label = Gtk::manage(new Gtk::Label("Sharing", 0.0f, 0.5f));
	label->set_attributes(attr_list);
	category->add(*label);
	alignment = Gtk::manage(new Gtk::Alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_TOP));
	alignment->set_padding(0,0,12,0);
	category->add(*alignment);
	options = Gtk::manage(new Gtk::VBox());
	alignment->add(*options);
	options->add(enable_global_peer_discovery_checkbutton);
	option = Gtk::manage(new Gtk::HBox());
	label = Gtk::manage(new Gtk::Label("User_name:", 0.0f, 0.5f, true));
	label->set_mnemonic_widget(username_entry);
	option->add(*label);
	option->add(username_entry);
	options->add(*option);

	category = Gtk::manage(new Gtk::VBox());
	categories->add(*category);
	label = Gtk::manage(new Gtk::Label("Downloading", 0.0f, 0.5f));
	label->set_attributes(attr_list);
	category->add(*label);
	alignment = Gtk::manage(new Gtk::Alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_TOP));
	alignment->set_padding(0,0,12,0);
	category->add(*alignment);
	options = Gtk::manage(new Gtk::VBox());
	alignment->add(*options);
	options->add(enable_download_resume_checkbutton);

	category = Gtk::manage(new Gtk::VBox());
	categories->add(*category);
	label = Gtk::manage(new Gtk::Label("System Tray", 0.0f, 0.5f));
	label->set_attributes(attr_list);
	category->add(*label);
	alignment = Gtk::manage(new Gtk::Alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_TOP));
	alignment->set_padding(0,0,12,0);
	category->add(*alignment);
	options = Gtk::manage(new Gtk::VBox());
	alignment->add(*options);
	options->add(enable_tray_icon_checkbutton);
	alignment = Gtk::manage(new Gtk::Alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_TOP));
	alignment->set_padding(0,0,12,0);
	options->add(*alignment);
	options = Gtk::manage(new Gtk::VBox());
	alignment->add(*options);
	options->add(minimize_on_close_checkbutton);
	options->add(start_in_tray_checkbutton);

	alignment = Gtk::manage(new Gtk::Alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_TOP));
	alignment->set_padding(6,6,6,6);
	alignment->add(*categories);
	get_vbox()->pack_start(*alignment, Gtk::PACK_SHRINK);
	get_vbox()->show_all();

	Gtk::Button* button;
	button = add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	button->signal_clicked().connect(boost::bind(&UFTTPreferencesDialog::on_control_button_clicked, this, Gtk::RESPONSE_CANCEL));
	button = add_button(Gtk::Stock::APPLY, Gtk::RESPONSE_APPLY);
	button->signal_clicked().connect(boost::bind(&UFTTPreferencesDialog::on_control_button_clicked, this, Gtk::RESPONSE_APPLY));
	button = add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
	button->signal_clicked().connect(boost::bind(&UFTTPreferencesDialog::on_control_button_clicked, this, Gtk::RESPONSE_OK));

	// NOTE: Always apply settings *before* connecting the signals.
	//       (See documentation on toggled signal)
	apply_settings();
	
	#define CONNECT_SIGNAL_HANDLER(widget, signal) \
		widget.signal_ ## signal().connect( \
			boost::bind(&UFTTPreferencesDialog::with_enable_apply_button_do, this, \
				(boost::function<void(void)>)boost::bind(&UFTTPreferencesDialog::on_ ##  widget ## _ ## signal, this)));
	CONNECT_SIGNAL_HANDLER(enable_tray_icon_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(minimize_on_close_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(start_in_tray_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(enable_auto_update_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(enable_download_resume_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(enable_global_peer_discovery_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(username_entry, changed);
	#undef CONNECT_SIGNAL_HANDLER

	/* TODO: (Requires more support from the core):
	 * 	bool autoupdate;
	 * 	int webupdateinterval; // 0:never, 1:daily, 2:weekly, 3:monthly
	 * 	bool traydoubleclick; --> Only if possible in a sane way
	*/
}

void UFTTPreferencesDialog::with_enable_apply_button_do(boost::function<void(void)> f) {
	set_response_sensitive(Gtk::RESPONSE_APPLY, true);
	f();
}

void UFTTPreferencesDialog::on_username_entry_changed() {
	// FIXME: Username in sharelist does not (really) update in real-time
	//        probably requires support from the Core and Settings:
	//          settings.set_username ->
	//          sig_username_changed in core ->
	//          sig_remove_share & sig_add_share in GUI (ShareList).
	settings->nickname = username_entry.get_text();
	signal_settings_changed(); // A bit hacky, see note above
}

void UFTTPreferencesDialog::on_enable_tray_icon_checkbutton_toggled() {
	settings->show_task_tray_icon = enable_tray_icon_checkbutton.get_active();
	minimize_on_close_checkbutton.set_sensitive(settings->show_task_tray_icon);
	start_in_tray_checkbutton.set_sensitive(settings->show_task_tray_icon);
}

void UFTTPreferencesDialog::on_minimize_on_close_checkbutton_toggled() {
	// FIXME: We don't handle mode 2
	settings->minimize_to_tray_mode = minimize_on_close_checkbutton.get_active() ? 1 : 0;
}

void UFTTPreferencesDialog::on_start_in_tray_checkbutton_toggled() {
	settings->start_in_tray = start_in_tray_checkbutton.get_active();
}

void UFTTPreferencesDialog::on_enable_auto_update_checkbutton_toggled() {}

void UFTTPreferencesDialog::on_enable_download_resume_checkbutton_toggled() {
	settings->experimentalresume = enable_download_resume_checkbutton.get_active();
}

void UFTTPreferencesDialog::on_enable_global_peer_discovery_checkbutton_toggled() {
	settings->enablepeerfinder = enable_global_peer_discovery_checkbutton.get_active();
}

void UFTTPreferencesDialog::apply_settings() {
	username_entry.set_text(settings->nickname);
	enable_tray_icon_checkbutton.set_active(settings->show_task_tray_icon);
	minimize_on_close_checkbutton.set_active(settings->minimize_to_tray_mode != 0);
	minimize_on_close_checkbutton.set_inconsistent(settings->minimize_to_tray_mode > 1);
	minimize_on_close_checkbutton.set_sensitive(settings->show_task_tray_icon);
	start_in_tray_checkbutton.set_active(settings->start_in_tray);
	start_in_tray_checkbutton.set_sensitive(settings->show_task_tray_icon);
	enable_download_resume_checkbutton.set_active(settings->experimentalresume);
	enable_global_peer_discovery_checkbutton.set_active(settings->enablepeerfinder);

	// Do this last, since we're firing signals with the above set_active's
	set_response_sensitive(Gtk::RESPONSE_APPLY, false);
}

void UFTTPreferencesDialog::on_control_button_clicked(Gtk::ResponseType response) {
	switch(response) {
		case Gtk::RESPONSE_CANCEL: {
			hide();
			*settings = previous_settings;
		}; break;
		case Gtk::RESPONSE_OK: {
			hide();
		}; /* break; */ // Fallthrough intentional
		case Gtk::RESPONSE_APPLY: {
			set_response_sensitive(Gtk::RESPONSE_APPLY, false);
			signal_settings_changed();
			previous_settings = *settings;
		}; break;
	}
	apply_settings();
}

bool UFTTPreferencesDialog::on_delete_event(GdkEventAny* event) {
	on_control_button_clicked(Gtk::RESPONSE_CANCEL);
	return true;
}

void UFTTPreferencesDialog::on_show() {
	previous_settings = *settings;
	Gtk::Dialog::on_show();
}
