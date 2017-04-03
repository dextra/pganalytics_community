#include "win.h"
#include <shellapi.h>

#include "config.h"
#include "resource.h"
#include "StateManager.h"
#include "util/time.h"
#include "util/fs.h"
#include "util/streams.h"
#include <cstdio>

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

/*** TIMER ***/
/* Based on: http://www.codeproject.com/Articles/146617/Simple-C-Timer-Wrapper */
static void CALLBACK TimerProc(void*, BOOLEAN);

///////////////////////////////////////////////////////////////////////////////
//
// class QueueTimer
//
class QueueTimer
{
public:
	QueueTimer()
	{
		m_hTimer = NULL;
	}

	virtual ~QueueTimer()
	{
		this->stop();
	}

	inline bool isRunning()
	{
		return (this->m_hTimer != NULL);
	}

	inline bool start(unsigned int interval,   // interval in ms
			   bool immediately = false,// true to call first event immediately
			   bool once = false        // true to call timed event only once
			  )
	{
		if (m_hTimer)
		{
			return false;
		}
		this->m_once = once;
		BOOL success = ::CreateTimerQueueTimer(&m_hTimer,
											 NULL,
											 TimerProc,
											 this,
											 immediately ? 0 : interval,
											 once ? 0 : interval,
											 WT_EXECUTEINTIMERTHREAD);
		return (success != 0);
	}

	inline void stop()
	{
		if (this->isRunning())
		{
			::DeleteTimerQueueTimer( NULL, this->m_hTimer, NULL );
			this->m_hTimer = NULL ;
		}
	}

	virtual void onTimedEvent() = 0;

private:
	HANDLE m_hTimer;
	bool m_once;
};

///////////////////////////////////////////////////////////////////////////////
//
// TimerProc
//
void CALLBACK TimerProc(void* param, BOOLEAN timerCalled)
{
	QueueTimer* timer = static_cast<QueueTimer*>(param);
	timer->onTimedEvent();
};

/* TODO: The most ugly debug function ever... Do a proper logging */
void File_Debug(const char * format, ...)
{
	va_list args;
	time_t t = pga::Util::time::now();
	std::tm local = pga::Util::time::localtime(t);
	//std::string last_error = GetLastErrorStdStr();
	FILE *f = fopen("C:\\Temp\\pganalytics-agente.debug.log", "a");
	va_start(args, format);
	fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d [%ld]: ",
			local.tm_year+1900, local.tm_mon+1, local.tm_mday,
			local.tm_hour, local.tm_min, local.tm_sec,
			GetCurrentThreadId()
		   );
	vfprintf(f, format, args);
	fprintf(f, "\n");
	va_end(args);
	/*
	if (!last_error.empty())
	{
		fprintf(f, "\t%s\n", last_error.c_str());
	}
	*/
	fclose(f);
}

