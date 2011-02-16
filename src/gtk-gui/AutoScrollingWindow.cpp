/*
    BFilter - a smart ad-filtering web proxy
    Copyright (C) 2002-2007  Joseph Artsimovich <joseph_a@mail.ru>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "AutoScrollingWindow.h"
#include <gtkmm/adjustment.h>
#include <math.h>

namespace Gtk
{

AutoScrollingWindow::AutoScrollingWindow()
:	m_savedState(*get_vadjustment()),
	m_isAtBottom(true),
	m_wasAtBottom(true)
{
	get_vadjustment()->signal_changed().connect(
		sigc::mem_fun(*this, &AutoScrollingWindow::onVAdjustmentChanged)
	);
	get_vadjustment()->signal_value_changed().connect(
		sigc::mem_fun(*this, &AutoScrollingWindow::onVAdjustmentValueChanged)
	);
}

AutoScrollingWindow::~AutoScrollingWindow()
{
}

void
AutoScrollingWindow::onVAdjustmentValueChanged()
{
	Gtk::Adjustment& adj = *get_vadjustment();
	m_wasAtBottom = m_isAtBottom;
	m_isAtBottom = isAtBottom(adj);

	if (m_wasAtBottom && !m_isAtBottom) {
		/*
		Unfortunately, it doesn't mean the user has moved the scrollbar.
		It can also happen when the scrolled surface reduces its height.
		There is no good way to make a distinction between these two
		situations, but I made the following observation:
		If it's not the result of user's action, we are going to
		immediately get the 'changed' signal without any actual
		changes between now and then.
		*/
		m_savedState = adj;
	}
}

void
AutoScrollingWindow::onVAdjustmentChanged()
{
	Gtk::Adjustment& adj = *get_vadjustment();

	if (m_wasAtBottom && !m_isAtBottom) {
		if (m_savedState == adj) {
			// See the above comments.
			m_isAtBottom = true;
		}
	}

	if (m_isAtBottom) {
		adj.set_value(adj.get_upper() - adj.get_page_size());
		adj.value_changed();
	}
}

bool
AutoScrollingWindow::isAtBottom(Gtk::Adjustment const& adj)
{
	double const diff = adj.get_upper() - adj.get_value() - adj.get_page_size();
	return (fabs(diff) < 1.0 || diff < adj.get_step_increment());
}


/*==================== AutoScrollingWindow::SavedState ====================*/

void
AutoScrollingWindow::SavedState::operator=(Gtk::Adjustment const& adj)
{
	m_lower = adj.get_lower();
	m_upper = adj.get_upper();
	m_pageSize = adj.get_page_size();
}

bool
AutoScrollingWindow::SavedState::operator==(Gtk::Adjustment const& adj) const
{
	return (m_lower == adj.get_lower() && m_upper == adj.get_upper()
		&& m_pageSize == adj.get_page_size());
}

} // namespace Gtk
