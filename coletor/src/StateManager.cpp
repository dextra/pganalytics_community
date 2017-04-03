#include "debug.h"
#include "StateManager.h"
#include "StorageManager.h"
#include "util/fs.h"
#include "util/string.h"
#include "common.h"
#include "UserConfig.h"
#include "ServerInfo.h"

#include <sys/file.h>
#include <fstream>
#include <cstring>
#include <cerrno>

BEGIN_APP_NAMESPACE

namespace StateManagerPrivate
{
static const char *prefix_state = ".state";
static const char *prefix_tmp = ".wal";
static const char *prefix_control = ".control";
void save_phase1(const std::map<std::string, std::string> &values, const std::string &tmpfilename)
{
	std::string line;
	std::ofstream ostr;
	ostr.exceptions(std::ios::failbit | std::ios::badbit);
	ostr.open(tmpfilename.c_str(), std::ios::out | std::ios::binary);
	ostr.exceptions(std::ios::badbit);
	forall (it, values)
	{
		ostr << it->first << " " << Util::escapeString(it->second) << std::endl;
	}
	ostr.close();
}
void validate_statename(const std::string &statename)
{
	ASSERT_EXCEPTION(!statename.empty(), std::runtime_error, "statename not defined");
}
} // namespace StateManagerPrivate

void StateManager::stateName(const std::string &value)
{
	this->m_statename = value;
}

const std::string &StateManager::stateName() const
{
	return this->m_statename;
}

/*** CollectorStateManager implementation ***/

std::map<std::string, std::string> CollectorStateManager::load()
{
	StateManagerPrivate::validate_statename(this->stateName());
	std::map<std::string, std::string> ret;
	std::string filename = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_STATES + DIRECTORY_SEPARATOR + this->m_statename + StateManagerPrivate::prefix_state;
	std::string line;
	std::ifstream istr;
	//DMSG(filename);
	if (!Util::fs::fileExists(filename))
	{
		/* If the state file does not exists, return an empty map */
		return ret; /* XXX: ret is empty */
	}
	istr.exceptions(std::ios::failbit | std::ios::badbit);
	istr.open(filename.c_str(), std::ios::in | std::ios::binary);
	istr.exceptions(std::ios::badbit);
	while (std::getline(istr, line))
	{
		std::string key;
		std::string value;
		size_t i;
		for (i = 0; i < line.size() && (line[i] != ' '); i++)
		{
			key += line[i];
		}
		if (key.empty() || line[i] != ' ')
		{
			throw std::runtime_error(std::string("corrupted state file: `") + filename + "'");
		}
		for (i++ /* skip space */; i < line.size(); i++)
		{
			value += line[i];
		}
		ret[key] = Util::unescapeString(value);
	}
	istr.close();
	return ret;
}

void CollectorStateManager::save(const std::map<std::string, std::string> &values)
{
	StateManagerPrivate::validate_statename(this->stateName());
	std::string filename = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_STATES + DIRECTORY_SEPARATOR + this->m_statename;
	std::string tmpfilename = filename + StateManagerPrivate::prefix_tmp;
	filename += StateManagerPrivate::prefix_state;
	StateManagerPrivate::save_phase1(values, tmpfilename);
	Util::fs::rename(tmpfilename, filename, true /* overwrite */);
}

void CollectorStateManager::save(const std::map<std::string, std::string> &values, StorageManager &storage)
{
	StateManagerPrivate::validate_statename(this->stateName());
	std::string filename = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_STATES + DIRECTORY_SEPARATOR + this->m_statename;
	std::string tmpfilename = filename + StateManagerPrivate::prefix_tmp;
	std::string controlfilename = filename + StateManagerPrivate::prefix_control;
	filename += StateManagerPrivate::prefix_state;
	// Flush the storage stream (just in case)
	storage.stream().flush();
	{
		// write control file
		std::ofstream ostr;
		ostr.exceptions(std::ios::failbit | std::ios::badbit);
		ostr.open(controlfilename.c_str(), std::ios::out | std::ios::binary);
		ostr.exceptions(std::ios::badbit);
		ostr << storage.fileName() << std::endl;
		ostr.close();
	} // end control file
	// Save to temp file
	StateManagerPrivate::save_phase1(values, tmpfilename);
	// COMMIT the storage
	storage.commit();
	// If storage commited, save the state file and remove control
	Util::fs::rename(tmpfilename, filename, true /* overwrite */);
	try
	{
		Util::fs::remove(controlfilename);
	}
	catch(std::runtime_error &e)
	{
		DMSG("Error while removing controlfile `" << controlfilename << "': " << e.what());
	}
}

