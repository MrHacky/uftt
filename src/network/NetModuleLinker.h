#ifndef NETMODULE_LINKER_H
#define NETMODULE_LINKER_H

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

class INetModule;
class UFTTCore;

namespace NetModuleLinker {
	std::vector<boost::shared_ptr<INetModule> > getNetModuleList(UFTTCore* core);

	void regNetModule(const boost::function<boost::shared_ptr<INetModule>(UFTTCore*)>& fp);

	template <class T>
	struct create_netmodule {
		boost::shared_ptr<INetModule> operator()(UFTTCore* core)
		{
			return boost::shared_ptr<INetModule>(new T(core));
		}
	};

	template<class T>
	int reg_netmodule() {
		regNetModule(create_netmodule<T>());
		return 1;
	};
}

#define REGISTER_NETMODULE_CLASS(cls) namespace NetModuleLinker{ \
	namespace ns_cls_ ## cls { \
		int x = NetModuleLinker::reg_netmodule<cls>(); \
	} \
}

#define DISABLE_NETMODULE_CLASS(cls) namespace NetModuleLinker{ \
	namespace ns_cls_ ## cls { \
		int x = 1; \
	} \
}

#endif//NETMODULE_LINKER_H
