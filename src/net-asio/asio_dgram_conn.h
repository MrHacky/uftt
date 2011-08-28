#ifndef ASIO_DGRAM_CONN_H
#define ASIO_DGRAM_CONN_H

#include <queue>

#include <boost/asio.hpp>
#include <boost/utility.hpp>
#include <boost/bind.hpp>

#include "../Types.h"
#include "../util/asio_timer_oneshot.h"

namespace dgram {
	namespace timeout {
		const boost::posix_time::time_duration resend     = boost::posix_time::milliseconds(500);          // 500ms
		const boost::posix_time::time_duration giveup     = boost::posix_time::minutes(1);                 // 1min
		const boost::posix_time::time_duration conn_reuse = boost::posix_time::minutes(10);                // 10min
	}

	struct packet {
		enum constants {
			headersize = 4*4,
			//maxmtu = 2048,
			maxmtu = 1536,
			sendmtu = 1400,
			datasize = maxmtu-headersize,
			signiature = 0x02,
		};

		enum flags_enum {
			flag_ack  = 1 << 0,
			flag_psh  = 1 << 1, // do we need this?
			flag_rst  = 1 << 2,
			flag_syn  = 1 << 3,
			flag_fin  = 1 << 4,
			flag_ping = 1 << 5, // packet has no data, but sequence number should be increased by 1
			flag_oob  = 1 << 6, // packet wants no acknowledgement
		};

		// actual binary data in packet
		uint8 header[4]; // includes flags, recvqid, and magic header signiature
		uint16 sendqid;  // senders qid
		uint16 window;
		uint32 seqnum;
		uint32 acknum;
		uint8 data[datasize];

		// calculated fields
		uint16 datalen;
		uint16 recvqid;  // recvers qid
		uint8 flags;
	};

	// item waiting to be sent
	struct snditem {
		const void* data;
		size_t len;
		boost::function<void(const boost::system::error_code&, size_t len)> handler;

		snditem(const void* data_, size_t len_,  boost::function<void(const boost::system::error_code&, size_t len)> handler_)
			: data(data_), len(len_), handler(handler_) {};
	};

	// item waiting for receiving data
	struct rcvitem {
		void* data;
		size_t len;
		boost::function<void(const boost::system::error_code&, size_t len)> handler;

		rcvitem(void* data_, size_t len_,  boost::function<void(const boost::system::error_code&, size_t len)> handler_)
			: data(data_), len(len_), handler(handler_) {};
	};

	struct null_handler {
		void operator()(const  boost::system::error_code& ec, size_t len) {};

		typedef void result_type;
	};

	template<typename Proto>
	class conn_service;

	template<typename Proto>
	class conn;

	template<typename Proto>
	class conn_impl : private boost::noncopyable {
		private:
			friend class conn_service<Proto>;
			friend class conn<Proto>;

			enum state_enum {
				closed = 0,
				listening,
				synsent,
				synreceived,
				established,
				closing,
				timewait,
				reuse,
				//finwait1,
				//finwait2,
				//lastack,
			};

			enum retflag {
				rf_sendpack,
			};

			boost::asio::io_service& service;
			conn_service<Proto>* cservice;

			// small receive buffer
			uint8 rdata[packet::maxmtu];
			size_t rdsize; // size of data in buffer
			size_t rdoff;  // offset of unused data in buffer

			typename Proto::endpoint endpoint;

			packet sendpack;
			packet sendpackonce;

			int32 state;
			bool fin_rcv;
			bool fin_snd;
			bool fin_ack;

			bool hashandle;

			uint16 lqid;
			uint16 rqid;

			uint32 snd_una; // oldest unacknowledged sequence number
			uint32 snd_nxt; // next sequence number to be sent
			uint32 snd_wnd; // remote receiver window size

			uint32 rcv_nxt; // next sequence number expected on an incoming segment
			uint16 rcv_wnd; // receiver window size

			boost::asio::deadline_timer short_timer;
			boost::asio::deadline_timer long_timer;
			bool resending;

			std::deque<snditem> send_queue;
			std::deque<rcvitem> recv_queue;

			// stats
			uint32 resend;
			uint32 send_undelivered;

			bool cansend()
			{
				return (state == listening)
				    || (state == synsent)
				    || (state == synreceived)
				    || (state == established);
			}

			bool canreceive()
			{
				if (fin_rcv)
					return false;

				return (state == listening)
				    || (state == synsent)
				    || (state == synreceived)
				    || (state == established)
				    || (state == closing);
			}
		public:
			boost::asio::io_service& get_io_service() {
				return service;
			}

