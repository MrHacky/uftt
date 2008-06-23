#ifndef IPX_CONN_H
#define IPX_CONN_H

#include "asio_ipx.h"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <queue>

#include "../types.h"

bool is_ack_acceptable(uint32 snduna, uint32 segack, uint32 sndnxt) {
	//return (0 < (segack-snduna)) && ((segack-snduna) <= (sndnxt-snduna));
	return (segack!=snduna && (segack-snduna) <= (sndnxt-snduna));
}

#define IPX_MAX_MTU 2048
struct ipx_packet {
	enum constants {
		headersize = 4*4,
		maxmtu = 2048,
		sendmtu = 1400,
		datasize = maxmtu-headersize,
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


	uint16 recvqid;  // recvers qid
	uint16 sendqid;  // senders qid
	uint32 seqnum;
	uint32 acknum;
	uint16 flags;
	uint16 window;
	uint8 data[datasize];
	uint16 datalen;
};

boost::asio::deadline_timer::duration_type short_timeout = boost::posix_time::milliseconds(250);          // 250ms
boost::asio::deadline_timer::duration_type stats_timeout = boost::posix_time::milliseconds(100);          // 100ms

boost::asio::deadline_timer::duration_type disconn_timeout = boost::posix_time::milliseconds(10*1000*60); // 10min
boost::asio::deadline_timer::duration_type active_timeout = boost::posix_time::milliseconds(5*1000);      // 5sec
boost::asio::deadline_timer::duration_type idle_timeout = boost::posix_time::milliseconds(1000*60);       // 1min
	
class ipx_conn {
	private:
		enum state_enum {
			closed = 0,
			listening,
			synsent,
			synreceived,
			established,
			finwait1,
			finwait2,
			timewait,
			lastack,
		};

		boost::asio::io_service& service;
		boost::shared_ptr<boost::asio::ipx::socket> socket;

		boost::asio::ipx::endpoint endpoint;

		ipx_packet sendpack;
		ipx_packet sendpackonce	;

		int32 state;
		uint16 lqid;
		uint16 rqid;

		
		uint32 snd_una; // oldest unacknowledged sequence number
		uint32 snd_nxt; // next sequence number to be sent
		uint32 snd_wnd; // remote receiver window size

		uint32 rcv_nxt; // next sequence number expected on an incoming segment
		uint16 rcv_wnd; // receiver window size


		boost::asio::deadline_timer short_timer;
		boost::asio::deadline_timer long_timer;

		//stats
		uint32 resend;

		std::deque<std::pair<std::pair<const void*, uint32>, boost::function<void(const boost::system::error_code&, size_t len)> > > send_queue;
		std::deque<std::pair<std::pair<      void*, uint32>, boost::function<void(const boost::system::error_code&, size_t len)> > > recv_queue;

		void check_queues()
		{
			check_send_queues();
			check_recv_queues();
		}

		void check_send_queues()
		{
			if (snd_una == snd_nxt && !send_queue.empty()) {
				uint32 trylen = send_queue.front().first.second;
				if (trylen > ipx_packet::sendmtu)
					trylen = ipx_packet::sendmtu;
				initsendpack(sendpack, trylen);
				memcpy(sendpack.data, send_queue.front().first.first, sendpack.datalen);
				sendpack.seqnum = snd_nxt++;
				send_packet();
				//send_queue.front().first.first = ((char*)send_queue.front().first.first) + trylen;
				//send_queue.front().first.second -= trylen;
				//if (send_queue.front().first.second == 0) {
					service.dispatch(boost::bind(send_queue.front().second, boost::system::error_code(), sendpack.datalen));
					send_queue.pop_front();
				//}
			}
		}

		void check_recv_queues()
		{
		}

		friend class ipx_acceptor;

		boost::function<void(const boost::system::error_code&)> handler;

		void initsendpack(ipx_packet& pack, uint16 datalen = 0, uint16 flags = 0) {
			pack.recvqid = rqid;
			pack.sendqid = lqid;
			pack.flags = flags | ipx_packet::flag_ack;
			pack.seqnum = snd_una;
			pack.acknum = rcv_nxt;
			pack.window = rcv_wnd; // - received packets in queue
			pack.datalen = datalen; // default
		}

		void send_packet_once() {
			send_packet_once(sendpackonce);
		}

