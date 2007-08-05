#include "FileInfo.h"

#include <iostream>

using namespace std;

FileInfo::FileInfo(const fs::path& path)
{
	assert(fs::exists(path));
	name = path.leaf();
	attrs = 0;
	if (fs::is_directory(path)) {
		attrs |= FATTR_DIR;
		size = 0;
		fs::directory_iterator end_iter; // default construction yields past-the-end
		for ( fs::directory_iterator iter( path );
		      iter != end_iter;
		      ++iter ) {
			shared_ptr<FileInfo> child(new FileInfo(iter->path()));
			files.push_back(child);
			size += child->size;
		}
	} else {
		size = fs::file_size(path);
	}
	cout << path << '\n';
}
