#include "ConnectionCommon.h"

ConnectionCommon::ConnectionCommon(boost::asio::io_service& service_, UFTTCore* core_)
: ConnectionBase(service_, core_)
{
}

/* filesender */
ConnectionCommon::filesender::filesender(boost::asio::io_service& service)
: file(service)
{
}

void ConnectionCommon::filesender::init(uint64 offset_)
{
	offset = offset_;
	fsize = boost::filesystem::file_size(path) - offset;
	file.open(path, ext::asio::fstream::in);
	file.fseeka(offset);
	hsent = false;
}

bool ConnectionCommon::filesender::getbuf(shared_vec buf) {
	if (!hsent) {
		hsent = true;
		buf->resize(16 + name.size());
		cmdinfo hdr;
		hdr.cmd = (offset == 0) ? CMD_OLD_FILE : CMD_REPLY_PARTIAL_FILE;//CMD_REPLY_PARTIAL_FILE; // file
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
}

/* dirsender */
void ConnectionCommon::dirsender::init()
{
	curiter = ext::filesystem::directory_iterator(path);
	hsent = false;
}

bool ConnectionCommon::dirsender::getbuf(shared_vec buf, ext::filesystem::path& newpath)
{
	if (!hsent) {
		hsent = true;
		buf->resize(16 + name.size());
		cmdinfo hdr;
		hdr.cmd = CMD_OLD_DIR; // dir
		hdr.len = 0;
		hdr.ver = name.size();
		memcpy(&(*buf)[0], &hdr, 16);
		memcpy(&(*buf)[16], name.data(), hdr.ver);
	} else
		buf->clear();

	if (curiter == enditer) {
		cmdinfo hdr;
		hdr.cmd = CMD_OLD_CDUP; // ..
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
}

/* sigmaker */
ConnectionCommon::sigmaker::sigmaker(boost::asio::io_service& service_)
: service(service_)
{
}

void ConnectionCommon::sigmaker::main() {
	size_t bread;
	using namespace std;
	// dostuff
	shared_vec sbuf(new std::vector<uint8>(16));

	pkt_put_uint32(CMD_REPLY_SIG_FILE, &((*sbuf)[0]));
	pkt_put_uint32(0, &((*sbuf)[4]));

	//pkt_put_vuint32(item->path.size(), *sbuf);
	//for (uint j = 0; j < item->path.size(); ++j)
	//	sbuf->push_back(item->path[j]);

	ext::filesystem::path path(item->path);
	FILE* fd = ext::filesystem::fopen(path, "rb");
	size_t dpos = sbuf->size();
	sbuf->resize(dpos + (item->pos.size()*item->psize) );

	for (uint i = 0; i < item->pos.size(); ++i) {
		platform::fseek64a(fd, item->pos[i]);
		bread = fread(&((*sbuf)[dpos]), 1, item->psize, fd);
		dpos += item->psize;
	}
	pkt_put_uint64(sbuf->size()-16, &((*sbuf)[8]));
	service.post(boost::bind(cb, sbuf));
	fclose(fd);
}

/* sigchecker */
ConnectionCommon::sigchecker::sigchecker(boost::asio::io_service& service_)
: service(service_)
{
}

uint64 ConnectionCommon::sigchecker::getoffset()
{
	if(!ext::filesystem::exists(path)) {
		return 0;
	}
	size_t bread;
	using namespace std;

	uint32 dpos = 0;
	vector<uint8> buf(item->psize);
	for (uint i = 0; i < item->pos.size(); ++i) {
		platform::fseek64a(fd, item->pos[i]);
		bread = fread(&buf[0], 1, item->psize, fd);
		if (bread != buf.size())
			return 0;
		if (dpos + item->psize > data.size())
			return 0;
		for (size_t j = 0; j < item->psize; ++j)
			if (buf[j] != data[dpos++])
				return 0;
	}

	return item->pos.back() + item->psize;
}

void ConnectionCommon::sigchecker::main() {
	fd = ext::filesystem::fopen(path, "rb");
	service.post(boost::bind(cb, getoffset()));
	fclose(fd);
}
