 

 

 

#include <curses.priv.h>

#include <tic.h>

MODULE_ID("$Id: free_ttype.c,v 1.21 2022/05/28 18:02:33 tom Exp $")

static void
really_free_termtype(TERMTYPE2 *ptr, bool freeStrings)
{
    T(("really_free_termtype(%s) %d", ptr->term_names, freeStrings));

    if (freeStrings) {
	FreeIfNeeded(ptr->str_table);
    }
    FreeIfNeeded(ptr->Booleans);
    FreeIfNeeded(ptr->Numbers);
    FreeIfNeeded(ptr->Strings);
#if NCURSES_XNAMES
    if (freeStrings) {
	FreeIfNeeded(ptr->ext_str_table);
    }
    FreeIfNeeded(ptr->ext_Names);
#endif
    memset(ptr, 0, sizeof(TERMTYPE));
    _nc_free_entry(_nc_head, ptr);
}

 
NCURSES_EXPORT(void)
_nc_free_termtype(TERMTYPE *ptr)
{
    really_free_termtype((TERMTYPE2 *) ptr, !NCURSES_EXT_NUMBERS);
}

 
NCURSES_EXPORT(void)
_nc_free_termtype1(TERMTYPE *ptr)
{
    really_free_termtype((TERMTYPE2 *) ptr, TRUE);
}

#if NCURSES_EXT_NUMBERS
NCURSES_EXPORT(void)
_nc_free_termtype2(TERMTYPE2 *ptr)
{
    really_free_termtype(ptr, TRUE);
}
#endif

#if NCURSES_XNAMES
NCURSES_EXPORT_VAR(bool) _nc_user_definable = TRUE;

NCURSES_EXPORT(int)
use_extended_names(bool flag)
{
    int oldflag = _nc_user_definable;

    START_TRACE();
    T((T_CALLED("use_extended_names(%d)"), flag));
    _nc_user_definable = flag;
    returnBool(oldflag);
}
#endif
