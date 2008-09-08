#ifndef SIMPLE_CONNECTION_H
#define SIMPLE_CONNECTION_H

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/foreach.hpp>

#include "Globals.h"

/// helper classes, TODO: nest them properly...
struct cmdinfo {
	uint32 cmd;
	uint32 ver;
	uint64 len;
};

struct filesender {
	std::string name;
	boost::filesystem::path path;
	services::diskio_filetype file;

	bool hsent;
	uint64 fsize;

	filesender() : file(*gdiskio) {};

	void init() {
		fsize = boost::filesystem::file_size(path);
		file.open(path, services::diskio_filetype::in);
		hsent = false;
		//cout << "<init(): " << path << " : " << fsize << " : " << hsent << '\n';
	};

	bool getbuf(shared_vec buf) {
		if (!hsent) {
			hsent = true;
			buf->resize(16 + name.size());
			cmdinfo hdr;
			hdr.cmd = 0x01; // file
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

		boost::asio::io_service& service;
		boost::asio::ip::tcp::socket socket;

		std::string sharename;
		boost::filesystem::path sharepath;

		filesender cursendfile;
		std::vector<dirsender> quesenddir;

		void getsharepath(std::string sharename);

		bool dldone;
		bool uldone;
		uint32 open_files;

		std::deque<shared_vec> sendqueue;
		bool issending;
		bool donesend;
		bool isreading;

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
			open_files = 0;
			transfered_bytes = 0;
		}

		uint64 transfered_bytes;
		boost::asio::deadline_timer progress_timer;
		boost::signal<void(uint64,std::string,uint32)> sig_progress;

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
				if (dldone && open_files == 0) {
					sig_progress(transfered_bytes, "Completed", 0);
					socket.close();
				} else if (uldone) {
					sig_progress(transfered_bytes, "Completed", 0);
					socket.close();
				} else {
					sig_progress(transfered_bytes, "Transfering", sendqueue.size()+open_files);
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
				std::cout << "error: " << e.message() << '\n';
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
				start_receive_command(rbuf);
			} else
				std::cout << "error: unknown tcp protocol version: " << protver << '\n';
		}

