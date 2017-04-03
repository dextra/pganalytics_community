#include "ServerInfo.h"
#include "util/streams.h"
#include "debug.h"
#include "util/fs.h"

#include "win.h"

#include <unistd.h> // gethostname
#ifndef __WIN32__
#include <sys/utsname.h> // uname
#else
// Source: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724301%28v=vs.85%29.aspx
#include <windows.h> // GetComputerNameEx
#include <stdio.h>
#include <tchar.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <errno.h>
#include <sstream>

BEGIN_APP_NAMESPACE

namespace ServerInfoPrivate
{
void uname(
	std::string *sysname,
	std::string *nodename,
	std::string *release,
	std::string *version,
	std::string *machine
);
std::string getHostName();
std::vector<DiskUsagePtr> getDiskUsageAll();
DiskUsagePtr getDiskUsageForPath(const std::string &path);

#ifdef __WIN32__
/* Get disk usage information on windows */

typedef BOOL (WINAPI *P_GDFSE)(LPCTSTR, PULARGE_INTEGER,
							   PULARGE_INTEGER, PULARGE_INTEGER);
typedef void (WINAPI *P_GETSYSINFO)(LPSYSTEM_INFO);

void uname(
	std::string *sysname,
	std::string *nodename,
	std::string *release,
	std::string *version,
	std::string *machine
)
{
	*sysname = "Windows";
	*nodename = getHostName();
	/* Get OS information */
	OSVERSIONINFO osvi;
	std::ostringstream ss;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (!::GetVersionEx(&osvi))
	{
		_dosmaperr(::GetLastError());
		throw std::runtime_error(std::string("GetVersion failed: ") + std::string(strerror(errno)));
	}
	ss << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << "." << osvi.dwBuildNumber;
	*release = ss.str();
	ss.str("");
	*version = osvi.szCSDVersion;
	/* Get hardware information */
	SYSTEM_INFO si;
	P_GETSYSINFO pGetSystemInfo = NULL;
	pGetSystemInfo = (P_GETSYSINFO)GetProcAddress(
						 GetModuleHandle ("kernel32.dll"),
						 "GetNativeSystemInfo");
	if (pGetSystemInfo)
	{
		DMSG("Using GetNativeSystemInfo");
		/* Has GetNativeSystemInfo */
		pGetSystemInfo(&si);
	}
	else
	{
		DMSG("Using GetSystemInfo");
		::GetSystemInfo(&si);
	}

	switch (si.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_INTEL:
		ss << "Intel";
		break;
	case PROCESSOR_ARCHITECTURE_MIPS:
		ss << "MIPS";
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
		ss << "Alpha";
		break;
	case PROCESSOR_ARCHITECTURE_PPC:
		ss << "PPC";
		break;
	case PROCESSOR_ARCHITECTURE_SHX:
		ss << "SHx";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		ss << "ARM";
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		ss << "IA64";
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA64:
		ss << "Alpha64";
		break;
	case PROCESSOR_ARCHITECTURE_MSIL:
		ss << "MSIL";
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		ss << "AMD64";
		break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
		ss << "IA32/WIN64";
		break;
	default:
		ss << "UNKNOWN-" << si.wProcessorArchitecture;
	}
	ss << " level \"" << si.wProcessorLevel << "\"";
	ss << " type \"" << si.dwProcessorType << "\"";
	ss << " revision \"" << si.wProcessorRevision << "\"";
	*machine = ss.str();
	ss.str("");
}

std::string getHostName()
{
	std::string ret;
	TCHAR buffer[1024] = TEXT("");
	DWORD dwSize = sizeof(buffer);
	if (!::GetComputerNameExA(ComputerNameDnsHostname, buffer, &dwSize))
	{
		_dosmaperr(::GetLastError());
		throw std::runtime_error(std::string("GetComputerNameEx failed: ") + std::string(strerror(errno)));
	}
	ret = buffer;
	return ret;
}

std::string getFsDeviceFor(const char *szDrive)
{
	std::string ret;
	UINT drive_type = ::GetDriveType(szDrive);
	char fstype[1024];
	switch (drive_type)
	{
	case DRIVE_UNKNOWN:
		ret = "UNKNOWN";
		break;
	case DRIVE_NO_ROOT_DIR:
		ret = "NO_ROOT_DIR";
		break;
	case DRIVE_REMOVABLE:
		ret = "REMOVABLE";
		break;
	case DRIVE_FIXED:
		ret = "FIXED";
		break;
	case DRIVE_REMOTE:
		ret = "REMOTE";
		break;
	case DRIVE_CDROM:
		ret = "CDROM";
		break;
	case DRIVE_RAMDISK:
		ret = "RAMDISK";
		break;
	default:
	{
		std::ostringstream ss;
		ss << "UNKNOWN_" << drive_type;
		ret = ss.str();
	}
	}
	ret.append(": ");
	/* Get the filesystem in use */
	if (!::GetVolumeInformation(szDrive, NULL, 0, NULL, NULL, NULL, fstype, 1024))
	{
		ret.append("?");
	}
	else
	{
		ret.append(fstype);
	}
	return ret;
}

std::vector<DiskUsagePtr> getDiskUsageAll()
{
	/* Based on: http://msdn.microsoft.com/en-us/library/aa366789%28v=vs.85%29.aspx */
	std::vector<DiskUsagePtr> ret;
	size_t bufSize = 1024;
	TCHAR szTemp[bufSize];
	szTemp[0] = '\0';
	if (!::GetLogicalDriveStrings(bufSize-1, szTemp))
	{
		_dosmaperr(::GetLastError());
		throw std::runtime_error(std::string("GetLogicalDriveStrings failed: ") + std::string(strerror(errno)));
	}
	else
	{
		TCHAR szDrive[3] = TEXT(" :");
		TCHAR* p = szTemp;


		do
		{
			/* Copy the drive letter to the template string */
			*szDrive = *p;

			/* XXX: This call seems unneeded for our purpose! */
			/* Look up each device name */
			/*
			TCHAR szName[MAX_PATH];
			if (!::QueryDosDevice(szDrive, szName, MAX_PATH)) {
				_dosmaperr(::GetLastError());
				throw std::runtime_error(std::string("DOS service failed: ") + std::string(strerror(errno)));
			}
			*/
			/* Add the device */
			ret.push_back(getDiskUsageForPath(szDrive));

			// Go to the next NULL character.
			while (*p++);
		}
		while (*p);   // end of string
	}

	return ret;
}

DiskUsagePtr getDiskUsageForPath(const std::string &path)
{
	/* Based on: http://support.microsoft.com/kb/231497 */
	std::string abs_path = Util::fs::absPath(path);
	DiskUsagePtr df_line = new DiskUsage();

	BOOL  fResult;

	const char  *pszDrive  = NULL;
	char szDrive[4] = "?";

	DWORD dwSectPerClust,
		  dwBytesPerSect,
		  dwFreeClusters,
		  dwTotalClusters;

	P_GDFSE pGetDiskFreeSpaceEx = NULL;

	unsigned __int64 i64FreeBytesToCaller,
			 i64TotalBytes,
			 i64FreeBytes;

	pszDrive = abs_path.c_str();

	if (pszDrive[1] == ':')
	{
		szDrive[0] = pszDrive[0];
		szDrive[1] = ':';
		szDrive[2] = '\\';
		szDrive[3] = '\0';

		pszDrive = szDrive;
	}

	/* Suppress unexpected messages, like "Please insert disk ... " for empty CD-ROM drivers */
	UINT prevmode = ::SetErrorMode(SEM_FAILCRITICALERRORS);

	/**
	 * Use GetDiskFreeSpaceEx if available; otherwise, use
	 * GetDiskFreeSpace.

	 * Note: Since GetDiskFreeSpaceEx is not in Windows 95 Retail, we
	 * dynamically link to it and only call it if it is present.  We
	 * don't need to call LoadLibrary on KERNEL32.DLL because it is
	 * already loaded into every Win32 process's address space.
	 */
	pGetDiskFreeSpaceEx = (P_GDFSE)GetProcAddress (
							  GetModuleHandle ("kernel32.dll"),
							  "GetDiskFreeSpaceExA");
	if (pGetDiskFreeSpaceEx)
	{
		fResult = pGetDiskFreeSpaceEx (pszDrive,
									   (PULARGE_INTEGER)&i64FreeBytesToCaller,
									   (PULARGE_INTEGER)&i64TotalBytes,
									   (PULARGE_INTEGER)&i64FreeBytes);
		if (fResult)
		{
			df_line->fsdevice(szDrive);
			df_line->fstype(getFsDeviceFor(szDrive));
			df_line->size((i64TotalBytes) / 1024 /* Kb */);
			df_line->used((i64TotalBytes - i64FreeBytes) / 1024 /* Kb */);
			df_line->available(i64FreeBytes / 1024 /* Kb */);
			df_line->usage("?");
			df_line->mountpoint(szDrive);
		}
	}
	else
	{
		/*
		_dosmaperr(::GetLastError());
		DMSG("Error returned by GetProcAddress: " << strerror(errno));
		*/
		fResult = GetDiskFreeSpace (pszDrive,
									&dwSectPerClust,
									&dwBytesPerSect,
									&dwFreeClusters,
									&dwTotalClusters);
		if (fResult)
		{
			/* force 64-bit math */
			i64TotalBytes = (__int64)dwTotalClusters * dwSectPerClust *
							dwBytesPerSect;
			i64FreeBytes = (__int64)dwFreeClusters * dwSectPerClust *
						   dwBytesPerSect;
			df_line->fsdevice(szDrive);
			df_line->fstype(getFsDeviceFor(szDrive));
			df_line->size((i64TotalBytes) / 1024 /* Kb */);
			df_line->used((i64TotalBytes - i64FreeBytes) / 1024 /* Kb */);
			df_line->available(i64FreeBytes / 1024 /* Kb */);
			df_line->usage("?");
			df_line->mountpoint(szDrive);
		}
	}

	/* Ignore error for removable and CD-ROM drivers */
	if (!fResult)
	{
		UINT dt = ::GetDriveType(szDrive);
		if (dt == DRIVE_REMOVABLE || dt == DRIVE_CDROM)
		{
			DMSG("Ignoring space for driver `" << szDrive << "'");
			df_line->fsdevice(szDrive);
			df_line->fstype(getFsDeviceFor(szDrive));
			df_line->size(0u);
			df_line->used(0u);
			df_line->available(0u);
			df_line->usage("?");
			df_line->mountpoint(szDrive);
			fResult = true;
		}
	}
	else
	{
		/* Calculate %usage */
		std::ostringstream str;
		str << (int)::round((df_line->used() * 100.0) / df_line->size()) << "%";
		df_line->usage(str.str());
	}

	/* Restore ErrorMode */
	::SetErrorMode(prevmode);

	if (!fResult)
	{
		_dosmaperr(::GetLastError());
		throw std::runtime_error(std::string("could not get free space for `") + path + "': " + std::string(strerror(errno)));
	}
	/*
	DMSG("df device=" << df_line->fsdevice()
			<< "; fstype=" << df_line->fstype()
			<< "; size=" << (df_line->size() / 1024.0 / 1024.0)
			<< "; used=" << (df_line->used() / 1024.0 / 1024.0)
			<< "; available=" << (df_line->available() / 1024.0 / 1024.0)
			<< "; usage=" << df_line->usage()
			<< "; mountpoint=" << df_line->mountpoint()
			<< "; functionused=" << (pGetDiskFreeSpaceEx ? "GetDiskFreeSpaceEx" : "GetDiskFreeSpace")
		);
	*/

	return df_line;

}

#else

void uname(
	std::string *sysname,
	std::string *nodename,
	std::string *release,
	std::string *version,
	std::string *machine
)
{
	struct utsname buf;
	if (::uname(&buf) != 0)
	{
		throw std::runtime_error(std::string("uname failed: ") + std::string(strerror(errno)));
	}
	*sysname = buf.sysname;
	*nodename = buf.nodename;
	*release = buf.release;
	*version = buf.version;
	*machine = buf.machine;
}

std::string getHostName()
{
	std::string ret;
	char v_hostname[1024];
	if (gethostname(v_hostname, sizeof(v_hostname)) != 0)
	{
		throw std::runtime_error(std::string("gethostname failed: ") + std::string(strerror(errno)));
	}
	ret = v_hostname;
	return ret;
}

/* Get disk usage information on POSIX df call */

DiskUsagePtr getDiskUsageFromDFLine(const std::string &dfline)
{
	/* We could just read directly from the stream, but seems safer to get line-by-line */
	std::istringstream ss(dfline);
	std::string fsdevice;
	std::string fstype;
	long long int size;
	long long int used;
	long long int available;
	std::string usage;
	std::string mountpoint;
	DiskUsagePtr df_line = new DiskUsage();
	ss
			>> fsdevice
			>> fstype
			>> size
			>> used
			>> available
			>> usage
			>> mountpoint
			;
	/*
	DMSG("Got disk usage line: "
		<< "\n\tfsdevice  : " << fsdevice
		<< "\n\tfstype    : " << fstype
		<< "\n\tsize      : " << size
		<< "\n\tused      : " << used
		<< "\n\tavailable : " << available
		<< "\n\tusage     : " << usage
		<< "\n\tmountpoint: " << mountpoint
		);
	*/
	df_line->fsdevice(fsdevice);
	df_line->fstype(fstype);
	df_line->size(size);
	df_line->used(used);
	df_line->available(available);
	df_line->usage(usage);
	df_line->mountpoint(mountpoint);
	return df_line;
}

std::vector<DiskUsagePtr> getDiskUsageAll()
{
	Util::io::iprocstream dfstr("df --portability --print-type");
	std::vector<DiskUsagePtr> ret;
	std::string line;
	(void)std::getline(dfstr, line); // skip first line (the header)
	while (std::getline(dfstr, line))
	{
		ret.push_back(ServerInfoPrivate::getDiskUsageFromDFLine(line));
	}
	return ret;
}

DiskUsagePtr getDiskUsageForPath(const std::string &path)
{
	Util::io::iprocstream dfstr(std::string("df --portability --print-type ") + Util::io::quoteProcArgument(path));
	std::string line;
	(void)std::getline(dfstr, line); // skip first line (the header)
	if (!std::getline(dfstr, line))
	{
		/* If no line, means some error happened */
		throw std::runtime_error(std::string("unknown or invalid location: ") + Util::io::quoteProcArgument(path));
	}
	else
	{
		return ServerInfoPrivate::getDiskUsageFromDFLine(line);
	}
}

#endif

} // namespace ServerInfoPrivate