			// functions
			void initsendpack(packet& pack, uint16 datalen = 0, uint8 flags = packet::flag_ack) {
				pack.recvqid = rqid;
				pack.sendqid = lqid;
				pack.flags = flags;
				pack.seqnum = snd_una;
				pack.acknum = rcv_nxt;
				pack.window = rcv_wnd; // - received packets in queue
				pack.datalen = datalen; // default

				if (fin_snd)
					pack.flags |= packet::flag_fin;

				pack.header[0] = (pack.recvqid >> 8) & 0xff;
				pack.header[1] = (pack.recvqid >> 0) & 0xff;
				pack.header[2] = pack.flags;
				pack.header[3] = packet::signiature;
			}

			int check_queues()
			{
				int ret = 0;
				ret |= check_recv_queues();
				ret |= check_send_queues();

				if (fin_rcv && fin_snd && fin_ack && state != timewait) {
					setstate(timewait);
				}
				return ret;
			}

			int check_send_queues()
			{
				int ret = 0;
				if (snd_una == snd_nxt && !send_queue.empty()) {
					snditem& front = send_queue.front();
					size_t trylen = front.len;
					if (trylen > packet::sendmtu)
						trylen = packet::sendmtu;
					initsendpack(sendpack, (uint16)trylen); // mtu always < 0xffff
					memcpy(sendpack.data,  front.data, trylen);
					sendpack.seqnum = snd_nxt++;
					send_packet();
					ret |= rf_sendpack;
					front.data = ((char*)front.data) + trylen;
					front.len -= trylen;
					send_undelivered += trylen;
					if (front.len == 0) {
						if (front.handler) {
							cservice->service.dispatch(boost::bind(front.handler, boost::system::error_code(), send_undelivered));
							send_undelivered = 0;
						}
						send_queue.pop_front();
					}
				}
				if (snd_una == snd_nxt && send_queue.empty() && state == closing && !fin_snd) {
					fin_snd = true;
					initsendpack(sendpack);
					sendpack.seqnum = snd_nxt++;
					send_packet();
					ret |= rf_sendpack;
				}

				if (state != closing && !cansend()) {
					while (!send_queue.empty()) {
						snditem front = send_queue.front();
						send_queue.pop_front();
						if (front.handler) {
							cservice->service.dispatch(boost::bind(front.handler,
								boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted),
								0
							));
						}
					}
				}
				return ret;
			}

