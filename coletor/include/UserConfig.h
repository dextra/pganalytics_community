#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include "config.h"
#include "SmartPtr.h"
#include "util/log.h"

BEGIN_APP_NAMESPACE


enum CollectTypes
{
	COLLECT_NONE     = 0,
	COLLECT_PG_STATS = 1,
	COLLECT_DF       = 2,
	COLLECT_SYSSTAT  = 4,
	COLLECT_SYSINFO  = 8,
	COLLECT_PG_LOGS  = 16,
	COLLECT_PARENT   = 256, /* Internal flag: so the server class can know it needs to ask for global */
	COLLECT_ALL      = 0xffffffff
};

/**
 * UserConfig class controls all the others declared here, so all of them
 * should use it as friend class
 */
DECLARE_SMART_CLASS(UserConfig);
DECLARE_SMART_CLASS(ServerConfig);
DECLARE_SMART_CLASS(InstanceConfig);

/**
 * To add a new configuration parameter, one must do:
 * 1. Add the setter/getter and member variable on the desired class (InstanceConfig, ServerConfig, UserConfig)
 * 2. At UserConfig::parse implementation function (on UserConfig.cpp), add the parameter as a call to ConfigParser::addParamDef
 * 3. At UserConfig.cpp, add the handling of the value on one of UserConfigPrivate::UserConfigParseHandler::handle* methods
 */

/**
 * InstanceConfig class
 *    Holds information about one database server instance
 */
class InstanceConfig : public SmartObject
{
	friend class UserConfig;
protected:
	std::string m_name;
	std::string m_conninfo;
	std::string m_role;
	std::string m_maintenanceDatabase;
	std::vector<std::string> m_databases;
	ServerConfig *m_serverConfig;
public:
	InstanceConfig(ServerConfigPtr serverConfig);
	const std::string &conninfo() const;
	void conninfo(const std::string &value);
	const std::string &role() const;
	void role(const std::string &value);
	const std::string &name() const;
	void name(const std::string &value);
	const std::string &maintenanceDatabase() const;
	void maintenanceDatabase(const std::string &value);
	ServerConfigPtr serverConfig() const;
	void databases(const std::vector<std::string> &value);
	const std::vector<std::string> &databases() const;
};

/**
 * ServerConfig class
 *     Holds information about one server and a list of available
 *     instances on this servers
 */
class ServerConfig : public SmartObject
{
	friend class UserConfig;
protected:
	std::string m_serverName;
	std::string m_hostname;
	std::string m_address;
	std::string m_collectDir;
	int m_logLevel;
	std::string m_pushCommand;
	std::vector<std::string> m_roles;
	std::vector<InstanceConfigPtr> m_instances;
	UserConfig *m_userConfig;
	int m_collect;
public:
	ServerConfig(UserConfigPtr userConfig);
	const std::string &serverName() const;
	const std::string &hostname() const;
	const std::string &address() const;
	const std::vector<std::string> &roles() const;
	const std::vector<InstanceConfigPtr> &instances() const;
	const std::string &collectDir() const;
	Util::log::LogLevel logLevel() const;
	void serverName(const std::string &value);
	void hostname(const std::string &value);
	void address(const std::string &value);
	void roles(const std::vector<std::string> &value);
	void instances(const std::vector<InstanceConfigPtr> &value);
	UserConfigPtr userConfig() const;
	void collectDir(const std::string &value);
	void logLevel(Util::log::LogLevel logLevel);
	void initializeCurrentServer();
	void collect(int value);
	void collect(const std::vector<std::string> &value);
	int collect() const;
	void pushCommand(const std::string &value);
	const std::string &pushCommand() const;
	bool needsCollect(CollectTypes type);
};

/**
 * UserConfig class
 *     Holds global customer information and a list of available servers.
 *     This class is also responsible of parsing the configuration file.
 */
namespace UserConfigPrivate
{
class UserConfigParseHandler;
}

class UserConfig : public SmartObject
{
	friend class UserConfigPrivate::UserConfigParseHandler;
protected:
	/* Global config */
	std::string m_customer;
	std::string m_bucket;
	std::string m_collectDir;
	Util::log::LogLevel m_logLevel;
	std::string m_accessKeyId;
	std::string m_secretAccessKey;
	std::string m_serverName;
	std::string m_pushCommand;
	ServerConfigPtr m_autoAddedServer;
	static UserConfigPtr m_current;
	std::map<std::string, ServerConfigPtr> m_servers;
	int m_collect;
	/* Singleton class, so the ctor/dtor are not public (should be private?) */
	UserConfig();
	virtual ~UserConfig() {}
public:
	const std::map<std::string, ServerConfigPtr> &servers() const;
	const ServerConfigPtr &server(const std::string &serverName) const;
	static inline UserConfigPtr instance()
	{
		return UserConfig::m_current;
	}
	static bool isLoaded()
	{
		return !UserConfig::m_current.isNull();
	}
	static UserConfigPtr parse(std::istream &istr);
	const std::string &customer() const;
	void customer(const std::string &value);
	const std::string &bucket() const;
	void bucket(const std::string &value);
	const std::string &collectDir() const;
	void collectDir(const std::string &value);
	Util::log::LogLevel logLevel() const;
	void logLevel(Util::log::LogLevel logLevel);
	void accessKeyId(const std::string &value);
	const std::string &accessKeyId() const;
	void secretAccessKey(const std::string &value);
	const std::string &secretAccessKey() const;
	void collect(int value);
	void collect(const std::vector<std::string> &value);
	int collect() const;
	void pushCommand(const std::string &value);
	const std::string &pushCommand() const;
	void serverName(const std::string &value);
	const std::string &serverName() const;
	bool needsCollect(CollectTypes type);
private:
	/* Only friend class will access this: */
	ServerConfigPtr createServerConfigForCurrent();
	void addServer(ServerConfigPtr server);
};

END_APP_NAMESPACE

#endif // __USER_CONFIG_H__

