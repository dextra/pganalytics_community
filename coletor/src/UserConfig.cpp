#include <iostream>
#include <stdexcept>
#include <string.h>

#include "util/fs.h"
#include "common.h"
#include "debug.h"

#include "UserConfig.h"
#include "ConfigParser.h"
#include "ServerInfo.h"

BEGIN_APP_NAMESPACE

namespace UserConfigPrivate
{
int collectTypesFromStringList(const std::vector<std::string> &value)
{
	bool first = true;
	struct
	{
		const char *name;
		CollectTypes flag;
	} collection_dict[] =
	{
		{"pg_stats", COLLECT_PG_STATS},
		{"df", COLLECT_DF},
		{"sysstat", COLLECT_SYSSTAT},
		{"sysinfo", COLLECT_SYSINFO},
		{"pg_logs", COLLECT_PG_LOGS}
	};
	int ret = COLLECT_ALL; /* Start with all */
	forall(it, value)
	{
		if (*it == "none")
		{
			ret = COLLECT_NONE;
		}
		else if (*it == "all")
		{
			ret = COLLECT_ALL;
		}
		else
		{
			bool negated = ((*it)[0] == '!' || (*it)[0] == '~');
			std::string val = (negated ? it->substr(1) : *it);
			int flag = -1;
			/* XXX: This search is O(N), but should we bother optimizing this? */
			for (size_t i = 0; i < lengthof(collection_dict); i++)
			{
				if (val == collection_dict[i].name)
				{
					flag = collection_dict[i].flag;
					break;
				}
			}
			if (flag == -1)
			{
				throw std::runtime_error(std::string("invalid flag \"") + *it + "\" for collect type");
			}
			else if (!negated)
			{
				if (first)
				{
					/* If we started the list with a inclusion item, so assume we want only those in the list */
					ret = COLLECT_NONE;
				}
				ret |= flag;
			}
			else
			{
				if (first)
				{
					/* If we started the list with a exclusion (negated) item, so assume we want all but those negated */
					ret = COLLECT_ALL;
				}
				ret &= ~flag;
			}
		}
		if (first)
		{
			first = false;
		}
	}
	return ret;
}
class UserConfigParseHandler : public ConfigHandler
{
public:
	UserConfigPtr config;
	ServerConfigPtr current_server;
	InstanceConfigPtr current_instance;
	std::vector<InstanceConfigPtr> current_instances;
	UserConfigParseHandler(UserConfigPtr config)
		: config(config)
	{}
	void handleString(const std::string &parents, const std::string &key, const std::string &value)
	{
		//DMSG("handleString(\"" << parents << "\", \"" << key << "\", \"" << value << "\");");
		bool validKey = false;
		if (parents.empty())
		{
			if (key == "server")
			{
				current_server = new ServerConfig(config);
				current_server->hostname(value);
				config->addServer(current_server);
				validKey = true;
			}
			else if (key == "customer")
			{
				config->customer(value);
				validKey = true;
			}
			else if (key == "bucket")
			{
				config->bucket(value);
				validKey = true;
			}
			else if (key == "collect_dir")
			{
				config->collectDir(value);
				validKey = true;
			}
			else if (key == "log_level")
			{
				config->logLevel(Util::log::logLevelFromString(value));
				validKey = true;
			}
			else if (key == "access_key_id")
			{
				config->accessKeyId(value);
				validKey = true;
			}
			else if (key == "secret_access_key")
			{
				config->secretAccessKey(value);
				validKey = true;
			}
			else if (key == "server_name")
			{
				config->serverName(value);
				validKey = true;
			}
			else if (key == "push_command")
			{
				config->pushCommand(value);
				validKey = true;
			}
		}
		else if (parents == "server")
		{
			ASSERT_INTERNAL_EXCEPTION(!current_server.isNull(), "current_server not initialized");
			if (key == "instance")
			{
				current_instance = new InstanceConfig(current_server);
				current_instance->name(value);
				validKey = true;
			}
			else if (key == "address")
			{
				current_server->address(value);
				validKey = true;
			}
			else if (key == "collect_dir")
			{
				current_server->collectDir(value);
				validKey = true;
			}
			else if (key == "log_level")
			{
				current_server->logLevel(Util::log::logLevelFromString(value));
				validKey = true;
			}
			else if (key == "push_command")
			{
				current_server->pushCommand(value);
				validKey = true;
			}
		}
		else if (parents == "server.instance")
		{
			ASSERT_INTERNAL_EXCEPTION(!current_instance.isNull(), "current_instance not initialized");
			if (key == "conninfo")
			{
				current_instance->conninfo(value);
				validKey = true;
			}
			else if (key == "role")
			{
				current_instance->role(value);
				validKey = true;
			}
			else if (key == "maintenance_database")
			{
				current_instance->maintenanceDatabase(value);
				validKey = true;
			}
		}
		if (!validKey)
		{
			throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + ": key \"" + (parents.empty() ? "" : parents + ".") + key + "\" unknown.");
		}
	}
	void handleStringArray(const std::string &parents, const std::string &key, const std::vector<std::string> &value)
	{
		if (key == "collect")
		{
			if (parents.empty())
			{
				config->collect(value);
			}
			else if (parents == "server")
			{
				ASSERT_INTERNAL_EXCEPTION(!current_server.isNull(), "current_server not initialized");
				current_server->collect(value);
			}
			else
			{
				throw std::runtime_error(std::string("config file error, \"collect\" is not valid inside \"") + parents + "\"");
			}
		}
		else if (parents == "server.instance" && key == "databases")
		{
			ASSERT_INTERNAL_EXCEPTION((!current_instance.isNull() && !current_server.isNull()), "current_server or current_instance not initialized");
			current_instance->databases(value);
		}
		else if (parents.empty() && key == "databases")
		{
			if (current_server.isNull())
			{
				current_server = config->createServerConfigForCurrent();
				ASSERT_INTERNAL_EXCEPTION((current_server->instances().size() == 1), "no instance created for current server");
				current_instance = (current_server->instances())[0];
			}
			current_instance->databases(value);
		}
		else
		{
			throw std::runtime_error(std::string("config file error, invalid key \"") + key + "\"");
		}
	}
	void beginGroup(const std::string &groupKey)
	{
	}
	void endGroup(const std::string &groupKey)
	{
		if (groupKey == "server")
		{
			ASSERT_INTERNAL_EXCEPTION(!current_server.isNull(), "current_server not initialized");
			current_server->instances(current_instances);
			current_instances.clear();
		}
		else if (groupKey == "server.instance")
		{
			ASSERT_INTERNAL_EXCEPTION((!current_instance.isNull() && !current_server.isNull()), "current_server or current_instance not initialized");
			current_instances.push_back(current_instance);
		}
	}
	void end()
	{
		/**
		 * If no "server" node was found, then add current server
		 * "automagically"
		 */
		if (config->servers().size() == 0)
		{
			current_server = config->createServerConfigForCurrent();
			DMSG("Added a server of name: " << current_server->hostname());
		}
	}
};
} // namespace UserConfigPrivate