ServerInfoPtr ServerInfo::m_current = (ServerInfo*)0;

ServerInfo::ServerInfo() : m_currentServerConfig(), m_is_kernel_info_loaded(false)
{
	this->m_hostname = ServerInfoPrivate::getHostName();
}

ServerConfigPtr ServerInfo::currentServerConfig()
{
	if (this->m_currentServerConfig.isNull())
	{
		UserConfigPtr uc = UserConfig::instance();
		if (uc->serverName().empty())
		{
			this->m_currentServerConfig = uc->server(this->hostname());
			this->m_currentServerConfig->initializeCurrentServer();
			uc->serverName(this->hostname());
		}
		else
		{
			this->m_currentServerConfig = uc->server(uc->serverName());
			this->m_currentServerConfig->initializeCurrentServer();
		}
	}
	return this->m_currentServerConfig;
}

void ServerInfo::loadKernelInfo() const
{
	if (!this->m_is_kernel_info_loaded)
	{
		ServerInfoPrivate::uname(
			&this->m_sysname,
			&this->m_nodename,
			&this->m_release,
			&this->m_version,
			&this->m_machine
		);
		this->m_is_kernel_info_loaded = true;
	}
}

void ServerInfo::reloadDiskUsage()
{
	this->m_diskUsage = ServerInfoPrivate::getDiskUsageAll();
}

DiskUsagePtr ServerInfo::diskUsageForPath(const std::string &path)
{
	return ServerInfoPrivate::getDiskUsageForPath(path);
}

long long unsigned int ServerInfo::directoryTreeSize(const std::string &path)
{
	long long unsigned int ret = 0;
	Util::fs::DirReader dir(path);
	while (dir.next())
	{
		std::string fname = dir.entry().d_name;
		if (fname != "." && fname != "..")
		{
			Util::fs::Stat st = Util::fs::fileStat(path + DIRECTORY_SEPARATOR + fname);
			if (S_ISDIR(st.st_mode))
			{
				ret += this->directoryTreeSize(path + DIRECTORY_SEPARATOR + fname);
			}
			else
			{
				ret += st.st_size;
			}
		}
	}
	return ret;
}

END_APP_NAMESPACE

