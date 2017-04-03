#ifndef __UTIL_FS_H__
#define __UTIL_FS_H__

#include "config.h"
#include <string>
#include <vector>
#include <sys/stat.h>

#include <dirent.h>

BEGIN_APP_NAMESPACE

namespace Util
{
namespace fs
{
typedef struct ::stat Stat;
typedef struct ::dirent DirEntry;
std::string absPath(const std::string &relPath);
std::string absPath(const std::string &workDir, const std::string &relPath);
std::string baseName(const std::string &path);
std::string dirName(const std::string &path);
std::string fileExtension(const std::string &path);
void mkdir_p(const std::string &path, int omode = 0700); /* default mode is u=rwx */
void remove(const std::string &path);
void rename(const std::string &oldpath, const std::string &newpath, bool overwrite = true);
Stat fileStat(const std::string &path);
bool fileExists(const std::string &path, Stat *res_stat);
bool fileExists(const std::string &path);
class DirReader
{
protected:
	DIR *d;
	struct dirent *curr_ent;
public:
	DirReader(const std::string &path);
	virtual ~DirReader();
	bool next();
	const DirEntry &entry();
	std::vector<std::string> getAll();
};
} // namespace fs
} // namespace Util

END_APP_NAMESPACE

#endif // __UTIL_FS_H__