/*** InstanceConfig implementation ***/

InstanceConfig::InstanceConfig(ServerConfigPtr serverConfig)
	: m_serverConfig(serverConfig)
{}

const std::string &InstanceConfig::conninfo() const
{
	return this->m_conninfo;
}

void InstanceConfig::conninfo(const std::string &value)
{
	this->m_conninfo = value;
}

const std::string &InstanceConfig::role() const
{
	return this->m_role;
}

void InstanceConfig::role(const std::string &value)
{
	this->m_role = value;
}

const std::string &InstanceConfig::name() const
{
	return this->m_name;
}

void InstanceConfig::name(const std::string &value)
{
	this->m_name = value;
}

const std::string &InstanceConfig::maintenanceDatabase() const
{
	return this->m_maintenanceDatabase;
}

void InstanceConfig::maintenanceDatabase(const std::string &value)
{
	this->m_maintenanceDatabase = value;
}

ServerConfigPtr InstanceConfig::serverConfig() const
{
	return this->m_serverConfig;
}

void InstanceConfig::databases(const std::vector<std::string> &value)
{
	this->m_databases = value;
}

const std::vector<std::string> &InstanceConfig::databases() const
{
	return this->m_databases;
}

/*** ServerConfig implementation ***/

ServerConfig::ServerConfig(UserConfigPtr userConfig)
	: m_logLevel(-1), m_userConfig(userConfig), m_collect(COLLECT_PARENT)
{}

const std::string &ServerConfig::serverName() const
{
	return this->m_serverName;
}

const std::string &ServerConfig::hostname() const
{
	return this->m_hostname;
}

const std::string &ServerConfig::address() const
{
	return this->m_address;
}

const std::vector<std::string> &ServerConfig::roles() const
{
	return this->m_roles;
}

