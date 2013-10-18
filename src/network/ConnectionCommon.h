#ifndef CONNECTION_COMMON_H
#define CONNECTION_COMMON_H

#include "ConnectionBase.h"

#include "../util/Filesystem.h"

#include "Misc.h"

class ConnectionCommon: public ConnectionBase {
	public:
		ConnectionCommon(boost::asio::io_service& service_, UFTTCore* core_);

	protected:
		enum {
			QITEM_REQUEST_SHARE = 0,
			QITEM_REQUEST_FILE,
			QITEM_REQUEST_DIR,
			QITEM_UNUSED3,
			QITEM_UNUSED4,
			QITEM_REQUESTED_FILE,
			QITEM_REQUESTED_FILESIG,
			QITEM_REQUESTED_FILESIG_BUSY,
			QITEM_REQUESTED_PARTFILE,
			QITEM_UNUSED8, //QITEM_REQUESTED_PARTFILE_BUSY,
		};

		enum {
			REQLIST_UNUSED0 = 0,
			REQLIST_FILE,
			REQLIST_DIR,
			REQLIST_UNUSED3, // REQLIST_CDUP
			REQLIST_ENDOFLIST,
		};

		enum {
			CMD_NONE = 0,
			CMD_OLD_FILE,
			CMD_OLD_DIR,
			CMD_OLD_CDUP,
			CMD_OLD_DONE,
			CMD_REQUEST_SHARE_DUMP,
			CMD_REPLY_UNKNOWN_COMMAND,
			CMD_REQUEST_COMMAND_LIST,
			CMD_REPLY_COMMAND_LIST,
			CMD_REQUEST_TREE_LIST,
			CMD_REPLY_TREE_LIST,
			CMD_REQUEST_FULL_FILE,
			CMD_REPLY_FULL_FILE,
			CMD_DISCONNECT,
			DEPRECATED_CMD_REQUEST_SIG_FILE,
			DEPRECATED_CMD_REPLY_SIG_FILE,
			CMD_REQUEST_PARTIAL_FILE,
			CMD_REPLY_PARTIAL_FILE,
			CMD_REQUEST_SIG_FILE,
			CMD_REPLY_SIG_FILE,
			END_OF_COMMANDS
		};

		struct cmdinfo {
			uint32 cmd;
			uint32 ver;
			uint64 len;
		};

		struct filesender {
			std::string name;
			ext::filesystem::path path;
			ext::asio::fstream file;

			bool hsent;
			uint64 fsize;
			uint64 offset;

			filesender(boost::asio::io_service& service);

			void init(uint64 offset_ = 0);

			bool getbuf(shared_vec buf);
		};

		struct dirsender {
			std::string name;
			ext::filesystem::path path;

			ext::filesystem::directory_iterator curiter;
			ext::filesystem::directory_iterator enditer;

			bool hsent;

			void init();
			bool getbuf(shared_vec buf, ext::filesystem::path& newpath);
		};

		struct qitem {
			int type;
			std::string path;
			uint64 fsize;
			uint64 poffset;
			uint32 psize;
			std::vector<uint64> pos;
			qitem(int type_, const std::string& path_, uint64 fsize_ = 0) : type(type_), path(path_), fsize(fsize_), poffset(0) {};
			qitem(int type_, const std::string& path_, const std::vector<uint64>& pos_, uint32 psize_) : type(type_), path(path_), fsize(-1), poffset(0), psize(psize_), pos(pos_) {};
		};
		std::deque<qitem> qitems;

		struct sigmaker {
			boost::asio::io_service& service;
			qitem* item;
			boost::function<void(shared_vec)> cb;

			sigmaker(boost::asio::io_service& service_);

			void main();
		};

		struct sigchecker {
			boost::asio::io_service& service;
			qitem* item;
			boost::function<void(uint64)> cb;
			std::vector<uint8> data;
			ext::filesystem::path path;
			FILE* fd;

			sigchecker(boost::asio::io_service& service_);

			uint64 getoffset();
			void main();
		};
};

#endif//CONNECTION_COMMON_H
