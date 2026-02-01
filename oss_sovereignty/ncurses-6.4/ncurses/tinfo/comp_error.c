 

 

 

#include <curses.priv.h>

#include <tic.h>

MODULE_ID("$Id: comp_error.c,v 1.40 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT_VAR(bool) _nc_suppress_warnings = FALSE;
NCURSES_EXPORT_VAR(int) _nc_curr_line = 0;  
NCURSES_EXPORT_VAR(int) _nc_curr_col = 0;  

#define SourceName	_nc_globals.comp_sourcename
#define TermType	_nc_globals.comp_termtype

NCURSES_EXPORT(const char *)
_nc_get_source(void)
{
    return SourceName;
}

NCURSES_EXPORT(void)
_nc_set_source(const char *const name)
{
    FreeIfNeeded(SourceName);
    SourceName = strdup(name);
}

NCURSES_EXPORT(void)
_nc_set_type(const char *const name)
{
#define MY_SIZE (size_t) MAX_NAME_SIZE
    if (TermType == 0)
	TermType = typeMalloc(char, MY_SIZE + 1);
    if (TermType != 0) {
	TermType[0] = '\0';
	if (name) {
	    _nc_STRNCAT(TermType, name, MY_SIZE, MY_SIZE);
	}
    }
}

NCURSES_EXPORT(void)
_nc_get_type(char *name)
{
#if NO_LEAKS
    if (name == 0 && TermType != 0) {
	FreeAndNull(TermType);
	return;
    }
#endif
    if (name != 0)
	_nc_STRCPY(name, TermType != 0 ? TermType : "", MAX_NAME_SIZE);
}

static NCURSES_INLINE void
where_is_problem(void)
{
    fprintf(stderr, "\"%s\"", SourceName ? SourceName : "?");
    if (_nc_curr_line >= 0)
	fprintf(stderr, ", line %d", _nc_curr_line);
    if (_nc_curr_col >= 0)
	fprintf(stderr, ", col %d", _nc_curr_col);
    if (TermType != 0 && TermType[0] != '\0')
	fprintf(stderr, ", terminal '%s'", TermType);
    fputc(':', stderr);
    fputc(' ', stderr);
}

NCURSES_EXPORT(void)
_nc_warning(const char *const fmt, ...)
{
    va_list argp;

    if (_nc_suppress_warnings)
	return;

    where_is_problem();
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    fprintf(stderr, "\n");
    va_end(argp);
}

NCURSES_EXPORT(void)
_nc_err_abort(const char *const fmt, ...)
{
    va_list argp;

    where_is_problem();
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    fprintf(stderr, "\n");
    va_end(argp);
    exit(EXIT_FAILURE);
}

NCURSES_EXPORT(void)
_nc_syserr_abort(const char *const fmt, ...)
{
    va_list argp;

    where_is_problem();
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    fprintf(stderr, "\n");
    va_end(argp);

#if defined(TRACE) || !defined(NDEBUG)
     
#ifndef USE_ROOT_ENVIRON
    if (getuid() != ROOT_UID)
#endif
	abort();
#endif
     
    exit(EXIT_FAILURE);
}

#if NO_LEAKS
NCURSES_EXPORT(void)
_nc_comp_error_leaks(void)
{
    FreeAndNull(SourceName);
    FreeAndNull(TermType);
}
#endif
