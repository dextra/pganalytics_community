#if defined(WIN32) || defined(__CYGWIN__)

#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0501)
#undef _WIN32_WINNT
#endif

#ifndef _WIN32_WINNT
/* At least CreateTimerQueueTimer/DeleteTimerQueueTimer needs such version 0x0500, and GetSystemTimes needs 0x0501 */
#define _WIN32_WINNT 0x0501
#endif

#if defined(WINVER) && (WINVER < 0x0501)
#undef WINVER
#endif

#ifndef WINVER
#define WINVER 0x0501
#endif
// Enable NOTIFYICONDATA fancy features
//#define NTDDI_VERSION NTDDI_WIN2K
//#define NTDDI_VERSION NTDDI_WINXP
#define NTDDI_VERSION NTDDI_VISTA
void _dosmaperr(unsigned long);
#include <windows.h>
#include <winbase.h>

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

extern void setup_unhandled_exception_catch();
extern void windows_print_stacktrace(CONTEXT* context);

#else // defined(WIN32) || defined(__CYGWIN__)

#define setup_unhandled_exception_catch(x) ((void)0)

#endif

