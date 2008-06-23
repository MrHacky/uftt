#ifndef QT_MAIN_H
#define QT_MAIN_H

//#include "../network/NetworkThread.h"
#include <boost/signal.hpp>
#include <boost/filesystem.hpp>

class QTMain {
	private:
		// implementation class (PIMPL idiom)
		class QTImpl* impl;

	public:
		QTMain( int argc, char **argv );
		~QTMain();
		
		template<typename T>
		void BindEvents(T* t)
		{
			t->sig_new_share.connect(sig_new_share);
			t->sig_new_autoupdate.connect(sig_new_autoupdate);
			t->sig_download_ready.connect(sig_download_ready);

			slot_refresh_shares.connect(boost::bind(&T::slot_refresh_shares, t));
			slot_add_local_share.connect(boost::bind(&T::slot_add_local_share, t, _1, _2));
			slot_download_share.connect(boost::bind(&T::slot_download_share, t, _1, _2));
			// do something smart...
		}

		boost::signal<void(std::string)> sig_new_share;
		boost::signal<void(std::string)> sig_new_autoupdate;
		boost::signal<void(std::string)> sig_download_ready;

		boost::signal<void()> slot_refresh_shares;
		boost::signal<void(std::string,boost::filesystem::path)> slot_add_local_share;
		boost::signal<void(std::string,boost::filesystem::path)> slot_download_share;
		
		int run();
};

//int ShowQTGui( int argc, char **argv );

#endif
