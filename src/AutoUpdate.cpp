#include "AutoUpdate.h"

// autoupdate only functions when ssl lib is available
#ifdef USE_OPENSSL

#include <boost/filesystem.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <boost/random/variate_generator.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_smallint.hpp>

#include "Platform.h"
#include "Globals.h"

#include <iostream>
#include <fstream>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "util/Base64.h"
#include "util/Filesystem.h"


using namespace std;

/*
	// Generate keys snippet
	RSA* rsapriv = RSA_generate_key(4096, 65537, NULL, NULL);;

	{
		FILE* privfile = fopen("c:\\temp\\uftt.private.key.web.dat", "wb");
		if (privfile) {
			PEM_write_RSAPrivateKey(privfile, rsapriv, NULL, NULL, 0, NULL, NULL);
			fclose(privfile);
		}
		FILE* pubfile = fopen("c:\\temp\\uftt.private.key.web.pub.dat", "wb");
		if (pubfile) {
			PEM_write_RSAPublicKey(pubfile, rsapriv);
			fclose(pubfile);
		}
	}
*/

namespace {
	const int retry_max(30);

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

	char thepubwebkey[] =
	"-----BEGIN RSA PUBLIC KEY-----\n"
	"MIICCgKCAgEAy2vyvFyVBLEVG2hfEX5dmJivUOETNp717gDyZ3b0FelqjPrVjmW4\n"
	"9+dQ0j77j45r5/iukYZC6leiRpzMK4LWDQ4yg980Ky5cDsnywMnEpcS1w9PnDsLG\n"
	"kb5tO6WgtCqLPtiP5kjUOK6s/3ctE41muoQO0D9Fe/h6FSEhE0XVJe70s94xOng3\n"
	"+9iAZrJ0jB7HTfl0idHEJ1LV7iqUvTEPYTFMVkf6QPCO9j8n9uX1KPqvuF07fQx8\n"
	"XcGcvxorj7YPUBuN0bkectf8kXdmWkmpO8EgsLHcDbyPtq7SByt5IeVf5IUcbqTj\n"
	"vWoVH0HhZSYATg7O7zi2CCWrnqNQX5rUDMEsNOVn8FtGPCor2KBWWa6gc7obyDa+\n"
	"MSB6dCD5OGken/vHpGsWE74JkcSkAp0WmZUNHh5oo1y3rm5wOZEw2IeAn/js1+kZ\n"
	"vLsMgI9TQS5Q92laUuaSwEG4RYbf4iuC6r9SDXP8NNYWcHXo/tN750ElaH6o5aSV\n"
	"UVdTxl8aH5jGikrrrWWirDVxSFaPjgIT0CmYIF9AnfLx0KM2FnX38s/CEBllS4iY\n"
	"6MK+CCkAElMSsBMZaYERhytWi3qxqbUKiI3u28JoqRVkuG4rifietUNHMhf6IhZj\n"
	"WT6HoksG00Qxv/0SO8+CHAKWlj6BGvLTPlwbfO30btlkvqPyj+zaxiUCAwEAAQ==\n"
	"-----END RSA PUBLIC KEY-----\n";

