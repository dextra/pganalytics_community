#include "util/fs.h"
#include "util/string.h"
#include "debug.h"
#include "win.h"

#include <stdlib.h> // realpath (POSIX), _fullpath (Win)
#include <cstdio> // rename, remove
#include <sys/stat.h>
#include <exception>
#include <stdexcept>
#include <errno.h>
#include <cstring>

#ifdef __WIN32__
#ifndef __BORLANDC__
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IXGRP 0
#define S_IRWXG 0
#define S_IROTH 0
#define S_IWOTH 0
#define S_IXOTH 0
#define S_IRWXO 0
#endif   /* __BORLANDC__ */
#endif

#define PATH_SEPERATORS "\\/"

BEGIN_APP_NAMESPACE

namespace Util
{
namespace fs
{
std::string absPath(const std::string &relPath)
{
#ifdef __WIN32__
	char path[MAXFILEPATH];
	if (_fullpath(path, relPath.c_str(), MAXFILEPATH) != NULL)
	{
		return path;
	}
#else
	char *path;
	if ((path = realpath(relPath.c_str(), NULL)) != NULL)
	{
		std::string tmp = path;
		free(path);
		return tmp;
	}
#endif
	/* If we reached here, means that the above failed */
	throw std::runtime_error(std::string("error on retrieving absPath(\"") + Util::escapeString(relPath) + "\"): " + strerror(errno));
}

std::string absPath(const std::string &workDir, const std::string &relPath)
{
	bool is_absolute = false;
#ifdef __WIN32__
	if (relPath[1] == ':')
	{
		is_absolute = true;
	}
#else
	if (relPath[0] == '/')
	{
		is_absolute = true;
	}
#endif
	if (is_absolute)
	{
		return absPath(relPath);
	}
	else if (std::string(PATH_SEPERATORS).find(workDir.substr(workDir.size()-1, 1)) == std::string::npos)
	{
		/* Does not end with "/" */
		return absPath(workDir + "/" + relPath);
	}
	else
	{
		return absPath(workDir + relPath);
	}
}

std::string baseName(const std::string &path)
{
	size_t last_slash = path.find_last_of(PATH_SEPERATORS);
	if (last_slash == std::string::npos)
		return path;
	return path.substr(last_slash + 1);
}

std::string dirName(const std::string &path)
{
	size_t last_slash = path.find_last_of(PATH_SEPERATORS);
	if (last_slash == std::string::npos)
		return "";
	return path.substr(0, last_slash);
}

std::string fileExtension(const std::string &path)
{
	size_t last_slash = path.find_last_of(PATH_SEPERATORS);
	size_t last_dot = path.find_last_of(".");
	if (last_dot == std::string::npos || (last_slash != std::string::npos && last_dot < last_slash))
		return "";
	return path.substr(last_dot + 1);
}

static int pg_mkdir_p(char *path, int omode)
{
	/* Copied from PostgreSQL source code: src/port/pgmkdirp.c */
	struct stat sb;
	mode_t		numask,
			 oumask;
	int			last,
			retval;
	char	   *p;

	retval = 0;
	p = path;

#ifdef __WIN32__
	/* skip network and drive specifiers for win32 */
	if (strlen(p) >= 2)
	{
		if (p[0] == '/' && p[1] == '/')
		{
			/* network drive */
			p = strstr(p + 2, "/");
			if (p == NULL)
			{
				errno = EINVAL;
				return -1;
			}
		}
		else if (p[1] == ':' &&
				 ((p[0] >= 'a' && p[0] <= 'z') ||
				  (p[0] >= 'A' && p[0] <= 'Z')))
		{
			/* local drive */
			p += 2;
		}
	}
#endif

	/*
	 * POSIX 1003.2: For each dir operand that does not name an existing
	 * directory, effects equivalent to those caused by the following command
	 * shall occcur:
	 *
	 * mkdir -p -m $(umask -S),u+wx $(dirname dir) && mkdir [-m mode] dir
	 *
	 * We change the user's umask and then restore it, instead of doing
	 * chmod's.  Note we assume umask() can't change errno.
	 */
	oumask = umask(0);
	numask = oumask & ~(S_IWUSR | S_IXUSR);
	(void) umask(numask);

	if (p[0] == '/')			/* Skip leading '/'. */
		++p;
	for (last = 0; !last; ++p)
	{
		if (p[0] == '\0')
			last = 1;
		else if (p[0] != '/')
			continue;
		*p = '\0';
		if (!last && p[1] == '\0')
			last = 1;

		if (last)
			(void) umask(oumask);

		/* check for pre-existing directory */
		if (stat(path, &sb) == 0)
		{
			if (!S_ISDIR(sb.st_mode))
			{
				if (last)
					errno = EEXIST;
				else
					errno = ENOTDIR;
				retval = -1;
				break;
			}
		}
#ifdef __WIN32__
		else if (_mkdir(path) < 0 && errno != EEXIST)
#else
		else if (mkdir(path, last ? omode : S_IRWXU | S_IRWXG | S_IRWXO) < 0)
#endif
		{
			retval = -1;
			break;
		}
		if (!last)
			*p = '/';
	}

	/* ensure we restored umask */
	(void) umask(oumask);

	return retval;
}

void mkdir_p(const std::string &path, int omode)
{
	char *path_work = new char[path.size() + 1];
	memcpy(path_work, path.c_str(), path.size() + 1);
	int ret = pg_mkdir_p(path_work, omode);
	std::string err_msg;
	if (ret != 0)
	{
		err_msg = std::string("mkdir error at location `") + path_work + "': " + strerror(errno);
	}
	delete[] path_work;
	if (!err_msg.empty())
	{
		throw std::runtime_error(err_msg);
	}
}

void remove(const std::string &path)
{
	if (::remove(path.c_str()) != 0)
	{
		throw std::runtime_error(std::string("remove failed for location `") + path + "': " + strerror(errno));
	}
}

void rename(const std::string &oldpath, const std::string &newpath, bool overwrite)
{
#ifdef __WIN32__
	if (!::MoveFileEx(oldpath.c_str(), newpath.c_str(), MOVEFILE_WRITE_THROUGH | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0)))
	{
		_dosmaperr(::GetLastError());
		throw std::runtime_error(std::string("rename failed (`") + oldpath + "' -> `" + newpath + "'): " + strerror(errno));
	}
#else
	if (!overwrite && Util::fs::fileExists(newpath))
	{
		errno = EEXIST;
		throw std::runtime_error(std::string("rename failed: ") + strerror(errno));
	}
	if (::rename(oldpath.c_str(), newpath.c_str()) != 0)
	{
		throw std::runtime_error(std::string("rename failed: ") + strerror(errno));
	}
#endif
}
} // namespace fs
} // namespace Util

END_APP_NAMESPACE

