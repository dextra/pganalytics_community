#ifndef __SERVER_INFO_H__
#define __SERVER_INFO_H__

#include <string>
#include <vector>

#include "SmartPtr.h"
#include "config.h"
#include "UserConfig.h"

BEGIN_APP_NAMESPACE

DECLARE_SMART_CLASS(DiskUsage);
class DiskUsage : public SmartObject
{
protected:
	std::string m_fsdevice;
	std::string m_fstype;
	long long int m_size;
	long long int m_used;
	long long int m_available;
	std::string m_usage;
	std::string m_mountpoint;
public:
	DiskUsage() : SmartObject() {}
	void fsdevice(const std::string &value)
	{
		this->m_fsdevice = value;
	}
	const std::string &fsdevice() const
	{
		return this->m_fsdevice;
	}
	void fstype(const std::string &value)
	{
		this->m_fstype = value;
	}
	const std::string &fstype() const
	{
		return this->m_fstype;
	}
	void size(long long int value)
	{
		this->m_size = value;
	}
	long long int size() const
	{
		return this->m_size;
	}
	void used(long long int value)
	{
		this->m_used = value;
	}
	long long int used() const
	{
		return this->m_used;
	}
	void available(long long int value)
	{
		this->m_available = value;
	}
	long long int available() const
	{
		return this->m_available;
	}
	void usage(const std::string &value)
	{
		this->m_usage = value;
	}
	const std::string &usage() const
	{
		return this->m_usage;
	}
	void mountpoint(const std::string &value)
	{
		this->m_mountpoint = value;
	}
	const std::string &mountpoint() const
	{
		return this->m_mountpoint;
	}
};

/**
 * Grab informations about current server
 * TODO: Make it cross-system, only POSIX for now
 */
DECLARE_SMART_CLASS(ServerInfo);
class ServerInfo : public SmartObject
{
protected:
	static ServerInfoPtr m_current;
	ServerConfigPtr m_currentServerConfig;
	mutable std::string m_hostname;
	mutable std::string m_sysname;
	mutable std::string m_nodename;
	mutable std::string m_release;
	mutable std::string m_version;
	mutable std::string m_machine;
	mutable bool m_is_kernel_info_loaded;
	mutable std::vector<DiskUsagePtr> m_diskUsage;
	void loadKernelInfo() const;
private:
	ServerInfo();
	virtual ~ServerInfo() {}
public:
	ServerConfigPtr currentServerConfig();
	static inline ServerInfoPtr instance()
	{
		if (ServerInfo::m_current.isNull())
		{
			ServerInfo::m_current = new ServerInfo();
		}
		return ServerInfo::m_current;
	}
	inline const std::string &hostname() const
	{
		return this->m_hostname;
	}
	inline const std::string &sysname() const
	{
		this->loadKernelInfo();
		return this->m_sysname;
	}
	inline const std::string &nodename() const
	{
		this->loadKernelInfo();
		return this->m_nodename;
	}
	inline const std::string &release() const
	{
		this->loadKernelInfo();
		return this->m_release;
	}
	inline const std::string &version() const
	{
		this->loadKernelInfo();
		return this->m_version;
	}
	inline const std::string &machine() const
	{
		this->loadKernelInfo();
		return this->m_machine;
	}
	void reloadDiskUsage();
	const std::vector<DiskUsagePtr> &diskUsage()
	{
		if (this->m_diskUsage.empty())
		{
			this->reloadDiskUsage();
		}
		return this->m_diskUsage;
	}
	DiskUsagePtr diskUsageForPath(const std::string &path);
	long long unsigned int directoryTreeSize(const std::string &path);
};

END_APP_NAMESPACE

#endif // __SERVER_INFO_H__