	// returns true when signature checks out
	bool checksigniature(const std::vector<uint8>& file, const std::string& bstring_expect) {
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

		if (sigsize < 512)
			return false;

		if (file.size() < (sigsize*2)+8)
			return false;

		vector<uint8> sig;
		sig.resize(sigsize);
		for (uint i = 0; i < sigsize; ++i)
			sig[i] = file[file.size()-8-sigsize+i];

		uint32 bstrsize = bstring_expect.size();
		if (bstrsize > 0xff)
			return false;

		if (file[file.size()-8-sigsize-1] != bstrsize)
			return false;

		if (file.size() < sigsize+8+1+bstrsize)
			return false;

		std::string bstring_file;
		for (uint i = 0; i < bstrsize; ++i)
			bstring_file.push_back((char)file[file.size()-8-sigsize-1-bstrsize+i]);

		if (bstring_file != bstring_expect)
			return false;

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

	struct SignatureChecker {
		boost::asio::io_service& service;
		shared_vec file;
		boost::function<void(bool)> handler;
		std::string bstring;
		bool signifneeded;
		bool trycompress;

		SignatureChecker(boost::asio::io_service& service_, shared_vec file_, const std::string bstring_, const boost::function<void(bool)>& handler_, bool signifneeded_ = false, bool trycompress_ = false)
			: service(service_), file(file_), handler(handler_), bstring(bstring_), signifneeded(signifneeded_), trycompress(trycompress_)
		{
			if (bstring.find("-deb-") != std::string::npos)
				trycompress = false;
		}

		static void try_compress(shared_vec file)
		{
			//ext::filesystem::path upxexe("D:\\Cygwin\\home\\bin\\upx.exe");
			ext::filesystem::path upxexe("C:\\Temp\\upx.exe");
			ext::filesystem::path tempexe("C:\\Temp\\ufft-temp.exe");

			if (!ext::filesystem::exists(upxexe))
				return;

			{
				ext::filesystem::ofstream wexe(tempexe, ios_base::out|ios_base::binary);
				wexe.write((char*)&((*file)[0]), file->size());
				if (wexe.fail()) return;
				wexe.close();
			}

			{
				vector<string> args;
				//args.push_back("-1");
				args.push_back("--best");
				args.push_back("--lzma");
				args.push_back(tempexe.native_file_string());

				cout << "Compressing now...\n";
				int retval = platform::RunCommand(upxexe.native_file_string(), &args, "", platform::RF_NO_WINDOW|platform::RF_WAIT_FOR_EXIT);
				if (retval != 0) return;
			}

			uint32 nsize = boost::numeric_cast<uint32>(boost::filesystem::file_size(tempexe));
			if (nsize == 0 || nsize >= file->size())
				return;

			{
				shared_vec nvec(new vector<uint8>(nsize));
				ext::filesystem::ifstream rexe(tempexe, ios_base::in|ios_base::binary);
				rexe.read((char*)&((*nvec)[0]), nvec->size());
				if (rexe.fail() || boost::numeric_cast<uint32>(rexe.gcount()) != nsize) return;
				rexe.close();
				file->swap(*nvec);
			}

			cout << "Compressed to " << nsize << " bytes\n";

		};


		void operator()()
		{
			service.post(boost::bind(handler, doCheck(file, bstring, signifneeded, trycompress)));
		}

		static bool doCheck(shared_vec file, const std::string bstring, bool signifneeded = false, bool trycompress = false)
		{
			bool hassignedbuild = false;

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

			if (checksigniature(*file, bstring)) {
				cout << "yay! this is a signed binary!\n";
				hassignedbuild = true;
			} else if (signifneeded && rsapriv && !(bstring.size() > 0xff)) {
				// it's not signed, but we have access to a private key
				// so we can sign it ourselves!

				if (trycompress) try_compress(file);

				// first append buildstring
				for (uint i = 0; i < bstring.size(); ++i)
					file->push_back(bstring[i]);
				file->push_back(bstring.size());

				uint sigsize = EVP_PKEY_size(&evppriv);
				vector<uint8> sig;
				sig.resize(sigsize);

				EVP_MD_CTX signctx;
				EVP_SignInit(&signctx, EVP_sha1());

				int r1 = EVP_SignUpdate(&signctx, &((*file)[0]), file->size());
				int r2 = EVP_SignFinal(&signctx, &sig[0], &sigsize, &evppriv);

				if (r1==1 && r2==1) {

					for (uint i = 0; i < sigsize; ++i)
						file->push_back(sig[i]);
					file->push_back((sigsize >> 0) & 0xff);
					file->push_back((sigsize >> 8) & 0xff);
					file->push_back((sigsize >>16) & 0xff);
					file->push_back((sigsize >>24) & 0xff);
					file->push_back('U');
					file->push_back('F');
					file->push_back('T');
					file->push_back('T');

					if (checksigniature(*file, bstring)) {
						hassignedbuild = true;
						cout << "yay! we just signed this binary!\n";

						// write signed build to file for debug purposes
						std::string fname = "c:\\temp\\uftt-signed";
						if (bstring.find("win32") != std::string::npos) fname = fname + ".exe";
						if (bstring.find("-deb-") != std::string::npos) fname = fname + ".deb.signed";
						ofstream sexe(fname.c_str(), ios_base::out|ios_base::binary);
						sexe.write((char*)&((*file)[0]), file->size());
						sexe.close();
					} else {
						cout << "bah! signing failed, private/public key mismatch.\n";
					}
				} else
					cout << "bah! signing failed for some reason.\n";
			} else
				cout << "no! this is not a signed binary!\n";

			return hassignedbuild;
		}
	};

	struct checkfile_helper {
		AutoUpdater* updater;
		boost::function<void(std::string, ext::filesystem::path, size_t, std::vector<uint8>)> cb_add;

		ext::filesystem::path file;
		std::string buildstring;
		bool signifneeded;

		checkfile_helper(AutoUpdater* updater_, ext::filesystem::path file_, const std::string& buildstring_, bool signifneeded_)
			: updater(updater_), file(file_), buildstring(buildstring_), signifneeded(signifneeded_)
		{
		}

		void operator()()
		{
			try {
				size_t size = boost::numeric_cast<uint32>(boost::filesystem::file_size(file));
				shared_vec filevec(new std::vector<uint8>(size));

				ext::filesystem::ifstream istr(file, ios_base::in|ios_base::binary);
				if (!istr.is_open()) return;

				istr.read((char*)&(*filevec)[0], size);
				uint32 read = boost::numeric_cast<uint32>(istr.gcount());
				if (read != size) return;

				bool ok = SignatureChecker::doCheck(filevec, buildstring, signifneeded, signifneeded);
				size_t dsize = filevec->size();
				if (dsize > 1024*4)
					dsize -= 1024*4;
				else
					dsize = 0;
				filevec->erase(filevec->begin(), filevec->begin() + dsize);
				if (ok)
					cb_add(buildstring, file, size, *filevec);
			} catch (...) {
			}
		}
	};

	void removefile_helper(boost::shared_ptr<boost::asio::deadline_timer> timer, const ext::filesystem::path& target, int retries, const boost::system::error_code& e = boost::system::error_code())
	{
		if (e) {
			cout << "Delete timer error: " << e.message() << '\n';
			return;
		}

		cout << "Trying delete...";
		try {
			boost::filesystem::remove(target);
		} catch (...) {
		}

		if (ext::filesystem::exists(target)) {
			cout << "Failed\n";
			if (retries > 0) {
				--retries;
				timer->expires_from_now(boost::posix_time::milliseconds((retry_max-retries)*100));
				timer->async_wait(boost::bind(&removefile_helper, timer, target, retries, boost::asio::placeholders::error));
			} else
				cout << "Giving up on deleting file: " << target << '\n';
		} else {
			cout << "Done\n";
		}
	}
}

int AutoUpdater::replace(const ext::filesystem::path& source, const ext::filesystem::path& target)
{
	boost::uintmax_t todomax = boost::filesystem::file_size(source);
	cout << "Source size: " << todomax << "\n";
	uint32 todo = boost::numeric_cast<uint32>(todomax); // throws when out of range
	if (todo == 0)
		throw std::runtime_error("Invalid file size");

	std::vector<uint8> filedata(todo);
	{
		ext::filesystem::ifstream istr(source, ios_base::in|ios_base::binary);
		istr.read((char*)&filedata[0], todo);
		uint32 read = boost::numeric_cast<uint32>(istr.gcount());
		if (read != todo)
			throw std::runtime_error("Reading failed");
	}

	int retries = retry_max;
	bool written = false;
	while (!written && retries >0) {
		cout << "Trying write...";
		--retries;
		platform::msSleep((retry_max-retries)*100);
		{
			ext::filesystem::ofstream ostr(target, ios_base::trunc|ios_base::out|ios_base::binary);
			if (ostr.is_open()) {
				ostr.write((char*)&filedata[0], filedata.size());
				if (ostr.fail()) {
					cout << "Failed to write\n";
					cout << "error writing\n";
					cout << "ostr.bad(): " << ostr.bad() << '\n';
					cout << "errno: " << errno << '\n';
				} else
					written = true;
				ostr.flush();
				ostr.close();
			} else
				cout << "Failed to open\n";
		}
		cout << "Done\n";
	}
	cout << flush;
	if (!written)
		throw std::runtime_error("Writing failed");

	vector<string> args;
	args.push_back("--delete");
	args.push_back(source.native_file_string());

	cout << "running: " << target << "\n";
	int run = platform::RunCommand(target.native_file_string(), &args, "", platform::RF_NEW_CONSOLE);
	cout << run << endl;
	return run;
}

void AutoUpdater::remove(boost::asio::io_service& result_service, boost::asio::io_service& work_service, const ext::filesystem::path& target)
{
	boost::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer(work_service));
	removefile_helper(timer, target, retry_max);
}

