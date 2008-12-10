#ifndef TESTMODULE_H
#define TESTMODULE_H

#include "INetModule.h"

class TestModule: public INetModule {
	public:
		TestModule(UFTTCore* core);
};

#endif//TESTMODULE_H
