#ifndef __UTIL_STRING_H__
#define __UTIL_STRING_H__

#include "config.h"

#include <sstream>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <iomanip>

BEGIN_APP_NAMESPACE

namespace Util
{
template<typename T>
std::string numberToString(T value)
{
	std::ostringstream ss;
	ss << value;
	return ss.str();
}
template<typename T>
T stringToNumber(const std::string &value, bool ignore_extra_chars = false)
{
	std::istringstream ss(value);
	T ret;
	if (!(ss >> ret))
	{
		throw std::runtime_error(std::string("cast failed: \"") + value + "\" is not a number");
	}
	if (!ss.eof() && !ignore_extra_chars)
	{
		throw std::runtime_error("cast failed: extra characters found");
	}
	return ret;
}
std::string timeToString(time_t value);
std::string unescapeString(const std::string &original, const std::string &quotes = "");
std::string escapeString(const std::string &original, const std::string &quotes = "");
} // namespace Util

END_APP_NAMESPACE

#endif // __UTIL_STRING_H__

