#include "../Types.h"

#include <string>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

enum FileAttrs {
	FATTR_DIR = 1 << 0,
};

struct FileInfo {
	std::string name;
	uint32 attrs;
	uint64 size;
	std::vector<shared_ptr<FileInfo> > file;
	uint64 UID;
	FileInfo(const std::string& path);
};

struct ShareInfo {
	std::string name;
	shared_ptr<FileInfo> root;
	uint64 UID;
	ShareInfo(const std::string& path);
};
