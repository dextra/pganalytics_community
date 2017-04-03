#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "collectors.h"
#include "StorageManager.h"
#include "debug.h"
#include "util/time.h"
#include "backend_shared.h"

#include "win.h"
#include <psapi.h>
#include <tchar.h>

#include <winioctl.h>

/*** Definitions to be used on Windows APIs calls ***/

/* Types used by GetPerformanceInfo function */
typedef struct _PERFORMANCE_INFORMATION
{
	DWORD  cb;
	SIZE_T CommitTotal;
	SIZE_T CommitLimit;
	SIZE_T CommitPeak;
	SIZE_T PhysicalTotal;
	SIZE_T PhysicalAvailable;
	SIZE_T SystemCache;
	SIZE_T KernelTotal;
	SIZE_T KernelPaged;
	SIZE_T KernelNonpaged;
	SIZE_T PageSize;
	DWORD  HandleCount;
	DWORD  ProcessCount;
	DWORD  ThreadCount;
} PERFORMANCE_INFORMATION, *PPERFORMANCE_INFORMATION;
typedef BOOL (WINAPI *P_GetPerformanceInfo)(PPERFORMANCE_INFORMATION, DWORD);

/* Types used by disk performance */
typedef struct _DISK_PERFORMANCE_V2
{
	LARGE_INTEGER BytesRead;
	LARGE_INTEGER BytesWritten;
	LARGE_INTEGER ReadTime;
	LARGE_INTEGER WriteTime;
	LARGE_INTEGER IdleTime;
	DWORD         ReadCount;
	DWORD         WriteCount;
	DWORD         QueueDepth;
	DWORD         SplitCount;
	LARGE_INTEGER QueryTime;
	DWORD         StorageDeviceNumber;
	WCHAR         StorageManagerName[8];
} DISK_PERFORMANCE_V2, *PDISK_PERFORMANCE_V2;
typedef BOOL (WINAPI *P_DeviceIoControl) (
	HANDLE hDevice,
	DWORD dwIoControlCode,
	LPVOID lpInBuffer,
	DWORD nInBufferSize,
	LPVOID lpOutBuffer,
	DWORD nOutBufferSize,
	LPDWORD lpBytesReturned,
	LPOVERLAPPED lpOverlapped
);

/* Store system times (not used directly by Win-API) */
typedef struct _SYSTEM_TIMES
{
	FILETIME lpIdleTime;
	FILETIME lpKernelTime;
	FILETIME lpUserTime;
} SYSTEM_TIMES;

// Create a string with last error message
std::string GetLastErrorStdStr()
{
	/* Source: http://www.codeproject.com/Tips/479880/GetLastError-as-std-string */
	DWORD error = GetLastError();
	if (error)
	{
		LPVOID lpMsgBuf;
		DWORD bufLen = FormatMessage(
						   FORMAT_MESSAGE_ALLOCATE_BUFFER |
						   FORMAT_MESSAGE_FROM_SYSTEM |
						   FORMAT_MESSAGE_IGNORE_INSERTS,
						   NULL,
						   error,
						   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						   (LPTSTR) &lpMsgBuf,
						   0, NULL );
		if (bufLen)
		{
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
			std::string result(lpMsgStr, lpMsgStr+bufLen);

			LocalFree(lpMsgBuf);

			return result;
		}
	}
	return std::string();
}

/* WinStats collector */
class WinStats
{
public:
	struct CpuUsage
	{
		double user;
		double kernel;
		double idle;
	};
	CpuUsage cpu;
	struct DiskUsage
	{
		long BytesRead;
		long BytesWritten;
		long ReadTime;
		long WriteTime;
		long IdleTime;
		long ReadCount;
		long WriteCount;
		long QueueDepth;
		long SplitCount;
		long QueryTime;
		int StorageDeviceNumber;
		std::string StorageManagerName;
	};
	std::vector<DiskUsage> disk;
	typedef PERFORMANCE_INFORMATION MemoryUsage; /* It is not really only memory, but it is what we care most */
	MemoryUsage memory;
	std::vector<char> drives;
protected:
	std::vector<DISK_PERFORMANCE_V2 *> last_disk_perf;
	SYSTEM_TIMES *last_system_times;
	inline void setSystemTimes(SYSTEM_TIMES *new_system_times);
	inline void clearLastSystemTimes()
	{
		if (this->last_system_times != NULL)
		{
			delete this->last_system_times;
			this->last_system_times = NULL;
		}
	}
	inline void setDiskPerfFor(size_t i, DISK_PERFORMANCE_V2 *new_disk_perf);
	inline void setDiskPerf(const std::vector<DISK_PERFORMANCE_V2 *> new_disk_perf)
	{
		for (size_t i = 0; i < new_disk_perf.size(); i++)
		{
			this->setDiskPerfFor(i, new_disk_perf[i]);
		}
	}
	inline void clearLastDiskPerf()
	{
		for (size_t i = 0; i < this->last_disk_perf.size(); i++)
		{
			if (this->last_disk_perf[i] != NULL)
			{
				delete this->last_disk_perf[i];
			}
		}
	}
	void readDrivesAvailable();
	SYSTEM_TIMES *collectCPUUsage();
	DISK_PERFORMANCE_V2 *collectDiskUsageFor(const std::string &device_name);
	void collectAndSetMemoryUsage();
	inline void collectAndSetDiskUsage()
	{
		for (size_t i = 0; i < this->drives.size(); i++)
		{
			this->setDiskPerfFor(i, this->collectDiskUsageFor(std::string("\\\\.\\") + this->drives[i] + ":"));
		}
	}
	inline void collectAndSetCPUUsage()
	{
		this->setSystemTimes(this->collectCPUUsage());
	}
public:
	void collectAll(long wait_ms = 1000);
	WinStats();
	virtual ~WinStats();
};

