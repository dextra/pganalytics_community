#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

/* Based pretty much on: http://stackoverflow.com/a/13219796/1596080 */
#define WIN32_WINNT 0x0501
#define WINVER 0x0501

#include <windows.h>
#include <winbase.h>
#include <psapi.h>
#include <tchar.h>
/*
#include <pdh.h>
*/

#include <winioctl.h>

/*** Definitions to be used on Windows APIs calls ***/

/* Types used by GetPerformanceInfo function */
typedef struct _PERFORMANCE_INFORMATION {
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
typedef struct _DISK_PERFORMANCE_V2 {
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
typedef struct _SYSTEM_TIMES {
	FILETIME lpIdleTime;
	FILETIME lpKernelTime;
	FILETIME lpUserTime;
} SYSTEM_TIMES;

// Create a string with last error message
std::string GetLastErrorStdStr() {
	/* Source: http://www.codeproject.com/Tips/479880/GetLastError-as-std-string */
	DWORD error = GetLastError();
	if (error) {
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
		if (bufLen) {
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
			std::string result(lpMsgStr, lpMsgStr+bufLen);

			LocalFree(lpMsgBuf);

			return result;
		}
	}
	return std::string();
}

class WinStats {
	public:
		struct CpuUsage {
			double user;
			double kernel;
			double idle;
		};
		CpuUsage cpu;
		struct DiskUsage {
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
	protected:
		std::vector<char> drives;
		std::vector<DISK_PERFORMANCE_V2 *> last_disk_perf;
		SYSTEM_TIMES *last_system_times;
		inline void setSystemTimes(SYSTEM_TIMES *new_system_times);
		inline void clearLastSystemTimes() {
			if (this->last_system_times != NULL) {
				delete this->last_system_times;
				this->last_system_times = NULL;
			}
		}
		inline void setDiskPerfFor(size_t i, DISK_PERFORMANCE_V2 *new_disk_perf);
		inline void setDiskPerf(const std::vector<DISK_PERFORMANCE_V2 *> new_disk_perf) {
			for (size_t i = 0; i < new_disk_perf.size(); i++) {
				this->setDiskPerfFor(i, new_disk_perf[i]);
			}
		}
		inline void clearLastDiskPerf() {
			for (size_t i = 0; i < this->last_disk_perf.size(); i++) {
				if (this->last_disk_perf[i] != NULL) {
					delete this->last_disk_perf[i];
				}
			}
		}
		void readDrivesAvailable();
		SYSTEM_TIMES *collectCPUUsage();
		DISK_PERFORMANCE_V2 *collectDiskUsageFor(const std::string &device_name);
		void collectAndSetMemoryUsage();
		inline void collectAndSetDiskUsage() {
			for (size_t i = 0; i < this->drives.size(); i++) {
				this->setDiskPerfFor(i, this->collectDiskUsageFor(std::string("\\\\.\\") + this->drives[i] + ":"));
			}
		}
		inline void collectAndSetCPUUsage() {
			this->setSystemTimes(this->collectCPUUsage());
		}
	public:
		void showStatistics(std::ostream &out);
		void collectAll(long wait_ms = 1000);
		void memoryUsage(std::ostream &out);
		void memoryPerf(std::ostream &out);
		void diskPerf(std::ostream &out);
		void cpuUsage(std::ostream &out);
		WinStats();
		virtual ~WinStats();
};

WinStats::WinStats()
	: last_system_times(NULL)
{}

WinStats::~WinStats() {
	this->clearLastSystemTimes();
	this->clearLastDiskPerf();
}

void WinStats::memoryUsage(std::ostream &out) {
	/*
	PROCESS_MEMORY_COUNTERS proc_mem_counter;
	GetProcessMemoryInfo(GetCurrentProcess(), &proc_mem_counter, sizeof(proc_mem_counter));
	return proc_mem_counter.WorkingSetSize;
	*/
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);
	ULONGLONG totalVirtualMem = memInfo.ullTotalPageFile;
	ULONGLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;
	ULONGLONG totalPhysMem = memInfo.ullTotalPhys;
	ULONGLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
	out
		<< " totalVirtualMem (MB) : " << (long)(totalVirtualMem / 1024.0 / 1024.0) << std::endl
		<< " virtualMemUsed (MB)  : " << (long)(virtualMemUsed / 1024.0 / 1024.0) << std::endl
		<< " totalPhysMem (MB)    : " << (long)(totalPhysMem / 1024.0 / 1024.0) << std::endl
		<< " physMemUsed (MB)     : " << (long)(physMemUsed / 1024.0 / 1024.0) << std::endl
		<< " VM (%)               : " << (virtualMemUsed * 100.0 / totalVirtualMem) << std::endl
		<< " PM (%)               : " << (physMemUsed * 100.0 / totalPhysMem) << std::endl
		;
}

void WinStats::memoryPerf(std::ostream &out) {
	PERFORMANCE_INFORMATION memPerfInfo;
	HMODULE psapi_lib = LoadLibrary("psapi.dll");
	P_GetPerformanceInfo pGetPerformanceInfo = NULL;
	if (psapi_lib) {
		pGetPerformanceInfo = (P_GetPerformanceInfo)GetProcAddress(psapi_lib, "GetPerformanceInfo");
	}
	if (pGetPerformanceInfo) {
		if (pGetPerformanceInfo(&memPerfInfo, sizeof(memPerfInfo))) {
			out
				             << "CommitTotal       (MB): " << (memPerfInfo.CommitTotal * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "CommitLimit       (MB): " << (memPerfInfo.CommitLimit * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "CommitPeak        (MB): " << (memPerfInfo.CommitPeak * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "PhysicalTotal     (MB): " << (memPerfInfo.PhysicalTotal * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "PhysicalAvailable (MB): " << (memPerfInfo.PhysicalAvailable * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "SystemCache       (MB): " << (memPerfInfo.SystemCache * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "KernelTotal       (MB): " << (memPerfInfo.KernelTotal * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "KernelPaged       (MB): " << (memPerfInfo.KernelPaged * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "KernelNonpaged    (MB): " << (memPerfInfo.KernelNonpaged * memPerfInfo.PageSize / 1024 / 1024)
				<< std::endl << "PageSize          : " << memPerfInfo.PageSize
				<< std::endl << "HandleCount       : " << memPerfInfo.HandleCount
				<< std::endl << "ProcessCount      : " << memPerfInfo.ProcessCount
				<< std::endl << "ThreadCount       : " << memPerfInfo.ThreadCount
				<< std::endl
				;
		} else {
			std::cerr << GetLastErrorStdStr() << std::endl;
		}
	} else {
		std::cerr << GetLastErrorStdStr() << std::endl;
	}
}

void WinStats::diskPerf(std::ostream &out) {
	DISK_PERFORMANCE_V2 diskPerfInfo_1;
	DISK_PERFORMANCE_V2 diskPerfInfo_2;
	P_DeviceIoControl pDeviceIoControl = NULL;
	pDeviceIoControl = (P_DeviceIoControl)GetProcAddress (
			GetModuleHandle ("kernel32.dll"),
			"DeviceIoControl");
	if (pDeviceIoControl) {
		HANDLE hDevice = CreateFile(
				"\\\\.\\C:", //"\\\\.\\PhysicalDrive0",
				0,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				0,
				NULL
				);
		if (hDevice == INVALID_HANDLE_VALUE) {
			std::cerr << "Could not open device" << std::endl;
		} else {
			DWORD lpBytesReturned;
			if (pDeviceIoControl(hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0, (LPVOID)&diskPerfInfo_1, sizeof(diskPerfInfo_1), &lpBytesReturned, (LPOVERLAPPED)NULL)) {
				std::cerr << "First call succeeded!" << std::endl;
				SleepEx(2000, FALSE);
				if (pDeviceIoControl(hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0, (LPVOID)&diskPerfInfo_2, sizeof(diskPerfInfo_2), &lpBytesReturned, (LPOVERLAPPED)NULL)) {
					double time_elapsed_s = (diskPerfInfo_2.QueryTime.QuadPart - diskPerfInfo_1.QueryTime.QuadPart) * 1e-7;
					std::cerr << "Second call succeeded!" << std::endl;
					out
						          << "BytesRead: " << (diskPerfInfo_2.BytesRead.QuadPart - diskPerfInfo_1.BytesRead.QuadPart)
						<< std::endl << "BytesWritten: " << (diskPerfInfo_2.BytesWritten.QuadPart - diskPerfInfo_1.BytesWritten.QuadPart) / 1024.0
						<< std::endl << "ReadTime: " << (diskPerfInfo_2.ReadTime.QuadPart - diskPerfInfo_1.ReadTime.QuadPart)
						<< std::endl << "WriteTime: " << (diskPerfInfo_2.WriteTime.QuadPart - diskPerfInfo_1.WriteTime.QuadPart)
						<< std::endl << "IdleTime: " << (diskPerfInfo_2.IdleTime.QuadPart - diskPerfInfo_1.IdleTime.QuadPart)
						<< std::endl << "ReadCount: " << (diskPerfInfo_2.ReadCount - diskPerfInfo_1.ReadCount)
						<< std::endl << "WriteCount: " << (diskPerfInfo_2.WriteCount - diskPerfInfo_1.WriteCount)
						<< std::endl << "QueueDepth: " << diskPerfInfo_2.QueueDepth
						<< std::endl << "SplitCount: " << (diskPerfInfo_2.SplitCount - diskPerfInfo_1.SplitCount)
						<< std::endl << "QueryTime: " << (diskPerfInfo_2.QueryTime.QuadPart - diskPerfInfo_1.QueryTime.QuadPart)
						<< std::endl << "StorageDeviceNumber: " << diskPerfInfo_1.StorageDeviceNumber
						<< std::endl << "StorageManagerName: " << ((char*)diskPerfInfo_1.StorageManagerName)
						<< std::endl << "Elapsed (s): " << time_elapsed_s
						<< std::endl << "Read Kb/s : " << (
								(double)((diskPerfInfo_2.BytesRead.QuadPart - diskPerfInfo_1.BytesRead.QuadPart) / 1024.0)
								/ time_elapsed_s
							)
						<< std::endl << "Write Kb/s : " << (
								(double)((diskPerfInfo_2.BytesWritten.QuadPart - diskPerfInfo_1.BytesWritten.QuadPart) / 1024.0)
								/ time_elapsed_s
							)
						<< std::endl << "Read sec/s : " << (
								(double)((diskPerfInfo_2.BytesRead.QuadPart - diskPerfInfo_1.BytesRead.QuadPart) / 512.0)
								/ time_elapsed_s
							)
						<< std::endl << "Write sec/s : " << (
								(double)((diskPerfInfo_2.BytesWritten.QuadPart - diskPerfInfo_1.BytesWritten.QuadPart) / 512.0)
								/ time_elapsed_s
							)
						<< std::endl << "TPS: " << (
								(double)(
									(diskPerfInfo_2.ReadCount - diskPerfInfo_1.ReadCount)
									+ (diskPerfInfo_2.WriteCount - diskPerfInfo_1.WriteCount)
								) / time_elapsed_s
							)
						<< std::endl << "Util (%): " << (
								(double)(
									(diskPerfInfo_2.WriteTime.QuadPart - diskPerfInfo_1.WriteTime.QuadPart)
									+ (diskPerfInfo_2.ReadTime.QuadPart - diskPerfInfo_1.ReadTime.QuadPart)
								) * 100.0 / (double)(
									(diskPerfInfo_2.WriteTime.QuadPart - diskPerfInfo_1.WriteTime.QuadPart)
									+ (diskPerfInfo_2.ReadTime.QuadPart - diskPerfInfo_1.ReadTime.QuadPart)
									+ (diskPerfInfo_2.IdleTime.QuadPart - diskPerfInfo_1.IdleTime.QuadPart)
								)
							)
						<< std::endl
						;
				}
			} else {
				std::cerr << "Error: " << GetLastErrorStdStr() << std::endl;
			}
			CloseHandle(hDevice);
		}
	} else {
		std::cerr << GetLastErrorStdStr() << std::endl;
	}
}

/* Based on: http://support.microsoft.com/kb/188768 */
#define FILETIME2BIGINT(x) ((((ULONGLONG) (x).dwHighDateTime) << 32) + (x).dwLowDateTime)

void WinStats::cpuUsage(std::ostream &out) {
	FILETIME lpIdleTime;
	FILETIME lpIdleTime2;
	FILETIME lpKernelTime;
	FILETIME lpKernelTime2;
	FILETIME lpUserTime;
	FILETIME lpUserTime2;
	double user;
	double kern;
	double idle;
	if (GetSystemTimes(&lpIdleTime, &lpKernelTime, &lpUserTime)
	) {
		SleepEx(1000, FALSE);
		GetSystemTimes(&lpIdleTime2, &lpKernelTime2, &lpUserTime2);
		/**
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
		user = (FILETIME2BIGINT(lpUserTime2) - FILETIME2BIGINT(lpUserTime));
		kern = (FILETIME2BIGINT(lpKernelTime2) - FILETIME2BIGINT(lpKernelTime));
		idle = (FILETIME2BIGINT(lpIdleTime2) - FILETIME2BIGINT(lpIdleTime));
		out << "CPU usage: " << (((user+kern) - idle) *100 / (user+kern)) << std::endl;
		out << "CPU user : " << (user * 100) / (user+kern) << std::endl;
		out << "CPU kern  : " << ((kern-idle) * 100) / (user+kern) << std::endl;
		out << "CPU idle : " << (idle * 100) / (user+kern) << std::endl;
	} else {
		out << "CPU data not available!\n";
	}
}

SYSTEM_TIMES *WinStats::collectCPUUsage() {
	SYSTEM_TIMES *ret = new SYSTEM_TIMES();
	if (!::GetSystemTimes(&(ret->lpIdleTime), &(ret->lpKernelTime), &(ret->lpUserTime))) {
		delete ret;
		throw std::runtime_error(GetLastErrorStdStr());
	}
	return ret;
}

void WinStats::setSystemTimes(SYSTEM_TIMES *new_system_times) {
	if (this->last_system_times != NULL) {
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

DISK_PERFORMANCE_V2 *WinStats::collectDiskUsageFor(const std::string &device_name) {
	P_DeviceIoControl pDeviceIoControl = NULL;
	pDeviceIoControl = (P_DeviceIoControl)GetProcAddress (
			GetModuleHandle ("kernel32.dll"),
			"DeviceIoControl");
	if (pDeviceIoControl) {
		HANDLE hDevice = CreateFile(
				device_name.c_str(),
				0,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				0,
				NULL
				);
		if (hDevice == INVALID_HANDLE_VALUE) {
			throw std::runtime_error("Could not open device: " + GetLastErrorStdStr());
		} else {
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
			) {
				CloseHandle(hDevice);
				return ret;
			} else {
				delete ret;
				CloseHandle(hDevice);
				throw std::runtime_error("DeviceIoControl failed: " + GetLastErrorStdStr());
			}
		}
	} else {
		throw std::runtime_error("Could not load DeviceIoControl: " + GetLastErrorStdStr());
	}
}

void WinStats::setDiskPerfFor(size_t i, DISK_PERFORMANCE_V2 *new_disk_perf) {
	if (this->last_disk_perf.size() < i+1) {
		this->last_disk_perf.resize(i+1, NULL);
	}
	if (this->disk.size() < i+1) {
		this->disk.resize(i+1);
	}
	if (this->last_disk_perf[i] != NULL) {
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

void WinStats::readDrivesAvailable() {
	size_t bufSize = 1024;
	TCHAR szTemp[bufSize];
	szTemp[0] = '\0';
	if (!::GetLogicalDriveStrings(bufSize-1, szTemp)) {
		throw std::runtime_error(std::string("GetLogicalDriveStrings failed: ") + GetLastErrorStdStr());
	} else {
		TCHAR* p = szTemp;
		char szDrive[4] = ".:\\";
		do {
			szDrive[0] = *p;
			UINT dt = ::GetDriveType(szDrive);
			if (dt != DRIVE_REMOVABLE && dt != DRIVE_CDROM) {
				this->drives.push_back(*p);
			}
			// Go to the next NULL character.
			while (*p++);
		} while (*p); // end of string
	}
}

void WinStats::collectAndSetMemoryUsage() {
	HMODULE psapi_lib = LoadLibrary("psapi.dll");
	P_GetPerformanceInfo pGetPerformanceInfo = NULL;
	if (psapi_lib) {
		pGetPerformanceInfo = (P_GetPerformanceInfo)GetProcAddress(psapi_lib, "GetPerformanceInfo");
	}
	if (pGetPerformanceInfo) {
		if (pGetPerformanceInfo(&(this->memory), sizeof(this->memory))) {
		} else {
			std::cerr << GetLastErrorStdStr() << std::endl;
		}
	} else {
		std::cerr << GetLastErrorStdStr() << std::endl;
	}
}

void WinStats::showStatistics(std::ostream &out) {
	bool first = true;
	if (this->drives.empty()) {
		this->readDrivesAvailable();
	}
	while (true) {
		this->setSystemTimes(this->collectCPUUsage());
		for (size_t i = 0; i < this->drives.size(); i++) {
			this->setDiskPerfFor(i, this->collectDiskUsageFor(std::string("\\\\.\\") + this->drives[i] + ":"));
		}
		if (first) {
			first = false;
		} else {
			this->collectAndSetMemoryUsage();
			out << std::endl << "-----------------------------" << std::endl << std::endl;
			out << "   MEMORY USAGE" << std::endl;
			out
				             << "CommitTotal       (MB): " << (this->memory.CommitTotal * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "CommitLimit       (MB): " << (this->memory.CommitLimit * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "CommitPeak        (MB): " << (this->memory.CommitPeak * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "PhysicalTotal     (MB): " << (this->memory.PhysicalTotal * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "PhysicalAvailable (MB): " << (this->memory.PhysicalAvailable * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "SystemCache       (MB): " << (this->memory.SystemCache * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "KernelTotal       (MB): " << (this->memory.KernelTotal * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "KernelPaged       (MB): " << (this->memory.KernelPaged * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "KernelNonpaged    (MB): " << (this->memory.KernelNonpaged * this->memory.PageSize / 1024 / 1024)
				<< std::endl << "PageSize          : " << this->memory.PageSize
				<< std::endl << "HandleCount       : " << this->memory.HandleCount
				<< std::endl << "ProcessCount      : " << this->memory.ProcessCount
				<< std::endl << "ThreadCount       : " << this->memory.ThreadCount
				<< std::endl
				<< std::endl
				;
			out
				<< "   CPU USAGE" << std::endl
				<< "CPU usage (%): " << (this->cpu.user + this->cpu.kernel) << std::endl
				<< "CPU user  (%): " << this->cpu.user << std::endl
				<< "CPU sys   (%): " << this->cpu.kernel << std::endl
				<< "CPU idle  (%): " << this->cpu.idle << std::endl
				<< std::endl
				;
			for (size_t i = 0; i < this->drives.size(); i++) {
				out << "   DISK USAGE FOR " << this->drives[i] << ":\\" << std::endl;
				double time_elapsed_s = this->disk[i].QueryTime * 1e-7;
				out
					             << "Elapsed (s): " << time_elapsed_s
					<< std::endl << "Read Kb/s : " << (
							(double)(this->disk[i].BytesRead / 1024.0)
							/ time_elapsed_s
						)
					<< std::endl << "Write Kb/s : " << (
							(double)(this->disk[i].BytesWritten / 1024.0)
							/ time_elapsed_s
						)
					<< std::endl << "Read sec/s : " << (
							(double)(this->disk[i].BytesRead / 512.0)
							/ time_elapsed_s
						)
					<< std::endl << "Write sec/s : " << (
							(double)(this->disk[i].BytesWritten / 512.0)
							/ time_elapsed_s
						)
					<< std::endl << "TPS: " << (
							(double)(this->disk[i].ReadCount + this->disk[i].WriteCount) / time_elapsed_s
						)
					<< std::endl << "Util (%): " << (
							(double)(
								this->disk[i].WriteTime + this->disk[i].ReadTime
							) * 100.0 / (double)(
								this->disk[i].WriteTime + this->disk[i].ReadTime + this->disk[i].IdleTime
							)
						)
					<< std::endl
					<< std::endl;
			}
		}
		SleepEx(1000, FALSE);
	}
}

void WinStats::collectAll(long wait_ms) {
	/* Read available disk drives */
	if (this->drives.empty()) {
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

int main(int argc, char *argv[]) {
	WinStats stats;
	stats.showStatistics(std::cout);
	return 0;
}

