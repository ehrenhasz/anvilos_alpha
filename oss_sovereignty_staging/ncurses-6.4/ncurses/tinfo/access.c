 

 

#include <curses.priv.h>

#include <ctype.h>

#ifndef USE_ROOT_ACCESS
#if HAVE_SETFSUID
#include <sys/fsuid.h>
#else
#include <sys/stat.h>
#endif
#endif

#include <tic.h>

MODULE_ID("$Id: access.c,v 1.31 2021/08/29 10:35:17 tom Exp $")

#define LOWERCASE(c) ((isalpha(UChar(c)) && isupper(UChar(c))) ? tolower(UChar(c)) : (c))

#ifdef _NC_MSC
# define ACCESS(FN, MODE) access((FN), (MODE)&(R_OK|W_OK))
#else
# define ACCESS access
#endif

NCURSES_EXPORT(char *)
_nc_rootname(char *path)
{
    char *result = _nc_basename(path);
#if !MIXEDCASE_FILENAMES || defined(PROG_EXT)
    static char *temp;
    char *s;

    temp = strdup(result);
    result = temp;
#if !MIXEDCASE_FILENAMES
    for (s = result; *s != '\0'; ++s) {
	*s = (char) LOWERCASE(*s);
    }
#endif
#if defined(PROG_EXT)
    if ((s = strrchr(result, '.')) != 0) {
	if (!strcmp(s, PROG_EXT))
	    *s = '\0';
    }
#endif
#endif
    return result;
}

 
NCURSES_EXPORT(bool)
_nc_is_abs_path(const char *path)
{
#if defined(__EMX__) || defined(__DJGPP__)
#define is_pathname(s) ((((s) != 0) && ((s)[0] == '/')) \
		  || (((s)[0] != 0) && ((s)[1] == ':')))
#else
#define is_pathname(s) ((s) != 0 && (s)[0] == '/')
#endif
    return is_pathname(path);
}

 
NCURSES_EXPORT(unsigned)
_nc_pathlast(const char *path)
{
    const char *test = strrchr(path, '/');
#ifdef __EMX__
    if (test == 0)
	test = strrchr(path, '\\');
#endif
    if (test == 0)
	test = path;
    else
	test++;
    return (unsigned) (test - path);
}

NCURSES_EXPORT(char *)
_nc_basename(char *path)
{
    return path + _nc_pathlast(path);
}

NCURSES_EXPORT(int)
_nc_access(const char *path, int mode)
{
    int result;

    if (path == 0) {
	result = -1;
    } else if (ACCESS(path, mode) < 0) {
	if ((mode & W_OK) != 0
	    && errno == ENOENT
	    && strlen(path) < PATH_MAX) {
	    char head[PATH_MAX];
	    char *leaf;

	    _nc_STRCPY(head, path, sizeof(head));
	    leaf = _nc_basename(head);
	    if (leaf == 0)
		leaf = head;
	    *leaf = '\0';
	    if (head == leaf)
		_nc_STRCPY(head, ".", sizeof(head));

	    result = ACCESS(head, R_OK | W_OK | X_OK);
	} else {
	    result = -1;
	}
    } else {
	result = 0;
    }
    return result;
}

NCURSES_EXPORT(bool)
_nc_is_dir_path(const char *path)
{
    bool result = FALSE;
    struct stat sb;

    if (stat(path, &sb) == 0
	&& S_ISDIR(sb.st_mode)) {
	result = TRUE;
    }
    return result;
}

NCURSES_EXPORT(bool)
_nc_is_file_path(const char *path)
{
    bool result = FALSE;
    struct stat sb;

    if (stat(path, &sb) == 0
	&& S_ISREG(sb.st_mode)) {
	result = TRUE;
    }
    return result;
}

#if HAVE_ISSETUGID
#define is_elevated() issetugid()
#elif HAVE_GETEUID && HAVE_GETEGID
#define is_elevated() \
	(getuid() != geteuid() \
	 || getgid() != getegid())
#else
#define is_elevated() FALSE
#endif

#if HAVE_SETFSUID
#define lower_privileges() \
	    int save_err = errno; \
	    setfsuid(getuid()); \
	    setfsgid(getgid()); \
	    errno = save_err
#define resume_elevation() \
	    save_err = errno; \
	    setfsuid(geteuid()); \
	    setfsgid(getegid()); \
	    errno = save_err
#else
#define lower_privileges()	 
#define resume_elevation()	 
#endif

#ifndef USE_ROOT_ENVIRON
 
NCURSES_EXPORT(int)
_nc_env_access(void)
{
    int result = TRUE;

    if (is_elevated()) {
	result = FALSE;
    } else if ((getuid() == ROOT_UID) || (geteuid() == ROOT_UID)) {
	result = FALSE;
    }
    return result;
}
#endif  

#ifndef USE_ROOT_ACCESS
 
NCURSES_EXPORT(FILE *)
_nc_safe_fopen(const char *path, const char *mode)
{
    FILE *result = NULL;
#if HAVE_SETFSUID
    lower_privileges();
    result = fopen(path, mode);
    resume_elevation();
#else
    if (!is_elevated() || *mode == 'r') {
	result = fopen(path, mode);
    }
#endif
    return result;
}

NCURSES_EXPORT(int)
_nc_safe_open3(const char *path, int flags, mode_t mode)
{
    int result = -1;
#if HAVE_SETFSUID
    lower_privileges();
    result = open(path, flags, mode);
    resume_elevation();
#else
    if (!is_elevated() || (flags & O_RDONLY)) {
	result = open(path, flags, mode);
    }
#endif
    return result;
}
#endif  
