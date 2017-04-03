#include "config.h"
#include "util/fs.h"
#include <stdexcept>
#include <exception>
#include <errno.h>
#include <cstring>

BEGIN_APP_NAMESPACE

namespace Util
{
namespace fs
{
DirReader::DirReader(const std::string &path)
	: d(NULL), curr_ent(NULL)
{
	d = opendir(path.c_str());
	if (d == NULL)
	{
		throw std::runtime_error(std::string("opendir failed: ") + strerror(errno));
	}
}
DirReader::~DirReader()
{
	if (d != NULL)
	{
		if (closedir(d) != 0)
		{
			throw std::runtime_error(std::string("closedir failed: ") + strerror(errno));
		}
		d = NULL;
	}
}
bool DirReader::next()
{
	errno = 0;
	curr_ent = readdir(d);
	if (curr_ent == NULL && errno)
	{
		throw std::runtime_error(std::string("readdir failed: ") + strerror(errno));
	}
	return (curr_ent != NULL);
}
const DirEntry &DirReader::entry()
{
	if (curr_ent == NULL)
	{
		throw std::runtime_error("invalid DirReader state");
	}
	return *curr_ent;
}
std::vector<std::string> DirReader::getAll()
{
	std::vector<std::string> ret;
	while (this->next())
	{
		ret.push_back(this->entry().d_name);
	}
	return ret;
}
} // namespace fs
} // namespace Util

END_APP_NAMESPACE

