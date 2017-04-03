
#include "config.h"
#include "util/fs.h"
#include "win.h"

/**
 * Based on PostgreSQL source file src/port/dirmod.c
 */

/* Don't modify declarations in system headers */
#if defined(WIN32) || defined(__CYGWIN__)
#undef rename
#endif

#include <unistd.h>
#include <sys/stat.h>

#if defined(WIN32) || defined(__CYGWIN__)
#include <windows.h>
#ifndef __CYGWIN__
#include <winioctl.h>
#else
#include <w32api/winioctl.h>
#endif
#endif

#ifndef EACCESS
#define EACCESS 2048
#endif

#if defined(WIN32) || defined(__CYGWIN__)

/*
 *	pgrename
 */
int
pgrename(const char *from, const char *to)
{
	int			loops = 0;

	/*
	 * We need to loop because even though PostgreSQL uses flags that allow
	 * rename while the file is open, other applications might have the file
	 * open without those flags.  However, we won't wait indefinitely for
	 * someone else to close the file, as the caller might be holding locks
	 * and blocking other backends.
	 */
#if defined(WIN32) && !defined(__CYGWIN__)
	while (!MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING))
#else
	while (rename(from, to) < 0)
#endif
	{
#if defined(WIN32) && !defined(__CYGWIN__)
		DWORD		err = GetLastError();

		_dosmaperr(err);

		/*
		 * Modern NT-based Windows versions return ERROR_SHARING_VIOLATION if
		 * another process has the file open without FILE_SHARE_DELETE.
		 * ERROR_LOCK_VIOLATION has also been seen with some anti-virus
		 * software. This used to check for just ERROR_ACCESS_DENIED, so
		 * presumably you can get that too with some OS versions. We don't
		 * expect real permission errors where we currently use rename().
		 */
		if (err != ERROR_ACCESS_DENIED &&
				err != ERROR_SHARING_VIOLATION &&
				err != ERROR_LOCK_VIOLATION)
			return -1;
#else
		if (errno != EACCES)
			return -1;
#endif

		if (++loops > 100)		/* time out after 10 sec */
			return -1;
		//pg_usleep(100000);		/* us */
	}
	return 0;
}


/* We undefined these above; now redefine for possible use below */
#define rename(from, to)		pgrename(from, to)
#endif   /* defined(WIN32) || defined(__CYGWIN__) */

#if defined(WIN32) && !defined(__CYGWIN__)

/*
 * The stat() function in win32 is not guaranteed to update the st_size
 * field when run. So we define our own version that uses the Win32 API
 * to update this field.
 */
int
pgwin32_safestat(const char *path, struct stat * buf)
{
	int			r;
	WIN32_FILE_ATTRIBUTE_DATA attr;

	r = stat(path, buf);
	if (r < 0)
		return r;

	if (!::GetFileAttributesEx(path, GetFileExInfoStandard, &attr))
	{
		_dosmaperr(::GetLastError());
		return -1;
	}

	/*
	 * XXX no support for large files here, but we don't do that in general on
	 * Win32 yet.
	 */
	buf->st_size = attr.nFileSizeLow;

	return 0;
}

#define stat pgwin32_safestat

#endif

/* Now our code comes */

#include <exception>
#include <stdexcept>
#include <errno.h>
#include <cstring>

BEGIN_APP_NAMESPACE

namespace Util
{
namespace fs
{

Stat fileStat(const std::string &path)
{
	Stat buf;
	if (stat(path.c_str(), &buf) != 0)
	{
		throw std::runtime_error(std::string("stat failed for file `") + path + "': " + strerror(errno));
	}
	return buf;
}

bool fileExists(const std::string &path, Stat *res_stat)
{
	if (stat(path.c_str(), res_stat) != 0)
	{
		if (!path.empty() && errno == ENOENT)
		{
			return false;
		}
		throw std::runtime_error(std::string("stat failed for file `") + path + "': " + strerror(errno));
	}
	return true;
}
bool fileExists(const std::string &path)
{
	Stat buf;
	return fileExists(path, &buf);
}

} // namespace fs
} // namespace Util

END_APP_NAMESPACE