bool AutoUpdater::isBuildBetter(const std::string& newstr, const std::string& oldstr)
{
	size_t newpos = newstr.find_last_of("-");
	size_t oldpos = oldstr.find_last_of("-");

	string newcfg = newstr.substr(0, newpos);
	string oldcfg = oldstr.substr(0, oldpos);

	if (newcfg != oldcfg) return false;

	string newver = newstr.substr(newpos+1);
	string oldver = oldstr.substr(oldpos+1);

	vector<string> newvervec;
	vector<string> oldvervec;
	boost::split(newvervec, newver, boost::is_any_of("._"));
	boost::split(oldvervec, oldver, boost::is_any_of("._"));

	for (uint i = 0; i < newvervec.size() && i < oldvervec.size(); ++i) {
		int newnum = atoi(newvervec[i].c_str());
		int oldnum = atoi(oldvervec[i].c_str());
		if (oldnum > newnum) return false;
		if (oldnum < newnum) return true;
	}

	if (newvervec.size() != oldvervec.size())
		return false;

	// they are the same....
	return false;
}

bool AutoUpdater::doSelfUpdate(const std::string& buildname, const ext::filesystem::path& target_, const ext::filesystem::path& selfpath)
{
	try {
		ext::filesystem::path target = target_;
		std::cout << "doSelfUpdate: " << target << '\n';
		if (!ext::filesystem::exists(target))
			return false;

		if (buildname.find("-win32.") != std::string::npos) {
			boost::uniform_smallint<> distr(0, 9);
			boost::variate_generator<boost::rand48&, boost::uniform_smallint<> > rand(rng, distr);
			ext::filesystem::path newtarget = target;//.string() + '.' + boost::lexical_cast<std::string>(rand()) + ".exe";
			do {
				char n = '0' + rand();
				newtarget = newtarget.string() + '.' + n + ".exe";
			} while (boost::filesystem::exists(newtarget));
			boost::filesystem::rename(target, newtarget);
			if (!boost::filesystem::exists(newtarget)) return false;
			target = newtarget;
		}

		{
			vector<uint8> newfile;
			uint32 todo = boost::numeric_cast<uint32>(boost::filesystem::file_size(target));
			newfile.resize(todo);
			ext::filesystem::ifstream istr(target, ios_base::in|ios_base::binary);
			if (!istr.is_open()) return false;
			istr.read((char*)&newfile[0], todo);
			uint32 read = boost::numeric_cast<uint32>(istr.gcount());
			if (read != todo) {
				cout << "failed to read the new file\n";
				return false;
			}
			if (!checksigniature(newfile, buildname)) {
				cout << "failed to verify the new file's signiature\n";
				return false;
			}

			if (buildname.find("-deb-") != std::string::npos) {
				std::cout << "Writing .deb package...";
				// TODO: key size always 512? actually read from file to find out
				newfile.resize(newfile.size() - 4 - 4 - 512 - 1 - buildname.size());

				ext::filesystem::path newtarget = target.branch_path() / (buildname +".deb");
				if (newtarget == target) {
					std::cout << "Huh? need different paths!\n";
					return false;
				}

				ext::filesystem::ofstream ostr(newtarget, ios_base::out|ios_base::binary);
				if (!ostr.is_open()) {
					cout << "Failed to open\n";
					return false;
				}

				ostr.write((char*)&newfile[0], newfile.size());
				if (ostr.fail()) {
					cout << "Failed to write\n";
					return false;
				}
				ostr.close();

				if (!ext::filesystem::exists(newtarget)) {
					cout << "Doublecheck failed\n";
					return false;
				}

				boost::filesystem::remove(target);

				std::cout << "write succeeded: " << newtarget << '\n';
				return true;
			}
		}

		string program = target.native_file_string();
		vector<string> args;
		args.push_back("--runtest");
		int test = platform::RunCommand(program, &args, "", platform::RF_NEW_CONSOLE|platform::RF_WAIT_FOR_EXIT);

		if (test != 0) {
			cout << "--runtest failed!\n";
			return false;
		}

		args.clear();
		args.push_back("--replace");
		args.push_back(selfpath.native_file_string());
		int replace = platform::RunCommand(program, &args, "", platform::RF_NEW_CONSOLE);
		return (replace == 0);
	} catch (std::exception& e) {
		cout << "exceptional failure: " << e.what() << '\n';
		return false;
	}
}


