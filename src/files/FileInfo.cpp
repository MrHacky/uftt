#include "FileInfo.h"

#include <iostream>

using namespace std;

FileInfo::FileInfo(const fs::path& path)
{
	assert(fs::exists(path));
	name = path.leaf();
	attrs = 0;
	SHA1Hasher hasher;
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
			// TODO: do this sorted!
			hasher.Update(child->name);
			hasher.Update(child->hash);
		}
	} else {
		hasher.Update(path);
		size = fs::file_size(path);
	}
	hasher.GetResult(hash); // store hash
	cout << path << '\n';
}

ShareInfo::ShareInfo(FileInfoRef fi)
	: root(fi)
{
}
