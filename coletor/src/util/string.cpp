#include "util/string.h"
#include "util/time.h"

BEGIN_APP_NAMESPACE

namespace Util
{

std::string timeToString(time_t value)
{
	std::tm tm = Util::time::gmtime(value);
	std::ostringstream ss;
	ss
			<< std::setfill('0') << std::setw(4) << (tm.tm_year+1900) << "-"
			<< std::setfill('0') << std::setw(2) << (tm.tm_mon+1)  << "-"
			<< std::setfill('0') << std::setw(2) << tm.tm_mday << " "
			<< std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
			<< std::setfill('0') << std::setw(2) << tm.tm_min  << ":"
			<< std::setfill('0') << std::setw(2) << tm.tm_sec
			<< "+00"
			;
	return ss.str();
}

std::string unescapeString(const std::string &original, const std::string &quotes)
{
	std::string ret;
	for (size_t i = 0; i < original.size(); i++)
	{
		if (original[i] == '\\')
		{
			/* It is escaping, check next character */
			i++;
			switch(original[i])
			{
				/* XXX: This could be on another function if we need to use again */
			case 'a':
				ret += '\a';
				break;
			case 'r':
				ret += '\r';
				break;
			case 'n':
				ret += '\n';
				break;
			case 't':
				ret += '\t';
				break;
			case '\\':
				ret += '\\';
				break;
			default:
				if (quotes.find(original[i]) == std::string::npos)
				{
					throw std::runtime_error(std::string("Invalid escape '") + original[i] + "' at " + numberToString(i) + " of \"" + original + "\"");
				}
				ret += original[i];
			}
		}
		else
		{
			ret += original[i];
		}
	}
	return ret;
}

std::string escapeString(const std::string &original, const std::string &quotes)
{
	std::string ret;
	for (size_t i = 0; i < original.size(); i++)
	{
		switch(original[i])
		{
		case '\a':
			ret += "\\a";
			break;
		case '\r':
			ret += "\\r";
			break;
		case '\n':
			ret += "\\n";
			break;
		case '\t':
			ret += "\\t";
			break;
		case '\\':
			ret += "\\\\";
			break;
		default:
			if (quotes.find(original[i]) != std::string::npos)
			{
				ret += "\\";
			}
			ret += original[i];
		}
	}
	return ret;
}

} // namespace Util

END_APP_NAMESPACE

