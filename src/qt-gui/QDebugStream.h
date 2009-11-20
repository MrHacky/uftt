//################
//# qdebugstream.h  #
//################

#ifndef Q_DEBUG_STREAM_H
#define Q_DEBUG_STREAM_H

#include <iostream>
#include <streambuf>
#include <string>

#include "qtextedit.h"

#include <boost/function.hpp>

class QDebugStream : public std::basic_streambuf<char>
{
public:
 QDebugStream(std::ostream &stream, const boost::function<void(QString)>& callback) : m_stream(stream)
 {
  m_old_buf = stream.rdbuf();
  stream.rdbuf(this);
  appender = callback;
 }
 ~QDebugStream()
 {
  // output anything that is left
  if (!m_string.empty())
   this->append(m_string.c_str());

  m_stream.rdbuf(m_old_buf);
 }

protected:
 virtual int_type overflow(int_type v)
 {
  if (v == '\n')
  {
   this->append(m_string.c_str());
   m_string.erase(m_string.begin(), m_string.end());
  }
  else
   m_string += v;

  return v;
 }

 virtual std::streamsize xsputn(const char *p, std::streamsize n)
 {
  m_string.append(p, p + n);

  size_t pos = 0;
  while (pos != std::string::npos)
  {
   pos = m_string.find('\n');
   if (pos != std::string::npos)
   {
    std::string tmp(m_string.begin(), m_string.begin() + pos);
    m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
    this->append(tmp.c_str());
   }
  }

  return n;
 }

 void append(const char* line)
 {
	appender(QString(line));
 }

 boost::function<void(QString)> appender;

private:
 std::ostream &m_stream;
 std::streambuf *m_old_buf;
 std::string m_string;
};

#endif
