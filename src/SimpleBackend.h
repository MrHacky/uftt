#include "Types.h"

#define UFTT_PORT (47189)

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/foreach.hpp>

#include "net-asio/asio_file_stream.h"

#include "AutoUpdate.h"

/*** evil globals here, TODO: remove them all! ***/
extern services::diskio_service* gdiskio;
extern AutoUpdater updateProvider;
extern std::string thebuildstring;

/// helper classes, TODO: nest them properly...
struct header {
	uint32 type;
	uint32 nlen;
	uint64 size;
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
			header hdr;
			hdr.type = 0x01; // file
			hdr.size = fsize;
			hdr.nlen = name.size();
			memcpy(&(*buf)[0], &hdr, 16);
			memcpy(&(*buf)[16], name.data(), hdr.nlen);
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
			int x = sizeof(header);
			hsent = true;
			buf->resize(16 + name.size());
			header hdr;
			hdr.type = 0x02; // dir
			hdr.size = 0;
			hdr.nlen = name.size();
			memcpy(&(*buf)[0], &hdr, 16);
			memcpy(&(*buf)[16], name.data(), hdr.nlen);
		} else
			buf->clear();

		if (curiter == enditer) {
			header hdr;
			hdr.type = 0x03; // ..
			hdr.size = 0;
			hdr.nlen = 0;
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
		void sig_download_ready(std::string url);

		bool dldone;
		bool uldone;
		uint32 open_files;

		std::deque<shared_vec> sendqueue;
		bool issending;
		bool donesend;
		bool isreading;

	public:
		SimpleTCPConnection(boost::asio::io_service& service_, SimpleBackend* backend_)
			: service(service_)
			, socket(service_)
			, progress_timer(service_)
			, backend(backend_)
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
				} else if (uldone) {
					sig_progress(transfered_bytes, "Completed", 0);
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

			if (protver != 1) {
				std::cout << "error: unknown tcp protocol version: " << protver << '\n';
				return;
			}

			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_namelen, this, _1, rbuf));
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
					header hdr;
					hdr.type = 0x04; // ..
					hdr.size = 0;
					hdr.nlen = 0;
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
			socket.close();
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
			std::cout << "got share name: " << sharename << '\n';
			getsharepath(sharename);
			if (sharepath == "") {
				shared_vec buildfile = updateProvider.getBuildExecutable(sharename);
				if (buildfile) {
					std::cout << "is a build share\n";
					std::string name = sharename + ".exe";
					rbuf->resize(16 + name.size());
					header hdr;
					hdr.type = 0x01; // file
					hdr.size = buildfile->size();
					hdr.nlen = name.size();
					memcpy(&(*rbuf)[0], &hdr, 16);
					memcpy(&(*rbuf)[16], name.data(), hdr.nlen);
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
			header hdr;
			hdr.type = 0x04;
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
			socket.close();
		}

		void handle_tcp_connect(std::string name, boost::filesystem::path dlpath)
		{
			sharename = name;
			sharepath = dlpath;

			shared_vec sbuf(new std::vector<uint8>());

			sbuf->push_back(1); // protocol version
			sbuf->push_back(0); // protocol version
			sbuf->push_back(0); // protocol version
			sbuf->push_back(0); // protocol version

			uint32 namelen = name.size();
			sbuf->push_back((namelen >>  0)&0xff);
			sbuf->push_back((namelen >>  8)&0xff);
			sbuf->push_back((namelen >> 16)&0xff);
			sbuf->push_back((namelen >> 24)&0xff);

			for (uint i = 0; i < namelen; ++i)
				sbuf->push_back(name[i]);

			boost::asio::async_write(socket, GETBUF(sbuf),
				boost::bind(&SimpleTCPConnection::handle_sent_name, this, _1, sbuf));
			//boost::asio::async_write(socket,
			//socket.async_write(

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
			if (e) {
				std::cout << "error: " << e.message() << '\n';
				return;
			}

			header hdr;
			memcpy(&hdr, &((*rbuf)[0]), 16);
			switch (hdr.type) {
				case 1: { // file
					rbuf->resize(hdr.nlen);
					boost::asio::async_read(socket, GETBUF(rbuf),
						boost::bind(&SimpleTCPConnection::handle_recv_file_header, this, _1, hdr, rbuf));
				}; break;
				case 2: { // dir
					rbuf->resize(hdr.nlen);
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
					if (dldone && open_files == 0) {
						this->sig_download_ready(
							std::string("uftt://")
							+socket.remote_endpoint().address().to_string()
							+"/"+sharename
						);
						socket.close(); // do this after the sig so the endpoint is still valid
					}
				}; break;
			}
		}

		void handle_recv_file_header(const boost::system::error_code& e, header hdr, shared_vec rbuf) {
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
				boost::bind(&SimpleTCPConnection::handle_ready_file, this, _1, file, done, hdr.size, rbuf, rbuf));

			// kick off async_read for when we received some data (capped by file size)
			rbuf->resize((hdr.size>1024*1024*10) ? 1024*1024*10 : (uint32)hdr.size); // cast is ok because of ?:
			boost::asio::async_read(socket, GETBUF(rbuf),
				boost::bind(&SimpleTCPConnection::handle_recv_file, this, _1, file, done, hdr.size, rbuf));

		}

		void handle_recv_dir_header(const boost::system::error_code& e, header hdr, shared_vec rbuf) {
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

					// wtf hax!!!
					if (dldone && open_files == 0) {
						this->sig_download_ready(
							std::string("uftt://")
							+socket.remote_endpoint().address().to_string()
							+"/"+sharename
						);
						socket.close(); // do this after the sig so the endpoint is still valid
					}

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

class SimpleBackend {
	private:
		boost::asio::io_service service;
		services::diskio_service diskio;
		boost::asio::ip::udp::socket udpsocket;
		boost::asio::ip::tcp::acceptor tcplistener;
		SimpleTCPConnectionRef newconn;
		std::vector<SimpleTCPConnectionRef> conlist;
		std::map<std::string, boost::filesystem::path> sharelist;

		boost::thread servicerunner;

		void servicerunfunc() {
			boost::asio::io_service::work wobj(service);
			service.run();
		}

		void start_tcp_accept() {
			newconn = SimpleTCPConnectionRef(new SimpleTCPConnection(service, this));
			tcplistener.async_accept(newconn->socket,
				boost::bind(&SimpleBackend::handle_tcp_accept, this, boost::asio::placeholders::error));
		}

		void handle_tcp_accept(const boost::system::error_code& e) {
			if (e) {
				std::cout << "tcp accept failed: " << e.message() << '\n';
			} else {
				std::cout << "handling tcp accept\n";
				conlist.push_back(newconn);
				newconn->handle_tcp_accept();
				sig_new_upload("upload", conlist.size()-1);
				start_tcp_accept();
			}
		}

		uint8 udp_recv_buf[1024];
		boost::asio::ip::udp::endpoint udp_recv_addr;
		void start_udp_receive() {
			udpsocket.async_receive_from(boost::asio::buffer(udp_recv_buf), udp_recv_addr,
				boost::bind(&SimpleBackend::handle_udp_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
			);
		}

		void handle_udp_receive(const boost::system::error_code& e, std::size_t len) {
			if (!e) {
				if (len >= 4) {
					uint32 rpver =
						(udp_recv_buf[0] <<  0) |
						(udp_recv_buf[1] <<  8) |
						(udp_recv_buf[2] << 16) |
						(udp_recv_buf[3] << 24);
					if (rpver == 1 && len >= 5) {
						std::cout << "got packet type " << (int)udp_recv_buf[4] << " from " << udp_recv_addr << "\n";
						switch (udp_recv_buf[4]) {
							case 1: { // type = broadcast;
								send_publish(udp_recv_addr, len >= 6 && udp_recv_buf[5] > 0 && len-6 >= udp_recv_buf[5]);
							}; break;
							case 2: { // type = reply;
								if (len >= 6) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
										std::string surl;
										surl += "uftt://";
										surl += udp_recv_addr.address().to_string();
										surl += '/';
										surl += sname;
										sig_new_share(surl);
									}
								}
							}; break;
							case 3: { // type = autoupdate share
								if (len >= 6) {
									uint32 slen = udp_recv_buf[5];
									if (len >= slen+6) {
										std::string sname((char*)udp_recv_buf+6, (char*)udp_recv_buf+6+slen);
										std::string surl;
										surl += "uftt://";
										surl += udp_recv_addr.address().to_string();
										surl += '/';
										surl += sname;
										sig_new_autoupdate(surl);
									}
								}
							}; break;
						}
					}
				}
			} else
				std::cout << "udp receive failed: " << e.message() << '\n';

			if (!e)
				start_udp_receive();
		}

		void send_query(boost::asio::ip::udp::endpoint ep) {
			uint8 udp_send_buf[1024];
			uint32 udp_protocol_version = 1;
			udp_send_buf[0] = (udp_protocol_version >>  0) & 0xff;
			udp_send_buf[1] = (udp_protocol_version >>  8) & 0xff;
			udp_send_buf[2] = (udp_protocol_version >> 16) & 0xff;
			udp_send_buf[3] = (udp_protocol_version >> 24) & 0xff;

			udp_send_buf[4] = 1; // type = broadcast;

			uint32 bslen = thebuildstring.size();
			if (bslen > 0xff) bslen = 0;
			udp_send_buf[5] = (uint8)bslen; // type = broadcast;
			for (uint i = 0; i < bslen; ++i)
				udp_send_buf[6+i] = thebuildstring[i];

			boost::system::error_code err;
			if (ep.address() != boost::asio::ip::address_v4::broadcast())
				udpsocket.send_to(
					boost::asio::buffer(udp_send_buf, 6+bslen),
					ep,
					0
					,err
				);
			else
				this->send_udp_broadcast(udpsocket, boost::asio::buffer(udp_send_buf, 6+bslen), ep.port(), 0, err);

			if (err)
				std::cout << "query failed: " << err.message() << '\n';
		}

		void send_publish(const boost::asio::ip::udp::endpoint& ep, const std::string& name, bool isbuild)
		{
			uint8 udp_send_buf[1024];
			memcpy(udp_send_buf, udp_recv_buf, 4);
			udp_send_buf[4] = isbuild ? 3 : 2;
			udp_send_buf[5] = name.size();
			memcpy(&udp_send_buf[6], name.data(), name.size());

			boost::system::error_code err;
			if (ep.address() != boost::asio::ip::address_v4::broadcast())
				udpsocket.send_to(
					boost::asio::buffer(udp_send_buf, name.size()+6),
					ep,
					0,
					err
				);
			else
				this->send_udp_broadcast(udpsocket, boost::asio::buffer(udp_send_buf, name.size()+6), ep.port(), 0, err);
			if (err)
				std::cout << "publish of '" << name << "' to '" << ep << "'failed: " << err.message() << '\n';
		}

		void send_publish(boost::asio::ip::udp::endpoint ep, bool buildstoo) {
			typedef std::pair<const std::string, boost::filesystem::path> shareiter;
			BOOST_FOREACH(shareiter& item, sharelist)
				if (item.first.size() < 0xff && !item.second.empty())
					send_publish(ep, item.first, false);

			if (buildstoo)
				BOOST_FOREACH(const std::string& name, updateProvider.getAvailableBuilds())
					send_publish(ep, name, true);
		}

		void add_local_share(std::string name, boost::filesystem::path path)
		{
			if (sharelist[name] != "")
				std::cout << "warning: replacing share with identical name\n";
			sharelist[name] = path;

			send_publish(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT), name, false);
		}

		void download_share(std::string shareurl, boost::filesystem::path dlpath, boost::function<void(uint64,std::string,uint32)> handler)
		{
			shareurl.erase(0, 7);
			size_t slashpos = shareurl.find_first_of("\\/");
			std::string host = shareurl.substr(0, slashpos);
			std::string share = shareurl.substr(slashpos+1);
			SimpleTCPConnectionRef newconn(new SimpleTCPConnection(service, this));
			newconn->sig_progress.connect(handler);
			conlist.push_back(newconn);
			boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(host), UFTT_PORT);
			newconn->socket.open(ep.protocol());
			std::cout << "Connecting...\n";
			newconn->socket.async_connect(ep, boost::bind(&SimpleBackend::dl_connect_handle, this, _1, newconn, share, dlpath));
		}

		void dl_connect_handle(const boost::system::error_code& e, SimpleTCPConnectionRef conn, std::string name, boost::filesystem::path dlpath)
		{
			if (e) {
				std::cout << "connect failed: " << e.message() << '\n';
			} else {
				std::cout << "Connected!\n";
				conn->handle_tcp_connect(name, dlpath);
			}
		}

		std::vector<boost::asio::ip::address> get_broadcast_adresses();

		template<typename BUF>
		void send_udp_broadcast(boost::asio::ip::udp::socket& sock, BUF buf, uint16 port, int flags, boost::system::error_code& err)
		{
			std::vector<boost::asio::ip::address> adresses;
			adresses = this->get_broadcast_adresses();
			BOOST_FOREACH(boost::asio::ip::address& addr, adresses) {
				udpsocket.send_to(
					buf,
					boost::asio::ip::udp::endpoint(addr, port),
					flags,
					err
				);
				if (err)
					std::cout << "broadcast to (" << addr << ") failed: " << err.message() << '\n';
			}
		}
		void attach_progress_handler(int num, boost::function<void(uint64,std::string,uint32)> handler)
		{
			conlist[num]->sig_progress.connect(handler);
		}

	public:
		SimpleBackend()
			: udpsocket(service)
			, tcplistener(service)
			, diskio(service)
		{
			gdiskio = &diskio;
			udpsocket.open(boost::asio::ip::udp::v4());
			udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), UFTT_PORT));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			tcplistener.open(boost::asio::ip::tcp::v4());
			tcplistener.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address(), UFTT_PORT));
			tcplistener.listen(16);

			// bind autoupdater
			updateProvider.newbuild.connect(
				boost::bind(&SimpleBackend::send_publish, this, boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT), _1, true)
			);

			start_udp_receive();
			start_tcp_accept();

			boost::thread tt(boost::bind(&SimpleBackend::servicerunfunc, this));
			servicerunner.swap(tt);
		}

		void getsharepath(SimpleTCPConnection* conn, std::string name) {
			conn->sharepath = sharelist[name];
		}

		// the main public interface starts here...
		boost::signal<void(std::string)> sig_new_share;
		boost::signal<void(std::string,int)> sig_new_upload;
		boost::signal<void(std::string)> sig_new_autoupdate;
		boost::signal<void(std::string)> sig_download_ready;

		void slot_refresh_shares()
		{
			service.post(boost::bind(&SimpleBackend::send_query, this, boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), UFTT_PORT)));
		}

		void slot_add_local_share(std::string name, boost::filesystem::path path)
		{
			service.post(boost::bind(&SimpleBackend::add_local_share, this, name, path));
		}

		void slot_download_share(std::string shareurl, boost::filesystem::path dlpath, boost::function<void(uint64,std::string,uint32)> handler)
		{
			service.post(boost::bind(&SimpleBackend::download_share, this, shareurl, dlpath, handler));
		}

		void slot_attach_progress_handler(int num, boost::function<void(uint64,std::string,uint32)> handler)
		{
			service.post(boost::bind(&SimpleBackend::attach_progress_handler, this, num, handler));
		}

		void do_manual_query(std::string hostname) {
			service.post(boost::bind(&SimpleBackend::send_query, this,
				boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(hostname), UFTT_PORT)
			));
		}

		void do_manual_publish(std::string hostname) {
			service.post(boost::bind(&SimpleBackend::send_publish, this,
				boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(hostname), UFTT_PORT)
			, true));
		}
};