void CollectorStateManager::unlink()
{
	StateManagerPrivate::validate_statename(this->stateName());
	/* Just remove the state file */
	std::string filename = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_STATES + DIRECTORY_SEPARATOR + this->m_statename;
	try
	{
		DMSG("Removing " << filename);
		Util::fs::remove(filename);
	}
	catch(std::runtime_error &e)
	{
		DMSG("We don't care about error");
	}
}

void CollectorStateManager::recoveryStates()
{
	std::string state_dir = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_STATES;
	std::string controlprefix = StateManagerPrivate::prefix_control;
	Util::fs::DirReader dr(state_dir);
	while (dr.next())
	{
		std::string controlfile = dr.entry().d_name;
		/*
		 * Verify if it is a control file (we are just looking for those)
		 */
		if (controlfile.size() > controlprefix.size() && controlfile.substr(controlfile.size() - controlprefix.size()) == controlprefix)
		{
			controlfile = state_dir + DIRECTORY_SEPARATOR + controlfile;
			std::string filename = controlfile.substr(0, controlfile.size() - controlprefix.size());
			std::string tmpfile = filename + StateManagerPrivate::prefix_tmp;
			std::string statefile = filename + StateManagerPrivate::prefix_state;
			DMSG("Recoverying controlfile=" << controlfile << ", tmpfile=" << tmpfile << ", statefile=" << statefile);
			if (!Util::fs::fileExists(tmpfile))
			{
				/**
				 * If there is no tmpfile, there is not to save at state,
				 * so the controlfile is stale and we must remove it.
				 */
				DMSG("tmpfile doesn't exists, removing control: " << controlfile);
				Util::fs::remove(controlfile);
			}
			else
			{
				/**
				 * Read the controlfile and get the storage filename that is
				 * saved on it. If such file exists at COLLECT_DIR_TMP, means
				 * that the operation must be rolled back, so we just remove
				 * the controlfile and tmpfile. Otherwise, means that the
				 * operation has been committed successfully, and we must
				 * apply the contents of tmpfile to statefile and also remove
				 * the controlfile
				 */
				std::string storagefile;
				std::ifstream istr;
				istr.exceptions(std::ios::failbit | std::ios::badbit);
				istr.open(controlfile.c_str(), std::ios::in | std::ios::binary);
				istr.exceptions(std::ios::badbit);
				if (!(istr >> storagefile))
				{
					storagefile = "";
				}
				istr.close();
				if (storagefile.empty())
				{
					DMSG("Couldn't read controlfile, removing: " << controlfile);
					Util::fs::remove(controlfile);
				}
				else
				{
					std::string storage_file = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_TMP + DIRECTORY_SEPARATOR + storagefile;
					if (Util::fs::fileExists(storage_file))
					{
						/* The file is at COLLECT_DIR_TMP, so it has failed */
						DMSG("Rolling back: " << controlfile);
						Util::fs::remove(controlfile);
						Util::fs::remove(tmpfile);
						Util::fs::remove(storage_file);
					}
					else
					{
						/**
						 * The storage file is not at "tmp", so it must has
						 * been COMMITed, apply the tmpfile to statefile and
						 * remove the controlfile.
						 * The order matters, as if some error occurs in between
						 * we'll go back here, but the temp file will not exist
						 * anymore and you'll just remove the controlfile.
						 */
						DMSG("Committing: " << controlfile);
						Util::fs::rename(tmpfile, statefile, true /* overwrite */);
						Util::fs::remove(controlfile);
					}
				}
			}
		}
	}
}

