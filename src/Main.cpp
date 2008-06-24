#include "Types.h"

//#define BOOST_ASIO_DISABLE_IOCP
//#define _WIN32_WINNT 0x0500
//#define WINVER 0x0500
//#define _WIN32_WINDOWS 0x0410

#include "BuildString.h"
#include "qt-gui/QTMain.h"
//#include "network/NetworkThread.h"
#include "net-asio/asio_ipx.h"
#include "net-asio/ipx_conn.h"
#include "SharedData.h"
#include "SimpleBackend.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

#include <string>

using namespace std;

using boost::asio::ipx;

std::string thebuildstring;
services::diskio_service* gdiskio;

#define BUFSIZE (1024*1024*16)
std::vector<uint8*> testbuffers;


shared_vec exefile;
bool hassignedbuild(false);


int runtest() {
	try {
		// TEST 0: all dependencies(dlls) have been loaded
		cout << "Loaded dll dependancies...Success\n";

		// todo, put something more here, like:
		cout << "Testing (a)synchronous network I/O...";
		{
			char sstr[] = "sendstring";
			char rstr[] = "1234567890";
			bool recvstarted = false;
			bool received = false;
			bool connected = false;
			bool accepted = false;
			bool sent = false;
			bool timedout = false;

			struct settrue {
				bool* value;
				settrue(bool* value_) : value(value_) {};
				void operator()(const boost::system::error_code& ec, size_t len = 0)
				{ 
					boost::asio::detail::throw_error(ec);
					*value = true;
				}
			};

			boost::asio::io_service service;
			boost::asio::ip::tcp::acceptor acceptor(service);
			boost::asio::ip::tcp::socket ssock(service);
			boost::asio::ip::tcp::socket rsock(service);

			acceptor.open(boost::asio::ip::tcp::v4());
			acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 23432));
			acceptor.listen(16);

			acceptor.async_accept(ssock, settrue(&accepted));

			rsock.open(boost::asio::ip::tcp::v4());
			rsock.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 23432), settrue(&connected));
	
			boost::asio::deadline_timer wd(service);
			wd.expires_from_now(boost::posix_time::seconds(10));
			wd.async_wait(settrue(&timedout));
			
			do {
				service.run_one();
				if (accepted && !sent) {
					// sync write here
					boost::asio::write(ssock, boost::asio::buffer(sstr));
					sent = true;
				}
				if (connected && !recvstarted) {
					// async read here
					boost::asio::async_read(rsock, boost::asio::buffer(rstr), settrue(&received));
					recvstarted = true;
				}
				//if (tries == 50 && !connected) {
				//	rsock.async_connect(acceptor.local_endpoint(), settrue(&connected));
				//}
			} while (!timedout && !received);
			//cout << "t: " << tries << '\n';
			//cout << "s: " << sstr << '\n';
			//cout << "r: " << rstr << '\n';
			if (timedout)
				cout << "Timed out...";
			if (!connected)
				throw std::runtime_error("connect failed");
			if (!accepted)
				throw std::runtime_error("accept failed");
			if (!sent)
				throw std::runtime_error("send failed");
			if (!recvstarted || !received)
				throw std::runtime_error("receive failed");
			if (string(sstr) != string(rstr))
				throw std::runtime_error("transfer failed");
		}
		cout << "Success\n";

		cout << "Testing udp broadcast...";
		{
			char sstr[] = "sendstring";
			char rstr[] = "1234567890";
			boost::asio::io_service service;
			boost::asio::ip::udp::socket udpsocket(service);
			
			udpsocket.open(boost::asio::ip::udp::v4());
			//udpsocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::address(), 23432));
			udpsocket.set_option(boost::asio::ip::udp::socket::broadcast(true));

			udpsocket.send_to(
				boost::asio::buffer(sstr), 
				boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), 23432)
				//boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 23432),
				,0
			);


		}
		cout << "Success\n";

		cout << "Testing asynchronous disk I/O...";
		cout << "Skipped\n";

		cout << "Testing boost libraries...";
		cout << "Skipped\n";

		cout << "Testing qt libraries...";
		cout << "Skipped\n";

		return 0; // everything worked, success!
	} catch (std::exception& e) {
		cout << "Failed!\n";
		cout << "\n";
		cout << "Reason: " << e.what() << '\n';
		cout << flush;
		return 1; // error
	} catch (...) {
		cout << "Failed!\n";
		cout << "\n";
		cout << "Reason: Unknown\n";
		cout << flush;
		return 1; // error
	}
}

enum {
	RF_NEW_CONSOLE   = 1 << 0,
	RF_WAIT_FOR_EXIT = 1 << 1,
};

