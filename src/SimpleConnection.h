#ifndef SIMPLE_CONNECTION_H
#define SIMPLE_CONNECTION_H

#include <set>
#include <queue>

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>

#include "Globals.h"

#include "../util/StrFormat.h"
#include "../util/Filesystem.h"

/// helper classes, TODO: nest them properly...
struct cmdinfo {
	uint32 cmd;
	uint32 ver;
	uint64 len;
};

inline uint32 pkt_get_uint32(uint8* buf)
{
	return (buf[0] << 0) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

inline void pkt_put_uint32(uint32 val, uint8* buf)
{
	buf[0] = (val >>  0) & 0xFF;
	buf[1] = (val >>  8) & 0xFF;
	buf[2] = (val >> 16) & 0xFF;
	buf[3] = (val >> 24) & 0xFF;
}

inline void pkt_put_uint64(uint64 val, uint8* buf)
{
	buf[0] = (val >>  0) & 0xFF;
	buf[1] = (val >>  8) & 0xFF;
	buf[2] = (val >> 16) & 0xFF;
	buf[3] = (val >> 24) & 0xFF;
	buf[4] = (val >> 32) & 0xFF;
	buf[5] = (val >> 40) & 0xFF;
	buf[6] = (val >> 48) & 0xFF;
	buf[7] = (val >> 56 ) & 0xFF;
}

inline void pkt_put_vuint64(uint64 val, std::vector<uint8>& buf)
{
	while (val & ~0x7F) {
		buf.push_back((uint8)0x80 | (val & 0x7F));
		val >>= 7;
	}
	buf.push_back((uint8) val);
}

inline uint64 pkt_get_vuint64(std::vector<uint8>& buf, uint &idx)
{
	uint64 result = 0;
	uint8 shift = 0;
	uint8 val;
	do {
		val = buf[idx++];
		result |= (uint64(val&0x7F) << shift);
		shift += 7;
	} while (val & 0x80);
	return result;
}

inline uint64 pkt_get_vuint64(std::vector<uint8>& buf)
{
	uint idx = 0;
	return pkt_get_vuint64(buf, idx);
}

inline void pkt_put_vuint32(uint32 val, std::vector<uint8>& buf)
{
	while (val & ~0x7F) {
		buf.push_back((uint8)0x80 | (val & 0x7F));
		val >>= 7;
	}
	buf.push_back((uint8) val);
}

inline uint32 pkt_get_vuint32(std::vector<uint8>& buf, uint &idx)
{
	uint32 result = 0;
	uint8 shift = 0;
	uint8 val;
	do {
		val = buf[idx++];
		result |= (uint32(val&0x7F) << shift);
		shift += 7;
	} while (val & 0x80);
	return result;
}

inline uint32 pkt_get_vuint32(std::vector<uint8>& buf)
{
	uint idx = 0;
	return pkt_get_vuint32(buf, idx);
}

struct filesender {
	std::string name;
	boost::filesystem::path path;
	services::diskio_filetype file;

	bool hsent;
	uint64 fsize;
	uint64 offset;

	filesender() : file(*gdiskio) {};

	void init(uint64 offset_ = 0) {
		offset = offset_;
		fsize = boost::filesystem::file_size(path) - offset;
		file.open(path, services::diskio_filetype::in);
		while (offset_ > 0) {
			int32 diff = (offset_ > 0x7fffffff) ? 0x7fffffff : (int32)offset_;
			file.fwseek(diff);
			offset_ -= diff;
		}
		hsent = false;
		//cout << "<init(): " << path << " : " << fsize << " : " << hsent << '\n';
	};

	bool getbuf(shared_vec buf) {
		if (!hsent) {
			hsent = true;
			buf->resize(16 + name.size());
			cmdinfo hdr;
			hdr.cmd = (offset == 0) ? 0x01 : 0x17;//CMD_REPLY_PARTIAL_FILE; // file
			hdr.len = fsize;
			hdr.ver = name.size();
			memcpy(&(*buf)[0], &hdr, 16);
			memcpy(&(*buf)[16], name.data(), hdr.ver);
			//fsize += buf->size();
			//handler(boost::system::error_code(), buf->size());
			return true;
		}
		return false;
		//cout << ">file.async_read_some(): " << path << " : " << buf->size() << " : " << fsize << '\n';
	};
};

struct dirsender {
	std::string name;
	boost::filesystem::path path;

	boost::filesystem::directory_iterator curiter;
	boost::filesystem::directory_iterator enditer;

	bool hsent;

	void init() {
		curiter = boost::filesystem::directory_iterator(path);
		hsent = false;
	};

	bool getbuf(shared_vec buf, boost::filesystem::path& newpath)
	{
		if (!hsent) {
			hsent = true;
			buf->resize(16 + name.size());
			cmdinfo hdr;
			hdr.cmd = 0x02; // dir
			hdr.len = 0;
			hdr.ver = name.size();
			memcpy(&(*buf)[0], &hdr, 16);
			memcpy(&(*buf)[16], name.data(), hdr.ver);
		} else
			buf->clear();

		if (curiter == enditer) {
			cmdinfo hdr;
			hdr.cmd = 0x03; // ..
			hdr.len = 0;
			hdr.ver = 0;
			uint32 clen = buf->size();
			buf->resize(clen+16);
			memcpy(&(*buf)[clen], &hdr, 16);
			return true;
		}

		newpath = curiter->path();

		++curiter;
		return false;
	};
};

class SimpleTCPConnection {
	private:
		friend class SimpleBackend;
		class SimpleBackend* backend;

		enum {
			CMD_NONE,
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
			CMD_REQUEST_SIG_FILE,
			CMD_REPLY_SIG_FILE,
			CMD_REQUEST_PARTIAL_FILE,
			CMD_REPLY_PARTIAL_FILE,
			END_OF_COMMANDS
		};

		boost::asio::io_service& service;
		boost::asio::ip::tcp::socket socket;

		std::string sharename;
		boost::filesystem::path sharepath;

		filesender cursendfile;
		std::vector<dirsender> quesenddir;
		bool newstyle;
		bool qitemsfilled;

		boost::filesystem::path getsharepath(std::string sharename);

		struct qitem {
			int type;
			std::string path;
			uint64 fsize;
			uint64 poffset;
			std::vector<uint64> pos;
			qitem(int type_, const std::string& path_, uint64 fsize_ = 0) : type(type_), path(path_), fsize(fsize_), poffset(0) {};
			qitem(int type_, const std::string& path_, const std::vector<uint64>& pos_, uint64 fsize_) : type(type_), path(path_), fsize(fsize_), pos(pos_), poffset(0) {};
		};
		std::deque<qitem> qitems;

		enum {
			QITEM_SIGREQ = 6,
			QITEM_SIGREQ_BUSY,
			QITEM_PARTFILE_REQ,
			QITEM_PARTFILE_BUSY,
		};

		struct sigmaker {
			boost::asio::io_service& service;
			qitem* item;
			boost::function<void(shared_vec)> cb;
			uint64 offset;

			sigmaker(boost::asio::io_service& service_) : service(service_) {};

			void main() {
				size_t bread;
				using namespace std;
				// dostuff
				shared_vec sbuf(new std::vector<uint8>(16));

				pkt_put_uint32(CMD_REPLY_SIG_FILE, &((*sbuf)[0]));
				pkt_put_uint32(0, &((*sbuf)[4]));

				//pkt_put_vuint32(item->path.size(), *sbuf);
				//for (uint j = 0; j < item->path.size(); ++j)
				//	sbuf->push_back(item->path[j]);
				pkt_put_vuint32(item->pos.size(), *sbuf);
				pkt_put_vuint64(item->pos[0], *sbuf);
				for (uint i = 1; i < item->pos.size(); ++i)
					pkt_put_vuint64(item->pos[i]-item->pos[i-1], *sbuf);

				FILE* fd = fopen(item->path.c_str(), "rb");
				size_t dpos = sbuf->size();
				sbuf->resize(dpos + 2*16*1024);
				bread = fread(&((*sbuf)[dpos]), 1, 16*1024, fd);
				dpos += 16*1024;
				offset -= 16*1024;
				fseek(fd, 0, SEEK_SET);
				while (offset > 0) {
					int32 diff = (offset > 0x7fffffff) ? 0x7fffffff : (int32)offset;
					fseek(fd, diff, SEEK_CUR);
					offset -= diff;
				}

				bread = fread(&((*sbuf)[dpos]), 1, 16*1024, fd);

				uint8 buf1;
				fseek(fd, item->pos[0], SEEK_SET);
				bread = fread(&buf1, 1, 1, fd);
				sbuf->push_back(buf1);
				for (uint i = 1; i < item->pos.size(); ++i) {
					fseek(fd, (item->pos[i]-1)-item->pos[i-1], SEEK_CUR);
					bread = fread(&buf1, 1, 1, fd);
					sbuf->push_back(buf1);
				}

				pkt_put_uint64(sbuf->size()-16, &((*sbuf)[8]));
				service.post(boost::bind(cb, sbuf));
				fclose(fd);
			}
		};

		struct sigchecker {
			boost::asio::io_service& service;
			qitem* item;
			boost::function<void(uint64)> cb;
			std::vector<uint8> data;
			boost::filesystem::path path;
			uint64 offset;
			FILE* fd;

			sigchecker(boost::asio::io_service& service_) : service(service_) {};

			uint64 getoffset()
			{
				size_t bread;
				using namespace std;
				uint64 fsize = boost::filesystem::file_size(path);

				vector<uint8> start(16*1024);
				vector<uint8> end(16*1024);

				bread = fread(&start[0], 1, 16*1024, fd);
				offset -= 16*1024;
				fseek(fd, 0, SEEK_SET);
				while (offset > 0) {
					int32 diff = (offset > 0x7fffffff) ? 0x7fffffff : (int32)offset;
					fseek(fd, diff, SEEK_CUR);
					offset -= diff;
				}
				bread = fread(&end[0], 1, 16*1024, fd);

				for (uint i = 0; i < 16*1024; ++i)
					if (data[i] != start[i])
						return i;

				uint8 buf1;
				fseek(fd, item->pos[0], SEEK_SET);
				bread = fread(&buf1, 1, 1, fd);
				if (buf1 != data[2*16*1024 + 0])
					return 0;
				for (uint i = 1; i < item->pos.size(); ++i) {
					fseek(fd, (item->pos[i]-1)-item->pos[i-1], SEEK_CUR);
					bread = fread(&buf1, 1, 1, fd);
					if (buf1 != data[2*16*1024 + i])
						return 0;
				}

				for (uint i = 0; i < 16*1024; ++i)
					if (data[1*16*1024 + i] != end[i])
						return 0;
				return fsize;
			}

			void main() {
				fd = fopen(path.native_file_string().c_str(), "rb");
				service.post(boost::bind(cb, getoffset()));
				fclose(fd);
			}
		};

		std::string error_message;
		bool dldone;
		bool uldone;
		uint32 open_files;

		std::deque<shared_vec> sendqueue;
		bool issending;
		bool donesend;
		bool isreading;

		std::set<uint32> lcommands;
		std::set<uint32> rcommands;
		bool rresume;

		cmdinfo rcmd;
	public:
		SimpleTCPConnection(boost::asio::io_service& service_, SimpleBackend* backend_)
			: backend(backend_)
			, service(service_)
			, socket(service_)
			, progress_timer(service_)
		{
			dldone = false;
			uldone = false;
			issending = false;
			isreading = false;
			donesend = false;
			newstyle = false;
			qitemsfilled = false;
			open_files = 0;
			transfered_bytes = 0;
			total_bytes = 0;

			// todo, make this more global?
			for (uint32 i = 1; i < END_OF_COMMANDS; ++i)
				lcommands.insert(i);
			for (uint32 i = 1; i <= 6; ++i)
				rcommands.insert(i);
			rresume = false;
		}

		uint64 transfered_bytes;
		uint64 total_bytes;
		boost::asio::deadline_timer progress_timer;
		boost::signal<void(uint64,std::string,uint32,uint64)> sig_progress;

		void start_update_progress() {
			//progress_timer.expires_from_now(boost::posix_time::seconds(1));
			progress_timer.expires_from_now(boost::posix_time::milliseconds(250));
			progress_timer.async_wait(boost::bind(&SimpleTCPConnection::update_progress_handler, this, boost::asio::placeholders::error));
		}

		void update_progress_handler(const boost::system::error_code& e)
		{
			if (e) {
				std::cout << "update_progress_handler failed: " << e.message() << '\n';
			} else {
				if (error_message != "") {
					sig_progress(transfered_bytes, std::string() + "Error: " + error_message, 0, total_bytes);
				} else if (dldone && open_files == 0) {
					sig_progress(transfered_bytes, "Completed", 0, total_bytes);
					disconnect();
				} else if (uldone) {
					sig_progress(transfered_bytes, "Completed", 0, total_bytes);
					disconnect();
				} else {
					sig_progress(transfered_bytes, "Transfering", sendqueue.size()+open_files, total_bytes);
					start_update_progress();
				}
			}
		}

		void handle_tcp_accept()
		{
			// TODO: connect handler somewhere
			start_update_progress();
			shared_vec rbuf(new std::vector<uint8>());
			rbuf->resize(4);
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_protver, this, _1, rbuf));
		}

		void handle_recv_protver(const boost::system::error_code& e, shared_vec rbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_recv_protver: %s", e.message()));
				return;
			}
			uint32 protver = 0;
			protver |= (rbuf->at(0) <<  0);
			protver |= (rbuf->at(1) <<  8);
			protver |= (rbuf->at(2) << 16);
			protver |= (rbuf->at(3) << 24);

			if (protver == 1) {
				boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_namelen, this, _1, rbuf));
			} else if (protver == 2) {
				rbuf->resize(16 + 8*lcommands.size());
				pkt_put_uint32(7, &((*rbuf)[0]));
				pkt_put_uint32(0, &((*rbuf)[4]));
				pkt_put_uint64(8*lcommands.size(), &((*rbuf)[8]));
				{
					int i = 16;
					BOOST_FOREACH(uint32 val, lcommands) {
						pkt_put_uint32(val, &((*rbuf)[i])); i += 4;
						pkt_put_uint32(val, &((*rbuf)[i])); i += 4;
					}
				}

				boost::asio::async_write(socket, GETBUF(rbuf),
					boost::bind(&SimpleTCPConnection::start_receive_command, this, rbuf, _1));
			} else
				std::cout << "error: unknown tcp protocol version: " << protver << '\n';
		}

		void start_receive_command()
		{
			start_receive_command(shared_vec(new std::vector<uint8>));
		}

		void start_receive_command(shared_vec rbuf)
		{
			if (qitemsfilled && qitems.empty()) {
				dldone = true;
				rbuf->resize(16);
				pkt_put_uint32(CMD_DISCONNECT, &((*rbuf)[0]));
				pkt_put_uint32(0, &((*rbuf)[4]));
				pkt_put_uint64(0, &((*rbuf)[8]));
				boost::asio::write(socket, GETBUF(rbuf));
				disconnect();
				return;
			}
			rbuf->resize(16);
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::command_header_handler, this, _1, rbuf));
		}

		void start_receive_command(shared_vec rbuf, const boost::system::error_code& e)
		{
			if (e) {
				disconnect(STRFORMAT("start_receive_command: %s", e.message()));
			} else {
				start_receive_command(rbuf);
			}
		}

		void command_header_handler(const boost::system::error_code& e, shared_vec rbuf)
		{
			if (e) {
				disconnect(STRFORMAT("command_header_handler: %s", e.message()));
				return;
			}

			cmdinfo hdr;
			memcpy(&hdr, &((*rbuf)[0]), 16);

			switch (hdr.cmd) {
				// commands 1..4 are old-style commands where hdr.len may not be correct
				case CMD_OLD_FILE: { // file
					rbuf->resize(hdr.ver);
					boost::asio::async_read(socket, GETBUF(rbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_file_header, this, _1, hdr, rbuf));
				}; break;
				case CMD_OLD_DIR: { // dir
					rbuf->resize(hdr.ver);
					boost::asio::async_read(socket, GETBUF(rbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_dir_header, this, _1, hdr, rbuf));
				}; break;
				case CMD_OLD_CDUP: { // ..
					//cout << "got '..'\n";
					sharepath /= "..";
					sharepath.normalize();
					start_receive_command(rbuf);
				}; break;
				case CMD_OLD_DONE: { // ..
					std::cout << "download finished!\n";
					dldone = true;
				}; break;
				case CMD_REPLY_PARTIAL_FILE: {
					qitem tqi = qitems.front();
					qitems.pop_front();
					kickoff_file_write(sharepath / tqi.path, hdr.len, rbuf, false, tqi.poffset);
				}; break;
				case CMD_REPLY_FULL_FILE: {
					qitem tqi = qitems.front();
					qitems.pop_front();
					kickoff_file_write(sharepath / tqi.path, hdr.len, rbuf, false, 0);
				}; break;
				default: {
					if (lcommands.count(hdr.cmd)) {
						uint32 chunk;
						if (hdr.len < 1024*1024)
							chunk = (uint32)hdr.len;
						else
							chunk = 1024*1024;

						rbuf->resize(chunk);
						boost::asio::async_read(socket, GETBUF(rbuf),
							boost::bind(&SimpleTCPConnection::command_body_handler, this, _1, rbuf, hdr));
					} else {
						disconnect(STRFORMAT("Unsupported command: %d", hdr.cmd));
					}
				};
			}
		}

		void command_body_handler(const boost::system::error_code& e, shared_vec rbuf, const cmdinfo& hdr)
		{
			if (e) {
				disconnect(STRFORMAT("command_body_handler: %s", e.message()));
				return;
			}

			bool handled = false;
			if (hdr.len == rbuf->size()) {
				// we got entire message
				handled = true;
				switch (hdr.cmd) {
					case CMD_REQUEST_SHARE_DUMP: { // request old style share dump
						if (hdr.len < 0xffff) {
							handle_recv_name(boost::system::error_code(), rbuf);
						} else
							disconnect(STRFORMAT("Requested share name too long: %d", hdr.len));
					}; break;
					case CMD_REPLY_UNKNOWN_COMMAND: { // unknown command
						uint32 command = pkt_get_uint32(&((*rbuf)[0]));
						std::cout << "remote end does not support command: " << command << '\n';
						// do something depending on state?
						if (command == 7)
							got_supported_commands(rbuf);
						else
							disconnect(STRFORMAT("Remote does not support command: %d", command));
					}; break;
					case CMD_REQUEST_COMMAND_LIST: { // request supported command list
						std::set<uint32> requested;
						for (uint32 i = 0; i+8 <= rbuf->size(); i += 8) {
							uint32 lver = pkt_get_uint32(&((*rbuf)[i+0]));
							uint32 hver = pkt_get_uint32(&((*rbuf)[i+4]));
							if (lver <= hver && (hver-lver) < 100)
								for (uint32 j = lver; j <= hver; ++j)
									if (lcommands.count(j))
										requested.insert(j);
						}
						rbuf->resize(16 + 8*requested.size());
						pkt_put_uint32(8, &((*rbuf)[0]));
						pkt_put_uint32(0, &((*rbuf)[4]));
						pkt_put_uint64(8*requested.size(), &((*rbuf)[8]));
						int i = 16;
						BOOST_FOREACH(uint32 val, requested) {
							pkt_put_uint32(val, &((*rbuf)[i])); i += 4;
							pkt_put_uint32(val, &((*rbuf)[i])); i += 4;
						}
						boost::asio::async_write(socket, GETBUF(rbuf),
							boost::bind(&SimpleTCPConnection::start_receive_command, this, rbuf, _1));
					}; break;
					case CMD_REPLY_COMMAND_LIST: { // supported command list
						for (uint32 i = 0; i+8 <= rbuf->size(); i += 8) {
							uint32 lver = pkt_get_uint32(&((*rbuf)[i+0]));
							uint32 hver = pkt_get_uint32(&((*rbuf)[i+4]));
							if (lver <= hver && (hver-lver) < 100)
								for (uint32 j = lver; j <= hver; ++j)
									rcommands.insert(j);
						}
						got_supported_commands(rbuf);
					}; break;
					case CMD_REQUEST_TREE_LIST: { // request file listing
						newstyle = true;
						if (hdr.len < 0xffff) {
							handle_request_listing(rbuf);
						} else
							disconnect(STRFORMAT("Requested share name too long: %d", hdr.len));
					}; break;
					case CMD_REPLY_TREE_LIST: { // reply file listing
						handle_reply_listing(rbuf);
					}; break;
					case CMD_REQUEST_FULL_FILE: {
						if (hdr.len < 0xffff) {
							qitem nqi(5, (sharepath / std::string(rbuf->begin(), rbuf->end())).string());
							qitems.push_back(nqi);
							checkwhattosend();
							start_receive_command(rbuf);
						} else
							disconnect(STRFORMAT("Requested share name too long: %d", hdr.len));
					}; break;
					case CMD_REQUEST_SIG_FILE: {
						uint idx = 0;
						uint32 slen = pkt_get_vuint32(*rbuf, idx);
						std::string name(rbuf->begin()+idx, rbuf->begin()+idx+slen);
						idx += slen;
						uint32 plen = pkt_get_vuint32(*rbuf, idx);
						std::vector<uint64> pos;
						uint64 tcpos = 0;
						for (uint32 i = 0; i < plen; ++i) {
							tcpos += pkt_get_vuint64(*rbuf, idx);
							pos.push_back(tcpos);
						}
						uint64 size = pkt_get_vuint64(*rbuf, idx);
						qitems.push_back(qitem(QITEM_SIGREQ, name, pos, size));
						checkwhattosend();
						start_receive_command(rbuf);
					}; break;
					case CMD_REPLY_SIG_FILE: {
						uint idx = 0;
						uint32 plen = pkt_get_vuint32(*rbuf, idx);
						std::vector<uint64> pos;
						uint64 tcpos = 0;
						for (uint32 i = 0; i < plen; ++i) {
							tcpos += pkt_get_vuint64(*rbuf, idx);
							pos.push_back(tcpos);
						}
						std::vector<uint8> data(rbuf->begin()+idx, rbuf->end());
						qitems.front().pos = pos;

						boost::shared_ptr<sigchecker> sc(new sigchecker(service));
						sc->cb = boost::bind(&SimpleTCPConnection::sigcheck_done , this, sc, _1);
						sc->item = &qitems.front();
						sc->data = data;
						sc->offset = qitems.front().poffset;
						sc->path = sharepath / qitems.front().path;
						gdiskio->get_work_service().post(boost::bind(&sigchecker::main, sc));
					}; break;
					case CMD_REQUEST_PARTIAL_FILE: {
						uint idx = 0;
						uint32 slen = pkt_get_vuint32(*rbuf, idx);
						std::string name(rbuf->begin()+idx, rbuf->begin()+idx+slen);
						idx += slen;
						uint64 poff = pkt_get_vuint64(*rbuf, idx);
						qitem nqi(5, (sharepath / name).string());
						nqi.poffset = poff;
						qitems.push_back(nqi);
						checkwhattosend();
						start_receive_command(rbuf);
					}; break;
					case CMD_DISCONNECT: {
						uldone = dldone = true;
						disconnect();
					}; break;
					default: {
						handled = false;
					};
				};
			}
			if (!handled) {
				// we possibly got partial message
				switch (hdr.cmd) {
					case CMD_REPLY_SIG_FILE:
					case CMD_REPLY_TREE_LIST: { // reply file listing
						uint64 chunk = hdr.len - rbuf->size();
						if (chunk > 1024*1024) chunk = 1024*1024;

						size_t osize = rbuf->size();
						rbuf->resize(rbuf->size() + (size_t)chunk);
						boost::asio::async_read(socket, boost::asio::buffer(&((*rbuf)[osize]), (size_t)chunk),
							boost::bind(&SimpleTCPConnection::command_body_handler, this, _1, rbuf, hdr));
					}; break;
					case 0:
					default: {
						disconnect(STRFORMAT("Message too large (%d, %d)", hdr.cmd, hdr.len));
					};
				};
			}
		}

		void got_supported_commands(shared_vec tbuf)
		{
			if (sharename.empty()) {
				// server side of connection
				start_receive_command(tbuf);
			} else if (rcommands.count(CMD_REQUEST_TREE_LIST) && rcommands.count(CMD_REPLY_FULL_FILE) && rcommands.count(CMD_DISCONNECT)) {
				// future type connection (with resume)
				qitems.push_back(qitem(0, sharename, 0));
				rresume = use_expiremental_resume() && rcommands.count(CMD_REQUEST_SIG_FILE) && rcommands.count(CMD_REQUEST_PARTIAL_FILE);
				handle_qitems(tbuf);
			} else if (rcommands.count(5)) {
				// simple style connection
				uint32 nlen = sharename.size();
				tbuf->resize(16 + nlen);
				pkt_put_uint32(5, &((*tbuf)[0]));
				pkt_put_uint32(0, &((*tbuf)[4]));
				pkt_put_uint64(nlen, &((*tbuf)[8]));

				for (uint i = 0; i < nlen; ++i)
					(*tbuf)[i+16] = sharename[i];

				boost::asio::async_write(socket, GETBUF(tbuf),
					boost::bind(&SimpleTCPConnection::start_receive_command, this, tbuf, _1));
			} else {
				disconnect("Remote doesn't support any useful methods");
			}
		}

		void handle_request_listing(shared_vec tbuf)
		{
			std::string name(tbuf->begin(), tbuf->end());
			//std::cout << name << '\n';
			std::vector<std::string> elems;
			boost::split(elems, name, boost::is_any_of("/"));

			if (elems.empty()) {
				disconnect("Root listing unsupported...", true);
				return;
			}

			boost::filesystem::path spath(getsharepath(elems[0]));
			sharepath = spath.branch_path();
			if (spath.empty() || !ext::filesystem::exists(spath)) {
				disconnect("Invalid share requested.", true);
				return;
			}

			for (uint i = 1; i < elems.size(); ++i)
				spath /= elems[i];
			if (!ext::filesystem::exists(spath)) {
				disconnect("Invalid path Requested.", true);
				return;
			}

			boost::filesystem::path curpath(elems.back());
			if (boost::filesystem::is_directory(spath)) {
				//std::cout << "Single directory!\n";

				tbuf->resize(16);
				pkt_put_uint32(10, &((*tbuf)[0]) + 0);
				pkt_put_uint32(0, &((*tbuf)[0]) + 4);

				tbuf->push_back(2); // dir
				pkt_put_vuint32(curpath.string().size(), *tbuf);
				for (uint i = 0; i < curpath.string().size(); ++i)
					tbuf->push_back(curpath.string()[i]);

				int curlevel = 0;
				boost::filesystem::recursive_directory_iterator curiter(spath);
				boost::filesystem::recursive_directory_iterator enditer;
				for (; curiter != enditer; ++curiter) {
					for (; curlevel > curiter.level(); --curlevel)
						curpath = curpath.branch_path();
					curpath /= curiter->leaf();
					++curlevel;
					if (boost::filesystem::is_directory(*curiter)) {
						tbuf->push_back(2); // dir
						pkt_put_vuint32(curpath.string().size(), *tbuf);
						for (uint i = 0; i < curpath.string().size(); ++i)
							tbuf->push_back(curpath.string()[i]);
						//std::cout << "Directory: " << curpath << '\n';
					} else {
						tbuf->push_back(1); // file
						total_bytes += boost::filesystem::file_size(*curiter);
						pkt_put_vuint64(boost::filesystem::file_size(*curiter), *tbuf);
						pkt_put_vuint32(curpath.string().size(), *tbuf);
						for (uint i = 0; i < curpath.string().size(); ++i)
							tbuf->push_back(curpath.string()[i]);
						//std::cout << "File: " << curpath << '\n';
					}
				}
			} else {
//				tbuf->clear();
				tbuf->resize(16);
				pkt_put_uint32(10, &((*tbuf)[0]) + 0);
				pkt_put_uint32(0, &((*tbuf)[0]) + 4);

				tbuf->push_back(1); // file
				total_bytes += boost::filesystem::file_size(spath);
				pkt_put_vuint64(boost::filesystem::file_size(spath), *tbuf);
				pkt_put_vuint32(curpath.string().size(), *tbuf);
				for (uint i = 0; i < curpath.string().size(); ++i)
					tbuf->push_back(curpath.string()[i]);

				//std::cout << "Single file!\n";
			}
			tbuf->push_back(4); // end of list
			pkt_put_uint64(tbuf->size() - 16, &((*tbuf)[0]) + 8);
			send_receive_packet(tbuf);
		}

		void handle_reply_listing(shared_vec tbuf)
		{
			uint pos = 0;
			while (pos < tbuf->size()) {
				int tp = tbuf->at(pos++);
				switch (tp) {
					case 1: { // file
						qitem qi(1, "", pkt_get_vuint64(*tbuf, pos));
						total_bytes += qi.fsize;
						uint32 nlen = pkt_get_vuint32(*tbuf, pos);
						for (uint i = 0; i < nlen; ++i)
							qi.path.push_back(tbuf->at(pos++));
						qitems.push_back(qi);
					}; break;
					case 2: { // dir
						qitem qi(2, "");
						uint32 nlen = pkt_get_vuint32(*tbuf, pos);
						for (uint i = 0; i < nlen; ++i)
							qi.path.push_back(tbuf->at(pos++));
						boost::filesystem::path dp = sharepath / qi.path;
						boost::filesystem::create_directory(dp);
						//qitems.push_front(qi);
					}; break;
					case 4: { // eol
						pos = tbuf->size();
					}; break;
				}
			}
			handle_qitems(tbuf);

		}

		void handle_qitems(shared_vec tbuf) {
			if (qitems.empty()) {
				qitemsfilled = true;
				start_receive_command(tbuf);
				return;
			}

			qitem& item = qitems.front();

			switch (item.type) {
				case 0: {
					uint32 nlen = item.path.size();
					tbuf->resize(16 + nlen);
					pkt_put_uint32(9, &((*tbuf)[0]));
					pkt_put_uint32(0, &((*tbuf)[4]));
					pkt_put_uint64(nlen, &((*tbuf)[8]));

					for (uint i = 0; i < nlen; ++i)
						(*tbuf)[i+16] = item.path[i];

					send_receive_packet(tbuf);
					qitems.pop_front();
				}; break;
				case 1: {
					tbuf->clear();
					qitemsfilled = true;
					for (uint i = 0; i < qitems.size() && qitems[i].type == 1; ++i) {
						qitem titem = qitems[i];
						size_t cstart = tbuf->size();
						tbuf->resize(tbuf->size()+16);
						if (rresume && ext::filesystem::exists(sharepath / titem.path) && boost::filesystem::file_size(sharepath / titem.path) > 1024*1024) {
							uint64 fsize = boost::filesystem::file_size(sharepath / titem.path);
							std::vector<uint64> pos;
							{
								uint64 start = 16*1024;
								uint64 stop = fsize - (16*1024);
								uint64 range = stop - start;
								pos.push_back(0);
								for (uint64 i = start; i < stop; i += (range / (16*1024)))
									pos.push_back(i);
								//std::sort(pos.begin(), pos.end());
							}
							pkt_put_uint32(CMD_REQUEST_SIG_FILE, &((*tbuf)[cstart+0]));
							pkt_put_uint32(0, &((*tbuf)[cstart+4]));
							item.type = QITEM_SIGREQ; // requested signiature
							item.poffset = fsize;
							pkt_put_vuint32(titem.path.size(), *tbuf);
							for (uint j = 0; j < titem.path.size(); ++j)
								tbuf->push_back(titem.path[j]);
							pkt_put_vuint32(pos.size()-1, *tbuf);
							for (uint i = 1; i < pos.size(); ++i)
								pkt_put_vuint64(pos[i]-pos[i-1], *tbuf);
							pkt_put_vuint64(fsize, *tbuf);
						} else {
							pkt_put_uint32(CMD_REQUEST_FULL_FILE, &((*tbuf)[cstart+0]));
							pkt_put_uint32(0, &((*tbuf)[cstart+4]));
							item.type = 5; // requested file
							//pkt_put_vuint32(item.path.size(), *tbuf);
							for (uint j = 0; j < titem.path.size(); ++j)
								tbuf->push_back(titem.path[j]);
						}
						pkt_put_uint64(tbuf->size() - cstart - 16, &((*tbuf)[cstart+8]));
					}
					send_receive_packet(tbuf);
				}; break;
			}
		}

		void send_receive_packet(shared_vec tbuf)
		{
			boost::asio::async_write(socket, GETBUF(tbuf),
				boost::bind(&SimpleTCPConnection::start_receive_command, this, tbuf, _1));
		}

		void sendpath(boost::filesystem::path path, std::string name = "")
		{
			if (name.empty()) name = path.leaf();

			if (!ext::filesystem::exists(path)) {
				disconnect(STRFORMAT("path does not exist: %s", path));
			}

			if (boost::filesystem::is_directory(path)) {
				dirsender newdir;
				newdir.name = name;
				newdir.path = path;
				newdir.init();
				quesenddir.push_back(newdir);
			} else {
				cursendfile.name = name;
				cursendfile.path = path;
				cursendfile.init();
			}
		}

		void handle_read_file(const boost::system::error_code& e, size_t len, shared_vec sbuf) {
			//cout << ">sendfileinfo(): " << cursendfile.path << " : " << len << " : " << cursendfile.fsize << '\n';
			isreading = false;
			if (e) {
				if (cursendfile.fsize == 0) {
					//cout << "closing file\n";
					cursendfile.file.close();
					cursendfile.name = "";
					checkwhattosend();
				} else {
					disconnect(STRFORMAT("handle_read_file: %s (%s)", e.message(), cursendfile.path));
				}
				return;
			}

			transfered_bytes += len;
			sbuf->resize(len);
			cursendfile.fsize -= len;
			//cout << ">async_write(): " << cursendfile.path << " : " << len << " : " << cursendfile.fsize << '\n';
			sendqueue.push_back(sbuf);
			checkwhattosend();
		}

		void handle_sent_buffer(const boost::system::error_code& e, size_t len, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_sent_buffer: %s", e.message()));
				return;
			}
			issending = false;
			checkwhattosend(sbuf);
		}

		void checkwhattosend(shared_vec sbuf = shared_vec()) {
			if (sendqueue.size() < 25) {
				// add new buffer to queue
				if (!sbuf) sbuf = shared_vec(new std::vector<uint8>());
				//shared_vec nbuf = shared_vec(new std::vector<uint8>());
				if (cursendfile.name != "") {
					if (!isreading) {
						bool hasbuf = cursendfile.getbuf(sbuf);
						if (hasbuf) {
							sendqueue.push_back(sbuf);
							service.post(boost::bind(&SimpleTCPConnection::checkwhattosend, this, shared_vec()));
						} else {
							uint64 rsize = cursendfile.fsize;
							if (rsize > 1024*1024*1) rsize = 1024*1024*1;
							if (rsize == 0)
								rsize = 1;
							sbuf->resize((size_t)rsize);
							isreading = true;
							cursendfile.file.async_read_some(GETBUF(sbuf),
								boost::bind(&SimpleTCPConnection::handle_read_file, this, _1, _2, sbuf));
						}
					}
				} else if (newstyle) {
					if (!qitems.empty()) {
						int qtype = qitems.front().type;
						if (qtype == 5) {
							sbuf->resize(16);
							uint64 off = qitems.front().poffset;
							cursendfile.name = qitems[0].path;
							cursendfile.path = qitems[0].path;
							cursendfile.init(off);
							cursendfile.hsent = true;
							qitems.pop_front();
							pkt_put_uint32((off == 0) ? CMD_REPLY_FULL_FILE : CMD_REPLY_PARTIAL_FILE, &((*sbuf)[0]));
							pkt_put_uint32(0, &((*sbuf)[4]));
							pkt_put_uint64(boost::filesystem::file_size(cursendfile.path) - off, &((*sbuf)[8]));

							sendqueue.push_back(sbuf);
							service.post(boost::bind(&SimpleTCPConnection::checkwhattosend, this, shared_vec()));
						} else if (qtype == QITEM_SIGREQ) {
							qitems.front().type = QITEM_SIGREQ_BUSY;
							qitems.front().path = (sharepath / qitems.front().path).native_file_string();
							boost::shared_ptr<sigmaker> sm(new sigmaker(service));
							sm->cb = boost::bind(&SimpleTCPConnection::sigmake_done, this, sm, _1);
							sm->item = &qitems.front();
							sm->offset = qitems.front().fsize;
							gdiskio->get_work_service().post(boost::bind(&sigmaker::main, sm));
						} else if (qtype == QITEM_SIGREQ_BUSY) {
							// ignore
						} else {
							std::cout << "Unknown item type: " << qtype << '\n';
						}
					}
				} else if (!quesenddir.empty()) {
					boost::filesystem::path newpath;

					bool dirdone = quesenddir.back().getbuf(sbuf, newpath);
					if (dirdone) {
						quesenddir.pop_back();
					} else {
						sendpath(newpath);
					}
					sendqueue.push_back(sbuf);
					service.post(boost::bind(&SimpleTCPConnection::checkwhattosend, this, shared_vec()));
				} else if (!donesend) {
					cmdinfo hdr;
					hdr.cmd = 0x04; // ..
					hdr.len = 0;
					hdr.ver = 0;
					sbuf->resize(16);
					memcpy(&(*sbuf)[0], &hdr, 16);

					sendqueue.push_back(sbuf);
					donesend = true;
				}
			}
			if (sendqueue.size() > 0 && !issending) {
				sbuf = sendqueue.front();
				sendqueue.pop_front();
				issending = true;
				boost::asio::async_write(socket, GETBUF(sbuf),
					boost::bind(&SimpleTCPConnection::handle_sent_buffer, this, _1, _2, sbuf));
			}
			if (sendqueue.size() == 0 && donesend && !issending)
				handle_sent_everything();
		}

		void sigmake_done(boost::shared_ptr<sigmaker> sm, shared_vec sbuf)
		{
			sendqueue.push_back(sbuf);
			qitems.pop_front();
			checkwhattosend();
		}

		void sigcheck_done(boost::shared_ptr<sigchecker> sc, uint64 offset)
		{
			qitem item = qitems.front();
			qitems.pop_front();
			item.type = QITEM_PARTFILE_REQ;
			item.poffset = offset;

			if (item.poffset == item.fsize) {
				// got the whole file, we're done!
				std::cout << "sigcheck_done: skipping file (" << item.path << ") of size '" << item.fsize << "', already got it\n";
			} else if (item.poffset < item.fsize) {
				std::cout << "sigcheck_done: resuming file (" << item.path << ") of size '" << item.fsize << "' at offset '" << item.poffset << "'\n";
				// still missing part of the file, request it
				shared_vec tbuf(new std::vector<uint8>(16));
				pkt_put_uint32(CMD_REQUEST_PARTIAL_FILE, &((*tbuf)[0]));
				pkt_put_uint32(0, &((*tbuf)[4]));

				pkt_put_vuint32(item.path.size(), *tbuf);
				for (uint j = 0; j < item.path.size(); ++j)
					tbuf->push_back(item.path[j]);

				pkt_put_vuint64(item.poffset, *tbuf);

				pkt_put_uint64(tbuf->size()-16, &((*tbuf)[8]));
				qitems.push_back(item);
				send_receive_packet(tbuf);
				return; // return to avoid start_receive_command() below
			} else { // item.poffset > item.fsize
				std::cout << "Resume: HUH?, got more of the file than is possible (" << item.path << ")\n";
			}

			start_receive_command();
		}

		void handle_sent_everything() {
			if (!newstyle)
				uldone = true;
		}

		void handle_recv_namelen(const boost::system::error_code& e, shared_vec rbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_recv_namelen: %s", e.message()));
				return;
			}
			uint32 namelen = 0;
			namelen |= (rbuf->at(0) <<  0);
			namelen |= (rbuf->at(1) <<  8);
			namelen |= (rbuf->at(2) << 16);
			namelen |= (rbuf->at(3) << 24);
			rbuf->resize(namelen);
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_name, this, _1, rbuf));
		}

		void handle_recv_name(const boost::system::error_code& e, shared_vec rbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_recv_name: %s", e.message()));
				return;
			}
			sharename.clear();
			for (uint i = 0; i < rbuf->size(); ++i)
				sharename.push_back(rbuf->at(i));
			got_share_name(rbuf);
		}

		void got_share_name(shared_vec rbuf)
		{
			std::cout << "got share name: " << sharename << '\n';
			sharepath = getsharepath(sharename);
			if (sharepath == "") {
				shared_vec buildfile = updateProvider.getBuildExecutable(sharename);
				if (buildfile) {
					std::cout << "is a build share\n";
					std::string name = sharename;
					if (sharename.find("win32") != std::string::npos) name = name + ".exe";
					if (sharename.find("-deb-") != std::string::npos) name = name + ".deb.signed";
					rbuf->resize(16 + name.size());
					cmdinfo hdr;
					hdr.cmd = 0x01; // file
					hdr.len = buildfile->size();
					hdr.ver = name.size();
					memcpy(&(*rbuf)[0], &hdr, 16);
					memcpy(&(*rbuf)[16], name.data(), hdr.ver);
					boost::asio::async_write(socket, GETBUF(rbuf),
						boost::bind(&SimpleTCPConnection::sent_autoupdate_header, this, _1, buildfile, rbuf));
				} else
					disconnect(STRFORMAT("share does not exist: %s(%s)", sharepath, sharename));
				return;
			}
			std::cout << "got share path: " << sharepath << '\n';
			sendpath(sharepath, sharename);
			checkwhattosend();
		}

		void sent_autoupdate_header(const boost::system::error_code& e, shared_vec buildfile, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("sent_autoupdate_header: %s", e.message()));
				return;
			}
			boost::asio::async_write(socket, GETBUF(buildfile),
						boost::bind(&SimpleTCPConnection::sent_autoupdate_file, this, _1, buildfile, sbuf));
		}

		void sent_autoupdate_file(const boost::system::error_code& e, shared_vec buildfile, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("sent_autoupdate_file: %s", e.message()));
				return;
			}
			transfered_bytes += buildfile->size();
			cmdinfo hdr;
			hdr.cmd = 0x04;
			sbuf->resize(16);
			memcpy(&(*sbuf)[0], &hdr, 16);
			boost::asio::async_write(socket, GETBUF(sbuf),
				boost::bind(&SimpleTCPConnection::sent_autoupdate_finish, this, _1, sbuf));
		}

		void sent_autoupdate_finish(const boost::system::error_code& e, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("sent_autoupdate_finish: %s", e.message()));
				return;
			}
			uldone = true;
		}

		void handle_tcp_connect(std::string name, boost::filesystem::path dlpath, uint32 version)
		{
			sharename = name;
			sharepath = dlpath;

			shared_vec sbuf(new std::vector<uint8>());

			sbuf->push_back((version >>  0)&0xff); // protocol version
			sbuf->push_back((version >>  8)&0xff); // protocol version
			sbuf->push_back((version >> 16)&0xff); // protocol version
			sbuf->push_back((version >> 24)&0xff); // protocol version

			uint32 namelen = name.size();
			if (version == 1) {
				sbuf->push_back((namelen >>  0)&0xff);
				sbuf->push_back((namelen >>  8)&0xff);
				sbuf->push_back((namelen >> 16)&0xff);
				sbuf->push_back((namelen >> 24)&0xff);

				for (uint i = 0; i < namelen; ++i)
					sbuf->push_back(name[i]);
			} else if (version == 2) {
				sbuf->resize(4 + 16 + 8*lcommands.size());
				pkt_put_uint32(7, &((*sbuf)[4]));
				pkt_put_uint32(0, &((*sbuf)[8]));
				pkt_put_uint64(8*lcommands.size(), &((*sbuf)[12]));
				{
					int i = 20;
					BOOST_FOREACH(uint32 val, lcommands) {
						pkt_put_uint32(val, &((*sbuf)[i])); i += 4;
						pkt_put_uint32(val, &((*sbuf)[i])); i += 4;
					}
				}
			} else {
				disconnect(STRFORMAT("Unknown version connecting: %s", version));
				return;
			}

			boost::asio::async_write(socket, GETBUF(sbuf),
				boost::bind(&SimpleTCPConnection::start_receive_command, this, sbuf, _1));

			start_update_progress();
		}

		void handle_recv_file_header(const boost::system::error_code& e, cmdinfo hdr, shared_vec rbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_recv_file_header: %s", e.message()));
				return;
			}

			std::string name(&((*rbuf)[0]), &((*rbuf)[0]) + rbuf->size());
			boost::filesystem::path path = sharepath / name;

			kickoff_file_write(path, hdr.len, rbuf, false, 0);
		}

		boost::function<void()> delayed_open_file;

		void delay_file_write(boost::filesystem::path path, uint64 fsize, shared_vec rbuf, bool dataready, uint64 offset)
		{
			delayed_open_file = boost::function<void()>();
			kickoff_file_write(path, fsize, rbuf, dataready, offset);
		}

		void kickoff_file_write(boost::filesystem::path path, uint64 fsize, shared_vec rbuf, bool dataready, uint64 offset)
		{
			if (delayed_open_file) {
				std::cout << "error: multiple file opens\n";
			}

			if (open_files > 256) {
				delayed_open_file = boost::bind(&SimpleTCPConnection::delay_file_write, this, path, fsize, rbuf, dataready, offset);
				return;
			}

			if (rbuf->empty()) dataready = false;

			bool* done = new bool;
			*done = false;

			++open_files;
			// kick off handle_ready_file for when the file is ready to write
			boost::shared_ptr<services::diskio_filetype> file(new services::diskio_filetype(*gdiskio));
			gdiskio->async_open_file(path, services::diskio_filetype::out|
				((offset == 0) ? services::diskio_filetype::create : services::diskio_filetype::in),
				*file,
				boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, fsize, rbuf, rbuf, offset));

			if (!dataready) {
				// kick off async_read for when we received some data (capped by file size)
				rbuf->resize((fsize>1024*1024*10) ? 1024*1024*10 : (uint32)fsize); // cast is ok because of ?:
				boost::asio::async_read(socket, GETBUF(rbuf),
					boost::bind(&SimpleTCPConnection::handle_recv_file, this, _1, file, done, fsize, rbuf));
			} else {
				handle_recv_file(boost::system::error_code(), file, done, fsize, rbuf);
			}
		}

		void handle_recv_dir_header(const boost::system::error_code& e, cmdinfo hdr, shared_vec rbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_recv_dir_header: %s", e.message()));
				return;
			}

			std::string name(&((*rbuf)[0]), &((*rbuf)[0]) + rbuf->size());

			//cout << "got dir '" << name << "'\n";
			sharepath /= name;
			boost::filesystem::create_directory(sharepath);
			start_receive_command(rbuf);
		}

		/** handle file receiving
		 *  @param e    an error occured
		 *  @param done true if file is ready to write (handle_ready_file already fired)
		 *  @param size amount of bytes left to receive for the file
		 *  @param wbuf the buffer into which we received the data
		 */
		void handle_recv_file(const boost::system::error_code& e, boost::shared_ptr<services::diskio_filetype> file, bool* done, uint64 size, shared_vec wbuf)
		{
			if (e) {
				disconnect(STRFORMAT("handle_recv_file: %s", e.message()));
				return;
			}

			transfered_bytes += wbuf->size();
			size -= wbuf->size();

			if (!*done) {
				*done = true;
				if (size == 0)
					start_receive_command(shared_vec(new std::vector<uint8>()));
			} else {
				shared_vec nrbuf;
				if (size == 0) {
					*done = true;
					start_receive_command(shared_vec(new std::vector<uint8>()));
				} else {
					*done = false;
					nrbuf = shared_vec(new std::vector<uint8>());
					nrbuf->resize((size>1024*1024*10) ? 1024*1024*10 : (uint32)size); // cast is ok because of ?:
					boost::asio::async_read(socket, GETBUF(nrbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_file, this, _1, file, done, size, nrbuf));
				};
				boost::asio::async_write(*file, GETBUF(wbuf),
					boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, size, nrbuf, wbuf, 0));
			}
		}

		/** handle file ready to write
		 *  @param e       an error occured
		 *  @param done    true if there is data ready to write (except if size==0, then we close the file)
		 *  @param size    amount of bytes left to write for the file
		 *  @param wbuf    the buffer from where we will write the data
		 *  @param curbuf  the buffer containing previous data (unused)
		 */
		void handle_ready_file(const boost::system::error_code& e, boost::shared_ptr<services::diskio_filetype> file, bool* done, uint64 size, shared_vec wbuf, shared_vec curbuf, uint64 offset)
		{
			if (e) {
				disconnect(STRFORMAT("handle_ready_file: %s", e.message()));
				return;
			}

			while (offset > 0) {
				int32 diff = (offset > 0x7fffffff) ? 0x7fffffff : (int32)offset;
				file->fwseek(diff);
				offset -= diff;
			}

			if (!*done) {
				*done = true;
			} else {
				if (size == 0) {
					delete done;
					file->close();
					--open_files;
					if (delayed_open_file)
						delayed_open_file();
					return;
				}
				size -= wbuf->size();
				shared_vec nrbuf;
				if (size == 0) {
					*done = true;
				} else {
					*done = false;
					nrbuf = shared_vec(new std::vector<uint8>());
					nrbuf->resize((size>1024*1024*10) ? 1024*1024*10 : (uint32)size); // cast is ok because of ?:
					boost::asio::async_read(socket, GETBUF(nrbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_file, this, _1, file, done, size, nrbuf));
				};
				boost::asio::async_write(*file, GETBUF(wbuf),
					boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, size, nrbuf, wbuf, 0));
			}
		}

		void disconnect(std::string errmsg = "", bool canrespond = false)
		{
			socket.close();
			if (!errmsg.empty()) {
				error_message += errmsg;
				error_message += "; ";
				std::cout << "Error: " << errmsg << '\n';
			}
		}

		bool use_expiremental_resume();
};
typedef boost::shared_ptr<SimpleTCPConnection> SimpleTCPConnectionRef;

#endif//SIMPLE_CONNECTION_H