std::vector<std::pair<std::string, std::string> > AutoUpdater::parseUpdateWebPage(const std::vector<uint8>& webpage)
{
	std::vector<std::pair<std::string, std::string> > result;
	uint8 content_start[] = "-----BEGIN RSA CHECKED CONTENT-----";
	uint8 content_end[] = "-----END RSA CHECKED CONTENT-----";
	uint8 signiature_start[] = "-----BEGIN RSA SIGNIATURE-----";
	uint8 signiature_end[] = "-----END RSA SIGNIATURE-----";

	typedef std::vector<uint8>::const_iterator vpos;

	vpos cspos = std::search(webpage.begin(), webpage.end(), content_start   , content_start    + sizeof(content_start   ) - 1);
	vpos cepos = std::search(webpage.begin(), webpage.end(), content_end     , content_end      + sizeof(content_end     ) - 1);
	vpos sspos = std::search(webpage.begin(), webpage.end(), signiature_start, signiature_start + sizeof(signiature_start) - 1);
	vpos sepos = std::search(webpage.begin(), webpage.end(), signiature_end  , signiature_end   + sizeof(signiature_end  ) - 1);

	if (cspos == webpage.end() || cepos == webpage.end() || sspos == webpage.end() || sepos == webpage.end())
		return result; // empty result

	cspos += sizeof(content_start) - 1;
	sspos += sizeof(signiature_start) - 1;

	std::vector<uint8> content(cspos, cepos);
	std::vector<uint8> sig(sspos, sepos);

	Base64::filter(&sig);
	sig = Base64::decode(sig);

	if (sig.size() == 0)
		sig.push_back(0); //dummy

	RSA* rsapub = NULL;
	RSA* rsapriv = NULL;

	{
		FILE* privfile = fopen("c:\\temp\\uftt.private.webkey.dat", "rb");
		if (privfile) {
			rsapriv = PEM_read_RSAPrivateKey(privfile, NULL, NULL, NULL);
			fclose(privfile);
		}

		BIO* pubmem = BIO_new_mem_buf(thepubwebkey, -1);
		rsapub = PEM_read_bio_RSAPublicKey(pubmem, NULL, NULL, NULL);
		BIO_free(pubmem);
	}
	EVP_PKEY evppub;
	EVP_PKEY evppriv;

	EVP_PKEY_assign_RSA(&evppub, rsapub);
	EVP_PKEY_assign_RSA(&evppriv, rsapriv);

	bool issigned = true;

	{
		int res;
		EVP_MD_CTX verifyctx;
		res = EVP_VerifyInit(&verifyctx, EVP_sha1());
		if (res != 1) issigned &= false;
		res = EVP_VerifyUpdate(&verifyctx, &content[0], content.size());
		if (res != 1) issigned &= false;
		res = EVP_VerifyFinal(&verifyctx, &sig[0], sig.size(), &evppub);
		if (res != 1) issigned &= false;
	}

	if (issigned) {
		// simple crappy parser
		int state = 0;
		int parse = 0;
		int oparse = 0;
		string href;
		string title;
		string sig;
		string attr;
		string attrval;
		BOOST_FOREACH(uint8 chr, content) {
			if (state == 0 && parse == 0 && chr == '<') ++parse;
			if (state == 0 && parse == 1 && chr == 'a') ++parse;
			if (state == 0 && parse == 1 && chr == 'A') ++parse;
			if (state == 0 && parse == 2 && chr == ' ') ++parse;
			if (state == 0 && parse == 3) {
				state = 1;
				parse = 0;
				oparse = 0;
				href.clear();
				title.clear();
				sig.clear();
				continue;
			}
			if (state == 1 && parse == 0) {
				if ((chr >= 'a' && chr <='z') || (chr >= 'A' && chr <= 'Z')) {
					attr.clear();
					attrval.clear();
					state =6;
				}
			}
			if (state == 6 && chr != '=' && chr !='"') attr.push_back(chr);
			if (state == 6 && chr == '=') {
				state = 7;
				parse = 0;
				oparse = 0;
				continue;
			}
			if (state == 7 && chr == '"') {
				state = 8;
				parse = 0;
				oparse = 0;
				continue;
			}
			if (state == 8 && chr != '"') attrval.push_back(chr);
			if (state == 8 && chr == '"') {
				if (attr == "sig") sig = attrval;
				if (attr == "href") href = attrval;
				state = 1;
				parse = 0;
				oparse = 0;
				continue;
			}

			if (state == 1 && chr == '>') {
				state = 4;
				parse = 0;
				oparse = 0;
				continue;
			}

			if (state == 4 && chr != '<') title.push_back(chr);
			if (state == 4 && chr == '<') {
				state = 5;
				parse = 0;
				oparse = 0;
				continue;
			}

			if (state == 5 && parse == 0 && chr == '/') ++parse;
			if (state == 5 && parse == 1 && chr == 'a') ++parse;
			if (state == 5 && parse == 1 && chr == 'A') ++parse;
			if (state == 5 && parse == 2 && chr == '>') ++parse;
			if (state == 5 && parse == 3) {
				if (!href.empty() && !title.empty()) {
					result.push_back(pair<string, string>(title, href));
				}
				state = 0;
				parse = 0;
				oparse = 0;
				continue;
			}

			if (oparse == parse)
				parse = 0;
			oparse = parse;
		}
	} else if (rsapriv) {
		uint sigsize = EVP_PKEY_size(&evppriv);
		sig.resize(sigsize);

		EVP_MD_CTX signctx;
		EVP_SignInit(&signctx, EVP_sha1());

		int r1 = EVP_SignUpdate(&signctx, &content[0], content.size());
		int r2 = EVP_SignFinal(&signctx, &sig[0], &sigsize, &evppriv);

		if (r1 == 1 && r2 == 1) {
			sig = Base64::encode(sig);
			sig.push_back('\n');
			sig.push_back('\0');
			cout << signiature_start << '\n';
			cout << &sig[0];
			cout << signiature_end << '\n';
		}
	}


//				BIO* pubmem = BIO_new_mem_buf(thepubkey, -1);
//				rsapub = PEM_read_bio_RSAPublicKey(pubmem, NULL, NULL, NULL);


	return result;
}

