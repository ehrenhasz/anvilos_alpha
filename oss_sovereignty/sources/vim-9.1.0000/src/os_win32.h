



#include "os_dos.h"		

#ifndef PROTO
# include <direct.h>		
#endif

#define BINARY_FILE_IO
#define USE_EXE_NAME		
#define USE_TERM_CONSOLE
#ifndef HAVE_STRING_H
# define HAVE_STRING_H
#endif
#ifndef HAVE_MATH_H
# define HAVE_MATH_H
#endif
#define HAVE_STRCSPN
#ifndef __GNUC__
#define HAVE_STRICMP
#define HAVE_STRNICMP
#endif
#ifndef HAVE_STRFTIME
# define HAVE_STRFTIME		
#endif
#define HAVE_MEMSET
#ifndef HAVE_LOCALE_H
# define HAVE_LOCALE_H 1
#endif
#ifndef HAVE_FCNTL_H
# define HAVE_FCNTL_H
#endif
#define HAVE_QSORT
#define HAVE_ST_MODE		

#define FEAT_SHORTCUT		


#define HAVE_ACL

#define USE_FNAME_CASE		
#if !defined(FEAT_CLIPBOARD)
# define FEAT_CLIPBOARD		
#endif
#if defined(__DATE__) && defined(__TIME__)
# define HAVE_DATE_TIME
#endif
#ifndef FEAT_GUI_MSWIN		
# define BREAKCHECK_SKIP    1	
#endif

#define HAVE_TOTAL_MEM

#define HAVE_PUTENV		

#if defined(FEAT_GUI_MSWIN) && !defined(VIMDLL)
# define NO_CONSOLE		
#endif



#define BROKEN_TOUPPER

#define FNAME_ILLEGAL "\"*?><|" 

#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#ifndef STRICT
# define STRICT
#endif
#ifndef COBJMACROS
# define COBJMACROS	
#endif
#ifndef PROTO


# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>


#undef small

# ifndef SM_CXPADDEDBORDER
#  define SM_CXPADDEDBORDER     92
# endif
#endif

typedef void (*sighandler_T)(int, int);


#define CMDBUFFSIZE 1024	



#define MAXPATHL	1024

#ifndef BASENAMELEN
# define BASENAMELEN	(_MAX_PATH - 5)	
#endif

#define TEMPNAMELEN	_MAX_PATH	

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	(2*1024)    
#endif

#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	(5*1024)    
#endif


#ifndef FILE_ATTRIBUTE_REPARSE_POINT
# define FILE_ATTRIBUTE_REPARSE_POINT	0x00000400
#endif
#ifndef IO_REPARSE_TAG_MOUNT_POINT
# define IO_REPARSE_TAG_MOUNT_POINT	0xA0000003
#endif
#ifndef IO_REPARSE_TAG_SYMLINK
# define IO_REPARSE_TAG_SYMLINK		0xA000000C
#endif
#ifndef IO_REPARSE_TAG_APPEXECLINK
# define IO_REPARSE_TAG_APPEXECLINK	0x8000001B
#endif


typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
	struct {
	    USHORT SubstituteNameOffset;
	    USHORT SubstituteNameLength;
	    USHORT PrintNameOffset;
	    USHORT PrintNameLength;
	    ULONG  Flags;
	    WCHAR  PathBuffer[1];
	} SymbolicLinkReparseBuffer;
	struct {
	    USHORT SubstituteNameOffset;
	    USHORT SubstituteNameLength;
	    USHORT PrintNameOffset;
	    USHORT PrintNameLength;
	    WCHAR  PathBuffer[1];
	} MountPointReparseBuffer;
	struct {
	    UCHAR DataBuffer[1];
	} GenericReparseBuffer;
	struct
	{
	    ULONG StringCount;
	    WCHAR StringList[1];
	} AppExecLinkReparseBuffer;
    } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#ifdef _MSC_VER
    
    
# define HAVE_TRY_EXCEPT 1
# include <malloc.h>		
#endif



#ifdef _DEBUG

# include <crtdbg.h>
# define ASSERT(f) _ASSERT(f)

# define TRACE			Trace
# define TRACE0(sz)		Trace(_T("%s"), _T(sz))
# define TRACE1(sz, p1)		Trace(_T(sz), p1)
# define TRACE2(sz, p1, p2)	Trace(_T(sz), p1, p2)
# define TRACE3(sz, p1, p2, p3) Trace(_T(sz), p1, p2, p3)
# define TRACE4(sz, p1, p2, p3, p4) Trace(_T(sz), p1, p2, p3, p4)


void __cdecl
Trace(char *pszFormat, ...);

#else 

  
# define ASSERT(f)		((void)0)
# define TRACE			1 ? (void)0 : (void)printf
# define TRACE0(sz)
# define TRACE1(sz, p1)
# define TRACE2(sz, p1, p2)
# define TRACE3(sz, p1, p2, p3)
# define TRACE4(sz, p1, p2, p3, p4)

#endif 


#define ASSERT_POINTER(p, type) \
    ASSERT(((p) != NULL)  &&  IsValidAddress((p), sizeof(type), FALSE))

#define ASSERT_NULL_OR_POINTER(p, type) \
    ASSERT(((p) == NULL)  ||  IsValidAddress((p), sizeof(type), FALSE))

#ifndef HAVE_SETENV
# define HAVE_SETENV
#endif
#define mch_getenv(x) (char_u *)getenv((char *)(x))
#define vim_mkdir(x, y) mch_mkdir(x)
