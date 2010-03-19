#ifndef STR_FORMAT_H
#define STR_FORMAT_H

#include <string>
#include <../Types.h>
#include <boost/format.hpp>

/**
 * convenience wrapper for boost::format
 *
 * @param fmt format string
 * @param ... values to fill in
 * @return formatted std::string
 *
 * @see http://www.boost.org/libs/format/doc/format.html
 *      for formatting info etc (its mostly compatible with printf, though)
 */
#define STRFORMAT(fmt, ...) \
	((StrFormat::detail::macro_str_format(fmt), ## __VA_ARGS__).str())

namespace StrFormat {
	namespace detail {
		// dont allow this class easily into normal namespace
		// it has dangerous overload for comma operator
		// so hide it in detail subnamespace
		class macro_str_format {
			private:
				boost::format bfmt;
				macro_str_format();
			public:
				macro_str_format(const std::string & fmt)
					: bfmt(fmt) {};
				template<typename T>
						macro_str_format& operator,(const T & v) {
					bfmt % v;
					return *this;
				}
				std::string str() {
					return bfmt.str();
				}
		};
	} // namespace detail

	inline std::string bytes(uint64 n, bool kibi = false)
	{
		static const char* size_suffix[] =
		{
			"" ,
			"K",
			"M",
			"G",
			"T",
			"P",
			"?"
		};
		const int maxsuf = (sizeof(size_suffix) / sizeof(size_suffix[0])) - 1;

		int suf;
		double size = (double)n;
		for (suf = 0; size >= 1000.0; ++suf)
			size /= 1024.0;
		suf = std::min(maxsuf, suf);
		int decs = (2 - (size >= 10.0) - (size >= 100.0)) * (suf > 0);
		std::string fstr = STRFORMAT("%%.%df %%s%sB", decs, kibi ? "i" : "");
		return STRFORMAT(fstr, (float)size, size_suffix[suf]);
	};

#if 0
	// wip optimised non-float uselessness
	template<int startsuf>
	string size_string_impl_32(uint32 size)
	{
		// too much time on my hands :O
		char buf[7];
		int bufpos = 0;
		int decpartbits = 0;
		int suf = startsuf;
		for (; (size >> decpartbits) >= 1000; ++suf)
			decpartbits += 10; // 2^10 = 1024

		int intpart = (size >> decpartbits);

		int decs = 2;
		if (intpart >= 10) {
			if (intpart >= 100) {
				int num = intpart / 100;
				intpart = intpart % 100;
				buf[bufpos++] = ("0123456789"[num]);
				--decs;
			}
			int num = intpart / 10;
			intpart = intpart % 10;
			buf[bufpos++] = ("0123456789"[num]);
			--decs;
		}
		buf[bufpos++] = ("0123456789"[intpart]);

		if (decs != 0 && suf != 0) {
			buf[bufpos++] = ('.');
			int decpart = (size >> (decpartbits-7)) & 0x7F;
			if (decs == 2) {
				decpart *= 10;
				int num = decpart / (0x7F);
				decpart = decpart % (0x7F);
				buf[bufpos++] = ("0123456789"[num]);
			}
			decpart *= 10;
			int num = decpart / (0x7F);
			buf[bufpos++] = ("0123456789"[num]);
		}

		buf[bufpos++] = ' ';
		if (suf > 5) suf = 5;
		if (suf > 0) buf[bufpos++] = (" KMGT?"[suf]);
		buf[bufpos++] = ('B');
		return std::string(buf, buf+bufpos);
	};


	string size_string(const uint64& size)
	{
		if (!(size > 0xffffffff)) {
			return size_string_impl_32<0>((uint32)size);
		} else {
			return size_string_impl_32<3>((uint32)(size>>30));
		}
	}

	string size_string(const uint32& size)
	{
		return size_string_impl_32<0>(size);
	}
#endif

} //namespace strformat

#endif//STR_FORMAT_H
