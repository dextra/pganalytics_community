#ifndef __UTIL_TIME_H__
#define __UTIL_TIME_H__

#include "config.h"
#include "debug.h"
#include <ctime>
#include <cerrno>
#include <cstring>

#include <exception>
#include <stdexcept>


#ifdef __WIN32__
struct tm *localtime_r(const time_t *timep, struct tm *result);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
#endif

BEGIN_APP_NAMESPACE

namespace Util
{
namespace time
{
std::tm localtime(const time_t &original);
std::tm gmtime(const time_t &original);
time_t mktime(std::tm &original);
time_t mktime(std::tm &original, const std::string &tz);
void usleep(long microsec);
inline void sleep(long milisec)
{
	usleep(milisec * 1000l);
}
inline time_t now()
{
	time_t ret;
	if (::time(&ret) == (time_t)(-1))
	{
		throw std::runtime_error(std::string("time failed: ") + strerror(errno));
	}
	return ret;
}
} // namespace time
} // namespace Util

END_APP_NAMESPACE

#endif // __UTIL_TIME_H__

