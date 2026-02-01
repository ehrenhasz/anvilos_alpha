 

 

 
#ifndef OS2_AWARE

#undef LIBDIR
#define LIBDIR			_nlos2_libdir
extern char *_nlos2_libdir;

#undef LOCALEDIR
#define LOCALEDIR		_nlos2_localedir
extern char *_nlos2_localedir;

#undef LOCALE_ALIAS_PATH
#define LOCALE_ALIAS_PATH	_nlos2_localealiaspath
extern char *_nlos2_localealiaspath;

#endif

#undef HAVE_STRCASECMP
#define HAVE_STRCASECMP 1
#define strcasecmp stricmp
#define strncasecmp strnicmp

 
#define getenv _nl_getenv

 
#define LC_MESSAGES_COMPAT (-1)