/*** MemoryStateManager implementation ***/

std::map<std::string, std::string> MemoryStateManager::load()
{
	StateManagerPrivate::validate_statename(this->stateName());
	std::map<std::string, StateManager::Map>::iterator it = this->m_saved.find(this->stateName());
	if (it == this->m_saved.end())
	{
		std::map<std::string, std::string> ret;
		return ret;
	}
	else
	{
		return it->second;
	}
}

void MemoryStateManager::save(const std::map<std::string, std::string> &values)
{
	StateManagerPrivate::validate_statename(this->stateName());
	this->m_saved[this->stateName()] = values;
}

void MemoryStateManager::save(const std::map<std::string, std::string> &values, StorageManager &storage)
{
	StateManagerPrivate::validate_statename(this->stateName());
	/* XXX: Not very 2-phase, but this classe is actually used only on tests */
	storage.commit();
	this->save(values);
}

void MemoryStateManager::unlink()
{
	StateManagerPrivate::validate_statename(this->stateName());
	this->m_saved.erase(this->stateName());
}

void MemoryStateManager::clearAll()
{
	this->m_saved.clear();
}

/*** ScopedFileLock implementation ***/

ScopedFileLock::ScopedFileLock()
	:
#ifndef __WIN32__
	m_fd(-1)
#else
	m_fd(NULL)
#endif
{ }

ScopedFileLock::ScopedFileLock(const std::string &lockname)
	:
#ifndef __WIN32__
	m_fd(-1)
#else
	m_fd(NULL)
#endif
{
	this->open(lockname);
}

void ScopedFileLock::open(const std::string &lockname)
{
	if (this->is_open())
	{
		throw std::runtime_error("lock file already opened");
	}
	this->m_filename = ServerInfo::instance()->currentServerConfig()->collectDir() + COLLECT_DIR_STATES + DIRECTORY_SEPARATOR + "." + lockname + ".lock";
#ifndef __WIN32__
	mode_t m = ::umask(0);
	this->m_fd = ::open(this->m_filename.c_str(), O_RDWR | O_CREAT, 0666);
	::umask(m);
	if(this->m_fd >= 0 && flock(m_fd, LOCK_EX | LOCK_NB) < 0)
	{
		::close(this->m_fd);
		this->m_fd = -1;
		throw ScopedFileLockFail(lockname);
	}
#else
	this->m_fd = ::CreateFile(
					 /* lpFilename */            this->m_filename.c_str(),
					 /* dwDesiredAccess */       GENERIC_READ | GENERIC_WRITE,
					 /* dwShareMode */           0,
					 /* lpSecurityAttributes */  NULL,
					 /* dwCreationDisposition */ CREATE_ALWAYS,
					 /* dwFlagsAndAttributes */  0,
					 /* hTemplateFile */         NULL
				 );
	if (!this->is_open())
	{
		if (::GetLastError() ==  ERROR_SHARING_VIOLATION)
		{
			throw ScopedFileLockFail(lockname);
		}
		else
		{
			_dosmaperr(::GetLastError());
		}
	}
#endif
	if (!this->is_open())
	{
		throw std::runtime_error(std::string("open failed for file `") + this->m_filename + "': " + strerror(errno));
	}
}

bool ScopedFileLock::is_open() const
{
#ifndef __WIN32__
	return (this->m_fd >= 0);
#else
	return (this->m_fd != NULL && this->m_fd != INVALID_HANDLE_VALUE);
#endif
}

ScopedFileLock::~ScopedFileLock()
{
	if(!this->is_open())
		return;
#ifdef __WIN32__
	::CloseHandle(this->m_fd);
	Util::fs::remove(this->m_filename);
#else
	Util::fs::remove(this->m_filename);
	::close(this->m_fd);
#endif
}

ScopedFileLockFail::ScopedFileLockFail(const std::string &lockname)
	: std::runtime_error(std::string("concurrent access denied for: ") + lockname)
{}

END_APP_NAMESPACE

