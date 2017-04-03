#ifndef __COLLECTORS_H__
#define __COLLECTORS_H__

#include "SmartPtr.h"
#include "config.h"
#include "UserConfig.h"
#include "ServerInfo.h"
#include "db/Database.h"

BEGIN_APP_NAMESPACE

class StateManager;
class StorageManager;

/**
 * @brief Abstract class to be inherited for others collectors
 */
DECLARE_SMART_CLASS(Collector);
class Collector : public SmartObject
{
public:
	virtual void execute() = 0;
	virtual ~Collector() {}
};

/**
 * @brief Abstract class to be used by collectors of data provided by a
 *        PostgreSQL instance
 */
class PgCollector : public Collector
{
protected:
	InstanceConfigPtr config_instance;
public:
	PgCollector(const InstanceConfigPtr config_instance)
		: config_instance(config_instance)
	{}
	PgCollector() {}
	const InstanceConfigPtr instance() const
	{
		return this->config_instance;
	}
	void instance(InstanceConfigPtr value)
	{
		this->config_instance = value;
	}
};

/**
 * @brief Collects information about statistics of one PostgreSQL instance in
 *        the current server
 */
class PgStatsCollector : public PgCollector
{
public:
	PgStatsCollector(const InstanceConfigPtr config_instance)
		: PgCollector(config_instance)
	{}
	PgStatsCollector()
		: PgCollector()
	{}
	void execute();
};

/**
 * @brief Collect the PostgreSQL log chunks
 */
class PgLogCollector : public PgCollector
{
public:
	PgLogCollector(const InstanceConfigPtr config_instance)
		: PgCollector(config_instance)
	{}
	PgLogCollector()
		: PgCollector()
	{}
	void execute();
	void processLogFile(const std::string &filepath, int port, const std::string &log_line_prefix = "");
	void processLogStream(std::istream &istr, StateManager &state_mgr, StorageManager &storage_mgr, const std::string &filepath = "<stream>", const std::string &log_line_prefix = "");
};

/**
 * @brief Collects information about disk usage on current server
 */
class DiskUsageCollector : public Collector
{
public:
	void execute();
};

/**
 * @brief Collects sysstat information present on the logs
 */
class SysstatCollector : public Collector
{
public:
	void execute();
};

END_APP_NAMESPACE

#endif // __COLLECTORS_H__