WinStats::WinStats()
	: last_system_times(NULL)
{}

WinStats::~WinStats()
{
	this->clearLastSystemTimes();
	this->clearLastDiskPerf();
}

/* Based on: http://support.microsoft.com/kb/188768 */
#define FILETIME2BIGINT(x) ((((ULONGLONG) (x).dwHighDateTime) << 32) + (x).dwLowDateTime)

SYSTEM_TIMES *WinStats::collectCPUUsage()
{
	SYSTEM_TIMES *ret = new SYSTEM_TIMES();
	if (!::GetSystemTimes(&(ret->lpIdleTime), &(ret->lpKernelTime), &(ret->lpUserTime)))
	{
		delete ret;
		throw std::runtime_error(GetLastErrorStdStr());
	}
	return ret;
}

void WinStats::setSystemTimes(SYSTEM_TIMES *new_system_times)
{
	if (this->last_system_times != NULL)
	{
		/**
		 * Summarize the values between this new and the old collect.
		 *
		 * - idle : is the time in "idle" state
		 * - user : is the time taken for user processes
		 * - kern : is the time taken for system processes **plus** the
		 *          time in "idle" state. Isn't that annoying?! :?
		 *
		 * So, we end up with:
		 * - Total CPU time (100%) = user+kern
		 * - User CPU usage = user * 100 / <Total CPU time>
		 * - System CPU usage = (kern-idle) * 100 / <Total CPU time>
		 * - CPU idle = idle * 100 / <Total CPU time>
		 *
		 * Notice that we are really (sanely) considering system as (kern-idle)
		 */
		double user = (FILETIME2BIGINT(new_system_times->lpUserTime)   - FILETIME2BIGINT(this->last_system_times->lpUserTime));
		double kern = (FILETIME2BIGINT(new_system_times->lpKernelTime) - FILETIME2BIGINT(this->last_system_times->lpKernelTime));
		double idle = (FILETIME2BIGINT(new_system_times->lpIdleTime)   - FILETIME2BIGINT(this->last_system_times->lpIdleTime));
		this->cpu.user = (user * 100.0) / (user+kern);
		this->cpu.kernel = ((kern-idle) * 100.0) / (user+kern);
		this->cpu.idle = (idle * 100.0) / (user+kern);
		delete this->last_system_times;
	}
	this->last_system_times = new_system_times;
}

DISK_PERFORMANCE_V2 *WinStats::collectDiskUsageFor(const std::string &device_name)
{
	P_DeviceIoControl pDeviceIoControl = NULL;
	pDeviceIoControl = (P_DeviceIoControl)GetProcAddress (
						   GetModuleHandle ("kernel32.dll"),
						   "DeviceIoControl");
	if (pDeviceIoControl)
	{
		HANDLE hDevice = CreateFile(
							 device_name.c_str(),
							 0,
							 FILE_SHARE_READ | FILE_SHARE_WRITE,
							 NULL,
							 OPEN_EXISTING,
							 0,
							 NULL
						 );
		if (hDevice == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Could not open device: " + GetLastErrorStdStr());
		}
		else
		{
			DISK_PERFORMANCE_V2 *ret = new DISK_PERFORMANCE_V2();
			DWORD lpBytesReturned;
			if (
				pDeviceIoControl(
					hDevice,
					IOCTL_DISK_PERFORMANCE,
					NULL,
					0,
					(LPVOID)ret,
					sizeof(*ret),
					&lpBytesReturned,
					(LPOVERLAPPED)NULL)
			)
			{
				CloseHandle(hDevice);
				return ret;
			}
			else
			{
				delete ret;
				CloseHandle(hDevice);
				throw std::runtime_error("DeviceIoControl failed: " + GetLastErrorStdStr());
			}
		}
	}
	else
	{
		throw std::runtime_error("Could not load DeviceIoControl: " + GetLastErrorStdStr());
	}
}

