#ifndef __LOG_MANAGER_H__
#define __LOG_MANAGER_H__

#include "config.h"
#include "common.h"
#include "SmartPtr.h"
#include "StateManager.h"
#include "util/streams.h"
#include "util/log.h"

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

BEGIN_APP_NAMESPACE

DECLARE_SMART_CLASS(LogManager);
class LogManager : public SmartObject
{
protected:
	static LogManagerPtr m_instance;
	Util::log::Logger m_logger;
	ScopedFileLockPtr m_lock;
	std::ofstream m_ofs;
	std::vector<ScopedFileLockPtr> m_push_locks;
	LogManager();
	virtual ~LogManager();
	void close();
public:
	static LogManagerPtr instance();
	static Util::log::Logger &logger();
	std::vector<std::string> logsToPush();
	void rotate();
};

END_APP_NAMESPACE

#endif // __LOG_MANAGER_H__

