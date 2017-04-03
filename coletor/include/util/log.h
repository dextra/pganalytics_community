#ifndef __UTIL_LOG_H__
#define __UTIL_LOG_H__

#include <iostream>
#include <sstream>
#include <list>
#include <stdexcept>

#include "config.h"

BEGIN_APP_NAMESPACE

namespace Util
{
namespace log
{

enum LogLevel {
	L_DEBUG   = 1,
	L_LOG     = 2,
	L_NOTICE  = 4,
	L_WARNING = 8,
	L_ERROR   = 16,
	L_FATAL   = 32,
	L_PANIC   = 64,
	L_SILENT  = 128
};

class LogBuffer : public std::streambuf {
	protected:
		struct buffer_sink
		{
			std::ostream *dest;
			LogLevel level;
			bool ended_with_newline;
			buffer_sink(std::ostream *dest, LogLevel level)
				: dest(dest), level(level), ended_with_newline(true)
			{}
			bool operator==(const buffer_sink &other)
			{
				return this->dest == other.dest;
			}
		};
		std::list<buffer_sink> m_buffers;
	public:
		explicit LogBuffer();
		virtual ~LogBuffer();
		void prepareNewItem(LogLevel level);
		void addSink(std::ostream *dest, LogLevel level);
		void removeSink(std::ostream *dest);
	protected:
		virtual int_type overflow(int_type ch);
		bool start_log_line;
		size_t m_pos;
		LogLevel m_level;
};

class Logger;
class LogStream : public std::ostream {
friend class Logger;
protected:
	LogBuffer buf;
	LogStream()
	{
		std::ostream::rdbuf(&buf);
	}
	virtual ~LogStream() {
	}
	inline void addSink(std::ostream *dest, LogLevel level) {
		buf.addSink(dest, level);
	}
	inline void addSink(std::ostream &out, LogLevel level) {
		this->addSink(&out, level);
	}
	inline void removeSink(std::ostream *dest) {
		buf.removeSink(dest);
	}
	inline void removeSink(std::ostream &out) {
		this->removeSink(&out);
	}
	std::ostream &prepareNewItem(LogLevel level) {
		buf.prepareNewItem(level);
		return (*this);
	}
};

class Logger {
protected:
	LogStream log_stream;
public:
	Logger() {}
	virtual ~Logger() {}
	inline void addSink(std::ostream *dest, LogLevel level) {
		log_stream.addSink(dest, level);
	}
	inline void addSink(std::ostream &out, LogLevel level) {
		this->addSink(&out, level);
	}
	inline void removeSink(std::ostream *dest) {
		log_stream.removeSink(dest);
	}
	inline void removeSink(std::ostream &out) {
		this->removeSink(&out);
	}
	Logger(std::ostream &out, LogLevel level)
	{
		this->addSink(out, level);
	}
	/* Logging methods */
	std::ostream &debug() {
		return log_stream.prepareNewItem(L_DEBUG);
	}
	std::ostream &log() {
		return log_stream.prepareNewItem(L_LOG);
	}
	std::ostream &notice() {
		return log_stream.prepareNewItem(L_NOTICE);
	}
	std::ostream &warning() {
		return log_stream.prepareNewItem(L_WARNING);
	}
	std::ostream &error() {
		return log_stream.prepareNewItem(L_ERROR);
	}
	std::ostream &fatal() {
		return log_stream.prepareNewItem(L_FATAL);
	}
	std::ostream &panic() {
		return log_stream.prepareNewItem(L_PANIC);
	}
};

inline LogLevel logLevelFromString(const std::string &level)
{
	if (level == "DEBUG")
		return L_DEBUG;
	else if (level == "LOG")
		return L_LOG;
	else if (level == "NOTICE")
		return L_NOTICE;
	else if (level == "WARNING")
		return L_WARNING;
	else if (level == "ERROR")
		return L_ERROR;
	else if (level == "FATAL")
		return L_FATAL;
	else if (level == "PANIC")
		return L_PANIC;
	else if (level == "SILENT")
		return L_SILENT;
	throw std::runtime_error(std::string("Invalid log level: ") + level);
}

} // namespace log
} // namespace Util

END_APP_NAMESPACE

#endif // __UTIL_LOG_H__