int RunDirect(const string& cmd, const vector<string>* args, const string& wd, int flags);

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>

char thepubkey[] = 
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIICCgKCAgEAprtlIDKFC0jgAL05jc1dyqN3ptxAI3oOBvHwLuDgTPpH9C5kHNaB\n"
"MiMO+gZ2AUBZny/aRIlAMrjjKEqbdUk9Xyu+MIBPsUy+wO8xYLMK7mXkwhfgKNDS\n"
"blYi8wueROOJrB8BsIWnAI2Jir7nfGWFekeMBc28HPOClCwy+aGED9yV/+iS3w40\n"
"InMXr3s0ISkfEZ4zZQ54KfMvNAeO0bF6f4jA1F1zFnWH3YHC3dSp8jnWev8dMLTh\n"
"dyRhZpYEyUds/uNTXd68TXctjE1PKx9ycv1npxijFXjqBaD9z8j4yq4/xj+NxRf5\n"
"a7hmlCBP1lmzv26uZbrqBE1R7MbA6qpj9ez3cFEaHjjdA5l3vpcVeaC2RcFNwxJC\n"
"dG7ojetDKI5JeuiOvb4d0DAl9yBAg49iaHHKoXKTaKQN3JsAk5n18pC2EzDirDxH\n"
"ynGTB7SW46jVEMb9yG4kTSRwcGjh10PYGrS+GUrD8i6df5PlRY75MrF9kxW3L+HM\n"
"Ls9CsIK4LN9uFAcdp7syXPuLBwO9Q85uGkS+AJxMt2Ek1GEoHDNlopUmL8fzJ8wW\n"
"iVhg6bOjDi0Pv5JvpI19Qw8DcZDdsdAEEwQ5wiq98GG+Nf02f1NGoI4Vm1oKk8QQ\n"
"7jbaFEd+p6pVKv2zv16UnoasyZBVmdkAHY4dRhwqz227rJCJMz2TndUCAwEAAQ==\n"
"-----END RSA PUBLIC KEY-----\n";

// returns true when signature checks out
bool checksigniature(const std::vector<uint8>& file) {
	RSA* rsapub = NULL;

	BIO* pubmem = BIO_new_mem_buf(thepubkey, -1);
	rsapub = PEM_read_bio_RSAPublicKey(pubmem, NULL, NULL, NULL);
	BIO_free(pubmem);

	if (!rsapub)
		return false;

	EVP_PKEY evppub;
	EVP_PKEY_assign_RSA(&evppub, rsapub);

	if (file.size() < 8)
		return false;

	string sigstr = "----";

	for (int i = 0; i < 4; ++i)
		sigstr[i] = file[file.size()-4+i];

	if (sigstr != "UFTT")
		return false;
	
	uint32 sigsize = 0;
	for (int i = 0; i < 4; ++i)
		sigsize |= file[file.size()-8+i] << (i*8);
	
	if (file.size() < (sigsize*2)+8)
		return false;

	vector<uint8> sig;
	sig.resize(sigsize);
	for (int i = 0; i < sigsize; ++i)
		sig[i] = file[file.size()-8-sigsize+i];

	int res;
	EVP_MD_CTX verifyctx;
	res = EVP_VerifyInit(&verifyctx, EVP_sha1());
	if (res != 1) return false;
	res = EVP_VerifyUpdate(&verifyctx, &file[0], file.size()-8-sigsize);
	if (res != 1) return false;
	res = EVP_VerifyFinal(&verifyctx, &sig[0], sig.size(), &evppub);
	if (res != 1) return false;

	return true;
}

