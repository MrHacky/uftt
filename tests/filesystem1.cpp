#include <iostream>
#include <string>

#include <boost/version.hpp>

#if BOOST_VERSION < 104400
// boost version too old for this test, do nop instead
int main(int argc, char** argv)
{
	std::cout << "NOP\n";
	return 0;
}
#else

// This test compares ext::filesystem::path behaviour with boost::filesystem::path (v3) behaviour
// It does this with a sort of brute-force approach: generating a lot of strings, containing
// combinations of characters that have special meaning in paths: '/\:.', and letters 'cb' to
// get some path components. Operations are performed using both path libraries, and the results
// should be identical. This assumes boost FS3 results are 'correct'. And in a way also tests
// boost behaviour. These tests will be particularly helpful when ext::filesystem starts
// to depend less on boost::filesystem internally for the operations which don't need any
// (unicode) character conversion. This test uses only ascii characters and their 8 bit encoding
// is the same in all codepages, so those conversions aren't tested either.

#define BOOST_FILESYSTEM_VERSION 3

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include "../src/util/Filesystem.h"

std::string efps, bfps;

int one() {
	std::cout << "EFPS=" << efps << "\n";
	std::cout << "BFPS=" << bfps << "\n";
	return 1;
}

int main(int argc, char** argv)
{
	std::cout << "Using boost::filesystem from boost " << BOOST_VERSION << " as baseline for comparision\n";
	std::vector<std::string> items;
	std::vector<std::string> itemsmore;
	std::string chars = "/\\:cb.";

	std::string item;
	size_t pos = 0;
	do {
		if (item.size() < 4)
			items.push_back(item);
		itemsmore.push_back(item);
		pos = 0;
		while (pos < item.size()) {
			size_t cpos = chars.find(item[pos]);
			if (cpos != chars.size()-1) {
				item[pos] = chars[cpos+1];
				pos = 0xffff;
				continue;
			}
			item[pos] = chars[0];
			++pos;
		}
		if (pos != 0xffff)
			item.push_back(chars[0]);
	} while (item.size() < 6);

	items.push_back("name with spaces");
	items.push_back("//server/share");
	items.push_back("http://127.0.0.1/location/file.html");

	BOOST_FOREACH(const std::string& s1, items) {
		ext::filesystem::path efp1(s1);
		boost::filesystem::path bfp1(s1);

		BOOST_FOREACH(const std::string& s2, items) {
			ext::filesystem::path efp2 = efp1 / s2;
			boost::filesystem::path bfp2 = bfp1 / s2;

			efps = efp2.string();
			bfps = bfp2.generic_string();

			if (efps != bfps) return one();
		}
	}

	BOOST_FOREACH(const std::string& s1, itemsmore) {
		ext::filesystem::path efp1(s1);
		boost::filesystem::path bfp1(s1);

		if (efps.empty() != bfps.empty()) return one();

		efps = efp1.string();
		bfps = bfp1.generic_string();
		if (efps != bfps) return one();

		efps = efp1.filename();
		bfps = bfp1.leaf().generic_string();
		if (efps != bfps) return one();

		efps = efp1.parent_path().string();
		bfps = bfp1.branch_path().generic_string();
		if (efps != bfps) return one();

		// This test must be last because boost::filesystem::path::normalize modifies its path
		efps = efp1.normalize().string();
		bfps = bfp1.normalize().generic_string();
		if (efps != bfps) return one();
	}

	// Verify ext::filesystem::path::normalize doesn't modify its path
	const ext::filesystem::path efp1("foo/bar/../barfoo");
	if (efp1.normalize().string() != "foo/barfoo") return one();

	return 0;
}

#endif
