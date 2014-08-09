#include "ScopeExitSignal.h"

ScopeExitSignal::ScopeExitSignal()
{
}

ScopeExitSignal::~ScopeExitSignal()
{
	sig();
}

void ScopeExitSignal::add(const SigType::slot_type &f)
{
	sig.connect(f, boost::signals2::at_front);
}