bool AutoUpdater::doSigning(const ext::filesystem::path& kfpath, const std::string& bstring, const ext::filesystem::path& ifpath, const ext::filesystem::path& ofpath)
{
	ext::filesystem::ifstream ifile(ifpath, ios_base::in|ios_base::binary);
	ext::filesystem::ofstream ofile(ofpath, ios_base::out|ios_base::binary);

	if (!ifile.is_open() || !ofile.is_open())
		return false;

	boost::uintmax_t todomax = boost::filesystem::file_size(ifpath);
	uint32 todo = boost::numeric_cast<uint32>(todomax); // throws when out of range
	if (todo == 0)
		return false;

	std::vector<uint8> file(todo);
	{
		ifile.read((char*)&file[0], todo);
		uint32 read = boost::numeric_cast<uint32>(ifile.gcount());
		if (read != todo)
			return false;
		if (ifile.fail())
			return false;
		ifile.close();
	}

	RSA* rsapriv = NULL;

	{
		FILE* privfile = ext::filesystem::fopen(kfpath, "rb");
		if (!privfile)
			return false;
		rsapriv = PEM_read_RSAPrivateKey(privfile, NULL, NULL, NULL);
		fclose(privfile);
	}

	EVP_PKEY evppriv;

	EVP_PKEY_assign_RSA(&evppriv, rsapriv);

	// first append buildstring
	for (uint i = 0; i < bstring.size(); ++i)
		file.push_back(bstring[i]);
	file.push_back(bstring.size());

	uint sigsize = EVP_PKEY_size(&evppriv);
	vector<uint8> sig;
	sig.resize(sigsize);

	EVP_MD_CTX signctx;
	EVP_SignInit(&signctx, EVP_sha1());

	int r1 = EVP_SignUpdate(&signctx, &file[0], file.size());
	int r2 = EVP_SignFinal(&signctx, &sig[0], &sigsize, &evppriv);

	if (r1==1 && r2==1) {

		for (uint i = 0; i < sigsize; ++i)
			file.push_back(sig[i]);
		file.push_back((sigsize >> 0) & 0xff);
		file.push_back((sigsize >> 8) & 0xff);
		file.push_back((sigsize >>16) & 0xff);
		file.push_back((sigsize >>24) & 0xff);
		file.push_back('U');
		file.push_back('F');
		file.push_back('T');
		file.push_back('T');

		if (checksigniature(file, bstring)) {
			cout << "yay! we just signed this binary!\n";

			ofile.write((char*)&file[0], file.size());
			//if (ofile.gcount() != file.size())
			//	return false;
			if (ofile.fail())
				return false;
			ofile.close();
		} else {
			cout << "bah! signing failed, private/public key mismatch.\n";
			return false;
		}
	} else {
		cout << "bah! signing failed for some reason.\n";
		return false;
	}

	return true;
}

