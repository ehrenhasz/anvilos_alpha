 
#if _GL_WINDOWS_STAT_TIMESPEC
extern struct timespec _gl_convert_FILETIME_to_timespec (const FILETIME *ft);
#else
extern time_t _gl_convert_FILETIME_to_POSIX (const FILETIME *ft);
#endif

 
extern int _gl_fstat_by_handle (HANDLE h, const char *path, struct stat *buf);

 
#define S_IREAD_UGO  (_S_IREAD | (_S_IREAD >> 3) | (_S_IREAD >> 6))
#define S_IWRITE_UGO (_S_IWRITE | (_S_IWRITE >> 3) | (_S_IWRITE >> 6))
#define S_IEXEC_UGO  (_S_IEXEC | (_S_IEXEC >> 3) | (_S_IEXEC >> 6))

#endif  