		void start_receive_command(shared_vec rbuf)
		{
			rbuf->resize(16);
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::command_header_handler, this, _1, rbuf));
		}

		void handle_generic_send(const boost::system::error_code& e, shared_vec sbuf)
		{
			if (e)
				std::cout << "error sending: " << e.message() << '\n';
		}

		void command_header_handler(const boost::system::error_code& e, shared_vec rbuf)
		{
			if (e) {
				std::cout << "error receiving command header: " << e.message() << '\n';
				return;
			}

			cmdinfo hdr;
			memcpy(&hdr, &((*rbuf)[0]), 16);

			switch (hdr.cmd) {
				case 1: { // file
					rbuf->resize(hdr.ver);
					boost::asio::async_read(socket, GETBUF(rbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_file_header, this, _1, hdr, rbuf));
				}; break;
				case 2: { // dir
					rbuf->resize(hdr.ver);
					boost::asio::async_read(socket, GETBUF(rbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_dir_header, this, _1, hdr, rbuf));
				}; break;
				case 3: { // ..
					//cout << "got '..'\n";
					sharepath /= "..";
					sharepath.normalize();
					start_recv_header(rbuf);
				}; break;
				case 4: { // ..
					std::cout << "download finished!\n";
					dldone = true;
				}; break;
				case 5: { // request old style share dump
					if (hdr.len < 0xffff) {
						rbuf->resize((uint32)hdr.len); // cast safe because of check above
						boost::asio::async_read(socket, GETBUF(rbuf),
							boost::bind(&SimpleTCPConnection::handle_recv_name, this, _1, rbuf));
					}
				}; break;
				case 6: { // unknown command
					uint32 command;
					boost::asio::read(socket, boost::asio::buffer(&command, 4));
					std::cout << "remote end does not support command: " << command << '\n';
					// do something depending on state?
					start_receive_command(rbuf);
				}; break;
				case -1:
				default: {
					std::cout << "skipping unkown command: " << hdr.cmd << " (" << hdr.len << ")\n";
					command_skip_handler(boost::system::error_code(), rbuf, 0, hdr.len);

					shared_vec sbufp = shared_vec(new std::vector<uint8>());
					sbufp->resize(20);
					std::vector<uint8>& sbuf = *sbufp;
					sbuf[ 0] = 0x06; sbuf[ 1] = 0x00; sbuf[ 2] = 0x00; sbuf[ 3] = 0x00;
					sbuf[ 8] = 0x04; sbuf[ 9] = 0x00; sbuf[10] = 0x00; sbuf[11] = 0x00;
					sbuf[12] = 0x00; sbuf[13] = 0x00; sbuf[14] = 0x00; sbuf[15] = 0x00;
					sbuf[16] = (hdr.cmd >>  0) & 0xFF;
					sbuf[17] = (hdr.cmd >>  8) & 0xFF;
					sbuf[18] = (hdr.cmd >> 16) & 0xFF;
					sbuf[19] = (hdr.cmd >> 24) & 0xFF;
					boost::asio::async_write(socket, GETBUF(sbufp),
						boost::bind(&SimpleTCPConnection::handle_generic_send, this, _1, sbufp));
				};
			}
		}

		void command_skip_handler(const boost::system::error_code& e, shared_vec rbuf, uint32 chunk, uint64 size)
		{
			if (e) {
				std::cout << "error skipping unknown command: " << e.message() << '\n';
				return;
			}

			size -= chunk;
			if (size != 0) {
				if (size > 1024*1024)
					chunk = 1024*1024;
				else
					chunk = (uint32)size; // cast is ok because of if check 3 lines above
				rbuf->resize(chunk);
				boost::asio::async_read(socket, GETBUF(rbuf),
					boost::bind(&SimpleTCPConnection::command_skip_handler, this, _1, rbuf, chunk, size));
			} else
				start_receive_command(rbuf);
		}

		void sendpath(boost::filesystem::path path, std::string name = "")
		{
			if (name.empty()) name = path.leaf();

			if (!boost::filesystem::exists(path))
				std::cout << "path does not exist: " << path << '\n';

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
					std::cout << "file read error: " << e.message() << '\n';
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
				std::cout << "error sending buffer: " << e.message() << '\n';
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

		void handle_sent_everything() {
			uldone = true;
		}

		void handle_recv_namelen(const boost::system::error_code& e, shared_vec rbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
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
				std::cout << "error: " << e.message() << '\n';
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
			getsharepath(sharename);
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
					std::cout << "share does not exist\n";
				return;
			}
			std::cout << "got share path: " << sharepath << '\n';
			sendpath(sharepath, sharename);
			checkwhattosend();
		}

		void sent_autoupdate_header(const boost::system::error_code& e, shared_vec buildfile, shared_vec sbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}
			boost::asio::async_write(socket, GETBUF(buildfile),
						boost::bind(&SimpleTCPConnection::sent_autoupdate_file, this, _1, buildfile, sbuf));
		}

		void sent_autoupdate_file(const boost::system::error_code& e, shared_vec buildfile, shared_vec sbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}
			cmdinfo hdr;
			hdr.cmd = 0x04;
			sbuf->resize(16);
			memcpy(&(*sbuf)[0], &hdr, 16);
			boost::asio::async_write(socket, GETBUF(sbuf),
				boost::bind(&SimpleTCPConnection::sent_autoupdate_finish, this, _1, sbuf));
		}

		void sent_autoupdate_finish(const boost::system::error_code& e, shared_vec sbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
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
				/*
				sbuf->push_back(0x07); sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00);
				sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00);
				sbuf->push_back((namelen >>  0)&0xff);
				sbuf->push_back((namelen >>  8)&0xff);
				sbuf->push_back((namelen >> 16)&0xff);
				sbuf->push_back((namelen >> 24)&0xff);
				sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00);
				for (uint i = 0; i < namelen; ++i)
					sbuf->push_back(name[i]);
					*/
				sbuf->push_back(0x05); sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00);
				sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00);
				sbuf->push_back((namelen >>  0)&0xff);
				sbuf->push_back((namelen >>  8)&0xff);
				sbuf->push_back((namelen >> 16)&0xff);
				sbuf->push_back((namelen >> 24)&0xff);
				sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00); sbuf->push_back(0x00);
				for (uint i = 0; i < namelen; ++i)
					sbuf->push_back(name[i]);
			} else
				std::cout << "unknown version connecting: " << version << '\n';

			boost::asio::async_write(socket, GETBUF(sbuf),
				boost::bind(&SimpleTCPConnection::handle_sent_name, this, _1, sbuf));

			start_update_progress();
		}

		void handle_sent_name(const boost::system::error_code& e, shared_vec sbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}
			std::cout << "sent share name\n";
			start_recv_header(sbuf);
		}

		void start_recv_header(shared_vec rbuf) {
			rbuf->resize(16);
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_header, this, _1, rbuf));
		}

		void handle_recv_header(const boost::system::error_code& e, shared_vec rbuf) {
			command_header_handler(e, rbuf);
		}

		void handle_recv_file_header(const boost::system::error_code& e, cmdinfo hdr, shared_vec rbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}

			std::string name(&((*rbuf)[0]), &((*rbuf)[0]) + rbuf->size());

			bool* done = new bool;
			*done = false;

			++open_files;
			// kick off handle_ready_file for when the file is ready to write
			boost::shared_ptr<services::diskio_filetype> file(new services::diskio_filetype(*gdiskio));
			gdiskio->async_open_file((sharepath / name), services::diskio_filetype::out|services::diskio_filetype::create,
				*file,
				boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, hdr.len, rbuf, rbuf));

			// kick off async_read for when we received some data (capped by file size)
			rbuf->resize((hdr.len>1024*1024*10) ? 1024*1024*10 : (uint32)hdr.len); // cast is ok because of ?:
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_file, this, _1, file, done, hdr.len, rbuf));

		}

		void handle_recv_dir_header(const boost::system::error_code& e, cmdinfo hdr, shared_vec rbuf) {
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}

			std::string name(&((*rbuf)[0]), &((*rbuf)[0]) + rbuf->size());

			//cout << "got dir '" << name << "'\n";
			sharepath /= name;
			boost::filesystem::create_directory(sharepath);
			start_recv_header(rbuf);
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
				std::cout << "error: " << e.message() << '\n';
				return;
			}

			transfered_bytes += wbuf->size();
			size -= wbuf->size();

			if (!*done) {
				*done = true;
				if (size == 0)
					start_recv_header(shared_vec(new std::vector<uint8>()));
			} else {
				shared_vec nrbuf;
				if (size == 0) {
					*done = true;
					start_recv_header(shared_vec(new std::vector<uint8>()));
				} else {
					*done = false;
					nrbuf = shared_vec(new std::vector<uint8>());
					nrbuf->resize((size>1024*1024*10) ? 1024*1024*10 : (uint32)size); // cast is ok because of ?:
					boost::asio::async_read(socket, GETBUF(nrbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_file, this, _1, file, done, size, nrbuf));
				};
				boost::asio::async_write(*file, GETBUF(wbuf),
					boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, size, nrbuf, wbuf));
			}
		}

		/** handle file ready to write
		 *  @param e       an error occured
		 *  @param done    true if there is data ready to write (except if size==0, then we close the file)
		 *  @param size    amount of bytes left to write for the file
		 *  @param wbuf    the buffer from where we will write the data
		 *  @param curbuf  the buffer containing previous data (unused)
		 */
		void handle_ready_file(const boost::system::error_code& e, boost::shared_ptr<services::diskio_filetype> file, bool* done, uint64 size, shared_vec wbuf, shared_vec curbuf)
		{
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}

			if (!*done) {
				*done = true;
			} else {
				if (size == 0) {
					delete done;
					file->close();
					--open_files;
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
					boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, size, nrbuf, wbuf));
			}
		}
};
typedef boost::shared_ptr<SimpleTCPConnection> SimpleTCPConnectionRef;

#endif//SIMPLE_CONNECTION_H