const std::vector<InstanceConfigPtr> &ServerConfig::instances() const
{
	return this->m_instances;
}

const std::string &ServerConfig::collectDir() const
{
	if (this->m_collectDir.empty())
		return this->userConfig()->collectDir();
	return this->m_collectDir;
}

Util::log::LogLevel ServerConfig::logLevel() const
{
	if (this->m_logLevel < 0)
		return this->userConfig()->logLevel();
	return (Util::log::LogLevel)this->m_logLevel;
}

void ServerConfig::serverName(const std::string &value)
{
	this->m_serverName = value;
}

void ServerConfig::hostname(const std::string &value)
{
	this->m_hostname = value;
}

void ServerConfig::address(const std::string &value)
{
	this->m_address = value;
}

void ServerConfig::roles(const std::vector<std::string> &value)
{
	this->m_roles = value;
}

void ServerConfig::instances(const std::vector<InstanceConfigPtr> &value)
{
	this->m_instances = value;
}

void ServerConfig::collectDir(const std::string &value)
{
	this->m_collectDir = value;
}

void ServerConfig::logLevel(Util::log::LogLevel value)
{
	this->m_logLevel = value;
}

void ServerConfig::collect(int value)
{
	this->m_collect = value;
}

void ServerConfig::collect(const std::vector<std::string> &value)
{
	this->m_collect = UserConfigPrivate::collectTypesFromStringList(value);
}

int ServerConfig::collect() const
{
	if (this->m_collect & COLLECT_PARENT)
	{
		return this->userConfig()->collect();
	}
	else
	{
		return this->m_collect;
	}
}

bool ServerConfig::needsCollect(CollectTypes type)
{
	return (type & this->collect());
}

void ServerConfig::pushCommand(const std::string &value)
{
	this->m_pushCommand = value;
}

const std::string &ServerConfig::pushCommand() const
{
	if (this->m_pushCommand.empty())
	{
		return this->userConfig()->pushCommand();
	}
	return this->m_pushCommand;
}

UserConfigPtr ServerConfig::userConfig() const
{
	return this->m_userConfig;
}

void ServerConfig::initializeCurrentServer()
{
	/* Create collect data directory */
	Util::fs::mkdir_p(this->collectDir());
	/* Create directory structure tree */
	Util::fs::mkdir_p(this->collectDir() + COLLECT_DIR_TMP);
	Util::fs::mkdir_p(this->collectDir() + COLLECT_DIR_NEW);
	Util::fs::mkdir_p(this->collectDir() + COLLECT_DIR_STATES);
	Util::fs::mkdir_p(this->collectDir() + COLLECT_DIR_LOG);
}

/*** UserConfig implementation ***/

UserConfigPtr UserConfig::m_current = 0;

UserConfig::UserConfig()
	: m_logLevel(Util::log::L_DEBUG), m_collect(COLLECT_ALL)
{}

const std::map<std::string, ServerConfigPtr> &UserConfig::servers() const
{
	return this->m_servers;
}

const ServerConfigPtr &UserConfig::server(const std::string &serverName) const
{
	std::map<std::string, ServerConfigPtr>::const_iterator si = this->m_servers.find(serverName);
	if (si == this->m_servers.end())
	{
		for (si = this->m_servers.begin(); si != this->m_servers.end(); si++)
		{
			DMSG("Available server: \"" << si->first << "\"");
			if (si->first == serverName)
				DMSG("What? Is it equal?");
		}
		throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + ": server \"" + serverName + "\" not found.");
	}
	return si->second;
}

