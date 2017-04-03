#include "util/log.h"
#include "util/time.h"
#include "config.h"
#include "common.h"

#include <unistd.h> // getpid
#include <iomanip>

BEGIN_APP_NAMESPACE

namespace Util
{
namespace log
{

void LogBuffer::addSink(std::ostream *dest, LogLevel level)
{
	this->m_buffers.push_back(buffer_sink(dest, level));
}

void LogBuffer::removeSink(std::ostream *dest)
{
	this->m_buffers.remove(buffer_sink(dest, L_DEBUG));
}

LogBuffer::LogBuffer()
	: start_log_line(true), m_pos(0)
{}

std::ios::int_type LogBuffer::overflow(std::ios::int_type ch)
{
	if (ch != traits_type::eof() && this->start_log_line)
	{
		std::ostringstream ostr;
		struct tm tm = Util::time::gmtime(Util::time::now());
		this->m_pos++;
		ostr
			<< std::setfill('0') << std::setw(4) << (tm.tm_year+1900) << "-"
			<< std::setfill('0') << std::setw(2) << (tm.tm_mon+1)  << "-"
			<< std::setfill('0') << std::setw(2) << tm.tm_mday << " "
			<< std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
			<< std::setfill('0') << std::setw(2) << tm.tm_min  << ":"
			<< std::setfill('0') << std::setw(2) << tm.tm_sec
			<< " UTC"
			<< " [" << getpid() << "-" << this->m_pos << "] ";
		switch (this->m_level)
		{
			case L_DEBUG:
				ostr << "DEBUG";
				break;
			case L_LOG:
				ostr << "LOG";
				break;
			case L_NOTICE:
				ostr << "NOTICE";
				break;
			case L_WARNING:
				ostr << "WARNING";
				break;
			case L_ERROR:
				ostr << "ERROR";
				break;
			case L_FATAL:
				ostr << "FATAL";
				break;
			case L_PANIC:
				ostr << "PANIC";
				break;
			case L_SILENT:
				throw std::logic_error("SILENT level cannot be used for output");
				break;
		}
		ostr << ": ";
		forall(buf, this->m_buffers)
		{
			if (this->m_level >= buf->level)
			{
				if (!buf->ended_with_newline)
				{
					buf->dest->put('\n');
				}
				buf->dest->write(ostr.str().c_str(), ostr.str().size());
				buf->ended_with_newline = false;
			}
		}
		this->start_log_line = false;
	}
	forall(buf, this->m_buffers)
	{
		if (this->m_level >= buf->level)
		{
			if (buf->ended_with_newline)
			{
				buf->dest->put('\t');
			}
			buf->dest->put(ch);
			buf->ended_with_newline = (ch == '\n');
		}
	}
	return ch;
}

void LogBuffer::prepareNewItem(LogLevel level)
{
	this->start_log_line = true;
	this->m_level = level;
}

LogBuffer::~LogBuffer()
{
	if (!this->start_log_line)
	{
		forall(buf, this->m_buffers)
		{
			if (!buf->ended_with_newline)
			{
				buf->dest->put('\n');
			}
		}
	}
}

/*
LogSingleStream::LogSingleStream(std::ostream &stream, LogLevel level)
	: LogStream()
{
	this->addSink(stream, level);
}

LogSingleStream cerr(std::cerr, LOG);
*/

} // namespace log
} // namespace Util

END_APP_NAMESPACE

