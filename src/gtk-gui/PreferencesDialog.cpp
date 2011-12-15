#include "PreferencesDialog.h"
#include <boost/function.hpp>
#include <gtkmm/stock.h>
#include <gtkmm/frame.h>
#include <iostream>
#include <pangomm/attrlist.h>

using namespace std;

UFTTPreferencesDialog::UFTTPreferencesDialog(UFTTSettingsRef _settings)
: settings(_settings),
  // Characters used for short-cuts: a b c i l m n o p r s t u v
  enable_global_peer_discovery_checkbutton("Announce shares over the _internet", true),
  enable_download_resume_checkbutton("Resume _partial downloads", true),
  enable_tray_icon_checkbutton("Enable system _tray icon", true),
  minimize_on_close_checkbutton("_Minimize to tray on close", true),
  start_in_tray_checkbutton("_Start in tray", true),
  enable_auto_clear_tasks_checkbutton(string() + "Automatically _remove completed tasks after (hh:mm:ss)", true),
  auto_clear_tasks_spinbutton_adjustment(abs(settings->auto_clear_tasks_after.get().total_seconds()), 0.0, 24*60*60*1.0, 1.0),
  auto_clear_tasks_spinbutton(auto_clear_tasks_spinbutton_adjustment, 1.0, 0),
  enable_notification_on_completion_checkbutton       ("Pop_up notification upon completion of a download", true),
  enable_blink_statusicon_on_completion_checkbutton   ("_Blink the tray icon upon completion of a download", true),
  enable_show_speeds_in_titlebar_checkbutton          ("Show cumulative transfer speeds in the tit_lebar", true),
  enable_show_speeds_in_statusicon_tooltip_checkbutton("Show cumulati_ve transfer speeds in the tooltip of the tray icon", true)
{
	set_modal(true);
	set_title("Preferences");
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
	auto_clear_tasks_spinbutton.set_numeric(false);
	auto_clear_tasks_spinbutton.set_width_chars(7);
	auto_clear_tasks_spinbutton.set_alignment(Gtk::ALIGN_RIGHT);
	option = Gtk::manage(new Gtk::HBox());
	option->set_spacing(12);
	option->add(enable_auto_clear_tasks_checkbutton);
	option->add(auto_clear_tasks_spinbutton);
	options->add(*option);

	category = Gtk::manage(new Gtk::VBox());
	categories->add(*category);
	label = Gtk::manage(new Gtk::Label("Notification", 0.0f, 0.5f));
	label->set_attributes(attr_list);
	category->add(*label);
	alignment = Gtk::manage(new Gtk::Alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_TOP));
	alignment->set_padding(0,0,12,0);
	category->add(*alignment);
	options = Gtk::manage(new Gtk::VBox());
	alignment->add(*options);
	options->add(enable_notification_on_completion_checkbutton);
	options->add(enable_show_speeds_in_titlebar_checkbutton);
	options->add(enable_blink_statusicon_on_completion_checkbutton);
	options->add(enable_show_speeds_in_statusicon_tooltip_checkbutton);

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
	CONNECT_SIGNAL_HANDLER(enable_auto_clear_tasks_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(auto_clear_tasks_spinbutton, changed);
	CONNECT_SIGNAL_HANDLER(enable_notification_on_completion_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(enable_show_speeds_in_titlebar_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(enable_blink_statusicon_on_completion_checkbutton, toggled);
	CONNECT_SIGNAL_HANDLER(enable_show_speeds_in_statusicon_tooltip_checkbutton, toggled);
	#undef CONNECT_SIGNAL_HANDLER
	// These signals have IO, so CONNECT_SIGNAL_HANDLER won't work :(
	sigc::slot<int, double*> slot1 = sigc::mem_fun(*this, &UFTTPreferencesDialog::on_auto_clear_tasks_spinbutton_input);
	sigc::slot<bool>         slot2 = sigc::mem_fun(*this, &UFTTPreferencesDialog::on_auto_clear_tasks_spinbutton_output);
	auto_clear_tasks_spinbutton.signal_input().connect(slot1);
	auto_clear_tasks_spinbutton.signal_output().connect(slot2);

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

/**
 * Convert the Entry text to a number.
 * The computed number should be written to *new_value.
 * Returns:
 *         * false: No conversion done, continue with default handler.
 *         * true: Conversion successful, don't call default handler.
 *         * Gtk::INPUT_ERROR: Conversion failed, don't call default handler.
 */
int UFTTPreferencesDialog::on_auto_clear_tasks_spinbutton_input(double* output) {
	(*output) = auto_clear_tasks_spinbutton.get_adjustment()->get_value();
	try {
		settings->auto_clear_tasks_after = boost::posix_time::duration_from_string(auto_clear_tasks_spinbutton.get_text());
		(*output) = settings->auto_clear_tasks_after.get().total_seconds();
	} catch(boost::bad_lexical_cast /*e*/) {};
	return true;
}

/**
 * Convert the Adjustment  position to text.
 * The computed text should be written via Gtk::Entry::set_text().
 * Returns:
 *         * false: No conversion done, continue with default handler.
 *         * true: Conversion successful, don't call default handler.
 */
bool UFTTPreferencesDialog::on_auto_clear_tasks_spinbutton_output() {
	auto_clear_tasks_spinbutton.set_text(boost::posix_time::to_simple_string(boost::posix_time::seconds(auto_clear_tasks_spinbutton.get_adjustment()->get_value())));
	return true;
}

void UFTTPreferencesDialog::on_auto_clear_tasks_spinbutton_changed() {
	// Only used to enable the apply button
}

void UFTTPreferencesDialog::on_enable_auto_clear_tasks_checkbutton_toggled() {
	if(settings->auto_clear_tasks_after == -settings->auto_clear_tasks_after) {
		// If the user specified 0 seconds (immediate mode) we can't take
		// the inverse, so add minute amount so that it is still effectively
		// immediate (total_seconds() == 0), but we can still distinguish
		// enabled from disabled.
		settings->auto_clear_tasks_after += boost::posix_time::microseconds(1);
	}
	settings->auto_clear_tasks_after = -settings->auto_clear_tasks_after;
	auto_clear_tasks_spinbutton.set_sensitive(settings->auto_clear_tasks_after >= boost::posix_time::seconds(0));
}

void UFTTPreferencesDialog::on_username_entry_changed() {
	settings->nickname = username_entry.get_text();
}

void UFTTPreferencesDialog::on_enable_tray_icon_checkbutton_toggled() {
	settings->show_task_tray_icon = enable_tray_icon_checkbutton.get_active();
	minimize_on_close_checkbutton.set_sensitive(settings->show_task_tray_icon);
	start_in_tray_checkbutton.set_sensitive(settings->show_task_tray_icon);
	enable_blink_statusicon_on_completion_checkbutton.set_sensitive(settings->show_task_tray_icon);
	enable_show_speeds_in_statusicon_tooltip_checkbutton.set_sensitive(settings->show_task_tray_icon);
}

void UFTTPreferencesDialog::on_minimize_on_close_checkbutton_toggled() {
	settings->close_to_tray = minimize_on_close_checkbutton.get_active();;
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

void UFTTPreferencesDialog::on_enable_notification_on_completion_checkbutton_toggled() {
	settings->notification_on_completion = enable_notification_on_completion_checkbutton.get_active();
}

void UFTTPreferencesDialog::on_enable_show_speeds_in_titlebar_checkbutton_toggled() {
	settings->show_speeds_in_titlebar = enable_show_speeds_in_titlebar_checkbutton.get_active();
}

void UFTTPreferencesDialog::on_enable_blink_statusicon_on_completion_checkbutton_toggled() {
	settings->blink_statusicon_on_completion = enable_blink_statusicon_on_completion_checkbutton.get_active();
}

void UFTTPreferencesDialog::on_enable_show_speeds_in_statusicon_tooltip_checkbutton_toggled() {
	settings->show_speeds_in_statusicon_tooltip = enable_show_speeds_in_statusicon_tooltip_checkbutton.get_active();
}


void UFTTPreferencesDialog::apply_settings() {
	username_entry.set_text(settings->nickname.get());
	enable_tray_icon_checkbutton.set_active(settings->show_task_tray_icon);
	minimize_on_close_checkbutton.set_active(settings->close_to_tray);
	minimize_on_close_checkbutton.set_sensitive(settings->show_task_tray_icon);
	start_in_tray_checkbutton.set_active(settings->start_in_tray);
	start_in_tray_checkbutton.set_sensitive(settings->show_task_tray_icon);
	enable_download_resume_checkbutton.set_active(settings->experimentalresume);
	enable_global_peer_discovery_checkbutton.set_active(settings->enablepeerfinder);
	auto_clear_tasks_spinbutton.set_adjustment(auto_clear_tasks_spinbutton_adjustment);
	auto_clear_tasks_spinbutton_adjustment.set_value(abs(settings->auto_clear_tasks_after.get().total_seconds()));
	auto_clear_tasks_spinbutton.set_sensitive(settings->auto_clear_tasks_after>=boost::posix_time::seconds(0));
	enable_auto_clear_tasks_checkbutton.set_active(auto_clear_tasks_spinbutton.is_sensitive());
	on_auto_clear_tasks_spinbutton_output(); // prevent signal_changed firing on_show()
	enable_notification_on_completion_checkbutton.set_active(settings->notification_on_completion);
	enable_show_speeds_in_titlebar_checkbutton.set_active(settings->show_speeds_in_titlebar);
	enable_blink_statusicon_on_completion_checkbutton.set_active(settings->blink_statusicon_on_completion);
	enable_blink_statusicon_on_completion_checkbutton.set_sensitive(settings->show_task_tray_icon);
	enable_show_speeds_in_statusicon_tooltip_checkbutton.set_active(settings->show_speeds_in_statusicon_tooltip);
	enable_show_speeds_in_statusicon_tooltip_checkbutton.set_sensitive(settings->show_task_tray_icon);

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