UserConfigPtr UserConfig::parse(std::istream &istr)
{
	UserConfig::m_current = new UserConfig();
	{
		UserConfigPrivate::UserConfigParseHandler handler(UserConfig::m_current);
		ConfigParser parser;
		/* Configuration file definition */
		parser.addParamDef("customer", ConfigParser::P_STR, true, false);
		parser.addParamDef("bucket", ConfigParser::P_STR, true, false);
		parser.addParamDef("collect_dir", ConfigParser::P_STR, false, false);
		parser.addParamDef("collect", ConfigParser::P_STR_LIST, false, false);
		parser.addParamDef("access_key_id", ConfigParser::P_STR, false, false);
		parser.addParamDef("secret_access_key", ConfigParser::P_STR, false, false);
		parser.addParamDef("server_name", ConfigParser::P_STR, false, false);
		parser.addParamDef("server", ConfigParser::P_STR, false, true); /* value is the server name */
		parser.addParamDef("databases", ConfigParser::P_STR_LIST, false, false);
		parser.addParamDef("push_command", ConfigParser::P_STR, false, false);
		parser.addParamDef("log_level", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.address", ConfigParser::P_STR, true, false);
		parser.addParamDef("server.collect_dir", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.collect", ConfigParser::P_STR_LIST, false, false);
		parser.addParamDef("server.push_command", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.log_level", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.instance", ConfigParser::P_STR, false, true); /* value is the instance name */
		parser.addParamDef("server.instance.conninfo", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.instance.role", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.instance.maintenance_database", ConfigParser::P_STR, false, false);
		parser.addParamDef("server.instance.databases", ConfigParser::P_STR_LIST, false, false);
		parser.parse(istr, handler);
	}
	return UserConfig::m_current;
}

const std::string &UserConfig::customer() const
{
	return this->m_customer;
}

void UserConfig::customer(const std::string &value)
{
	this->m_customer = value;
}

const std::string &UserConfig::bucket() const
{
	return this->m_bucket;
}

void UserConfig::bucket(const std::string &value)
{
	this->m_bucket = value;
}

const std::string &UserConfig::collectDir() const
{
	return this->m_collectDir;
}

void UserConfig::collectDir(const std::string &value)
{
	this->m_collectDir = value;
}

Util::log::LogLevel UserConfig::logLevel() const
{
	return this->m_logLevel;
}

void UserConfig::logLevel(Util::log::LogLevel value)
{
	this->m_logLevel = value;
}

void UserConfig::accessKeyId(const std::string &value)
{
	this->m_accessKeyId = value;
}

const std::string &UserConfig::accessKeyId() const
{
	return this->m_accessKeyId;
}

void UserConfig::secretAccessKey(const std::string &value)
{
	this->m_secretAccessKey = value;
}

const std::string &UserConfig::secretAccessKey() const
{
	return this->m_secretAccessKey;
}

void UserConfig::collect(int value)
{
	this->m_collect = value;
}

void UserConfig::collect(const std::vector<std::string> &value)
{
	this->m_collect = UserConfigPrivate::collectTypesFromStringList(value);
}

int UserConfig::collect() const
{
	return this->m_collect;
}

void UserConfig::pushCommand(const std::string &value)
{
	this->m_pushCommand = value;
}

const std::string &UserConfig::pushCommand() const
{
	return this->m_pushCommand;
}

void UserConfig::serverName(const std::string &value)
{
	if (!this->m_autoAddedServer.isNull())
	{
		std::map<std::string, ServerConfigPtr>::iterator it = this->m_servers.find(this->m_autoAddedServer->hostname());
		this->m_autoAddedServer->hostname(value);
		if (it != this->m_servers.end())
		{
			this->m_servers.erase(it);
		}
		this->addServer(this->m_autoAddedServer);
	}
	this->m_serverName = value;
}

const std::string &UserConfig::serverName() const
{
	return this->m_serverName;
}

bool UserConfig::needsCollect(CollectTypes type)
{
	return (type & this->collect());
}

void UserConfig::addServer(ServerConfigPtr server)
{
	std::pair<std::map<std::string, ServerConfigPtr>::iterator, bool> ret;
	ret = this->m_servers.insert(std::pair<std::string, ServerConfigPtr>(server->hostname(), server));
	if (!ret.second)
	{
		throw std::runtime_error(std::string("invalid configuration file, duplicated server \"") + server->hostname() + "\"");
	}
}

ServerConfigPtr UserConfig::createServerConfigForCurrent()
{
	ServerInfoPtr server_info = ServerInfo::instance();
	ServerConfigPtr ret = new ServerConfig(this);
	if (this->serverName().empty())
	{
		ret->hostname(server_info->hostname());
	}
	else
	{
		ret->hostname(this->serverName());
	}
	if (ret->needsCollect(COLLECT_PG_STATS))
	{
		/* If it needs to collect pg stats, assume instance with default connection */
		InstanceConfigPtr inst = new InstanceConfig(ret);
		std::vector<InstanceConfigPtr> instances;
		inst->name("default");
		inst->conninfo("");
		instances.push_back(inst);
		ret->instances(instances);
		/* TODO: On Debian-like distros we can try using pg_lsclusters */
	}
	this->addServer(ret);
	this->m_autoAddedServer = ret;
	return ret;
}

END_APP_NAMESPACE

