#ifndef __COLLECTOR_STORAGE_MANAGER_H__
#define __COLLECTOR_STORAGE_MANAGER_H__

#include "config.h"
#include "SmartPtr.h"
#include "UserConfig.h"
#include "util/streams.h"

#include <iostream>
#include <sstream>
#include <fstream>

BEGIN_APP_NAMESPACE

DECLARE_SMART_CLASS(StorageManager);
class StorageManager : public SmartObject
{
public:
	StorageManager() {}
	virtual ~StorageManager() {}
	virtual void begin() = 0;
	virtual void rollback() = 0;
	virtual void commit() = 0;
	virtual std::ostream &stream() = 0;
	virtual const std::string &fileName() const = 0;
};

/**
 * CollectorStorageManager class is responsible to implement the storage layer
 * when saving collected data into files. It works by saving files inside the
 * location defined by the ServerConfig::collectDir() (could be defined at
 * configuration file on global.collect_dir or server.collect_dir). Inside
 * collectDir, is created two subdirectoryes: "tmp" and "new".
 *
 * When CollectorStorageManager::begin() is called, an ofstream is opened to
 * an unique file located at "tmp" subdirectory. After saving the data on the
 * stream, the holder must call CollectorStorageManager::commit(), then the
 * file stream will be closed and the underline file will be moved from "tmp"
 * to "new".
 *
 * If the user calls CollectorStorageManager::rollback() or the object is
 * destroyed without a prior call to commit(), the file stream will be closed
 * and the file (at "tmp") will be removed.
 *
 * CollectorStorageManager is a virtual class, each kind of file must have a
 * class inheriting this and providing the desired suffix for the filename.
 *
 * Example:
 *
 *     PgStatsCollectorStorageManager manager(current_server_config);
 *     manager.begin();
 *     manager.stream() << data_to_save;
 *     manager.commit();
 *
 * It is exception safe, as if any exception occurs, the dtor will call
 * rollback() method.
 */

DECLARE_SMART_CLASS(CollectorStorageManager);
class CollectorStorageManager : public StorageManager
{
protected:
	std::ofstream m_ostream;
	Util::io::gzipstream *m_gzstream;
	std::string m_tmppath;
	std::string m_newpath;
	std::string m_fileName;
	ServerConfigPtr m_server;
private:
	bool close();
public:
	CollectorStorageManager(ServerConfigPtr server);
	virtual ~CollectorStorageManager();
	virtual void rollback();
	virtual void commit();
	inline std::ostream &stream()
	{
		if (this->m_gzstream)
		{
			return *this->m_gzstream;
		}
		else
		{
			return this->m_ostream;
		}
	}
	void begin();
	const std::string &fileName() const;
protected:
	virtual std::string fileNameSuffix() const = 0;
};

class PgStatsCollectorStorageManager : public CollectorStorageManager
{
protected:
	bool m_globalObjectsOnly;
public:
	PgStatsCollectorStorageManager(ServerConfigPtr server, bool globalObjectsOnly = false)
		: CollectorStorageManager(server), m_globalObjectsOnly(globalObjectsOnly) {}
	virtual ~PgStatsCollectorStorageManager() {}
protected:
	std::string fileNameSuffix() const;
};

class DiskUsageCollectorStorageManager : public CollectorStorageManager
{
public:
	DiskUsageCollectorStorageManager(ServerConfigPtr server)
		: CollectorStorageManager(server) {}
	virtual ~DiskUsageCollectorStorageManager() {}
protected:
	std::string fileNameSuffix() const;
};

class SysstatCollectorStorageManager : public CollectorStorageManager
{
public:
	SysstatCollectorStorageManager(ServerConfigPtr server)
		: CollectorStorageManager(server) {}
	virtual ~SysstatCollectorStorageManager() {}
protected:
	std::string fileNameSuffix() const;
};

class PgLogCollectorStorageManager : public CollectorStorageManager
{
public:
	PgLogCollectorStorageManager(ServerConfigPtr server)
		: CollectorStorageManager(server) {}
	virtual ~PgLogCollectorStorageManager() {}
protected:
	std::string fileNameSuffix() const;
};

class MemoryStorageManager : public StorageManager
{
protected:
	std::ostringstream m_stream;
	std::string m_filename;
	std::vector<std::string> m_saved;
	bool m_opened;
public:
	MemoryStorageManager();
	virtual ~MemoryStorageManager() {}
	void clear();
	void begin();
	void rollback();
	void commit();
	void close();
	std::ostream &stream();
	const std::string &fileName() const;
	std::vector<std::string> &data();
};

END_APP_NAMESPACE

#endif // __COLLECTOR_STORAGE_MANAGER_H__


