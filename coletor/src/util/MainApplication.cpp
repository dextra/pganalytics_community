#include "util/app.h"
#include "util/fs.h"
#include "util/streams.h"
#include "getopt_long.h"
#include "debug.h"

#include <sstream>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#ifndef __WIN32__
#include <execinfo.h> // backtrace, backtrace_symbols
#endif


BEGIN_APP_NAMESPACE

namespace Util
{
/* static */ MainApplicationPtr MainApplication::m_instance = 0;
MainApplication::MainApplication()
	: m_argc(0), m_optindex(0)
{}

void MainApplication::args(int argc, char *argv[])
{
	if (this->m_argc > 0)
	{
		throw std::runtime_error("arguments already given, it can't be given twice");
	}
	MainApplication::m_instance = this;
	time(&this->m_startTime);
	this->m_argc = argc;
	this->m_argv = argv;
	optIndex(1);
}

const std::string &MainApplication::absolutePath() const
{
	if (this->m_absolutePath.empty())
	{
		this->m_absolutePath = Util::fs::absPath(this->argv(0));
	}
	return this->m_absolutePath;
}

/* static */ MainApplicationPtr MainApplication::instance()
{
	if (MainApplication::m_instance.isNull())
	{
		throw std::runtime_error("no MainApplication instance available");
	}
	return MainApplication::m_instance;
}

int MainApplication::argc() const
{
	return this->m_argc;
}

const char *MainApplication::argv(int pos) const
{
	if (pos < 0 || pos >= argc())
	{
		std::stringstream ss;
		ss << "out of bound argv position: " << pos;
		throw std::runtime_error(ss.str());
	}
	return this->m_argv[pos];
}

const char * const *MainApplication::argv() const
{
	return this->m_argv;
}

const std::vector<std::string> MainApplication::args() const
{
	if (this->m_args.size() == 0)
	{
		this->m_args.reserve(argc());
		for (int i = 0; i < argc(); i++)
		{
			this->m_args.push_back(this->argv(i));
		}
	}
	return this->m_args;
}

char MainApplication::getopt(struct option *longopts, const std::string &opts)
{
	return getopt_long(this->m_argc, this->m_argv, opts.c_str(), longopts, &m_optindex);
}

char MainApplication::getopt(struct option *longopts)
{
	static option *last_opt = NULL;
	static std::string opts;
	if (last_opt != longopts)
	{
		struct option *lopt = NULL;
		last_opt = longopts;
		opts = "+";
		for (size_t i = 0; longopts[i].name != NULL; i++)
		{
			lopt = &(longopts[i]);
			if ((lopt->val >= 'a' && lopt->val <= 'z') || (lopt->val >= 'A' && lopt->val <= 'Z'))
			{
				opts += lopt->val;
				if (lopt->has_arg == required_argument)
				{
					opts += ':';
				}
			}
		}
	}
	return this->getopt(longopts, opts);
}

const std::vector<std::string> MainApplication::extraArgs() const
{
	std::vector<std::string> ret;
	for (int i = this->optIndex(); i < argc(); i++)
	{
		ret.push_back(this->argv(i));
	}
	return ret;
}

int MainApplication::optIndex() const
{
	return optind;
}

void MainApplication::optIndex(int value)
{
	if (value > argc())
	{
		throw std::runtime_error("optIndex value out of range");
	}
	optind = value;
}

char *MainApplication::optarg()
{
	return ::optarg;
}

void MainApplication::printStackTrace(std::ostream &ostr, bool get_position)
{
#ifndef __WIN32__
	const int STACK_SIZE = 100;
	void *buffer[STACK_SIZE];
	char **stack_item;
	size_t size;

	// get void*'s for all entries on the stack
	size = ::backtrace(buffer, STACK_SIZE);

	stack_item = ::backtrace_symbols(buffer, size);
	if (stack_item == NULL)
	{
		ostr << "Could not get backtrace: " << ::strerror(errno) << std::endl;
	}
	else
	{
		for (size_t i = 0; i < size; i++)
		{
			ostr << "#" << (size - i) << ": " << stack_item[i];
			if (get_position)
			{
				std::string result;
				std::string progname = stack_item[i];
				size_t pos;
				std::ostringstream addr2line;
				pos = progname.find_first_of("(");
				if (pos != std::string::npos)
				{
					try
					{
						progname = progname.substr(0, pos);
						addr2line << "addr2line " << buffer[i] << " -e " << progname;
						Util::io::iprocstream proc(addr2line.str());
						std::getline(proc, result);
					}
					catch (...)
					{
						result = "XX";
					}
				}
				if (result.empty())
				{
					result = "??";
				}
				ostr << " at " << result;
			}
			ostr << std::endl;
		}
		::free(stack_item);
	}
#endif
}

void MainApplication::updateProcessTitle(const std::string &new_title)
{
	int argv0size = strlen(this->m_argv[0]);
	strncpy(this->m_argv[0], new_title.c_str(), argv0size);
}

} // namespace Util

END_APP_NAMESPACE

