#include "GTKImpl.h"
#include <glib/gthread.h>
#include <ios>

using namespace std;

UFTTWindow::UFTTWindow(UFTTCore& _core, UFTTSettings& _settings) 
: core(_core),
  settings(_settings)
{

}
