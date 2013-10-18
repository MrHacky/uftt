#ifndef SIMPLE_CONNECTION_H
#define SIMPLE_CONNECTION_H

#include "ConnectionCommon.h"

#include <set>
#include <queue>
#include <fstream>

#include <boost/version.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_smallint.hpp>

#include "../Globals.h"

#include "../Platform.h"
#include "../UFTTCore.h"

#include "../util/StrFormat.h"
#include "../util/Coro.h"

template <typename SockType, typename SockInit>
class SimpleConnection: public ConnectionCommon {
	private:
		SockType socket;

		std::string sharename;
		ext::filesystem::path writesharepath; //
		ext::filesystem::path readsharepath; // sharename is the name of the share we are uploading
		ext::filesystem::path cwdsharepath; // for old style connections

		filesender cursendfile;
		std::vector<dirsender> quesenddir;
		bool newstyle;
		bool qitemsfilled;

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

		uint32 maxBufSize;                             // Receive buffer size, starts small so that we get an early progress update
		size_t buffer_position;                        // Where in the current buffer we are (see checkwhattosend)
		uint64 bytes_transferred_since_last_update;    // Number of bytes transferred since last update (see update_statistics)

		cmdinfo rcmd;

		std::vector<std::string> getPathElems(const std::string& path) {
			std::vector<std::string> elems;
			boost::split(elems, path, boost::is_any_of("/"));
			elems.erase(std::remove_if(elems.begin(), elems.end(), mem_fun_ref(&std::string::empty)), elems.end());
			return elems;
		}

		ext::filesystem::path getReadShareFilePath(std::string pathspec)
		{
			std::vector<std::string> elems = getPathElems(pathspec);
			for (size_t i = 0; i < elems.size(); ++i)
				if (elems[i] == "..")
					return ext::filesystem::path(); // not allowed
			if (elems[0] != sharename) // TODO: allow changing share?
				return ext::filesystem::path();
			ext::filesystem::path p = readsharepath;
			for (size_t i = 1; i< elems.size(); ++i)
				p /= elems[i];
			return p;
		}

		ext::filesystem::path getWriteShareFilePath(std::string pathspec)
		{
			std::vector<std::string> elems = getPathElems(pathspec);
			for (size_t i = 0; i < elems.size(); ++i)
				if (elems[i] == "..")
					return ext::filesystem::path(); // not allowed
			if (elems[0] != sharename)
				return ext::filesystem::path();
			ext::filesystem::path p = writesharepath;
			for (size_t i = 1; i< elems.size(); ++i)
				p /= elems[i];
			return p;
		}

	public:
		SimpleConnection(boost::asio::io_service& service_, UFTTCore* core_, SockInit sockinit_)
			: ConnectionCommon(service_, core_)
			, socket(sockinit_)
			, cursendfile(service_)
			, buffer_position(0)
			, bytes_transferred_since_last_update(0)
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
			taskinfo.transferred = 0;
			taskinfo.size = 0;
			maxBufSize = 1024*10;

			// todo, make this more global?
			for (uint32 i = 1; i < END_OF_COMMANDS; ++i)
				lcommands.insert(i);
			for (uint32 i = 1; i <= 6; ++i)
				rcommands.insert(i);
			lcommands.erase(DEPRECATED_CMD_REQUEST_SIG_FILE);
			lcommands.erase(DEPRECATED_CMD_REPLY_SIG_FILE);

