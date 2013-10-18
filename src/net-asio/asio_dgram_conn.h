#ifndef ASIO_DGRAM_CONN_H
#define ASIO_DGRAM_CONN_H

#include <queue>

#include <boost/asio.hpp>
#include <boost/utility.hpp>
#include <boost/bind.hpp>

#include "../Types.h"
#include "../util/asio_timer_oneshot.h"

namespace dgram {
	enum { max_window_size = 1 }; // just 1 for now until its fully working
	enum { max_queue_size = 128 };

	namespace timeout {
		const boost::posix_time::time_duration resend     = boost::posix_time::milliseconds(500);          // 500ms
		const boost::posix_time::time_duration giveup     = boost::posix_time::minutes(1);                 // 1min
		const boost::posix_time::time_duration conn_reuse = boost::posix_time::minutes(10);                // 10min
	}

	bool inwindow(uint32 seqnum, uint32 wndstart, uint32 wndlen)
	{
		for (size_t i = 0; i < wndlen; ++i)
			if (seqnum == wndstart+i)
				return true;
		return false;
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
			flag_ping = 1 << 5, // packet has no data but should still increase seqnum
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
		const uint8* data;
		size_t len;
		size_t ofs;
		boost::function<void(const boost::system::error_code&, size_t len)> handler;

		snditem(const uint8* data_, size_t len_, boost::function<void(const boost::system::error_code&, size_t len)> handler_)
			: data(data_), len(len_), ofs(0), handler(handler_) {};
	};

	// item waiting for receiving data
	struct rcvitem {
		uint8* data;
		size_t len;
		size_t ofs;
		boost::function<void(const boost::system::error_code&, size_t len)> handler;

		rcvitem(uint8* data_, size_t len_,  boost::function<void(const boost::system::error_code&, size_t len)> handler_)
			: data(data_), len(len_), ofs(0), handler(handler_) {};
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
				rf_sendpack = 1,
			};

			boost::asio::io_service& service;
			conn_service<Proto>* cservice;

			typename Proto::endpoint endpoint;

			packet sendpack;
			packet sendpackonce;

			int32 state;
			bool fin_rcv;
			bool fin_snd;
			bool fin_ack;

			bool hashandle;
			bool hastimeout;

			uint16 lqid;
			uint16 rqid;

			uint32 snd_una; // oldest unacknowledged sequence number
			uint32 snd_nxt; // next sequence number to be sent
			uint32 snd_wnd; // remote receiver window size

			uint32 rcv_nxt; // next sequence number expected on an incoming segment

			boost::asio::deadline_timer short_timer;
			boost::asio::deadline_timer long_timer;
			bool resending;

			std::deque<snditem> send_buffers;
			std::deque<packet> send_packets_queue;
			std::deque<packet> send_packets;

			std::deque<rcvitem> recv_buffers;
			std::deque<packet> recv_packets_queue;
			std::deque<packet> recv_packets;

			bool inrcv;

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
				pack.datalen = datalen; // default

