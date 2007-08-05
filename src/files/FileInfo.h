#ifndef FILE_INFO_H
#define FILE_INFO_H

#include "../Types.h"
#include "../sha1/SHA1.h"

#include <string>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

enum FileAttrs {
	FATTR_DIR = 1 << 0,
};

class FileInfo {
	public:
		std::string name;
		uint32 attrs;
		uint64 size;
		std::vector<shared_ptr<FileInfo> > files;
		SHA1 hash;
		FileInfo(const fs::path& path);
		FileInfo();
};
typedef shared_ptr<FileInfo> FileInfoRef;

struct ShareInfo {
	shared_ptr<FileInfo> root;
	ShareInfo(shared_ptr<FileInfo> fi);
};

#endif
