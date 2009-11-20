#ifndef ASIO_HTTP_REQUEST_H
#define ASIO_HTTP_REQUEST_H

//
// based on async_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//                         Simon B. Sasburg
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "../Types.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

namespace boost { namespace asio {

class http_request
{
	public:
		http_request(boost::asio::io_service& io_service)
		: resolver(io_service)
		, socket(io_service)
		, totalsize(0)
		{
		}

		void async_http_request(
			const std::string& server, const std::string& path, uint16 port_,
			boost::function<void(int, boost::system::error_code)> handler_)
		{
			handlerfunc = handler_;
			doconnect(server, path, port_);
		}

		void async_http_request(
			const std::string& url,
			boost::function<void(int, boost::system::error_code)> handler_)
		{
			handlerfunc = handler_;

			if (url.size() < 7 || std::string(url.begin(), url.begin()+7) != "http://") {
				get_io_service().post(boost::bind(&http_request::handler, this, -1, boost::system::posix_error::make_error_code(boost::system::posix_error::bad_address)));
				return;
			}

			std::string hoststr = url.substr(7);
			std::string::size_type slashpos = hoststr.find_first_of('/');
			std::string portstr;
			std::string substr;
			if (slashpos == std::string::npos) {
				substr = "/";
			} else {
				substr = hoststr.substr(slashpos);
				hoststr.erase(slashpos);
			}
			std::string::size_type colonpos = hoststr.find_first_of(':');
			uint16 iport;
			if (colonpos == std::string::npos)
				iport = 80;
			else {
				portstr = hoststr.substr(colonpos+1);
				hoststr.erase(colonpos);
				try {
					iport = boost::lexical_cast<int>(portstr);
				} catch (...) {
					get_io_service().post(boost::bind(&http_request::handler, this, -1, boost::system::posix_error::make_error_code(boost::system::posix_error::bad_address)));
					return;
				}
			}

			doconnect(hoststr, substr, iport);
		}

		void doconnect(const std::string& server, const std::string& path, uint16 port)
		{
			// Use 'Connection: close' so EOF equals the end of the data
			std::ostream request_stream(&request);
			request_stream << "GET " << path << " HTTP/1.0\r\n";
			request_stream << "Host: " << server << ':' << port << "\r\n";
			request_stream << "Accept: */*\r\n";
			request_stream << "Connection: close\r\n\r\n";

			boost::asio::ip::tcp::resolver::query query(server, boost::lexical_cast<std::string>(port));

			resolver.async_resolve(query,
				boost::bind(&http_request::handle_resolve, this, boost::asio::placeholders::error, boost::asio::placeholders::iterator)
			);
		}

		const std::vector<uint8> & getContent() {
			return content;
		}

		int getTotalSize() {
			return totalsize;
		}

		boost::asio::io_service& get_io_service() {
			return socket.get_io_service();
		}

	private:
		void handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
		{
			if (!err) {
				// try connecting until we succeed
				boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
				socket.async_connect(endpoint,
					boost::bind(&http_request::handle_connect, this, boost::asio::placeholders::error, ++endpoint_iterator)
				);
			} else {
				// report error
				handler(-1, err);
			}
		}

		void handle_connect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
		{
			if (!err) {
				// The connection was successful. Send the request.
				boost::asio::async_write(socket, request,
					boost::bind(&http_request::handle_write_request, this, boost::asio::placeholders::error)
				);
			}
			else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator())
			{
				// The connection failed. Try the next endpoint in the list.
				socket.close();
				handle_resolve(boost::system::error_code() ,endpoint_iterator);
			} else {
				// report error
				handler(-1, err);
			}
		}

		void handle_write_request(const boost::system::error_code& err)
		{
			if (!err) {
				// Read the response status line.
				boost::asio::async_read_until(socket, response, "\r\n",
					boost::bind(&http_request::handle_read_status_line, this,boost::asio::placeholders::error)
				);
			} else {
				// report error
				handler(-1, err);
			}
		}

		void handle_read_status_line(const boost::system::error_code& err)
		{
			if (!err) {
				// Check that response is OK.
				std::istream response_stream(&response);
				std::string http_version;
				unsigned int status_code;

				response_stream >> http_version;
				response_stream >> status_code;

				if (status_code != 200)
				{
					// Report error
					std::cout << "Response returned with status code " << status_code << "\n";
					handler(-1, boost::system::posix_error::make_error_code(boost::system::posix_error::bad_message));
					return;
				}

				std::string status_message;
				std::getline(response_stream, status_message);
				if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
					std::cout << "Invalid response\n";
					handler(-1, boost::system::posix_error::make_error_code(boost::system::posix_error::bad_message));
					return;
				}

				// Read the response headers, which are terminated by a blank line.
				boost::asio::async_read_until(socket, response, "\r\n\r\n",
					boost::bind(&http_request::handle_read_headers, this, boost::asio::placeholders::error)
				);
			} else {
				//report error
				handler(-1, err);
			}
		}

		void handle_read_headers(const boost::system::error_code& err)
		{
			if (!err) {
				// Process the response headers.
				std::istream response_stream(&response);
				std::string header;
				while (std::getline(response_stream, header) && header != "\r") {
					if (boost::algorithm::istarts_with(header, "Content-Length: ")) {
						std::string num(header.begin()+16, header.end()-1);
						try {
							totalsize = boost::lexical_cast<int>(num);
						} catch (...) {
							totalsize = 0;
						}
					}
				}

				// Write whatever content we already have to output.
				if (response.size() > 0) {
					std::stringstream ss;
					ss << &response;

					for (uint i = 0; i < ss.str().size(); ++i)
						content.push_back(ss.str()[i]);

					// report progress
					handler(ss.str().size(), boost::system::error_code());
				}

				// Start reading remaining data until EOF.
				handle_read_content(boost::system::error_code(), 0);
			} else {
				// report error
				handler(-1, err);
			}
		}

		void handle_read_content(const boost::system::error_code& err, size_t len)
		{
			if (!err) {
				// report progress
				handler(len, boost::system::error_code());

				content.insert(content.end(), bigbuf, bigbuf + len);

				// Continue reading remaining data until EOF.
				socket.async_receive(boost::asio::buffer(bigbuf),
					boost::bind(&http_request::handle_read_content, this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
				);
			} else if (err != boost::asio::error::eof) {
				// report error
				handler(-1, err);
			} else {
				// report we are done
				handler(-1, boost::system::error_code());
			}
		}

		void handler(int code, const boost::system::error_code& err) {
			if (!handlerfunc) return;

			handlerfunc(code, err);

			if (code == -1) {
				// called handler for the last time
				// in case clearing the handler triggers the destruction of the http_request object,
				// make sure handlerfunc is already cleared by keeping a copy of the handler object
				// alive by passing it to a function to be called later
				get_io_service().post(boost::bind(&http_request::clearhandler, handlerfunc));
				handlerfunc.clear();
			}
		}

		static void clearhandler(const boost::function<void(int, boost::system::error_code)>& h)
		{
			//h.clear();
		}

		uint8 bigbuf[100*1024];
		boost::asio::ip::tcp::resolver resolver;
		boost::asio::ip::tcp::socket socket;
		boost::asio::streambuf request;
		boost::asio::streambuf response;
		boost::function<void(int, boost::system::error_code)> handlerfunc;
		std::vector<uint8> content;
		uint32 totalsize;
};

} }

#endif//ASIO_HTTP_REQUEST_H
