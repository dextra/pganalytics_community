#ifndef __LOG_PARSER_H__
#define __LOG_PARSER_H__

#include "poco_regexp.h"
#include <iostream>

class LineHandler;

class LogParser
{
protected:
	Poco::RegularExpression *lineParserRE;
	std::vector<std::string> current_line;
	std::vector<bool> current_info_available;
	std::vector<size_t> match_index;
	bool first_line;
	LineHandler &handler;
public:
	enum LinePrefixEnum
	{
		E_PREFIX_APPNAME = 0,
		E_PREFIX_USERNAME,
		E_PREFIX_DBNAME,
		E_PREFIX_REMOTE_HOST_PORT,
		E_PREFIX_REMOTE_HOST,
		E_PREFIX_PID,
		E_PREFIX_TIMESTAMP,
		E_PREFIX_TIMESTAMP_MS,
		E_PREFIX_COMMAND_TAG,
		E_PREFIX_SQL_STATE,
		E_PREFIX_SESSION_ID,
		E_PREFIX_SESSION_LINE,
		E_PREFIX_SESSION_TIMESTAMP,
		E_PREFIX_VXID,
		E_PREFIX_XID,
		E_PREFIX_TYPE,
		E_PREFIX_MESSAGE
	};
	LogParser(LineHandler &handler);
	virtual ~LogParser();
	void generateLineParserRE(const std::string &prefix);
	inline bool isLogInfoAvailable(LinePrefixEnum item) const
	{
		return (this->match_index[(size_t)item] > 0);
	}
	inline bool isCurrentLogInfoAvailable(LinePrefixEnum item) const
	{
		return isLogInfoAvailable(item) && this->current_info_available[this->match_index[(size_t)item]];
	}
	inline const std::string &currentLogInfoFor(LinePrefixEnum item) const
	{
		if (!this->isLogInfoAvailable(item))
		{
			throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + ": item not available.");
		}
		return this->current_line[this->match_index[(size_t)item]];
	}
	inline void currentLogInfoFor(LinePrefixEnum item, const std::string &value)
	{
		if (!this->isLogInfoAvailable(item))
		{
			throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + ": item not available.");
		}
		this->current_line[this->match_index[(size_t)item]] = value;
		this->current_info_available[this->match_index[(size_t)item]] = true;
	}
	inline void appendMessage(const std::string &value)
	{
		this->current_line[this->match_index[(size_t)E_PREFIX_MESSAGE]] += value;
	}
	inline size_t logInfoIndex(LinePrefixEnum item) const
	{
		if (!this->isLogInfoAvailable(item))
		{
			throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + ": item not available.");
		}
		return this->match_index[(size_t)item];
	}
	void sendLogBuffer();
	void consume(std::string &line);
	void finalize();
};

#endif /* #ifndef __LOG_PARSER_H__ */

