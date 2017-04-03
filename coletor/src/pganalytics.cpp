/** Copyright 2014 Dextra **/

#include <iostream>
#include <unistd.h> // gethostname
#include <fstream>
#include <typeinfo>
#include <vector>
#include <string>
#include <algorithm>

#include "getopt_long.h"

#include "config.h"
#include "common.h"
#include "UserConfig.h"
#include "db/Database.h"
#include "db/pq.h"
#include "ServerInfo.h"
#include "collectors.h"
#include "StateManager.h"
#include "util/app.h"
#include "util/fs.h"
#include "util/time.h"
#include "LogManager.h"
#include "pganalytics.h"
#include "push.h"

BEGIN_APP_NAMESPACE

int PgAnalyticsApp::main()
{
	setup_unhandled_exception_catch();
	static struct option long_options[] =
	{
		{"config-file", required_argument, NULL, 'c'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	char *opt_config_file = (char*)0;
	char c;
	int ret = 0;
	bool in_help = false;
	std::string action;
	while ((c = getopt(long_options)) != -1)
	{
		switch(c)
		{
		case 'c':
			opt_config_file = this->optarg();
			break;
		case 'V':
			version();
			return 0;
		case 'h':
			help(this->argv(0));
			return 0;
		default:
			std::cerr << "Unknown argument " << c << std::endl;
			return 1;
		}
	}
	if (optIndex() < argc())
	{
		/* An action was used */
		action = argv(optIndex());
		optIndex(optIndex() + 1);
	}
	else
	{
		/* No action? Then show the help */
		help(argv(0));
		return 1;
	}
	/* Check if the user is asking for help */
	if (action == "help")
	{
		/* Asking for help */
		in_help = true;
		if (optIndex() < argc())
		{
			/* An action was used */
			action = argv(optIndex());
			optIndex(optIndex() + 1);
		}
		else
		{
			/* No action? Then show the help */
			help(argv(0));
			return 1;
		}
	}
	else if (optIndex() < argc() && (std::string(argv(optIndex())) == "--help" || std::string(argv(optIndex())) == "-h"))
	{
		/* Help in form of: ... action {-h|--help} */
		in_help = true;
		optIndex(optIndex() + 1);
	}
	try
	{
		if (!in_help)
		{
			/* Load the config file and server info */
			if (opt_config_file)
			{
				loadConfigFile(opt_config_file);
			}
			else
			{
				loadConfigFile();
			}
		}
		/* Check the action */
		if (action == "cron")
		{
			if (!in_help)
			{
				ret = action_cron();
			}
			else
			{
				help_cron(argv(0));
			}
		}
		else if (action == "push")
		{
			if (!in_help)
			{
				ret = action_push();
			}
			else
			{
				help_push(argv(0));
			}
		}
		else if (action == "collect")
		{
			if (!in_help)
			{
				ret = action_collect();
			}
			else
			{
				help_collect(argv(0));
			}
		}
		else if (action == "help")
		{
			help(argv(0));
		}
		else
		{
			optIndex(optIndex() - 1);
			validate_extra_args();
		}
	}
	catch(std::exception &e)
	{
		/**
		 * XXX: We should think DRY and avoid this repetition, but as such
		 * errors should be rare, and writing to cerr is just a least atemp
		 * to let the user know about it, looks like this repetition is
		 * actually better than a complex function call and tries.
		 */
		ret = 2;
		try
		{
			LogManager::logger().error()
				<< e.what() << std::endl
				<< "DETAIL: exception object: " << typeid(e).name() << std::endl
				<< "SYSTEM ERROR: " << errno << " - " << ::strerror(errno) << std::endl;
		}
		catch(...)
		{
			/* An error on the logger might happen, so just throw the message on cerr */
			std::cerr
				<< "FATAL: Could not get Logger access" << std::endl
				<< "ERROR: " << e.what() << std::endl
				<< "DETAIL: exception object: " << typeid(e).name() << std::endl
				<< "SYSTEM ERROR: " << errno << " - " << ::strerror(errno) << std::endl;
		}
	}
	catch(...)
	{
		/**
		 * XXX: We should never, EVER, get in here!
		 */
		ret = 3;
		try
		{
			LogManager::logger().error() << "unknown exception" << std::endl;
		}
		catch(...)
		{
			std::cerr
				<< "FATAL: Could not get Logger access" << std::endl
				<< "unknown exception" << std::endl;
		}
	}
	return ret;
}

void PgAnalyticsApp::version()
{
	std::cout << "pgAnalytics collector " << APP_MAJOR_VERSION << "." << APP_MINOR_VERSION << "." << APP_PATCH_VERSION << " (" << GIT_DESC << ")" << std::endl;
}

void PgAnalyticsApp::help(const char *progname)
{
	version();
	std::cout
			<< "Usage:\n"
			<< "  " << progname << " [GENERAL OPTIONS] ACTION [ACTION OPTIONS]\n"
			<< "\n"
			<< "General options:\n"
			<< "  -c, --config-file=FILE       path to configuration file\n"
			<< "  -v, --version                version information\n"
			<< "  -h, --help                   show this help, then exit\n"
			<< "\n"
			<< "Actions:\n"
			<< "  cron        collect and push the data (must be called in as a cron job)\n"
			<< "  collect     collect data and store locally to be sent latter\n"
			<< "  push        send collected data to pgAnalytics server\n"
			<< "  help        show this help or a help about an action\n"
			<< "\n"
			<< "Execute `" << progname << " help ACTION` for more information of an specific action\n"
			;
	std::cout.flush();
}

void PgAnalyticsApp::help_cron(const char *progname)
{
	version();
	std::cout
			<< "Call `collect` and `push` action.\n"
			<< "\n"
			<< "Usage:\n"
			<< "  This action should be called from a cron job similar as:\n"
			<< "\n"
			<< "      0 * * * *    " << Util::fs::absPath(progname) << " cron\n"
			<< "\n"
			<< "  The above will schedule the collector to be executed at each hour.\n"
			;
	std::cout.flush();
}

int PgAnalyticsApp::action_cron()
{
	int ret = 0;
	ret += this->action_collect();
	ret += this->action_push();
	return ret;
}

void PgAnalyticsApp::help_collect(const char *progname)
{
	version();
	std::cout
			<< "Collect statistical data and save at \"collect_dir\".\n"
			<< "Usage: " << progname << " collect [TYPE ...]\n"
			<< "\n"
			<< "Type of collection (TYPE):\n"
			<< " - \"pg_stats\": collect PostgreSQL statistics\n"
			<< " - \"pg_logs\": collect PostgreSQL log files\n"
			<< " - \"df\": collect disk usage\n"
			<< " - \"sysinfo\": collect system information\n"
			<< " - \"sysstat\": collect operational system data\n"
			<< "The above types can be prefixed by a tilde (\"~\") character to exclude them.\n"
			<< "Examples:\n"
			<< "- Collect only pg_stats and df data: \n"
			<< "    " << progname << " collect pg_stats df\n"
			<< "- Collect all information available, except pg_logs and sysinfo: \n"
			<< "    " << progname << " collect all ~pg_logs ~sysinfo\n"
			<< "\n"
			;
	std::cout.flush();
}

int PgAnalyticsApp::action_collect()
{
	int ret = 0;
	std::vector<std::string> collect_types;
	while (optIndex() < argc())
	{
		collect_types.push_back(argv(optIndex()));
		optIndex(optIndex()+1);
	}
	/* No options for now */
	validate_extra_args();
	/* Load current server informations/configurations */
	ServerInfoPtr si = ServerInfo::instance();
	UserConfigPtr userConfig = this->userConfig();
	ServerConfigPtr currentServerConfig = si->currentServerConfig();
	if (!collect_types.empty())
	{
		userConfig->collect(collect_types);
	}
	/* Lock snapshot */
	ScopedFileLock lock("snapshot");
	/* Recovery the state files */
	CollectorStateManager::recoveryStates();
	/* Execute the collectors */
	if (currentServerConfig->needsCollect(COLLECT_PG_STATS) || currentServerConfig->needsCollect(COLLECT_PG_LOGS))
	{
		forall (inst, currentServerConfig->instances())
		{
			if (currentServerConfig->needsCollect(COLLECT_PG_STATS))
			{
				DMSG("starting pg stats collector for instance: " << (*inst)->name() << " (conninfo: " << (*inst)->conninfo() << ")");
				try
				{
					PgStatsCollector stats(*inst);
					stats.execute();
				}
				catch(std::exception &e)
				{
					std::cerr << "Error on PostgreSQL stats collector: " << e.what() << std::endl;
					ret++;
				}
			}
			if (currentServerConfig->needsCollect(COLLECT_PG_LOGS))
			{
				DMSG("starting pg logs collector for instance: " << (*inst)->name() << " (conninfo: " << (*inst)->conninfo() << ")");
				try
				{
					PgLogCollector logs(*inst);
					logs.execute();
				}
				catch(std::exception &e)
				{
					std::cerr << "Error on PostgreSQL logs collector: " << e.what() << std::endl;
					ret++;
				}
			}
		}
	}
	if (currentServerConfig->needsCollect(COLLECT_DF))
	{
		DMSG("starting df collector");
		try
		{
			si->reloadDiskUsage();
			DiskUsageCollector df;
			df.execute();
		}
		catch(std::exception &e)
		{
			std::cerr << "Error on disk usage collector: " << e.what() << std::endl;
			ret++;
		}
	}
	if (currentServerConfig->needsCollect(COLLECT_SYSSTAT))
	{
		DMSG("starting sysstat collector");
		try
		{
			SysstatCollector ss;
			ss.execute();
		}
		catch(std::exception &e)
		{
			std::cerr << "Error on sysstat collector: " << e.what() << std::endl;
			ret++;
		}
	}
	return ret;
}

void PgAnalyticsApp::validate_extra_args()
{
	if (optIndex() < argc())
	{
		throw std::runtime_error(std::string("too many command-line arguments (first is \"") + argv(optIndex()) + "\")");
	}
	/* ok */
}

void PgAnalyticsApp::loadConfigFile(const std::string &path)
{
	std::string rpath;
	std::ifstream ifs;
	ifs.exceptions(std::ios::failbit | std::ios::badbit);
	if (path.empty())
	{
		/* Deduce path from the executable location */
		std::string last_level_path = Util::fs::baseName(Util::fs::dirName(MainApplication::absolutePath()));
		if (last_level_path == "bin")
		{
			/* If app is inside "/path/to/bin", then the config file must be at "/path/to/etc" */
			/* Two calls of dirname means going to ../../ */
			rpath = Util::fs::dirName(Util::fs::dirName(MainApplication::absolutePath())) + DIRECTORY_SEPARATOR + "etc" + DIRECTORY_SEPARATOR + "pganalytics.conf";
		}
		else
		{
			rpath = Util::fs::dirName(MainApplication::absolutePath()) + DIRECTORY_SEPARATOR + "pganalytics.conf";
		}
	}
	else
	{
		rpath = path;
	}
	DMSG("Reading configuration from " << rpath);
	ifs.open(rpath.c_str(), std::ios::in | std::ios::binary);
	ifs.exceptions(std::ios::badbit);
	UserConfig::parse(ifs);
	/**
	 * XXX: The log manager can only generate log files in "collect_dir" after
	 * we know the "collect_dir" value, so we can only starting saving logs
	 * there after parsing the configuration file, so far any call to Logger
	 * would go to stderr, so we call a "rotate" here to force it to generate
	 * the new log file into "${collect_dir}/log" directory.
	 * See also: LogManager::rotate at src/LogManager.cpp
	 */
	LogManager::instance()->rotate();
	DMSG("Configuration loaded: `" << rpath << "'");
}

UserConfigPtr PgAnalyticsApp::userConfig() const
{
	return UserConfig::instance();
}

void PgAnalyticsApp::help_push(const char *progname)
{
}

int PgAnalyticsApp::action_push()
{
	Push push;
	push.execute();
	return 0;
}

PgAnalyticsApp::~PgAnalyticsApp() {}

END_APP_NAMESPACE

MAIN_APPLICATION_ENTRY(pga::PgAnalyticsApp);

