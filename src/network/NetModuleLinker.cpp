#include "NetModuleLinker.h"

#include "../Types.h"

#define LINK_NETMODULE_CLASS(cls) namespace NetModuleLinker { \
	namespace ns_cls_ ## cls { \
		extern int x; \
		int y = x; \
	} \
}

#include "NetModuleLinker.inc"

#undef LINK_NETMODULE_CLASS

namespace {
	// NOTE: Regular pointer is required here, static initialization will overwrite values stored
	//       in shared_ptr before it's constructor gets executed...
	static std::vector<boost::function<std::shared_ptr<INetModule>(UFTTCore*)> >* netmodulelist = NULL;
}

namespace NetModuleLinker {
	std::vector<std::shared_ptr<INetModule> > getNetModuleList(UFTTCore* core)
	{
		std::vector<std::shared_ptr<INetModule> > res;

		for (uint i = 0; i < netmodulelist->size(); ++i) {
			try {
				std::shared_ptr<INetModule> nm;
				nm = (*netmodulelist)[i](core);
				res.push_back(nm);
			} catch (std::exception& /*e*/) {
				//std::cout << "E: " << e.what << '\n';
			}
		}

		return res;
	}

	void regNetModule(const boost::function<std::shared_ptr<INetModule>(UFTTCore*)>& fp) {
		if (netmodulelist == NULL) { // FIXME: leak leak...
			netmodulelist = new std::vector<boost::function<std::shared_ptr<INetModule>(UFTTCore*)> >();
		}
		netmodulelist->push_back(fp);
	};
}
