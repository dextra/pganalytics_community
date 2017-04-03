#include "util/time.h"
#include "LogManager.h"

#include "util/app.h"
#include "util/fs.h"
#include "StateManager.h"
#include "ServerInfo.h"

#include <iomanip>
#include <typeinfo>

BEGIN_APP_NAMESPACE

namespace LogManagerPrivate
{
	static std::string generateLogFileName()
	{
		static time_t now = (time_t)(-1);
		static int cnt = 0;
		std::stringstream ss;
		if (now == (time_t)(-1))
		{
			try
			{
				Util::MainApplicationPtr app = Util::MainApplication::instance();
				now = app->startTime();
			}
			catch (...)
			{
				::time(&now);
			}
		}
		if (now == (time_t)(-1))
		{
			throw std::runtime_error(std::string("time call failed: ") + strerror(errno));
		}
		cnt++;
		ss
				<< now
				<< "-" << std::hex << std::setfill('0') << std::setw(6) << getpid()
				<< "-" << std::hex << std::setfill('0') << std::setw(4) << cnt;
		ss << ".log";
		return ss.str();
	}
}

LogManagerPtr LogManager::m_instance((LogManager*)NULL);

LogManager::LogManager()
{
	/* Nothing to see here, check LogManager::instance() */
}

LogManager::~LogManager()
{
	this->close();
}

LogManagerPtr LogManager::instance()
{
	if (LogManager::m_instance.isNull())
	{
		LogManager::m_instance = new LogManager();
		LogManager::m_instance->m_logger.addSink(std::cerr, Util::log::L_DEBUG);
		LogManager::m_instance->rotate();
	}
	return LogManager::m_instance;
}

Util::log::Logger &LogManager::logger()
{
	return LogManager::instance()->m_logger;
}

void LogManager::rotate()
{
	/**
	 * If the config file is not yet loaded, then just use cerr sink (already added).
	 * See also: PgAnalyticsApp::loadConfigFile at src/pganalytics.cpp
	 */
	if (UserConfig::isLoaded())
	{
		ServerConfigPtr conf = ServerInfo::instance()->currentServerConfig();
		std::string logname;
		if (conf->logLevel() == Util::log::L_SILENT)
		{
			/* If SILENT, just close current (if any) and don't even open a new one */
			this->close();
			return;
		}
		logname = LogManagerPrivate::generateLogFileName();
		DMSG("Log output: " << (conf->collectDir() + COLLECT_DIR_LOG + DIRECTORY_SEPARATOR + logname));
		try
		{
			/* Lock, to avoid removal */
			this->m_lock = new ScopedFileLock(logname);
			/* Open the log file */
			Util::fs::mkdir_p(conf->collectDir() + COLLECT_DIR_LOG + DIRECTORY_SEPARATOR);
			this->close(); /* Close stream, if opened */
			this->m_ofs.exceptions(std::ios::badbit);
			this->m_ofs.open(std::string(conf->collectDir() + COLLECT_DIR_LOG + DIRECTORY_SEPARATOR + logname).c_str(), std::ios::out);
			this->m_logger.addSink(this->m_ofs, conf->logLevel());
		}
		catch(ScopedFileLockFail &e)
		{
			this->m_logger.error() << typeid(e).name() << ": " << e.what() << std::endl;
		}
	}
}

void LogManager::close()
{
	if (this->m_ofs.is_open())
	{
		this->m_logger.removeSink(this->m_ofs);
		this->m_ofs.close();
	}
}

std::vector<std::string> LogManager::logsToPush()
{
	ServerConfigPtr server_config = ServerInfo::instance()->currentServerConfig();
	Util::fs::DirReader dir_reader(server_config->collectDir() + COLLECT_DIR_LOG);
	/* Get ordered file list */
	std::vector<std::string> ret; /* Use an array to send it in order */
	while (dir_reader.next() && ret.size() < MAX_FILES_TO_PUSH)
	{
		std::string filename = dir_reader.entry().d_name;
		if (filename != "." && filename != ".." && Util::fs::fileExtension(filename) == "log")
		{
			try
			{
				ScopedFileLockPtr lock = new ScopedFileLock(filename);
				this->m_push_locks.push_back(lock);
				ret.push_back(filename);
			}
			catch (ScopedFileLockFail &e)
			{
				/* do nothing, means the file is in use */
			}
		}
	}
	std::sort(ret.begin(), ret.end());
	return ret;
}

END_APP_NAMESPACE

