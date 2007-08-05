#include "NetworkThread.h"

#include <iostream>

using namespace std;

void NetworkThread::operator()()
{
	while (true) {
		cerr << '.' << endl;
		usleep(200000);
	}
}