class Scheduler : protected QueueTimer
{
protected:
	void setupScheduler()
	{
		this->m_now = pga::Util::time::now();
		this->m_localtime = pga::Util::time::localtime(this->m_now);
		if (!this->m_first_time && this->m_localtime.tm_min % 5 == 0 && this->m_localtime.tm_sec >= 15 && this->m_localtime.tm_sec <= 45)
		{
			if (this->m_force_reschedule)
			{
				File_Debug("Scheduling for 5 minutes");
				QueueTimer::stop();
				QueueTimer::start(60*5*1000);
				/* It is scheduled at each 5 minutes now, than no need to reschedule */
				this->m_force_reschedule = false;
			}
			else
			{
				File_Debug("No need to reschedule");
			}
		}
		else
		{
			int min_complement = 5 - (this->m_localtime.tm_min % 5);
			int sec_complement = this->m_localtime.tm_sec;
			/* Wait to complete (just about, not exact) 00 seconds */
			File_Debug("Schedule will start at %d ms (%d:%d)", (60*min_complement - sec_complement) * 1000, min_complement, sec_complement);
			//min_complement = sec_complement = 0; /* DEBUG */
			QueueTimer::stop();
			QueueTimer::start((60*min_complement - sec_complement + 30 /* plus 30 seconds, just in case */) * 1000);
			/* It is not scheduled at each 5 minutes, than need to reschedule on next run: */
			this->m_force_reschedule = true;
			this->m_first_time = false;
		}
	}
	void onTimedEvent()
	{
		/* Check if the scheduler is still fine, also grab current localtime (at m_localtime) */
		this->setupScheduler();
		File_Debug("Verifying action...");
		/* At each "5-55/10" minutes, call sysstat collection */
		if (((this->m_localtime.tm_min + 5) % 10) == 0)
		{
			File_Debug("sysstat action...");
			ShellExecute(NULL, "open", "pganalytics.exe", "collect sysstat", this->m_exepath.c_str(), SW_HIDE);
		}
		/* At each "0" minute, call cron action, ignoring sysstat */
		if (this->m_localtime.tm_min == 0)
		{
			File_Debug("cron action...");
			ShellExecute(NULL, "open", "pganalytics.exe", "cron all ~sysstat", this->m_exepath.c_str(), SW_HIDE);
		}
	}
protected:
	time_t m_now;
	std::string m_exepath;
	struct tm m_localtime;
	bool m_force_reschedule;
	bool m_first_time;
public:
	Scheduler()
		: m_force_reschedule(true), m_first_time(true)
	{
		HMODULE hModule = GetModuleHandle(NULL);
		char path[MAX_PATH];
		GetModuleFileNameA(hModule, path, MAX_PATH);
		this->m_exepath = pga::Util::fs::dirName(pga::Util::fs::absPath(path));
	}
	virtual ~Scheduler() {}
	void start()
	{
		this->m_force_reschedule = true;
		this->m_first_time = true;
		this->setupScheduler();
	}
	void stop()
	{
		QueueTimer::stop();
	}
};

/*** Service ***/

/* Based on: http://www.codeproject.com/Articles/499465/Simple-Windows-Service-in-Cplusplus */

Scheduler scheduler;
SERVICE_STATUS        g_ServiceStatus; // = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME "pganalytics"

int main(int argc, TCHAR *argv[])
{
	char service_name[sizeof(SERVICE_NAME)+1];
	strcpy(service_name, SERVICE_NAME);
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{service_name, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
		{NULL, NULL}
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		File_Debug("Main: StartServiceCtrlDispatcher returned error");
		return GetLastError();
	}

	File_Debug("Main: Exit");
	return 0;
}


VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	HANDLE hThread;
	File_Debug("ServiceMain: Entry");
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
	if (g_StatusHandle == 0)
	{
		File_Debug("ServiceMain: RegisterServiceCtrlHandler returned error");
		goto EXIT;
	}
	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		File_Debug("ServiceMain: SetServiceStatus returned error");
	}
	/*
	 * Perform tasks necessary to start the service here
	 */
	File_Debug("ServiceMain: Performing Service Start Operations");
	// Create stop event to wait on later.
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		File_Debug("ServiceMain: CreateEvent(g_ServiceStopEvent) returned error");
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;
		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			File_Debug("ServiceMain: SetServiceStatus returned error");
		}
		goto EXIT;
	}
	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		File_Debug("ServiceMain: SetServiceStatus returned error");
	}
	// Start the thread that will perform the main task of the service
	hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
	File_Debug("ServiceMain: Waiting for Worker Thread to complete");
	// Wait until our worker thread exits effectively signaling that the service needs to stop
	WaitForSingleObject(hThread, INFINITE);
	File_Debug("ServiceMain: Worker Thread Stop Event signaled");
	/*
	 * Perform any cleanup tasks
	 */
	File_Debug("ServiceMain: Performing Cleanup Operations");
	CloseHandle(g_ServiceStopEvent);
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		File_Debug("ServiceMain: SetServiceStatus returned error");
	}

EXIT:
	File_Debug("ServiceMain: Exit");
	return;
}


VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	File_Debug("ServiceCtrlHandler: Entry");
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP :
		File_Debug("ServiceCtrlHandler: SERVICE_CONTROL_STOP Request");
		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;
		/*
		 * Perform tasks neccesary to stop the service here
		 */
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;
		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			File_Debug("ServiceCtrlHandler: SetServiceStatus returned error");
		}
		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);
		break;
	default:
		break;
	}
	File_Debug("ServiceCtrlHandler: Exit");
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	File_Debug("ServiceWorkerThread: Entry");
	scheduler.start();
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		// Wait for message
		Sleep(3000);
	}
	scheduler.stop();
	File_Debug("ServiceWorkerThread: Exit");
	return ERROR_SUCCESS;
}