void AutoUpdater::checkfile(boost::asio::io_service& service, const ext::filesystem::path& target, const std::string& bstring, bool signifneeded)
{
	checkfile_helper helper(this, target, bstring, signifneeded);
	helper.cb_add = service.wrap(boost::bind(&AutoUpdater::addBuild, this, _1, _2, _3, _4));
	boost::thread thrd(helper);
}

ext::filesystem::path AutoUpdater::getUpdateFilepath(const std::string& buildname) const
{
	std::map<std::string, boost::shared_ptr<buildinfo> >::const_iterator iter;
	iter = filedata.find(buildname);
	if (iter == filedata.end())
		return "";

	boost::shared_ptr<std::vector<uint8> > buf = getUpdateBuffer(buildname);
	if (!buf) return "";
	return iter->second->path;
}

boost::shared_ptr<std::vector<uint8> > AutoUpdater::getUpdateBuffer(const std::string& buildname) const
{
	boost::shared_ptr<std::vector<uint8> > null;

	std::map<std::string, boost::shared_ptr<buildinfo> >::const_iterator iter;
	iter = filedata.find(buildname);
	if (iter == filedata.end())
		return null;

	try {
		boost::shared_ptr<buildinfo> info = iter->second;
		uint64 size = boost::filesystem::file_size(info->path);
		if (size != info->len) return null;
		ext::filesystem::ifstream ifs(info->path, ios_base::in|ios_base::binary);
		boost::shared_ptr<std::vector<uint8> > res(new std::vector<uint8>(info->len));
		ifs.read((char*)&(*res)[0], info->len);
		if (ifs.gcount() != info->len) return null;
		if (info->len <= info->sig.size()) return null;
		for (size_t i = 0; i < info->sig.size(); ++i)
			if (info->sig[i] != (*res)[i + res->size() - info->sig.size()])
				return null;
		return res;
	} catch (std::exception& /*e*/) {
		return null;
	}
}