			int check_recv_queues()
			{
				int ret = 0;
				while (rdsize != 0 && !recv_queue.empty()) {
					#undef min
					size_t len = std::min(recv_queue.front().len, rdsize-rdoff);
					memcpy(recv_queue.front().data, rdata+rdoff, len);

					rdoff += len;

					if (rdsize == rdoff) {
						rdsize = 0;
						++rcv_nxt;
						initsendpack(sendpackonce);
						send_packet_once();
					}

					rcvitem ritem = recv_queue.front();
					recv_queue.pop_front();
					cservice->service.dispatch(boost::bind(ritem.handler, boost::system::error_code(), len));
				}

				if (rdsize == 0 && !canreceive()) {
					while (!recv_queue.empty()) {
						rcvitem ritem = recv_queue.front();
						recv_queue.pop_front();
						cservice->service.dispatch(boost::bind(ritem.handler, boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted), 0));
					};
					if (handler) {
						cservice->service.dispatch(boost::bind(handler, boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted)));
						handler.clear();
					}
				}
				return ret;
			}

			void send_packet_once() {
				send_packet_once(sendpackonce);
			}

			void send_packet_once(packet& pack) {
				//mcout(9) << STRFORMAT("[%3d,%3d] ", pack.sendid, pack.recvid) << "send udp packet: " << pack.type << "  len:" << pack.len << "\n";
				//socket->send_to(boost::asio::buffer(&pack, ipx_packet::headersize+pack.datalen), endpoint);
				try {
					cservice->socket.send_to(boost::asio::buffer(&pack, packet::headersize+pack.datalen), endpoint);
				} catch (boost::system::system_error& error) {
					if (handler) {
						cservice->service.dispatch(boost::bind(handler, error.code()));
						handler.clear();
					}
					// should clear these handler too?
					if (!recv_queue.empty()) cservice->service.dispatch(boost::bind(recv_queue.front().handler, error.code(), 0));
					if (!send_queue.empty()) cservice->service.dispatch(boost::bind(recv_queue.front().handler, error.code(), 0));
				}
				//udp_sock->async_send_to(boost::asio::buffer(&sendpack, 4*5), udp_addr, boost::bind(&ignore));
			}

			void send_packet(const boost::system::error_code& error = boost::system::error_code(), bool newresend = true)
			{
				if (newresend) resending = true;
				if (!error && resending) {
					send_packet_once(sendpack);

					short_timer.expires_from_now(timeout::resend);
					short_timer.async_wait(boost::bind(&conn_impl<Proto>::send_packet, this, boost::asio::placeholders::error, false));
					++resend; // count all packets we send while using a timeout
				} else
					--resend; // and subtract all canceled timeouts
			}

			// for when a connection is established
			boost::function<void(const boost::system::error_code&)> handler;

			void handle_read(packet* pack, size_t len)
			{
				snd_wnd = pack->window;
				switch (state) {
					case closed: {
					}; break;
					case listening: {
						if (pack->flags & packet::flag_syn) {
							snd_nxt = snd_una+1;
							setstate(synreceived);
							initsendpack(sendpack, 0, packet::flag_syn | packet::flag_ack);
							send_packet();
						}
					}; break;
					case synsent:
					case synreceived: {
						if (pack->flags & packet::flag_ack && pack->acknum == snd_una+1) {
							++snd_una;
							initsendpack(sendpackonce);
							send_packet_once();
							setstate(established);
							cservice->service.dispatch(boost::bind(handler, boost::system::error_code()));
							handler.clear();
							check_queues();
						}
					}; break;
					case established:
					case closing: {
						if (pack->seqnum == rcv_nxt) {
							if (pack->datalen > 0 && !fin_rcv) {
								// there is new data in the packet
								if (!recv_queue.empty() && rdsize == 0) {
									// we have a receiver waiting and no more data buffered
									if (recv_queue.front().len < pack->datalen) {
										// the packet contains more data than the receiver requested, so we buffer it
										memcpy(rdata, pack->data, pack->datalen);
										rdsize = pack->datalen;
										rdoff = 0;
									} else {
										// the receiver requested more data than the packet contains, so pass it directly
										memcpy(recv_queue.front().data, pack->data, pack->datalen);
										++rcv_nxt;
										cservice->service.dispatch(boost::bind(recv_queue.front().handler, boost::system::error_code(), pack->datalen));
										recv_queue.pop_front();
									}
								}
							}
							if (rdsize == 0 && pack->flags & packet::flag_fin) {
								fin_rcv = true;
								++rcv_nxt;
							}
						}
						if (pack->flags & packet::flag_ack && pack->acknum == snd_una+1) {
							// the packed acks previously unacked data
							++snd_una;
							if (fin_snd) fin_ack = true;
							setstate(state); // clears send timeouts
						}
						int check = check_queues();
						if (!(check & rf_sendpack) && pack->seqnum != rcv_nxt) {
							// sender could use new ack packet
							initsendpack(sendpackonce);
							send_packet_once();
						}
					}; break;
					case timewait: {
						if (fin_rcv && pack->seqnum != rcv_nxt) {
							// sender could use new ack packet
							initsendpack(sendpackonce);
							send_packet_once();
						}
					}; break;
					case reuse: {
					}; break;
				}
			}

			void setstate(uint32 newstate, bool resend=false) {
				state = newstate;
				short_timer.cancel();
				long_timer.cancel();

				if (state == reuse) {
					if (!hashandle) {
						clearconn();
					}
					return;
				}

				if (state != timewait) {
					long_timer.expires_from_now(timeout::giveup);
				} else {
					long_timer.expires_from_now(timeout::conn_reuse);
				}
				long_timer.async_wait(boost::bind(&conn_impl<Proto>::giveup, this, boost::asio::placeholders::error));

				resending = false;
			}

			void giveup(const boost::system::error_code& e)
			{
				if (!e) {
					if (state != timewait) {
						if (resending) {
							setstate(timewait);
							check_queues();
						} else {
							setstate(state);
						}
					} else {
						setstate(reuse);
					}
				}
			}

			void clearconn()
			{
				cservice->delconn(this);
			}

		public:
			conn_impl(conn_service<Proto>* cservice_)
			: cservice(cservice_), short_timer(cservice_->service), long_timer(cservice_->service), service(cservice_->service)
			{
				rcv_wnd = 1;
				rdoff = rdsize = 0;
				fin_rcv = fin_snd = fin_ack = false;
				send_undelivered = 0;
				cservice->addconn(this);
			}

			conn_impl(conn_service<Proto>& cservice_)
			: cservice(&cservice_), short_timer(cservice_.service), long_timer(cservice_.service), service(cservice_.service)
			{
				rcv_wnd = 1;
				rdoff = rdsize = 0;
				fin_rcv = fin_snd = fin_ack = false;
				send_undelivered = 0;
				cservice->addconn(this);
			}

			typename Proto::endpoint remote_endpoint() const {
				return endpoint;
			}

			template <typename MBS, typename Handler>
			void async_read_some(const MBS& mbs, const Handler& handler)
			{
				if (!canreceive()) {
					service.dispatch(boost::bind<void>(handler,
						boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted),
						0
					));
					return;
				};

				void* buf;
				size_t buflen = 0;
				typename MBS::const_iterator iter = mbs.begin();
				typename MBS::const_iterator end = mbs.end();
				for (;buflen == 0 && iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::mutable_buffer buffer(*iter);
					buf = boost::asio::buffer_cast<void*>(buffer);
					buflen = boost::asio::buffer_size(buffer);
				}
				if (buflen == 0) {
					// no-op
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
					return;
				};

				recv_queue.push_back(
					rcvitem(
						buf,
						buflen,
						handler
					)
				);
				check_queues();
			}

			template <typename CBS, typename Handler>
			void async_write_some(const CBS& cbs, const Handler& handler)
			{
				if (!cansend()) {
					service.dispatch(boost::bind<void>(handler,
						boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted),
						0
					));
					return;
				};

				typename CBS::const_iterator iter = cbs.begin();
				typename CBS::const_iterator end = cbs.end();
				size_t tlen = 0;
				for (;iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::const_buffer buffer(*iter);
					const void* buf = boost::asio::buffer_cast<const void*>(buffer);
					size_t buflen = boost::asio::buffer_size(buffer);
					if (buflen > 0) {
						tlen += buflen;
						send_queue.push_back(
							snditem(
								buf,
								buflen,
								boost::function<void(const boost::system::error_code&, size_t len)>()
							)
						);
					}
				}
				if (tlen == 0)
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
				else
					send_queue.back().handler = handler;

				check_queues();
			}

			template <typename CBS>
			size_t write_some(const CBS& cbs, boost::system::error_code& ec)
			{
				if (!cansend()) {
					ec = boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted);
					return 0;
				};

				typename CBS::const_iterator iter = cbs.begin();
				typename CBS::const_iterator end = cbs.end();
				size_t tlen = 0;
				for (;iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::const_buffer buffer(*iter);
					const void* buf = boost::asio::buffer_cast<const void*>(buffer);
					size_t buflen = boost::asio::buffer_size(buffer);
					if (buflen > 0) {
						tlen += buflen;
						send_queue.push_back(
							snditem(
								buf,
								buflen,
								boost::function<void(const boost::system::error_code&, size_t len)>()
							)
						);
					}
				}

				if (tlen != 0)
					send_queue.back().handler = null_handler();

				check_queues();

				return tlen;
			}

			template <typename ConnectHandler>
			void async_connect(const typename Proto::endpoint& endpoint_, const ConnectHandler& handler_)
			{
				snd_una = cservice->get_isn();
				handler = handler_;
				endpoint = endpoint_;
				snd_nxt = snd_una+1;
				rqid = 0xffff;

				setstate(synsent);
				initsendpack(sendpack, 0, packet::flag_syn);
				send_packet();
			}

			template <typename AcceptHandler>
			void async_accept(const AcceptHandler& handler_)
			{
				snd_una = cservice->get_isn();
				cservice->acceptlog.push_back(lqid);
				handler = handler_;

				state = listening; // no setstate() to avoid giveup timeout
			}

			template <typename AcceptHandler>
			void async_accept(typename Proto::endpoint& peer_endpoint, const AcceptHandler& handler)
			{
				peer_endpoint = endpoint;
				async_accept(handler);
			}

			void close()
			{
				if (state != closed && state != closing && state != timewait && state != reuse) {
					setstate(closing);
					check_queues();
				}
			}

			bool is_open()
			{
				return cansend() || canreceive();
			}
	};

	// connection handle class;
	template<typename Proto>
	class conn : private boost::noncopyable {
		private:
			conn_impl<Proto>* impl;
		public:
			conn(conn_service<Proto>& cservice_) {
				impl = new conn_impl<Proto>(cservice_);
				impl->hashandle = true;
			};

			~conn()
			{
				close();
				impl->hashandle = false;
				if (impl->state == conn_impl<Proto>::reuse)
					impl->setstate(impl->state);
			}

			typename Proto::endpoint remote_endpoint() const {
				return impl->remote_endpoint();
			}

			template <typename MBS, typename Handler>
			void async_read_some(const MBS& mbs, const Handler& handler)
			{
				impl->async_read_some(mbs, handler);
			}

			template <typename CBS, typename Handler>
			void async_write_some(const CBS& cbs, const Handler& handler)
			{
				impl->async_write_some(cbs, handler);
			}

			template <typename CBS>
			size_t write_some(const CBS& cbs, boost::system::error_code& ec)
			{
				return impl->write_some(cbs, ec);
			}

			template <typename ConnectHandler>
			void async_connect(const typename Proto::endpoint& endpoint_, const ConnectHandler& handler_)
			{
				impl->async_connect(endpoint_, handler_);
			}

			template <typename AcceptHandler>
			void async_accept(const AcceptHandler& handler_)
			{
				impl->async_accept(handler_);
			}

			void close()
			{
				impl->close();
			}

			bool is_open()
			{
				return impl->is_open();
			}

			boost::asio::io_service& get_io_service() {
				return impl->get_io_service();
			}
	};

	template<typename Proto>
	class conn_service {
		private:
			friend class conn<Proto>;
			friend class conn_impl<Proto>;
			boost::asio::io_service& service;
			typename Proto::socket& socket;

			std::vector<conn_impl<Proto>*> conninfo;
			std::deque<size_t> acceptlog;

			uint64 isn_ofs;

			void addconn(conn_impl<Proto>* tconn)
			{
				for (size_t i = 0; i < conninfo.size(); ++i)
					if (!conninfo[i]) {
						conninfo[i] = tconn;
						tconn->lqid = i;
						return;
					}

				if (conninfo.size() == 0xffff) {
					tconn->lqid = 0;
					throw std::runtime_error("Too many connections\n");
				}

				tconn->lqid = conninfo.size();
				conninfo.push_back(tconn);
			}

			void delconn(conn_impl<Proto>* tconn)
			{
				//asio_timer_oneshot(service, timeout::conn_reuse, boost::bind(&conn_service<Proto>::clrconn, this, tconn->lqid));
				service.post(boost::bind(&conn_service<Proto>::clrconn, this, tconn->lqid));
			}

			void clrconn(size_t cqid)
			{
				delete conninfo[cqid];
				conninfo[cqid] = NULL;
			}

			bool is_valid_idx(size_t idx) {
				if (idx >= conninfo.size())
					return false;

				if (conninfo[idx] == NULL)
					return false;

				return true;
			}

			// calculate initial sequence number
			uint32 get_isn()
			{
				uint32 tps = boost::posix_time::microsec_clock::resolution_traits_type::ticks_per_second;
				boost::posix_time::ptime ut(boost::posix_time::microsec_clock::universal_time());
				boost::posix_time::time_duration ud = ut - boost::posix_time::ptime(boost::posix_time::min_date_time);
				if (tps > 250000)
					tps = 250000;

				uint64 ret = ud.ticks();
				ret *= tps;
				ret /= 1000000;
				return ret & 0xffffffff;
			}

		public:
			conn_service(boost::asio::io_service& service_, typename Proto::socket& socket_)
			: service(service_), socket(socket_)
			{
			}

			void handle_read(uint8* pack, size_t len, typename Proto::endpoint* peer)
			{
				handle_read((packet*) pack, len, peer);
			}

			void handle_read(packet* pack, size_t len, typename Proto::endpoint* peer)
			{
				if (packet::headersize > len)
					return;

				if (pack->header[3] != packet::signiature)
					return;

				pack->recvqid = (pack->header[0] << 8) | (pack->header[1] << 0);
				pack->flags   = pack->header[2];
				pack->datalen = len - packet::headersize;

				conn_impl<Proto>* tconn = NULL;
				if ((pack->flags & packet::flag_syn) && !(pack->flags & packet::flag_ack)) {
					while (!acceptlog.empty() && !is_valid_idx(acceptlog.front()))
						acceptlog.pop_front();

					if (!acceptlog.empty()) {
						tconn = conninfo[acceptlog.front()];
						acceptlog.pop_front();
					}
				} else if (is_valid_idx(pack->recvqid))
					tconn = conninfo[pack->recvqid];

				if (tconn) {
					if ((tconn->state == conn_impl<Proto>::listening || tconn->state == conn_impl<Proto>::synsent) && pack->flags & packet::flag_syn) {
						tconn->endpoint = *peer;
						tconn->rqid = pack->sendqid;
						tconn->rcv_nxt = pack->seqnum+1;
					}

					if (pack->sendqid == tconn->rqid && *peer == tconn->endpoint)
						tconn->handle_read(pack, len);
				}

			}
	};

};

#endif//ASIO_DGRAM_CONN_H
