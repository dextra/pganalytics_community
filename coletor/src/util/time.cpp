#include "util/time.h"

#include "config.h"
#include "win.h"
#include <ctime>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <sys/time.h>

#include <exception>
#include <stdexcept>

#include <cstdlib>


#ifdef __WIN32__
#include <cstdio>
struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	*result = *(::localtime(timep));
	FILE *f = fopen("C:\\Temp\\localtime-call.txt", "a");
	fprintf(f, "Timer called: %04d-%02d-%02d %02d:%02d:%02d\n",
			result->tm_year+1900, result->tm_mon+1, result->tm_mday,
			result->tm_hour, result->tm_min, result->tm_sec
		   );
	fclose(f);
	return result;
}
struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
	*result = *(::gmtime(timep));
	return result;
}
#endif


BEGIN_APP_NAMESPACE

namespace Util
{
namespace time
{
std::tm localtime(const time_t &original)
{
	std::tm ret;
	if (::localtime_r(&original, &ret) == NULL)
	{
		throw std::runtime_error(std::string("localtime failed: ") + strerror(errno));
	}
	return ret;
}

std::tm gmtime(const time_t &original)
{
	std::tm ret;
	if (::gmtime_r(&original, &ret) == NULL)
	{
		throw std::runtime_error(std::string("gmtime failed: ") + strerror(errno));
	}
	return ret;
}

time_t mktime(std::tm &original)
{
	time_t ret = ::mktime(&original);
	if (ret == (time_t)(-1))
	{
		throw std::runtime_error(std::string("mktime failed: ") + strerror(errno));
	}
	return ret;
}

time_t mktime(std::tm &original, const std::string &tz)
{
	/**
	 * XXX: This implementation is not thread safe we may want change that
	 *      (or use a locking mechanism) if we need thread safety
	 */
	time_t ret;
	char *or_tz;

	/* LOCK */
	or_tz = ::getenv("TZ");
#ifndef __WIN32__
	::setenv("TZ", tz.c_str(), 1);
#else
	std::string set_env = std::string("TZ=") + tz;
	::putenv(set_env.c_str());
#endif
	::tzset();
	ret = mktime(original);
	if (or_tz)
	{
#ifndef __WIN32__
		::setenv("TZ", or_tz, 1);
#else
		set_env = "TZ=";
		::putenv(set_env.c_str());
#endif
	}
	else
	{
#ifndef __WIN32__
		::unsetenv("TZ");
#else
		set_env = std::string("TZ=") + or_tz;
		::putenv(set_env.c_str());
#endif
	}
	::tzset();
	/* UNLOCK */
	return ret;
}

void usleep(long microsec)
{
	/* Based on pg_usleep */
	if (microsec > 0)
	{
#ifndef __WIN32__
		struct timeval delay;
		delay.tv_sec = microsec / 1000000L;
		delay.tv_usec = microsec % 1000000L;
		(void) select(0, NULL, NULL, NULL, &delay);
#else
		SleepEx((microsec < 500 ? 1 : (microsec + 500) / 1000), FALSE);
#endif
	}
}
} // namespace time
} // namespace Util

END_APP_NAMESPACE