const std::vector<std::string>& AutoUpdater::getAvailableBuilds() const
{
	return buildstrings;
}

void AutoUpdater::addBuild(const std::string& build, boost::shared_ptr<buildinfo> info)
{
	buildstrings.push_back(build);
	filedata[build] = info;
	newbuild(build);
}

void AutoUpdater::addBuild(const std::string& build, const ext::filesystem::path& path, size_t len, const std::vector<uint8>& sig)
{
	boost::shared_ptr<buildinfo> info(new buildinfo());
	info->path = path;
	info->len = len;
	info->sig = sig;
	addBuild(build, info);
}

#else//USE_OPENSSL

int AutoUpdater::replace(const ext::filesystem::path& source, const ext::filesystem::path& target)
{
	// TODO: we could enable this even without SSL?
	return -1;
}

void AutoUpdater::remove(boost::asio::io_service& result_service, boost::asio::io_service& work_service, const ext::filesystem::path& target)
{
	// TODO: we could enable this even without SSL?
}

bool AutoUpdater::isBuildBetter(const std::string& newstr, const std::string& oldstr)
{
	return false;
}

bool AutoUpdater::doSelfUpdate(const std::string& buildname, const ext::filesystem::path& target, const ext::filesystem::path& selfpath)
{
	return false;
}

std::vector<std::pair<std::string, std::string> > AutoUpdater::parseUpdateWebPage(const std::vector<uint8>& webpage)
{
	return std::vector<std::pair<std::string, std::string> >();
}

bool AutoUpdater::doSigning(const ext::filesystem::path& keyfile, const std::string& build, const ext::filesystem::path& infile, const ext::filesystem::path& outfile)
{
	return false;
}

void AutoUpdater::checkfile(boost::asio::io_service& service, const ext::filesystem::path& target, const std::string& bstring, bool signifneeded)
{
}

boost::shared_ptr<std::vector<uint8> > AutoUpdater::getUpdateBuffer(const std::string& buildname) const
{
	return boost::shared_ptr<std::vector<uint8> >();
}

ext::filesystem::path AutoUpdater::getUpdateFilepath(const std::string& buildname) const
{
	return "";
}

const std::vector<std::string>& AutoUpdater::getAvailableBuilds() const
{
	return buildstrings;
}

#endif//USE_OPENSSL(else)
