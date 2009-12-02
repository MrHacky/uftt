#ifndef UFTT_GUI_H
	#define UFTT_GUI_H

	#include <boost/shared_ptr.hpp>
	#include "UFTTCore.h"

	class UFTTCore;
	class UFTTSettings;
	class UFTTGui {
		public:
			static const boost::shared_ptr<UFTTGui> makeGui(int argc, char** argv, UFTTCore& core, UFTTSettings& settings);
			virtual void bindEvents(UFTTCore* t) = 0;
			virtual int run() = 0;
		private:
//			UFTTGui(const UFTTGui&); // Non-copyable?
	};

#endif // UFTT_GUI_H