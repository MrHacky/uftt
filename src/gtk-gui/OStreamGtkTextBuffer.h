#ifndef OSTREAM_GTK_TEXTBUFFER_H
	#define OSTREAM_GTK_TEXTBUFFER_H

	#include <string>
	#include <iostream>
	#include <streambuf>
	#include <boost/function.hpp>
	#include <gtkmm/textbuffer.h>
	#include "dispatcher_marshaller.h"

class OStreamGtkTextBuffer : public std::basic_streambuf<char>, public Gtk::TextBuffer {
	public:
		OStreamGtkTextBuffer(std::ostream& stream) : src_stream(stream) {
			src_buffer = src_stream.rdbuf();
			src_stream.rdbuf(this);
		}

		~OStreamGtkTextBuffer() {
			src_stream.rdbuf(src_buffer);
		}

	protected:
		virtual int_type overflow(int_type v) { // Important for flushing and the use of std::endl as newline
			current_line += v;
			if (v == '\n') {
				dispatcher.invoke(boost::bind(&OStreamGtkTextBuffer::_append, this, current_line));
				current_line.clear();
			}
			return v;
		}

		virtual std::streamsize xsputn(const char* p, std::streamsize n) {
			current_line += std::string(p,p+n);
			if(current_line.find("\n") != std::string::npos) {
				dispatcher.invoke(boost::bind(&OStreamGtkTextBuffer::_append, this, current_line));
				current_line.clear();
			}
			return n;
		}

	private:
		void _append(const std::string str) {
			Gtk::TextBuffer::iterator end = this->end();
			insert(end, str);
		}
		std::string          current_line;
		std::ostream&        src_stream;
		std::streambuf*      src_buffer;
		DispatcherMarshaller dispatcher;
};

#endif // OSTREAM_GTK_TEXTBUFFER_H