void WinStats::setDiskPerfFor(size_t i, DISK_PERFORMANCE_V2 *new_disk_perf)
{
	if (this->last_disk_perf.size() < i+1)
	{
		this->last_disk_perf.resize(i+1, NULL);
	}
	if (this->disk.size() < i+1)
	{
		this->disk.resize(i+1);
	}
	if (this->last_disk_perf[i] != NULL)
	{
		DISK_PERFORMANCE_V2 *last_disk_perf_i = this->last_disk_perf[i];
		//double time_elapsed_s = (new_disk_perf->QueryTime.QuadPart - last_disk_perf_i->QueryTime.QuadPart) * 1e-7;
		this->disk[i].BytesRead = (new_disk_perf->BytesRead.QuadPart - last_disk_perf_i->BytesRead.QuadPart);
		this->disk[i].BytesWritten = (new_disk_perf->BytesWritten.QuadPart - last_disk_perf_i->BytesWritten.QuadPart);
		this->disk[i].ReadTime = (new_disk_perf->ReadTime.QuadPart - last_disk_perf_i->ReadTime.QuadPart);
		this->disk[i].WriteTime = (new_disk_perf->WriteTime.QuadPart - last_disk_perf_i->WriteTime.QuadPart);
		this->disk[i].IdleTime = (new_disk_perf->IdleTime.QuadPart - last_disk_perf_i->IdleTime.QuadPart);
		this->disk[i].ReadCount = (new_disk_perf->ReadCount - last_disk_perf_i->ReadCount);
		this->disk[i].WriteCount = (new_disk_perf->WriteCount - last_disk_perf_i->WriteCount);
		this->disk[i].QueueDepth = new_disk_perf->QueueDepth;
		this->disk[i].SplitCount = (new_disk_perf->SplitCount - last_disk_perf_i->SplitCount);
		this->disk[i].QueryTime = (new_disk_perf->QueryTime.QuadPart - last_disk_perf_i->QueryTime.QuadPart);
		this->disk[i].StorageDeviceNumber = last_disk_perf_i->StorageDeviceNumber;
		this->disk[i].StorageManagerName = ((char*)last_disk_perf_i->StorageManagerName);
		delete this->last_disk_perf[i];
	}
	this->last_disk_perf[i] = new_disk_perf;
}

void WinStats::readDrivesAvailable()
{
	size_t bufSize = 1024;
	TCHAR szTemp[bufSize];
	szTemp[0] = '\0';
	if (!::GetLogicalDriveStrings(bufSize-1, szTemp))
	{
		throw std::runtime_error(std::string("GetLogicalDriveStrings failed: ") + GetLastErrorStdStr());
	}
	else
	{
		TCHAR* p = szTemp;
		char szDrive[4] = ".:\\";
		do
		{
			szDrive[0] = *p;
			UINT dt = ::GetDriveType(szDrive);
			if (dt != DRIVE_REMOVABLE && dt != DRIVE_CDROM)
			{
				this->drives.push_back(*p);
			}
			// Go to the next NULL character.
			while (*p++);
		}
		while (*p);   // end of string
	}
}

void WinStats::collectAndSetMemoryUsage()
{
	HMODULE psapi_lib = LoadLibrary("psapi.dll");
	P_GetPerformanceInfo pGetPerformanceInfo = NULL;
	if (psapi_lib)
	{
		pGetPerformanceInfo = (P_GetPerformanceInfo)GetProcAddress(psapi_lib, "GetPerformanceInfo");
	}
	if (pGetPerformanceInfo)
	{
		if (pGetPerformanceInfo(&(this->memory), sizeof(this->memory)))
		{
		}
		else
		{
			std::cerr << GetLastErrorStdStr() << std::endl;
		}
	}
	else
	{
		std::cerr << GetLastErrorStdStr() << std::endl;
	}
}

