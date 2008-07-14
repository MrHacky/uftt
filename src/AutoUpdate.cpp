#include "AutoUpdate.h"

#include <boost/filesystem.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "Platform.h"

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>

using namespace std;

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

		SignatureChecker(boost::asio::io_service& service_, shared_vec file_, const std::string bstring_, const boost::function<void(bool)>& handler_, bool signifneeded_ = false)
			: service(service_), file(file_), handler(handler_), bstring(bstring_), signifneeded(signifneeded_)
		{
		}

		void operator()() {
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
						ofstream sexe("uftt-signed.exe", ios_base::out|ios_base::binary);
						sexe.write((char*)&((*file)[0]), file->size());
						sexe.close();
					} else {
						cout << "bah! signing failed, private/public key mismatch.\n";
					}
				} else
					cout << "bah! signing failed for some reason.\n";
			} else
				cout << "no! this is not a signed binary!\n";

			service.post(boost::bind(handler, hassignedbuild));
		}
	};

	struct checkfile_helper {
		AutoUpdater* updater;
		services::diskio_filetype file;
		boost::shared_ptr<std::vector<uint8> > filedata;
		std::string buildstring;
		bool signifneeded;
		services::diskio_service& disk_service;
		boost::function<void(string,shared_vec)> cb_add;
		checkfile_helper(AutoUpdater* updater_, services::diskio_service& disk_service_, size_t len, const std::string& bstring, bool signifneeded_)
			: updater(updater_), file(disk_service_), filedata(new std::vector<uint8>(len)), buildstring(bstring), signifneeded(signifneeded_), disk_service(disk_service_)
		{
		}

		static void open_handler(boost::shared_ptr<checkfile_helper> helper, const boost::system::error_code& e)
		{
			if (e) {
				cout << "Failed to open file\n";
				return;
			}
			boost::asio::async_read(helper->file, GETBUF(helper->filedata),
				boost::bind(&checkfile_helper::read_handler, helper, _1, _2));
		}

		static void read_handler(boost::shared_ptr<checkfile_helper> helper, const boost::system::error_code& e, size_t len)
		{
			if (e) {
				cout << "Failed to read file\n";
				return;
			}
			helper->disk_service.get_work_service().post(SignatureChecker(helper->disk_service.get_io_service(), helper->filedata, helper->buildstring,
				boost::bind(&checkfile_helper::sign_handler, helper, _1), helper->signifneeded));
		}

		static void sign_handler(boost::shared_ptr<checkfile_helper> helper, bool issigned)
		{
			if (issigned) {
				cout << "Signed: Yes\n";
				helper->cb_add(helper->buildstring, helper->filedata);
			} else
				cout << "Signed: No\n";
		}
	};

	void removefile_helper(boost::shared_ptr<boost::asio::deadline_timer> timer, const boost::filesystem::path& target, int retries, const boost::system::error_code& e = boost::system::error_code())
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

		if (boost::filesystem::exists(target)) {
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

bool checksigniaturex(const std::vector<uint8>& file, const std::string& bstring_expect) {
	return checksigniature(file, bstring_expect);
}

int AutoUpdater::replace(const boost::filesystem::path& source, const boost::filesystem::path& target)
{
	boost::uintmax_t todomax = boost::filesystem::file_size(source);
	cout << "Source size: " << todomax << "\n";
	uint32 todo = boost::numeric_cast<uint32>(todomax); // throws when out of range
	if (todo == 0)
		throw std::runtime_error("Invalid file size");

	std::vector<uint8> filedata(todo);
	{
		ifstream istr(source.native_file_string().c_str(), ios_base::in|ios_base::binary);
		istr.read((char*)&filedata[0], todo);
		uint32 read = istr.gcount();
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
			ofstream ostr;
			ostr.open(target.native_file_string().c_str(), ios_base::trunc|ios_base::out|ios_base::binary);
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

void AutoUpdater::remove(boost::asio::io_service& result_service, boost::asio::io_service& work_service, const boost::filesystem::path& target)
{
	boost::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer(work_service));
	removefile_helper(timer, target, retry_max);
}

void AutoUpdater::checkfile(services::diskio_service& disk_service, boost::asio::io_service& result_service, boost::asio::io_service& work_service, const boost::filesystem::path& target, const std::string& bstring, bool signifneeded)
{
	try {
		boost::uintmax_t todomax = boost::filesystem::file_size(target);
		cout << "Checking '" << target << "' for signiature (size=" << todomax << ")\n";
		uint32 todo = boost::numeric_cast<uint32>(todomax); // throws when out of range
		boost::shared_ptr<checkfile_helper> helper(new checkfile_helper(this, disk_service, todo, bstring, signifneeded));
		helper->cb_add = boost::bind(&AutoUpdater::addBuild, this, _1, _2);
		disk_service.async_open_file(target, services::diskio_filetype::in, helper->file,
			boost::bind(&checkfile_helper::open_handler, helper, boost::asio::placeholders::error));
	} catch (std::exception& e) {
		cout << "Checking failed: " << e.what() << '\n';
	}
}

boost::shared_ptr<std::vector<uint8> > AutoUpdater::getBuildExecutable(const std::string& buildname) const
{
	std::map<std::string, boost::shared_ptr<std::vector<uint8> > >::const_iterator iter;
	iter = filedata.find(buildname);
	if (iter == filedata.end())
		return boost::shared_ptr<std::vector<uint8> >(); // NULL
	else
		return iter->second;
}

const std::vector<std::string>& AutoUpdater::getAvailableBuilds() const
{
	return buildstrings;
}

void AutoUpdater::addBuild(std::string name, shared_vec data)
{
	buildstrings.push_back(name);
	filedata[name] = data;
	newbuild(name);
}

