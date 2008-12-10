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
	std::vector<boost::function<boost::shared_ptr<INetModule>(UFTTCore*)> > netmodulelist;
}

namespace NetModuleLinker {
	std::vector<boost::shared_ptr<INetModule> > getNetModuleList(UFTTCore* core)
	{
		std::vector<boost::shared_ptr<INetModule> > res;

		for (uint i = 0; i < netmodulelist.size(); ++i) {
			try {
				boost::shared_ptr<INetModule> nm;
				nm = netmodulelist[i](core);
				res.push_back(nm);
			} catch (std::exception& /*e*/) {
				//std::cout << "E: " << e.what << '\n';
			}
		}

		return res;
	}

	void regNetModule(const boost::function<boost::shared_ptr<INetModule>(UFTTCore*)>& fp) {
		netmodulelist.push_back(fp);
	};
}