		void send_packet_once(ipx_packet& pack) {
			//mcout(9) << STRFORMAT("[%3d,%3d] ", pack.sendid, pack.recvid) << "send udp packet: " << pack.type << "  len:" << pack.len << "\n";
			socket->send_to(boost::asio::buffer(&pack, ipx_packet::headersize+pack.datalen), endpoint);
			//udp_sock->async_send_to(boost::asio::buffer(&sendpack, 4*5), udp_addr, boost::bind(&ignore));
		}

		void send_packet(const boost::system::error_code& error = boost::system::error_code())
		{
			if (!error) {
				send_packet_once(sendpack);

				short_timer.expires_from_now(short_timeout);
				short_timer.async_wait(boost::bind(&ipx_conn::send_packet, this, boost::asio::placeholders::error));
				++resend; // count all packets we send while using a timeout
			} else
				--resend; // and subtract all canceled timeouts
		}

		void handle_read(ipx_packet* pack, size_t len) 
		{
			pack->datalen = len - ipx_packet::headersize;
			snd_wnd = pack->window;
			switch (state) {
				case closed: {
				}; break;
				case listening: {
					if (pack->flags & ipx_packet::flag_syn) {
						snd_nxt = snd_una+1;
						initsendpack(sendpackonce, 0, ipx_packet::flag_syn);
						send_packet_once();
						setstate(synreceived);
					}
				}; break;
				case synsent:
				case synreceived: {
					if (pack->flags & ipx_packet::flag_ack && pack->acknum == snd_una+1) {
						++snd_una;
						initsendpack(sendpackonce);
						send_packet_once();
						setstate(established);
						service.dispatch(boost::bind(handler, boost::system::error_code()));
						handler.clear();
					}
				}; break;
				case established: {
					if (pack->datalen > 0 && pack->seqnum == rcv_nxt) {
						if (!recv_queue.empty()) {
							if (recv_queue.front().first.second < pack->datalen) {
								// handle rerror
							} else {
								memcpy(recv_queue.front().first.first, pack->data, pack->datalen);
								++rcv_nxt;
								service.dispatch(boost::bind(recv_queue.front().second, boost::system::error_code(), pack->datalen));
								recv_queue.pop_front();
							}

						}
					}
					if (pack->flags & ipx_packet::flag_ack && pack->acknum == snd_una+1) {
						++snd_una;
						setstate(established);
					}
					check_queues();
					if (pack->seqnum != rcv_nxt) {
						initsendpack(sendpackonce);
						send_packet_once();
					}
				}; break;
				case finwait1: {
				}; break;
				case finwait2: {
				}; break;
				case timewait: {
				}; break;
				case lastack: {
				}; break;
			}
		}

		void setstate(uint32 newstate, bool resend=false) {
			state = newstate;
			short_timer.cancel();
			long_timer.cancel();
		}
	public:
		ipx_conn(boost::asio::io_service& service_)
			: service(service_), state(closed), short_timer(service_), long_timer(service_)
		{
			rcv_wnd = 1;
		};

		template <typename MBS, typename Handler>
		void async_read_some(MBS& mbs, const Handler& handler)
		{
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
				std::pair<std::pair<void*, uint32>, boost::function<void(const boost::system::error_code&, size_t len)> >(
					std::pair<void*, uint32>(buf, buflen),
					handler
				)
			);
			check_queues();
		}

		template <typename CBS, typename Handler>
		void async_write_some(CBS& cbs, const Handler& handler)
		{
			const void* buf = NULL;
			size_t buflen = 0;
			typename CBS::const_iterator iter = cbs.begin();
			typename CBS::const_iterator end = cbs.end();
			for (;buflen == 0 && iter != end; ++iter) {
				// at least 1 buffer
				boost::asio::const_buffer buffer(*iter);
				buf = boost::asio::buffer_cast<const void*>(buffer);
				buflen = boost::asio::buffer_size(buffer);
			}
			if (buflen == 0) {
				// no-op
				service.dispatch(boost::bind<void>(handler, boost::system::error_code(), 0));
				return;
			};

			send_queue.push_back(
				std::pair<std::pair<const void*, uint32>, boost::function<void(const boost::system::error_code&, size_t len)> >(
					std::pair<const void*, uint32>(buf, buflen),
					handler
				)
			);
			check_queues();
		}

		template <typename ConnectHandler>
		void async_connect(const boost::asio::ipx::endpoint& endpoint_, const ConnectHandler& handler_)
		{
			boost::shared_ptr<boost::asio::ipx::socket> sock(new boost::asio::ipx::socket(service));
			sock->open();
			sock->bind(boost::asio::ipx::endpoint(boost::asio::ipx::address::local(), 0));
			async_connect(sock, endpoint_, handler_);
		}

