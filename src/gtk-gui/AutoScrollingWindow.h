/*
    NOTE:
    This file was taken from BFilter (original copyright notice follows) and
    *slightly* modified to better fit in UFTT.

    Orignal copyright notice:
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

#ifndef AUTOSCROLLINGWINDOW_H_
#define AUTOSCROLLINGWINDOW_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtkmm/scrolledwindow.h>

namespace Gtk
{
	class Adjustment;
}

namespace Gtk
{

class AutoScrollingWindow : public Gtk::ScrolledWindow
{
public:
	AutoScrollingWindow();
	
	virtual ~AutoScrollingWindow();
private:
	class SavedState
	{
	public:
		SavedState(Gtk::Adjustment const& adj) { *this = adj; }
		
		void operator=(Gtk::Adjustment const& adj);
		
		bool operator==(Gtk::Adjustment const& adj) const;
	private:
		double m_lower;
		double m_upper;
		double m_pageSize;
	};
	
	void onVAdjustmentChanged();
	
	void onVAdjustmentValueChanged();
	
	static bool isAtBottom(Gtk::Adjustment const& adj);
	
	SavedState m_savedState;
	bool m_isAtBottom;
	bool m_wasAtBottom;
};

} // namespace Gtk

#endif