void WinStats::collectAll(long wait_ms)
{
	/* Read available disk drives */
	if (this->drives.empty())
	{
		this->readDrivesAvailable();
	}
	/* Clear current reads, if any, just in case */
	this->clearLastSystemTimes();
	this->clearLastDiskPerf();
	/*
	 * XXX: Left CPU collections closer to SleepEx call. It is just an attempt
	 *      to make the computations interfere the least possible on CPU usage.
	 */
	/* First collect: */
	this->collectAndSetDiskUsage();
	this->collectAndSetCPUUsage();
	SleepEx(wait_ms, FALSE);
	/* Second collect: */
	this->collectAndSetCPUUsage();
	this->collectAndSetDiskUsage();
	this->collectAndSetMemoryUsage();
}

BEGIN_APP_NAMESPACE

namespace CollectorPrivate
{

extern void startSnapshot(StorageManager &storage, const std::string &snap_type, InstanceConfigPtr instance = NULL, const std::string &dbname = "");

} // namespace CollectorPrivate

void SysstatCollector::execute()
{
	WinStats stats;
	int sampling_seconds = 10;
	time_t now = Util::time::now();
	DMSG("Sampling " << sampling_seconds << " seconds...");
	stats.collectAll(sampling_seconds * 1000);
	SysstatCollectorStorageManager storage(ServerInfo::instance()->currentServerConfig());
	storage.begin();
	CollectorPrivate::startSnapshot(storage, "sysstat");
	std::ostream &out = storage.stream();
	out
			<< COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE << " sn_sysstat_cpu interval,timestamp,cpu,_user,_system,_idle\n"
			<< sampling_seconds << ";" << now << ";-1;"
			<< /* %user   */ stats.cpu.user << ";"
			<< /* %system */ stats.cpu.kernel << ";"
			<< /* %idle   */ stats.cpu.idle << "\n"
			<< "\\.\n";
	out
			<< COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE << " sn_sysstat_memusage interval,timestamp,kbmemfree,kbmemused,_memused,kbcached,kbcommit,_commit\n"
			<< sampling_seconds << ";" << now << ";"
			<< /* kbmemfree */ ((double)(stats.memory.PhysicalAvailable * stats.memory.PageSize) / 1024.0) << ";"
			<< /* kbmemused */ ((double)((stats.memory.PhysicalTotal - stats.memory.PhysicalAvailable) * stats.memory.PageSize) / 1024.0) << ";"
			<< /* _memused  */ (100.0 - (double)(stats.memory.PhysicalAvailable * 100.0) / (double)stats.memory.PhysicalTotal) << ";"
			<< /* kbcached  */ ((double)(stats.memory.SystemCache * stats.memory.PageSize) / 1024.0) << ";"
			<< /* kbcommit  */ ((double)(stats.memory.CommitTotal * stats.memory.PageSize) / 1024.0) << ";"
			<< /* _commit   */ ((double)(stats.memory.CommitTotal * 100.0) / (double)stats.memory.CommitLimit) << "\n"
			<< "\\.\n";
	out
			<< COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE << " sn_sysstat_disks interval,timestamp,dev,tps,rd_sec_s,wr_sec_s,avgrq_sz,_util\n";
	for (size_t i = 0; i < stats.drives.size(); i++)
	{
		double time_elapsed_s = stats.disk[i].QueryTime * 1e-7;
		out
				<< sampling_seconds << ";" << now << ";"
				<< /* dev      */ stats.drives[i] << ":\\" << ";"
				<< /* tps      */ ((double)(stats.disk[i].ReadCount + stats.disk[i].WriteCount) / time_elapsed_s) << ";"
				<< /* rd_sec_s */ (((double)stats.disk[i].BytesRead / 512.0) / time_elapsed_s) << ";"
				<< /* wr_sec_s */ (((double)stats.disk[i].BytesWritten / 512.0) / time_elapsed_s) << ";"
				;
		/* avgrq_sz */
		if ((stats.disk[i].ReadCount + stats.disk[i].WriteCount) == 0)
		{
			out << "\\N";
		}
		else
		{
			out << (
					((double)(stats.disk[i].BytesRead + stats.disk[i].BytesWritten) / 512.0) /* total sizes in sectors */
					/
					(double)(stats.disk[i].ReadCount + stats.disk[i].WriteCount)             /* number of requests */
				);
		}
		out << ";";
		/* %util */
		if ((stats.disk[i].WriteTime + stats.disk[i].ReadTime + stats.disk[i].IdleTime) == 0.0)
		{
			out << "\\N";
		}
		else
		{
			out << (
					(double)(
						stats.disk[i].WriteTime + stats.disk[i].ReadTime
					) * 100.0 / (double)(
						stats.disk[i].WriteTime + stats.disk[i].ReadTime + stats.disk[i].IdleTime
					)
				);
		}
		out << "\n";
	}
	out << "\\.\n";
	storage.commit();
}

END_APP_NAMESPACE