int imain( int argc, char **argv )
{
	if (argc > 1 && string(argv[1]) == "--runtest")
		return runtest();

	if (argc > 2 && string(argv[1]) == "--delete") {
		int retries = 10;
		while (boost::filesystem::exists(argv[2]) && retries > 0) {
			--retries;
			Sleep((10-retries)*100);
			boost::filesystem::remove(argv[2]);
		}
	}

	{
		exefile = shared_vec(new vector<uint8>());
		uint32 todo = boost::filesystem::file_size(argv[0]);
		exefile->resize(todo);
		ifstream istr(argv[0], ios_base::in|ios_base::binary);
		istr.read((char*)&((*exefile)[0]), todo);
		uint32 read = istr.gcount();
		if (read != todo) {
			cout << "failed to read ourself\n";
			exefile->resize(0);
		}
	}

	if (true) {
		RSA* rsapub = NULL;// = RSA_generate_key(4096, 65537, NULL, NULL);
		RSA* rsapriv = NULL;

		{
			FILE* privfile = fopen("c:\\temp\\uftt.private.key.dat", "rb");
			if (privfile) {
				rsapriv = PEM_read_RSAPrivateKey(privfile, NULL, NULL, NULL);
				fclose(privfile);
			}

			BIO* pubmem = BIO_new_mem_buf(thepubkey, -1);
			rsapub = PEM_read_bio_RSAPublicKey(pubmem, NULL, NULL, NULL);
			BIO_free(pubmem);
		}
		EVP_PKEY evppub;
		EVP_PKEY evppriv;

		EVP_PKEY_assign_RSA(&evppub, rsapub);
		EVP_PKEY_assign_RSA(&evppriv, rsapriv);

		if (checksigniature(*exefile)) {
			cout << "yay! this is a signed binary!\n";
			hassignedbuild = true;
		} else if (rsapriv) {
			// it's not signed, but we have access to a private key
			// so we can sign it ourselves!

			uint sigsize = EVP_PKEY_size(&evppriv);
			vector<uint8> sig;
			sig.resize(sigsize);

			EVP_MD_CTX signctx;
			EVP_SignInit(&signctx, EVP_sha1());

			int r1 = EVP_SignUpdate(&signctx, &((*exefile)[0]), exefile->size());
			int r2 = EVP_SignFinal(&signctx, &sig[0], &sigsize, &evppriv);

			if (r1==1 && r2==1) {

				for (int i = 0; i < sigsize; ++i)
					exefile->push_back(sig[i]);
				exefile->push_back((sigsize >> 0) & 0xff);
				exefile->push_back((sigsize >> 8) & 0xff);
				exefile->push_back((sigsize >>16) & 0xff);
				exefile->push_back((sigsize >>24) & 0xff);
				exefile->push_back('U');
				exefile->push_back('F');
				exefile->push_back('T');
				exefile->push_back('T');

				hassignedbuild = true;
				cout << "yay! we just signed this binary!\n";
				if (!checksigniature(*exefile)) {
					cout << "WTF! this is not possible!!!\n";
					return 1;
				}

				// write signed build to file for debug purposes
				ofstream sexe("uftt-signed.exe", ios_base::out|ios_base::binary);
				sexe.write((char*)&((*exefile)[0]), exefile->size());
				sexe.close();
			}
		}
	}

	if (argc > 2 && string(argv[1]) == "--replace") {
		cout << "Not implemented yet!\n";
		ofstream ostr;
		int retries = 10;
		while (!ostr.is_open() && retries >0) {
			cout << "retrying...\n";
			--retries;
			Sleep((10-retries)*100);
			ostr.open(argv[2], ios_base::trunc|ios_base::out|ios_base::binary);
		}
		cout << "open:" << ostr.is_open() << '\n';
		if (ostr.is_open()) {
			ostr.write((char*)&((*exefile)[0]), exefile->size());
			if (ostr.fail())
				cout << "error writing\n";
			ostr.close();
			cout << "rewritten!\n";
		}
		string program(argv[2]);
		vector<string> args;
		args.push_back("--delete");
		args.push_back(argv[0]);
		cout << program << '\n';
		int run = RunDirect(program, &args, "", RF_NEW_CONSOLE);
		return run;
	};


	try {
		thebuildstring = string(BUILD_STRING);
		stringstream sstamp;
		
		int month, day, year;
		{
			char temp [] = __DATE__;
			unsigned char i;

			// ANSI C Standard month names
			const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
			year = atoi(temp + 7);
			*(temp + 6) = 0;
			day = atoi(temp + 4);
			*(temp + 3) = 0;
			for (i = 0; i < 12; i++) {
				if (!strcmp(temp, months[i])) {
					month = i + 1;
				}
			}
		}

		string tstamp(__TIME__);
		BOOST_FOREACH(char& chr, tstamp)
			if (chr == ':') chr = '_';

		sstamp << year << '_' << month << '_' << day << '_' << tstamp;

		size_t pos = thebuildstring.find("$(TIMESTAMP)");
		if (pos != string::npos) {
			thebuildstring.erase(pos, strlen("$(TIMESTAMP)"));
			thebuildstring.insert(pos, sstamp.str());
		}
	} catch (...) {
	}

	SimpleBackend backend;

	QTMain gui(argc, argv);

	gui.BindEvents(&backend);

	backend.slot_refresh_shares();

	cout << "Build: " << thebuildstring << '\n';
	cout << "Signed: " << (hassignedbuild ? "yes" : "no") << '\n';

	int ret = gui.run();

	terminating = true;

	// hax...
	exit(ret);

	return ret;
}

int main( int argc, char **argv ) {
	try {
		return imain(argc, argv);
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	} catch (...) {
		cout << "unknown exception\n";
	}
}