				pack.header[0] = (pack.recvqid >> 8) & 0xff;
				pack.header[1] = (pack.recvqid >> 0) & 0xff;
				pack.header[2] = pack.flags;
				pack.header[3] = packet::signiature;
			}

			int check_queues()
			{
				int ret = 0;
				ret |= check_recv_buffers();
				ret |= check_send_buffers();

				if (fin_rcv && fin_snd && fin_ack && state != timewait) {
					setstate(timewait);
				}
				return ret;
			}

			// moves data: send_buffers -> send_packets_queue
			void handle_send_buffers()
			{
				if (send_buffers.empty()) {
					if (state == closing && !fin_snd) {
						fin_snd = true;
						send_packets_queue.push_back(packet());
						send_packets_queue.back().flags = packet::flag_fin | packet::flag_ping;
					}
					return;
				}
				size_t totalsize = 0;
				for (auto b: send_buffers)
					totalsize += (b.len - b.ofs);

				packet* np = nullptr;
				if (!send_packets_queue.empty())
					np = &send_packets_queue.back();

				while ((totalsize > 0) && (send_packets_queue.size() + (np ? 0 : 1) < max_queue_size)) {
					assert(!send_buffers.empty());
					snditem& sendbuf = send_buffers.front();
					if (!np) {
						send_packets_queue.push_back(packet());
						np = &send_packets_queue.back();
						np->datalen = 0;
						np->flags = 0;
					}

					size_t copy = std::min<size_t>(packet::sendmtu - np->datalen, sendbuf.len - sendbuf.ofs);
					std::copy(sendbuf.data + sendbuf.ofs, sendbuf.data + sendbuf.ofs + copy, np->data + np->datalen);
					np->datalen += copy;
					sendbuf.ofs += copy;
					totalsize -= copy;

					if (np->datalen == packet::sendmtu)
						np = nullptr;
					if (sendbuf.len == sendbuf.ofs) {
						auto sitem = sendbuf;
						send_buffers.pop_front();
						if (sitem.handler)
							cservice->service.dispatch(boost::bind(sitem.handler, boost::system::error_code(), sitem.len));
					}
				}
			}

			// sends data: send_packets_queue -> send_packets 
			int handle_send_packets_queue()
			{
				int ret = 0;
				while (inwindow(snd_nxt, snd_una, calc_snd_wnd()) && !send_packets_queue.empty()) {
					send_packets.push_back(std::move(send_packets_queue.front()));
					send_packets_queue.pop_front();

					packet& pack = send_packets.back();
					initsendpack(pack, pack.datalen, pack.flags | packet::flag_ack);
					pack.seqnum = snd_nxt++;
					send_packet_impl(pack);

					ret |= rf_sendpack;
				}
				return ret;
			}

			int check_send_buffers()
			{
				int ret = 0;
				handle_send_buffers();

				if (!inrcv)
					ret |= handle_send_packets_queue();

				if (state != closing && !cansend()) {
					while (!send_buffers.empty()) {
						snditem front = send_buffers.front();
						send_buffers.pop_front();
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

			// moves data: recv_packets -> recv_packets_queue
			void handle_recv_packets()
			{
				if (recv_packets_queue.size() >= max_queue_size) return;

				while (!recv_packets.empty() && (recv_packets[0].flags & packet::flag_ack)) {
					assert(recv_packets.front().seqnum == rcv_nxt);
					if (recv_packets[0].datalen > 0)
						recv_packets_queue.push_back(recv_packets.front());
					recv_packets.pop_front();
					++rcv_nxt;
				}
			}

			// calls handlers: recv_packets_queue -> recv_buffers
			void handle_recv_packets_queue()
			{
				if (fin_rcv)
					recv_packets_queue.clear();
				size_t ofs = 0;
				while (!recv_buffers.empty() && !recv_packets_queue.empty()) {
					auto& recvbuf = recv_buffers[0];
					auto& recvpack = recv_packets_queue[0];
					if (recvpack.flags & packet::flag_fin)
						fin_rcv = true;
					if (recvpack.datalen == 0) {
						recv_packets_queue.pop_front();
						continue;
					}
					size_t copy = std::min<size_t>(recvbuf.len - ofs, recvpack.datalen);
					std::copy(recvpack.data, recvpack.data + copy, recvbuf.data + ofs);
					if (copy != recvpack.datalen) {
						recvpack.datalen -= copy;
						for (size_t i = 0; i < recvpack.datalen; ++i)
							recvpack.data[i] = recvpack.data[i+copy];
						//std::copy(recvpack.data + copy, recvpack.data + recvpack.datalen, recvpack.data);
					} else
						recv_packets_queue.pop_front();

					ofs += copy;

					if (ofs == recvbuf.len || recv_packets_queue.empty()) {
						auto ritem = recvbuf;
						recv_buffers.pop_front();
						if (ritem.handler)
							cservice->service.dispatch(boost::bind(ritem.handler, boost::system::error_code(), ofs));
						ofs = 0;
					}
				}
			}

			int check_recv_buffers()
			{
				int ret = 0;
				handle_recv_packets();
				handle_recv_packets_queue();

				if (!recv_buffers.empty() && !canreceive()) {
					while (!recv_buffers.empty()) {
						rcvitem ritem = recv_buffers.front();
						recv_buffers.pop_front();
						cservice->service.dispatch(boost::bind(ritem.handler, boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted), 0));
					};
					if (handler) {
						cservice->service.dispatch(boost::bind(handler, boost::system::posix_error::make_error_code(boost::system::posix_error::connection_aborted)));
						handler.clear();
					}
				}
				return ret;
			}

			int calc_snd_wnd()
			{
				return snd_wnd;
			}

			int calc_rcv_wnd()
			{
				int space = max_queue_size - recv_packets_queue.size();
				return std::min<int>(space, max_window_size);
			}

			void send_packet_impl(packet& pack)
			{
				if (pack.flags & packet::flag_ack)
					pack.acknum = rcv_nxt;
				pack.window = calc_rcv_wnd();

				try {
					cservice->socket.send_to(boost::asio::buffer(&pack, packet::headersize+pack.datalen), endpoint);
				} catch (boost::system::system_error& error) {
					if (handler) {
						cservice->service.dispatch(boost::bind(handler, error.code()));
						handler.clear();
					}
					// should clear these handler too?
					if (!recv_buffers.empty()) cservice->service.dispatch(boost::bind(recv_buffers.front().handler, error.code(), 0));
					if (!send_buffers.empty()) cservice->service.dispatch(boost::bind(send_buffers.front().handler, error.code(), 0));
				}

				if (!hastimeout && !send_packets.empty()) {
					short_timer.expires_from_now(timeout::resend);
					short_timer.async_wait(boost::bind(&conn_impl<Proto>::send_packet_impl_retry, this, boost::asio::placeholders::error));
					hastimeout = true;
				}
			}

			void send_packet_impl_retry(const boost::system::error_code& error)
			{
				if (error) return; //aborted
				hastimeout = false;
				if (send_packets.empty()) return; // nothing to do
				for (auto& p: send_packets)
					send_packet_impl(p);
			}


			void send_packet_once() {
				send_packet_once(sendpackonce);
			}

			void send_packet_once(packet& pack) {
				//mcout(9) << STRFORMAT("[%3d,%3d] ", pack.sendid, pack.recvid) << "send udp packet: " << pack.type << "  len:" << pack.len << "\n";
				//socket->send_to(boost::asio::buffer(&pack, ipx_packet::headersize+pack.datalen), endpoint);
				assert(pack.datalen == 0);
				assert(!(pack.flags & packet::flag_ping));
				send_packet_impl(pack);
				//udp_sock->async_send_to(boost::asio::buffer(&sendpack, 4*5), udp_addr, boost::bind(&ignore));
			}

			/*
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
			*/

			void stream_packet(const packet& p)
			{
			}

			// for when a connection is established
			boost::function<void(const boost::system::error_code&)> handler;

			void handle_ack(uint32 ack)
			{
				if (!inwindow(ack, snd_una+1, snd_nxt-snd_una))
					return;
				while (snd_una != ack) {
					assert(!send_packets.empty());
					assert(send_packets[0].seqnum = snd_una);
					++snd_una;
					if (send_packets[0].flags & packet::flag_fin)
						fin_ack = true;
					send_packets.pop_front();
				}
				if (state == synsent || state == synreceived) {
					setstate(established);
					cservice->service.dispatch(boost::bind(handler, boost::system::error_code()));
					handler.clear();
				}

				if (!send_packets.empty()) {
					short_timer.expires_from_now(timeout::resend);
					short_timer.async_wait(boost::bind(&conn_impl<Proto>::send_packet_impl_retry, this, boost::asio::placeholders::error));
					hastimeout = true;
				} else {
					short_timer.cancel();
					hastimeout = false;
				}

				setstate(state); // clears timeouts?
			}

			void handle_packet_data(packet* pack)
			{
				if (!inwindow(pack->seqnum, rcv_nxt, calc_rcv_wnd()))
					return;

				if (pack->datalen == 0 && !(pack->flags & packet::flag_ping))
					return;

				uint32 wndidx = pack->seqnum - rcv_nxt;

				if (recv_packets.size() <= wndidx) recv_packets.resize(wndidx+1);
				if (!(recv_packets[wndidx].flags & packet::flag_ack)) {
					recv_packets[wndidx] = *pack;
					pack = &recv_packets[wndidx];
					pack->flags |= packet::flag_ack;
				}
			}

			void handle_read(packet* pack, size_t len)
			{
				snd_wnd = std::max<uint32>(pack->window, 1);
				inrcv = true;
				switch (state) {
					case closed: {
					}; break;
					case listening: {
						if (pack->flags & packet::flag_syn) {
							snd_nxt = snd_una;
							setstate(synreceived);

							assert(send_packets.empty() && send_packets_queue.empty());
							send_packets.push_back(packet());
							packet& pack = send_packets.back();

							initsendpack(pack, 0, packet::flag_syn | packet::flag_ack | packet::flag_ping);
							pack.seqnum = snd_nxt++;
							send_packet_impl(pack);
						}
					}; break;
					case synsent:
					case synreceived:
					case established:
					case closing: {
						if (pack->flags & packet::flag_ack)
							handle_ack(pack->acknum);

						handle_packet_data(pack);

						bool needack = (pack->seqnum != rcv_nxt);
						int check = check_queues();
						check |= handle_send_packets_queue();
						needack = needack || (pack->datalen > 0) || (pack->flags & packet::flag_ping);
						if (!(check & rf_sendpack) && needack) {
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
				inrcv = false;
			}

			void setstate(uint32 newstate, bool resend=false) {
				state = newstate;
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
				fin_rcv = fin_snd = fin_ack = false;
				send_undelivered = 0;
				hastimeout = false;
				cservice->addconn(this);
			}

			conn_impl(conn_service<Proto>& cservice_)
			: cservice(&cservice_), short_timer(cservice_.service), long_timer(cservice_.service), service(cservice_.service)
			{
				fin_rcv = fin_snd = fin_ack = false;
				send_undelivered = 0;
				hastimeout = false;
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

				uint8* buf;
				size_t buflen = 0;
				typename MBS::const_iterator iter = mbs.begin();
				typename MBS::const_iterator end = mbs.end();
				for (;buflen == 0 && iter != end; ++iter) {
					// at least 1 buffer
					boost::asio::mutable_buffer buffer(*iter);
					buf = boost::asio::buffer_cast<uint8*>(buffer);
					buflen = boost::asio::buffer_size(buffer);
				}
				if (buflen == 0) {
					// no-op
					service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
					return;
				};

				recv_buffers.push_back(
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
					const uint8* buf = boost::asio::buffer_cast<const uint8*>(buffer);
					size_t buflen = boost::asio::buffer_size(buffer);
					if (buflen > 0) {
						tlen += buflen;
						send_buffers.push_back(
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
					send_buffers.back().handler = handler;

				check_queues();
			}

			// read_some impossible to implement, because we can't turn
			// asynchronous operations into synchronous ones, and we can't
			// do synchronous reads on our udp socket, because we don't own
			// it and other kinds of packets may be received on it
			//template <typename CBS>
			//size_t read_some(const CBS& cbs, boost::system::error_code& ec);

			// this adds the buffers without handlers, so error may not be reported directly
			// its also weird to call this when handlers are still pending, but thats an error on the user part (interleaving writes)
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
						boost::shared_ptr<std::vector<uint8> > sbuf(new std::vector<uint8>(buflen));
						memcpy(&(*sbuf)[0], buf, buflen);

						tlen += buflen;
						send_buffers.push_back(
							snditem(
								&(*sbuf)[0],
								buflen,
								// handler does nothing except keep buffer alive through shared_ptr
								boost::bind(&std::vector<uint8>::size, sbuf)
							)
						);
					}
				}

				check_queues();

				return tlen;
			}

			template <typename ConnectHandler>
			void async_connect(const typename Proto::endpoint& endpoint_, const ConnectHandler& handler_)
			{
				snd_nxt = snd_una = cservice->get_isn();
				handler = handler_;
				endpoint = endpoint_;
				rqid = 0xffff;

				setstate(synsent);

				assert(send_packets.empty() && send_packets_queue.empty());

				send_packets.push_back(packet());
				packet& pack = send_packets.back();

				initsendpack(pack, 0, packet::flag_syn | packet::flag_ping);
				pack.acknum = 1;

				pack.seqnum = snd_nxt++;
				send_packet_impl(pack);
			}

			template <typename AcceptHandler>
			void async_accept(const AcceptHandler& handler_)
			{
				snd_nxt = snd_una = cservice->get_isn();
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

				//std::cout << "got udp packet: len:" << pack->datalen << " D:" << (int)pack->data[0] << "\n";
				if ((pack->flags & packet::flag_syn) && !(pack->flags & packet::flag_ack)) {
					if (pack->acknum < 5) {
						++pack->acknum;
						try {
							socket.send_to(boost::asio::buffer(pack, packet::headersize+pack->datalen), *peer);
						} catch (...) {};
						return;
					}
				}

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

}

#endif//ASIO_DGRAM_CONN_H
