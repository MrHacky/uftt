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
#include "../types.h"
namespace boost { namespace asio {

class http_request
{
public:
	http_request(boost::asio::io_service& io_service,
      const std::string& server, const std::string& path, uint16 port_ = 80)
    : resolver_(io_service),
      socket_(io_service)
	{
		doconnect(server, path, port_);
	}

	http_request(boost::asio::io_service& io_service, const std::string& url)
    : resolver_(io_service),
      socket_(io_service)
	{
		using std::string;
		if (string(url.begin(), url.begin()+7) != "http://")
			throw std::runtime_error("not a valid url");

	string hoststr = url.substr(7);
	string::size_type colonpos = hoststr.find_first_of(':');
	string::size_type slashpos = hoststr.find_first_of('/');
	string portstr;
	string substr;
	if (slashpos == string::npos) {
		substr = "/";
	} else {
		substr = hoststr.substr(slashpos);
		hoststr.erase(slashpos);
	}
	uint16 iport;
	if (colonpos == string::npos)
		iport = 80;
	else {
		portstr = hoststr.substr(colonpos+1);
		iport = atoi(portstr.c_str());
		hoststr.erase(colonpos);
	}

		doconnect(hoststr, substr, iport);
	}

	void doconnect(const std::string& server, const std::string& path, uint16 port_)
	{
		port = port_;
    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    std::ostream request_stream(&request_);
    request_stream << "GET " << path << " HTTP/1.0\r\n";
	request_stream << "Host: " << server << ':' << port << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    boost::asio::ip::tcp::resolver::query query(server, "http");
    resolver_.async_resolve(query,
        boost::bind(&http_request::handle_resolve, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::iterator));
  }

	void setHandler(boost::function<void(boost::system::error_code)> handler_) {
		handler = handler_;
	}

	const std::vector<uint8> & getContent() {
		return content;
	}


	boost::asio::io_service& get_io_service() {
		return socket_.get_io_service();
	}

private:
  void handle_resolve(const boost::system::error_code& err,
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
  {
    if (!err)
    {
      // Attempt a connection to the first endpoint in the list. Each endpoint
      // will be tried until we successfully establish a connection.
      boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
	  endpoint = boost::asio::ip::tcp::endpoint(endpoint.address(), port);
	  //endpoint.port = port;
      socket_.async_connect(endpoint,
          boost::bind(&http_request::handle_connect, this,
            boost::asio::placeholders::error, ++endpoint_iterator));
    }
    else
    {
      std::cout << "Error: " << err.message() << "\n";
    }
  }

  void handle_connect(const boost::system::error_code& err,
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
  {
    if (!err)
    {
      // The connection was successful. Send the request.
      boost::asio::async_write(socket_, request_,
          boost::bind(&http_request::handle_write_request, this,
            boost::asio::placeholders::error));
    }
    else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator())
    {
      // The connection failed. Try the next endpoint in the list.
      socket_.close();
      boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
      socket_.async_connect(endpoint,
          boost::bind(&http_request::handle_connect, this,
            boost::asio::placeholders::error, ++endpoint_iterator));
    }
    else
    {
      std::cout << "Error: " << err.message() << "\n";
    }
  }

  void handle_write_request(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Read the response status line.
      boost::asio::async_read_until(socket_, response_, "\r\n",
          boost::bind(&http_request::handle_read_status_line, this,
            boost::asio::placeholders::error));
    }
    else
    {
      std::cout << "Error: " << err.message() << "\n";
    }
  }

  void handle_read_status_line(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Check that response is OK.
      std::istream response_stream(&response_);
      std::string http_version;
      response_stream >> http_version;
      unsigned int status_code;
      response_stream >> status_code;
      std::string status_message;
      std::getline(response_stream, status_message);
      if (!response_stream || http_version.substr(0, 5) != "HTTP/")
      {
        std::cout << "Invalid response\n";
        return;
      }
      if (status_code != 200)
      {
        std::cout << "Response returned with status code ";
        std::cout << status_code << "\n";
        return;
      }

      // Read the response headers, which are terminated by a blank line.
      boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
          boost::bind(&http_request::handle_read_headers, this,
            boost::asio::placeholders::error));
    }
    else
    {
      std::cout << "Error: " << err << "\n";
    }
  }

  void handle_read_headers(const boost::system::error_code& err)
  {
    if (!err)
    {
      // Process the response headers.
      std::istream response_stream(&response_);
      std::string header;
      while (std::getline(response_stream, header) && header != "\r")
        /*std::cout << header << "\n"*/;
      //std::cout << "\n";

      // Write whatever content we already have to output.
	  if (response_.size() > 0) {
		std::stringstream ss;
		ss << &response_;

		for (uint i = 0; i < ss.str().size(); ++i)
			content.push_back(ss.str()[i]);

	  }

      // Start reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          boost::bind(&http_request::handle_read_content, this,
            boost::asio::placeholders::error, 0, false));
    }
    else
    {
      std::cout << "Error: " << err << "\n";
    }
  }

  void handle_read_content(const boost::system::error_code& err, size_t len, bool usebuf)
  {
    if (!err)
    {
      // Write all of the data that has been read so far.
		if (!usebuf) {
			std::stringstream ss;
			ss << &response_;

			for (uint i = 0; i < ss.str().size(); ++i)
				bigbuf[i] = ss.str()[i];
			len = ss.str().size();
		}

		//std::cout << "len: " << len << '\n';

		content.insert(content.end(), bigbuf, bigbuf + len);
		//if (!ss.str().empty())
			//content.insert(content.end(), &ss.str()[0], &ss.str()[0] + ss.str().size());
      //std::cout << &response_;

      // Continue reading remaining data until EOF.
		socket_.async_receive(boost::asio::buffer(bigbuf),
			boost::bind(&http_request::handle_read_content, this,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, true)
			);

//      boost::asio::async_read(socket_, response_,
//          boost::asio::transfer_at_least(1),
//          boost::bind(&http_request::handle_read_content, this,
//            boost::asio::placeholders::error, 0, false));
    }
    else if (err != boost::asio::error::eof)
    {
      std::cout << "Error: " << err << "\n";
	  handler(err);
	} else {
		handler(boost::system::error_code());
	}
  }

  uint8 bigbuf[100*1024];
  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
  boost::function<void(boost::system::error_code)> handler;
  std::vector<uint8> content;
  uint16 port;
};

} } // namespaces
/*
int tmain(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cout << "Usage: async_client <server> <path>\n";
      std::cout << "Example:\n";
      std::cout << "  async_client www.boost.org /LICENSE_1_0.txt\n";
      return 1;
    }

    boost::asio::io_service io_service;
	boost::asio::http_request c(io_service, argv[1], argv[2]);
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 0;
}
*/

#endif//ASIO_HTTP_REQUEST_H
