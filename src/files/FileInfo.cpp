#include "FileInfo.h"

#include <iostream>

using namespace std;

FileInfo::FileInfo(const fs::path& path)
{
	cout << path << '\n';
}