			rresume = false;
		}

		SockType& getSocket() {
			return socket;
		}

		boost::asio::deadline_timer progress_timer;

		uint32 getRcvBufSize(uint64 size)
		{
			return ((size>maxBufSize) ? maxBufSize : (uint32)size);
		}

		void start_update_progress() {
			progress_timer.expires_from_now(boost::posix_time::milliseconds(250));
			progress_timer.async_wait(boost::bind(&SimpleConnection::update_progress_handler, this, boost::asio::placeholders::error));
		}

		void update_progress_handler(const boost::system::error_code& e)
		{
			if (e) {
				std::cout << "update_progress_handler failed: " << e.message() << '\n';
			} else {
				taskinfo.queue = sendqueue.size()+open_files;
				if (error_message != "") {
					taskinfo.error_message = error_message;
					taskinfo.status = TASK_STATUS_ERROR;
					disconnect();
				} else if (dldone && open_files == 0) {
					taskinfo.status = TASK_STATUS_COMPLETED;
					boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
					taskinfo.speed = (taskinfo.transferred * 1000000L) / (boost::posix_time::microsec_clock::universal_time() - taskinfo.start_time).total_microseconds();
					disconnect();
				} else if (uldone) { // FIXME: Never triggers?
					taskinfo.status = TASK_STATUS_COMPLETED;
					boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
					taskinfo.speed = (taskinfo.transferred * 1000000L) / (boost::posix_time::microsec_clock::universal_time() - taskinfo.start_time).total_microseconds();
					disconnect();
				} else {
					taskinfo.status = TASK_STATUS_TRANSFERRING;
					start_update_progress();
				}
				sig_progress(taskinfo);
			}
		}

		void handle_tcp_accept()
		{
			// TODO: connect handler somewhere
			start_update_progress();
			shared_vec rbuf(new std::vector<uint8>());
			rbuf->resize(4);

			// not really correct, shareinfo.host is supposed to be the one sharing the share
			taskinfo.shareinfo.host = STRFORMAT("%s", socket.remote_endpoint());

			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleConnection::handle_recv_protver, this, _1, rbuf));
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
					boost::bind(&SimpleConnection::handle_recv_namelen, this, _1, rbuf)
				);
			} else if (protver == 2) {
				rbuf->resize(16 + 8*lcommands.size());
				pkt_put_uint32(CMD_REQUEST_COMMAND_LIST, &((*rbuf)[0]));
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
					boost::bind(&SimpleConnection::start_receive_command, this, rbuf, _1));
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
				try {
					boost::asio::write(socket, GETBUF(rbuf));
				} catch (std::exception& /*e*/) {
					// ignore errors, we were disconnecting anyway
				}
				disconnect();
				return;
			}
			rbuf->resize(16);
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleConnection::command_header_handler, this, _1, rbuf));
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
						boost::bind(&SimpleConnection::handle_recv_file_header, this, _1, hdr, rbuf));
				}; break;
				case CMD_OLD_DIR: { // dir
					rbuf->resize(hdr.ver);
					boost::asio::async_read(socket, GETBUF(rbuf),
						boost::bind(&SimpleConnection::handle_recv_dir_header, this, _1, hdr, rbuf));
				}; break;
				case CMD_OLD_CDUP: { // ..
					//cout << "got '..'\n";
					cwdsharepath /= "..";
					cwdsharepath.normalize();
					start_receive_command(rbuf);
				}; break;
				case CMD_OLD_DONE: { // ..
					std::cout << "download finished!\n";
					dldone = true;
				}; break;
				case CMD_REPLY_PARTIAL_FILE: {
					qitem tqi = qitems.front();
					qitems.pop_front();
					kickoff_file_write(getWriteShareFilePath(tqi.path), hdr.len, rbuf, tqi.poffset);
				}; break;
				case CMD_REPLY_FULL_FILE: {
					qitem tqi = qitems.front();
					qitems.pop_front();
					kickoff_file_write(getWriteShareFilePath(tqi.path), hdr.len, rbuf, 0);
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
							boost::bind(&SimpleConnection::command_body_handler, this, _1, rbuf, hdr));
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
						if (command == CMD_REQUEST_COMMAND_LIST)
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
						pkt_put_uint32(CMD_REPLY_COMMAND_LIST, &((*rbuf)[0]));
						pkt_put_uint32(0, &((*rbuf)[4]));
						pkt_put_uint64(8*requested.size(), &((*rbuf)[8]));
						int i = 16;
						BOOST_FOREACH(uint32 val, requested) {
							pkt_put_uint32(val, &((*rbuf)[i])); i += 4;
							pkt_put_uint32(val, &((*rbuf)[i])); i += 4;
						}
						boost::asio::async_write(socket, GETBUF(rbuf),
							boost::bind(&SimpleConnection::start_receive_command, this, rbuf, _1));
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
							try {
								handle_request_listing(rbuf);
							} catch (std::exception& e) {
								disconnect(STRFORMAT("Error during indexing phase: %s", e.what()));
							}
						} else
							disconnect(STRFORMAT("Requested share name too long: %d", hdr.len));
					}; break;
					case CMD_REPLY_TREE_LIST: { // reply file listing
						try {
							// handle_reply_listing may throw if something goes wrong.
							// E.g. we want to create a target directory, but
							// a file with that name already exists.
							handle_reply_listing(rbuf);
						} catch (std::exception& e) {
							disconnect(STRFORMAT("Error while handling listing reply: %s", e.what()));
						}
					}; break;
					case CMD_REQUEST_FULL_FILE: {
						if (hdr.len < 0xffff) {
							qitem nqi(QITEM_REQUESTED_FILE, getReadShareFilePath(std::string(rbuf->begin(), rbuf->end())).string());
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
						uint32 sizenum = pkt_get_vuint32(*rbuf, idx);
						if (sizenum != 1) disconnect("multiple sizes in CMD_REQUEST_SIG_FILE not supported.");
						uint32 psize = pkt_get_vuint32(*rbuf, idx);
						uint32 plen = pkt_get_vuint32(*rbuf, idx);
						std::vector<uint64> pos;
						uint64 tcpos = 0;
						for (uint32 i = 0; i < plen; ++i) {
							tcpos += pkt_get_vuint64(*rbuf, idx);
							pos.push_back(tcpos);
						}
						qitems.push_back(qitem(QITEM_REQUESTED_FILESIG, name, pos, psize));
						checkwhattosend();
						start_receive_command(rbuf);
					}; break;
					case CMD_REPLY_SIG_FILE: {
						boost::shared_ptr<sigchecker> sc(new sigchecker(service));
						sc->cb = boost::bind(&SimpleConnection::sigcheck_done , this, sc, _1);
						sc->item = &qitems.front();
						sc->data = *rbuf;
						sc->path = getWriteShareFilePath(qitems.front().path);
						core->get_work_service().post(boost::bind(&sigchecker::main, sc));
					}; break;
					case CMD_REQUEST_PARTIAL_FILE: {
						uint idx = 0;
						uint32 slen = pkt_get_vuint32(*rbuf, idx);
						std::string name(rbuf->begin()+idx, rbuf->begin()+idx+slen);
						idx += slen;
						uint64 poff = pkt_get_vuint64(*rbuf, idx);
						qitem nqi(QITEM_REQUESTED_FILE, getReadShareFilePath(name).string());
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
							boost::bind(&SimpleConnection::command_body_handler, this, _1, rbuf, hdr));
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
				qitems.push_back(qitem(QITEM_REQUEST_SHARE, sharename, 0));
				sharename = getPathElems(sharename).back();

				rresume = core->getSettingsRef()->experimentalresume && rcommands.count(CMD_REQUEST_SIG_FILE) && rcommands.count(CMD_REQUEST_PARTIAL_FILE);
				handle_qitems(tbuf);
			} else if (rcommands.count(CMD_REQUEST_SHARE_DUMP)) {
				// simple style connection
				uint32 nlen = sharename.size();
				tbuf->resize(16 + nlen);
				pkt_put_uint32(CMD_REQUEST_SHARE_DUMP, &((*tbuf)[0]));
				pkt_put_uint32(0, &((*tbuf)[4]));
				pkt_put_uint64(nlen, &((*tbuf)[8]));

				for (uint i = 0; i < nlen; ++i)
					(*tbuf)[i+16] = sharename[i];

				cwdsharepath = "";

				boost::asio::async_write(socket, GETBUF(tbuf),
					boost::bind(&SimpleConnection::start_receive_command, this, tbuf, _1));
			} else {
				disconnect("Remote doesn't support any useful methods");
			}
		}

		void handle_request_listing(shared_vec tbuf)
		{
			std::string name(tbuf->begin(), tbuf->end());
			//std::cout << name << '\n';
			std::vector<std::string> elems = getPathElems(name);

			if (elems.empty()) {
				disconnect("Root listing unsupported...", true);
				return;
			}

			taskinfo.shareinfo.name = platform::makeValidUTF8(name);
			ext::filesystem::path spath(core->getLocalSharePath(elems[0]));
			if (spath.empty() && elems.size() == 2 && elems[0] == AutoUpdaterTag) {
				spath = updateProvider.getUpdateFilepath(elems[1]);
				readsharepath = spath;
				sharename = elems[1];
				taskinfo.path = spath;
				elems[0] = elems[1];
				elems.resize(1);
			} else {
				readsharepath = spath;
				sharename = elems[0];
				taskinfo.path = readsharepath.parent_path(); // TODO
			}

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

			ext::filesystem::path curpath(elems.back());
			if (ext::filesystem::exists(spath)) {
				if (boost::filesystem::is_directory(spath)) {
					//std::cout << "Single directory!\n";

					tbuf->resize(16);
					pkt_put_uint32(CMD_REPLY_TREE_LIST, &((*tbuf)[0]) + 0);
					pkt_put_uint32(0, &((*tbuf)[0]) + 4);

					tbuf->push_back(REQLIST_DIR); // dir
					pkt_put_vuint32(curpath.string().size(), *tbuf);
					for (uint i = 0; i < curpath.string().size(); ++i)
						tbuf->push_back(curpath.string()[i]);

					int curlevel = 0;
					// TODO: move this to a different thread (io thread?)
					ext::filesystem::recursive_directory_iterator curiter(spath);
					ext::filesystem::recursive_directory_iterator enditer;
					for (; curiter != enditer; ++curiter) {
						const ext::filesystem::path iterpath = curiter->path();
						for (; curlevel > curiter.level(); --curlevel)
							curpath = curpath.parent_path();
						curpath /= iterpath.filename();
						++curlevel;
						if (ext::filesystem::exists(iterpath)) {
							if (boost::filesystem::is_directory(iterpath)) {
								tbuf->push_back(REQLIST_DIR); // dir
								pkt_put_vuint32(curpath.string().size(), *tbuf);
								for (uint i = 0; i < curpath.string().size(); ++i)
									tbuf->push_back(curpath.string()[i]);
								//std::cout << "Directory: " << curpath << '\n';
							} else {
								tbuf->push_back(REQLIST_FILE); // file
								taskinfo.size += boost::filesystem::file_size(iterpath);
								pkt_put_vuint64(boost::filesystem::file_size(iterpath), *tbuf);
								pkt_put_vuint32(curpath.string().size(), *tbuf);
								for (uint i = 0; i < curpath.string().size(); ++i)
									tbuf->push_back(curpath.string()[i]);
								//std::cout << "File: " << curpath << '\n';
							}
						}
					}
				} else {
	//				tbuf->clear();
					tbuf->resize(16);
					pkt_put_uint32(CMD_REPLY_TREE_LIST, &((*tbuf)[0]) + 0);
					pkt_put_uint32(0, &((*tbuf)[0]) + 4);

					tbuf->push_back(REQLIST_FILE); // file
					taskinfo.size += boost::filesystem::file_size(spath);
					pkt_put_vuint64(boost::filesystem::file_size(spath), *tbuf);
					pkt_put_vuint32(curpath.string().size(), *tbuf);
					for (uint i = 0; i < curpath.string().size(); ++i)
						tbuf->push_back(curpath.string()[i]);

					//std::cout << "Single file!\n";
				}
			}
			tbuf->push_back(REQLIST_ENDOFLIST); // end of list
			pkt_put_uint64(tbuf->size() - 16, &((*tbuf)[0]) + 8);
			send_receive_packet(tbuf);
		}

		void handle_reply_listing(shared_vec tbuf)
		{
			uint pos = 0;
			while (pos < tbuf->size()) {
				int tp = tbuf->at(pos++);
				switch (tp) {
					case REQLIST_FILE: { // file
						qitem qi(QITEM_REQUEST_FILE, "", pkt_get_vuint64(*tbuf, pos));
						taskinfo.size += qi.fsize;
						uint32 nlen = pkt_get_vuint32(*tbuf, pos);
						for (uint i = 0; i < nlen; ++i)
							qi.path.push_back(tbuf->at(pos++));
						qitems.push_back(qi);
					}; break;
					case REQLIST_DIR: { // dir
						qitem qi(QITEM_REQUEST_DIR, "");
						uint32 nlen = pkt_get_vuint32(*tbuf, pos);
						for (uint i = 0; i < nlen; ++i)
							qi.path.push_back(tbuf->at(pos++));
						ext::filesystem::path dp = getWriteShareFilePath(qi.path);
						boost::filesystem::create_directory(dp);
						//qitems.push_front(qi);
					}; break;
					case REQLIST_ENDOFLIST: { // eol
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

			switch (qitems.front().type) {
				case QITEM_REQUEST_SHARE: {
					qitem& item = qitems.front();
					uint32 nlen = item.path.size();
					tbuf->resize(16 + nlen);
					pkt_put_uint32(CMD_REQUEST_TREE_LIST, &((*tbuf)[0]));
					pkt_put_uint32(0, &((*tbuf)[4]));
					pkt_put_uint64(nlen, &((*tbuf)[8]));

					for (uint i = 0; i < nlen; ++i)
						(*tbuf)[i+16] = item.path[i];

					qitems.pop_front();
					send_receive_packet(tbuf);
				}; break;
				case QITEM_REQUEST_FILE: {
					tbuf->clear();
					qitemsfilled = true;
					for (uint i = 0; i < qitems.size() && qitems[i].type == QITEM_REQUEST_FILE; ++i) {
						qitem& titem = qitems[i];
						size_t cstart = tbuf->size();
						tbuf->resize(tbuf->size()+16);
						ext::filesystem::path itempath = getWriteShareFilePath(titem.path);
						if (rresume && ext::filesystem::exists(itempath) && boost::filesystem::file_size(itempath) > 1024*1024) {
							uint64 fsize = boost::filesystem::file_size(itempath);
							std::vector<uint64> pos;
							uint32 psize;
							{
								// decide on piece size and positions
								uint64 tsize = fsize / 100;
								uint64 pieces = tsize / (1024 * 1024);
								if (tsize < 48*1024) tsize = 48*1024;
								if (pieces < 3) pieces = 3;
								psize = boost::numeric_cast<uint32>(tsize / pieces);

								uint64 start = psize;
								uint64 stop = fsize - psize;
								uint64 range = stop - start;

								pos.push_back(0);
								pos.push_back(stop);
								pieces -= 2;
								boost::uniform_smallint<> distr(0, psize*3);
								boost::variate_generator<boost::rand48&, boost::uniform_smallint<> >
									rand(rng, distr);
								for (int i = 0; i < pieces; ++i) {
									pos.push_back(start + ((range * (i+1)) / (pieces+1)) + rand());
								}

								std::sort(pos.begin(), pos.end());
								titem.pos = pos;
								titem.psize = psize;
							}

							// build packet
							pkt_put_uint32(CMD_REQUEST_SIG_FILE, &((*tbuf)[cstart+0]));
							pkt_put_uint32(0, &((*tbuf)[cstart+4]));
							titem.type = QITEM_REQUESTED_FILESIG; // requested signiature
							titem.poffset = fsize;
							pkt_put_vuint32(titem.path.size(), *tbuf);
							for (uint j = 0; j < titem.path.size(); ++j)
								tbuf->push_back(titem.path[j]);

							pkt_put_vuint32(1, *tbuf);
							pkt_put_vuint32(psize, *tbuf);
							pkt_put_vuint32(pos.size(), *tbuf);
							pkt_put_vuint64(pos[0], *tbuf);
							for (uint i = 1; i < pos.size(); ++i)
								pkt_put_vuint64(pos[i]-pos[i-1], *tbuf);
						} else {
							pkt_put_uint32(CMD_REQUEST_FULL_FILE, &((*tbuf)[cstart+0]));
							pkt_put_uint32(0, &((*tbuf)[cstart+4]));
							titem.type = QITEM_REQUESTED_FILE; // requested file
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
				boost::bind(&SimpleConnection::start_receive_command, this, tbuf, _1));
		}

		void sendpath(ext::filesystem::path path, std::string name = "")
		{
			if (name.empty()) name = path.filename();

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

			sbuf->resize(len);
			cursendfile.fsize -= len;
			//cout << ">async_write(): " << cursendfile.path << " : " << len << " : " << cursendfile.fsize << '\n';
			sendqueue.push_back(sbuf);
			checkwhattosend();
		}

		void update_statistics(uint64 bytes_transferred) {
			boost::posix_time::ptime         now(boost::posix_time::microsec_clock::universal_time());
			boost::posix_time::time_duration time_elapsed(now - taskinfo.last_progress_report);
			taskinfo.transferred                += bytes_transferred; // total
			bytes_transferred_since_last_update += bytes_transferred; // since last update
			if(time_elapsed >= boost::posix_time::time_duration(boost::posix_time::milliseconds(200))) { // Update speed estimate 5 times per second
				double alpha = 0.95;
				taskinfo.speed = (uint64)(
					       alpha  * taskinfo.speed +
					(1.0 - alpha) * ((bytes_transferred_since_last_update*1000000.0) / time_elapsed.total_microseconds())
				);
				bytes_transferred_since_last_update = 0;
				taskinfo.last_progress_report = now;
			}
		}

		void handle_sent_buffer(const boost::system::error_code& e, size_t ofs, size_t len, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_sent_buffer: %s", e.message()));
				return;
			}
			issending = false;
			update_statistics(len);
			size_t bufsize = sbuf->size();
			ofs += len;
			if (ofs == bufsize) {
				checkwhattosend(sbuf);
				return;
			}

			// need to send another chunk of the current buffer

			size_t target_speed = 1024*1024*8; // default to 8MB per buffer //FIXME: why not try to send the whole buffer?
			// we aim for 1/4 of the current bps, so 4 updates per second
			if (taskinfo.speed / 4 <= std::numeric_limits<size_t>::max()) {
				target_speed = (size_t)(taskinfo.speed / 4);
			} //FIXME: On extremely high-speed networks this will limit the max-buffersize to 8MB
			size_t todo = target_speed;
			todo = std::min(todo, (size_t)1024*1024*32); // Be gentle on the memory use, max 32MB buffers
			todo = std::max(todo, (size_t)1024*1); // But transfer at least 1KB
			todo = std::min(todo, bufsize - ofs); // limited by whats left in the current buffer
			//TODO: IDEA: would it be useful to round to the nearest multiple of (MTU-ProtocolHeaderSize)?

			issending = true;
			uint8* bufstart = &((*sbuf)[0]);
			boost::asio::async_write(socket, boost::asio::buffer(bufstart + ofs, todo),
				boost::bind(&SimpleConnection::handle_sent_buffer, this, _1, ofs, _2, sbuf));

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
							service.post(boost::bind(&SimpleConnection::checkwhattosend, this, shared_vec()));
						} else {
							uint64 rsize = cursendfile.fsize;
							if (rsize > 1024*1024*1) rsize = 1024*1024*1;
							if (rsize == 0)
								rsize = 1;
							sbuf->resize((size_t)rsize);
							isreading = true;
							cursendfile.file.async_read_some(GETBUF(sbuf),
								boost::bind(&SimpleConnection::handle_read_file, this, _1, _2, sbuf));
						}
					}
				} else if (newstyle) {
					if (!qitems.empty()) {
						int qtype = qitems.front().type;
						if (qtype == QITEM_REQUESTED_FILE) {
							sbuf->resize(16);
							uint64 off = qitems.front().poffset;
							cursendfile.name = qitems[0].path;
							cursendfile.path = qitems[0].path;
							try {
								cursendfile.init(off);
								cursendfile.hsent = true;
								qitems.pop_front();
								pkt_put_uint32((off == 0) ? CMD_REPLY_FULL_FILE : CMD_REPLY_PARTIAL_FILE, &((*sbuf)[0]));
								pkt_put_uint32(0, &((*sbuf)[4]));
								if(ext::filesystem::exists(cursendfile.path)) {
									pkt_put_uint64(boost::filesystem::file_size(cursendfile.path) - off, &((*sbuf)[8]));
								}
								else {
									pkt_put_uint64(0, &((*sbuf)[8]));
								}
								sendqueue.push_back(sbuf);
								service.post(boost::bind(&SimpleConnection::checkwhattosend, this, shared_vec()));
							} catch (std::exception& e) {
								disconnect(STRFORMAT("checkwhattosend: failed to open file '%s': %s", cursendfile.path, e.what()));
							}
						} else if (qtype == QITEM_REQUESTED_FILESIG) {
							qitems.front().type = QITEM_REQUESTED_FILESIG_BUSY;
							qitems.front().path = getReadShareFilePath(qitems.front().path).string();
							boost::shared_ptr<sigmaker> sm(new sigmaker(service));
							sm->cb = boost::bind(&SimpleConnection::sigmake_done, this, sm, _1);
							sm->item = &qitems.front();
							core->get_work_service().post(boost::bind(&sigmaker::main, sm));
						} else if (qtype == QITEM_REQUESTED_FILESIG_BUSY) {
							// ignore
						} else {
							std::cout << "Unknown item type: " << qtype << '\n';
						}
					}
				} else if (!quesenddir.empty()) {
					ext::filesystem::path newpath;

					bool dirdone = quesenddir.back().getbuf(sbuf, newpath);
					if (dirdone) {
						quesenddir.pop_back();
					} else {
						sendpath(newpath);
					}
					sendqueue.push_back(sbuf);
					service.post(boost::bind(&SimpleConnection::checkwhattosend, this, shared_vec()));
				} else if (!donesend) {
					cmdinfo hdr;
					hdr.cmd = CMD_OLD_DONE; // ..
					hdr.len = 0;
					hdr.ver = 0;
					sbuf->resize(16);
					memcpy(&(*sbuf)[0], &hdr, 16);

					sendqueue.push_back(sbuf);
					donesend = true;
				}
			} // fi (sendqueue.size() < 25)
			if (sendqueue.size() > 0 && !issending) {
				// actually initiates sending
				shared_vec sbuf = sendqueue.front();
				sendqueue.pop_front();
				handle_sent_buffer(boost::system::error_code(), 0, 0, sbuf);
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
			item.type = QITEM_REQUESTED_PARTFILE;
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
				boost::bind(&SimpleConnection::handle_recv_name, this, _1, rbuf));
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
			taskinfo.shareinfo.name = platform::makeValidUTF8(sharename);
			taskinfo.isupload = true;
			readsharepath = core->getLocalSharePath(sharename);
			taskinfo.path = readsharepath.parent_path();
			if (readsharepath == "") {
				shared_vec buildfile = updateProvider.getUpdateBuffer(sharename);
				if (buildfile) {
					std::cout << "is a build share\n";
					std::string name = sharename;
					if (sharename.find("win32") != std::string::npos) name = name + ".exe";
					if (sharename.find("-deb-") != std::string::npos) name = name + ".deb.signed";
					rbuf->resize(16 + name.size());
					cmdinfo hdr;
					hdr.cmd = CMD_OLD_FILE; // file
					hdr.len = buildfile->size();
					hdr.ver = name.size();
					memcpy(&(*rbuf)[0], &hdr, 16);
					memcpy(&(*rbuf)[16], name.data(), hdr.ver);
					boost::asio::async_write(socket, GETBUF(rbuf),
						boost::bind(&SimpleConnection::sent_autoupdate_header, this, _1, buildfile, rbuf));
				} else
					disconnect(STRFORMAT("share does not exist: %s(%s)", readsharepath, sharename));
				return;
			}
			std::cout << "got share path: " << readsharepath << '\n';
			sendpath(readsharepath, sharename);
			checkwhattosend();
		}

		void sent_autoupdate_header(const boost::system::error_code& e, shared_vec buildfile, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("sent_autoupdate_header: %s", e.message()));
				return;
			}
			boost::asio::async_write(socket, GETBUF(buildfile),
						boost::bind(&SimpleConnection::sent_autoupdate_file, this, _1, buildfile, sbuf));
		}

		void sent_autoupdate_file(const boost::system::error_code& e, shared_vec buildfile, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("sent_autoupdate_file: %s", e.message()));
				return;
			}
			cmdinfo hdr;
			hdr.cmd = CMD_OLD_DONE;
			sbuf->resize(16);
			memcpy(&(*sbuf)[0], &hdr, 16);
			boost::asio::async_write(socket, GETBUF(sbuf),
				boost::bind(&SimpleConnection::sent_autoupdate_finish, this, _1, sbuf));
		}

		void sent_autoupdate_finish(const boost::system::error_code& e, shared_vec sbuf) {
			if (e) {
				disconnect(STRFORMAT("sent_autoupdate_finish: %s", e.message()));
				return;
			}
			uldone = true;
		}

		void handle_tcp_connect(std::string name, ext::filesystem::path dlpath, uint32 version)
		{
			sharename = name;
			writesharepath = dlpath / getPathElems(sharename).back();

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
				pkt_put_uint32(CMD_REQUEST_COMMAND_LIST, &((*sbuf)[4]));
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
				boost::bind(&SimpleConnection::start_receive_command, this, sbuf, _1));

			start_update_progress();
		}

		void handle_recv_file_header(const boost::system::error_code& e, cmdinfo hdr, shared_vec rbuf) {
			if (e) {
				disconnect(STRFORMAT("handle_recv_file_header: %s", e.message()));
				return;
			}

			std::string name(&((*rbuf)[0]), &((*rbuf)[0]) + rbuf->size());
			ext::filesystem::path path = getWriteShareFilePath((cwdsharepath / name).string());

			kickoff_file_write(path, hdr.len, rbuf, 0);
		}

		void handle_recv_dir_header(const boost::system::error_code& e, cmdinfo hdr, shared_vec rbuf)
		{
			if (e) {
				disconnect(STRFORMAT("handle_recv_dir_header: %s", e.message()));
				return;
			}

			std::string name(&((*rbuf)[0]), &((*rbuf)[0]) + rbuf->size());

			//cout << "got dir '" << name << "'\n";
			cwdsharepath /= name;
			try {
				boost::filesystem::create_directory(getWriteShareFilePath(cwdsharepath.string()));
			} catch (ext::filesystem::filesystem_error& e) {
				disconnect(STRFORMAT("handle_recv_dir_header: %s", e.what()));
				return;
			}
			start_receive_command(rbuf);
		}

		boost::function<void()> delayed_open_file;

		/** Initiate transfer data from socket to file.
		 *  When all file data is read from the socket,
		 *  start_receive_command will be called
		 *
		 *  @param path Path of the file to write the data to
		 *  @param fsize Amount of data to read from the socket/write to the file
		 *  @param rbuf Buffer to optionally reuse
		 *  @param offset Offset in the file where to start writing
		 *
		 *  The amount of data consumed from the socket is fsize-offset
		 */
		void kickoff_file_write(ext::filesystem::path path, uint64 fsize, shared_vec rbuf, uint64 offset)
		{
			(new coro_receive_into_file(this, path, offset, fsize, rbuf))->start();
		}

		struct coro_receive_into_file: public ext::coro::base<coro_receive_into_file>
		{
			private:
				SimpleConnection* conn;
				ext::filesystem::path path;
				ext::asio::fstream file;
				uint64 offset;
				uint64 bytesleft;
				shared_vec rbuf;
				shared_vec wbuf;
				int forkops;
				bool stopped;
			public:
				coro_receive_into_file(SimpleConnection* conn_, const ext::filesystem::path& path_, uint64 offset_, uint64 fsize_, shared_vec rbuf)
				: conn(conn_), path(path_), file(conn->service), offset(offset_), bytesleft(fsize_), rbuf(rbuf), wbuf(new std::vector<uint8>(0)), stopped(false)
				{};

				void operator()(ext::coro coro, const boost::system::error_code& ec = boost::system::error_code(), size_t /* transferred */ = 0) {
					if (stopped) return;
					if (ec) {
						stopped = true;
						conn->disconnect(STRFORMAT("coro_receive_into_file: %s (%d)", ec.message(), this->stateval(coro)));
						return;
					}
					CORO_REENTER(coro) {
						if (conn->open_files > 256) {
							// if too many files open, yield here until some are closed
							CORO_YIELD conn->delayed_open_file = coro(this);
						}
						++conn->open_files;

						// fork here, child will open file, parent receives data (inside loop)
						forkops = 2;
						CORO_FORK file.async_open(
							path,
							ext::asio::fstream::out | ((offset == 0) ? ext::asio::fstream::create : ext::asio::fstream::in),
							coro(this)
						);
						if (coro.is_child() && offset > 0) file.fseeka(offset);
						for (;;) {
							if (coro.is_parent()) {
								rbuf->resize(conn->getRcvBufSize(bytesleft));
								CORO_YIELD boost::asio::async_read(conn->socket, GETBUF(rbuf), coro(this));
								conn->maxBufSize = std::min(conn->maxBufSize*2, uint32(1024*1024*10));
								bytesleft -= rbuf->size();
								if (bytesleft == 0) conn->start_receive_command();
							} else
								conn->update_statistics(wbuf->size());

							// join here so we have data to write (receive), and the file is ready for writing again (open/write)
							if (--forkops > 0) CORO_BREAK; //join

							// nothing left to receive, break out of loop for final write
							if (bytesleft == 0) break;

							// swap receive/write buffers: buffer we just wrote can be reused for receive, buffer we just received needs to be written
							boost::swap(rbuf, wbuf);

							// fork here, child will write data to file, parent will receive new data (next iteration)
							forkops = 2;
							CORO_FORK boost::asio::async_write(file, GETBUF(wbuf), coro(this));
						}
						// final write of the data we just received
						CORO_YIELD boost::asio::async_write(file, GETBUF(rbuf),  coro(this));
						conn->update_statistics(rbuf->size());
						file.close();

						// notify anyone waiting for files to be closed
						--conn->open_files;
						if (conn->delayed_open_file) {
							// be extra safe and copy the callback locally, so delayed_open_file is cleared before we call it
							boost::function<void()> local_callback;
							local_callback.swap(conn->delayed_open_file);
							local_callback();
						}
					}
				};
		};

		void disconnect(std::string errmsg = "", bool canrespond = false)
		{
			socket.close();
			if (!errmsg.empty()) {
				error_message += errmsg;
				error_message += "; ";
				std::cout << "Error: " << errmsg << '\n';
			}
		}
};

#endif//SIMPLE_CONNECTION_H
