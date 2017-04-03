#ifndef __STATE_MANAGER_H__
#define __STATE_MANAGER_H__
/**
 * This file contains classes to manage states persisted on disks, like
 * last processed sysstat.
 * It also has class to lock the application based upon a name.
 */

#include "config.h"
#include "SmartPtr.h"
#include "win.h"

#include <stdexcept>
#include <map>
#include <string>

BEGIN_APP_NAMESPACE

class StorageManager;

DECLARE_SMART_CLASS(StateManager);
class StateManager : public SmartObject
{
protected:
	std::string m_statename;
public:
	typedef std::map<std::string, std::string> Map;
	StateManager(const std::string &statename)
		: m_statename(statename)
	{}
	StateManager() {}
	void stateName(const std::string &value);
	const std::string &stateName() const;
	virtual std::map<std::string, std::string> load() = 0;
	virtual void save(const std::map<std::string, std::string> &values) = 0;
	virtual void save(const std::map<std::string, std::string> &values, StorageManager &storage) = 0;
	virtual void unlink() = 0;
};

class CollectorStateManager : public StateManager
{
public:
	CollectorStateManager(const std::string &statename)
		: StateManager(statename)
	{}
	CollectorStateManager()
		: StateManager()
	{}
	std::map<std::string, std::string> load();
	void save(const std::map<std::string, std::string> &values);
	void save(const std::map<std::string, std::string> &values, StorageManager &storage);
	void unlink();
	static void recoveryStates();
};

class MemoryStateManager : public StateManager
{
protected:
	std::map<std::string, StateManager::Map> m_saved;
public:
	MemoryStateManager(const std::string &statename)
		: StateManager(statename)
	{}
	MemoryStateManager()
		: StateManager()
	{}
	std::map<std::string, std::string> load();
	void save(const std::map<std::string, std::string> &values);
	void save(const std::map<std::string, std::string> &values, StorageManager &storage);
	void unlink();
	void clearAll();
};

DECLARE_SMART_CLASS(ScopedFileLock);
class ScopedFileLock : public SmartObject
{
protected:
#ifndef __WIN32__
	int m_fd;
#else
	HANDLE m_fd;
#endif
	std::string m_filename;
public:
	ScopedFileLock();
	ScopedFileLock(const std::string &lockname);
	void open(const std::string &lockname);
	bool is_open() const;
	virtual ~ScopedFileLock();
};

class ScopedFileLockFail : public std::runtime_error
{
public:
	ScopedFileLockFail(const std::string &lockname);
};

END_APP_NAMESPACE

#endif // __STATE_MANAGER_H__