		template <typename ConnectHandler>
		void async_connect(boost::shared_ptr<boost::asio::ipx::socket> sock_, const boost::asio::ipx::endpoint& endpoint_, const ConnectHandler& handler_)
		{
			snd_una = 111;
			handler = handler_;
			endpoint = endpoint_;
			state = synsent;
			socket = sock_;
			snd_nxt = snd_una+1;
			initsendpack(sendpack, 0, ipx_packet::flag_syn);
			send_packet();
			start_connect_packet_handler();
		}

	private:
		ipx_packet recvpack;
		boost::asio::ipx::endpoint recvaddr;

		void start_connect_packet_handler() {
			socket->async_receive_from(boost::asio::buffer(&recvpack, ipx_packet::maxmtu), recvaddr,
				boost::bind(&ipx_conn::connect_packet_handler, this, _1, _2));
		}

		void connect_packet_handler(const boost::system::error_code& e, size_t len)
		{
			if (!e && ipx_packet::headersize <= len) {
				if (recvpack.flags & ipx_packet::flag_syn) {
					endpoint = recvaddr;
					rqid = recvpack.sendqid;
					rcv_nxt = recvpack.seqnum+1;
				}
				if (recvpack.sendqid == rqid /*&& recvaddr == endpoint*/) {
						handle_read(&recvpack, len);
				}
			}
			if (!e) {
				start_connect_packet_handler();
			} else {
				std::cout << e.message() << '\n';
			}
		}

};

struct null_deleter
{
    void operator()(void const *) const
    {
    }
};

class ipx_acceptor: public boost::asio::ipx::socket {
	private:
		boost::asio::io_service& service;

		boost::asio::ipx::endpoint recvaddr;
		ipx_packet recvpack;

		struct listeninfo {
			boost::asio::ipx::endpoint endpoint;
			unsigned int quickid;
		};

		void start_read() {
			this->async_receive_from(boost::asio::buffer(&recvpack, ipx_packet::maxmtu), recvaddr,
				boost::bind(&ipx_acceptor::handle_read, this, _1, _2));
		}

		std::deque<ipx_conn*> acceptlog;

		std::vector<ipx_conn*> conninfo;
	public:
		ipx_acceptor(boost::asio::io_service& service_)
			: boost::asio::ipx::socket(service_), service(service_)
		{
		}

		void listen(int backlog_ = 16)
		{
			start_read();
			// todo
		}

		void handle_read(const boost::system::error_code& e, size_t len)
		{
			if (!e && ipx_packet::headersize <= len) {
				if (recvpack.flags & ipx_packet::flag_syn) {
					if (!acceptlog.empty()) {
						ipx_conn* peer = acceptlog.front(); acceptlog.pop_front();
						peer->endpoint = recvaddr;
						peer->socket = boost::shared_ptr<boost::asio::ipx::socket>(this, null_deleter() );
						peer->rqid = recvpack.sendqid;
						peer->lqid = conninfo.size();
						peer->rcv_nxt = recvpack.seqnum+1;
						peer->snd_una = 222;
						conninfo.push_back(peer);
						peer->handle_read(&recvpack, len);
						//peer->handler(boost::system::error_code());
						//peer->handler = boost::function<void(const boost::system::error_code&)>();
					} // ignoring requests otherwise...
				} else if (recvpack.recvqid < conninfo.size() && conninfo[recvpack.recvqid]) {
					ipx_conn* peer = conninfo[recvpack.recvqid];
					peer->endpoint = recvaddr;
					if (recvpack.sendqid == peer->rqid && recvaddr == peer->endpoint)
						peer->handle_read(&recvpack, len);
				}
			}

			if (!e) {
				start_read();
			}
		}

		template <typename AcceptHandler>
		void async_accept(ipx_conn& peer, const AcceptHandler& handler)
		{
			acceptlog.push_back(&peer);
			peer.handler = handler;
			peer.state = ipx_conn::listening;
		}

		template <typename AcceptHandler>
		void async_accept(ipx_conn& peer, boost::asio::ipx::endpoint& peer_endpoint, const AcceptHandler& handler)
		{
			peer_endpoint = peer.endpoint;
			async_accept(peer, handler);
		}
};

#endif//IPX_CONN_H