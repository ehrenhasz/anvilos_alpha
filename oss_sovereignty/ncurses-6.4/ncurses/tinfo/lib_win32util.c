 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_win32util.c,v 1.2 2021/09/04 10:54:35 tom Exp $")

#ifdef _NC_WINDOWS
#include <io.h>

#ifdef _NC_CHECK_MINTTY
#define PSAPI_VERSION 2
#include <psapi.h>
#include <tchar.h>

#define array_length(a) (sizeof(a)/sizeof(a[0]))

 
NCURSES_EXPORT(int)
_nc_console_checkmintty(int fd, LPHANDLE pMinTTY)
{
    HANDLE handle = _nc_console_handle(fd);
    DWORD dw;
    int code = 0;

    T((T_CALLED("lib_winhelper::_nc_console_checkmintty(%d, %p)"), fd, pMinTTY));

    if (handle != INVALID_HANDLE_VALUE) {
        dw = GetFileType(handle);
	if (dw == FILE_TYPE_PIPE) {
	    if (GetNamedPipeInfo(handle, 0, 0, 0, 0)) {
	        ULONG pPid;
		 
		if (GetNamedPipeServerProcessId(handle, &pPid)) {
		    TCHAR buf[MAX_PATH];
		    DWORD len = 0;
		     
		    HANDLE pHandle = OpenProcess(
						 PROCESS_CREATE_THREAD
						 | PROCESS_QUERY_INFORMATION
						 | PROCESS_VM_OPERATION
						 | PROCESS_VM_WRITE
						 | PROCESS_VM_READ,
						 FALSE,
						 pPid);
		    if (pMinTTY)
		        *pMinTTY = INVALID_HANDLE_VALUE;
		    if (pHandle != INVALID_HANDLE_VALUE) {
		        if ((len = GetProcessImageFileName(
							   pHandle,
							   buf,
							   (DWORD)
							   array_length(buf)))) {
			    TCHAR *pos = _tcsrchr(buf, _T('\\'));
			    if (pos) {
			        pos++;
				if (_tcsnicmp(pos, _TEXT("mintty.exe"), 10)
				    == 0) {
				    if (pMinTTY)
				        *pMinTTY = pHandle;
				    code = 1;
				}
			    }
			}
		    }
		}
	    }
	}
    }
    returnCode(code);
}
#endif  

#define JAN1970 116444736000000000LL	 

NCURSES_EXPORT(int)
_nc_gettimeofday(struct timeval *tv, void *tz GCC_UNUSED)
{
    union {
	FILETIME ft;
	long long since1601;	 
    } data;

    GetSystemTimeAsFileTime(&data.ft);
    tv->tv_usec = (long) ((data.since1601 / 10LL) % 1000000LL);
    tv->tv_sec = (long) ((data.since1601 - JAN1970) / 10000000LL);
    return (0);
}

#endif 
