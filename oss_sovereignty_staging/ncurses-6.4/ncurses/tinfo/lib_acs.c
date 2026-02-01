 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_acs.c,v 1.50 2020/02/02 23:34:34 tom Exp $")

#if BROKEN_LINKER || USE_REENTRANT
#define MyBuffer _nc_prescreen.real_acs_map
NCURSES_EXPORT(chtype *)
NCURSES_PUBLIC_VAR(acs_map) (void)
{
    if (MyBuffer == 0)
	MyBuffer = typeCalloc(chtype, ACS_LEN);
    return MyBuffer;
}
#undef MyBuffer
#else
NCURSES_EXPORT_VAR (chtype) acs_map[ACS_LEN] =
{
    0
};
#endif

#ifdef USE_TERM_DRIVER
NCURSES_EXPORT(chtype)
NCURSES_SP_NAME(_nc_acs_char) (NCURSES_SP_DCLx int c)
{
    chtype *map;
    if (c < 0 || c >= ACS_LEN)
	return (chtype) 0;
    map = (SP_PARM != 0) ? SP_PARM->_acs_map :
#if BROKEN_LINKER || USE_REENTRANT
	_nc_prescreen.real_acs_map
#else
	acs_map
#endif
	;
    return map[c];
}
#endif  

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_init_acs) (NCURSES_SP_DCL0)
{
    chtype *fake_map = acs_map;
    chtype *real_map = SP_PARM != 0 ? SP_PARM->_acs_map : fake_map;
    int j;

    T(("initializing ACS map"));

     
    if (real_map != fake_map) {
	for (j = 1; j < ACS_LEN; ++j) {
	    real_map[j] = 0;
	    fake_map[j] = A_ALTCHARSET | (chtype) j;
	    if (SP_PARM)
		SP_PARM->_screen_acs_map[j] = FALSE;
	}
    } else {
	for (j = 1; j < ACS_LEN; ++j) {
	    real_map[j] = 0;
	}
    }

     
    real_map['l'] = '+';	 
    real_map['m'] = '+';	 
    real_map['k'] = '+';	 
    real_map['j'] = '+';	 
    real_map['u'] = '+';	 
    real_map['t'] = '+';	 
    real_map['v'] = '+';	 
    real_map['w'] = '+';	 
    real_map['q'] = '-';	 
    real_map['x'] = '|';	 
    real_map['n'] = '+';	 
    real_map['o'] = '~';	 
    real_map['s'] = '_';	 
    real_map['`'] = '+';	 
    real_map['a'] = ':';	 
    real_map['f'] = '\'';	 
    real_map['g'] = '#';	 
    real_map['~'] = 'o';	 
    real_map[','] = '<';	 
    real_map['+'] = '>';	 
    real_map['.'] = 'v';	 
    real_map['-'] = '^';	 
    real_map['h'] = '#';	 
    real_map['i'] = '#';	 
    real_map['0'] = '#';	 
     
    real_map['p'] = '-';	 
    real_map['r'] = '-';	 
    real_map['y'] = '<';	 
    real_map['z'] = '>';	 
    real_map['{'] = '*';	 
    real_map['|'] = '!';	 
    real_map['}'] = 'f';	 
     
    real_map['L'] = '+';	 
    real_map['M'] = '+';	 
    real_map['K'] = '+';	 
    real_map['J'] = '+';	 
    real_map['T'] = '+';	 
    real_map['U'] = '+';	 
    real_map['V'] = '+';	 
    real_map['W'] = '+';	 
    real_map['Q'] = '-';	 
    real_map['X'] = '|';	 
    real_map['N'] = '+';	 
     
    real_map['C'] = '+';	 
    real_map['D'] = '+';	 
    real_map['B'] = '+';	 
    real_map['A'] = '+';	 
    real_map['G'] = '+';	 
    real_map['F'] = '+';	 
    real_map['H'] = '+';	 
    real_map['I'] = '+';	 
    real_map['R'] = '-';	 
    real_map['Y'] = '|';	 
    real_map['E'] = '+';	 

#ifdef USE_TERM_DRIVER
    CallDriver_2(SP_PARM, td_initacs, real_map, fake_map);
#else
    if (ena_acs != NULL) {
	NCURSES_PUTP2("ena_acs", ena_acs);
    }
#if NCURSES_EXT_FUNCS && defined(enter_pc_charset_mode) && defined(exit_pc_charset_mode)
     
#define PCH_KLUDGE(a,b) (a != 0 && b != 0 && !strcmp(a,b))
    if (PCH_KLUDGE(enter_pc_charset_mode, enter_alt_charset_mode) &&
	PCH_KLUDGE(exit_pc_charset_mode, exit_alt_charset_mode)) {
	size_t i;
	for (i = 1; i < ACS_LEN; ++i) {
	    if (real_map[i] == 0) {
		real_map[i] = (chtype) i;
		if (real_map != fake_map) {
		    if (SP != 0)
			SP->_screen_acs_map[i] = TRUE;
		}
	    }
	}
    }
#endif

    if (acs_chars != NULL) {
	size_t i = 0;
	size_t length = strlen(acs_chars);

	while (i + 1 < length) {
	    if (acs_chars[i] != 0 && UChar(acs_chars[i]) < ACS_LEN) {
		real_map[UChar(acs_chars[i])] = UChar(acs_chars[i + 1]) | A_ALTCHARSET;
		T(("#%d real_map[%s] = %s",
		   (int) i,
		   _tracechar(UChar(acs_chars[i])),
		   _tracechtype(real_map[UChar(acs_chars[i])])));
		if (SP != 0) {
		    SP->_screen_acs_map[UChar(acs_chars[i])] = TRUE;
		}
	    }
	    i += 2;
	}
    }
#ifdef TRACE
     
    if (USE_TRACEF(TRACE_CALLS)) {
	size_t n, m;
	char show[ACS_LEN * 2 + 1];
	for (n = 1, m = 0; n < ACS_LEN; n++) {
	    if (real_map[n] != 0) {
		show[m++] = (char) n;
		show[m++] = (char) ChCharOf(real_map[n]);
	    }
	}
	show[m] = 0;
	if (acs_chars == NULL || strcmp(acs_chars, show))
	    _tracef("%s acs_chars %s",
		    (acs_chars == NULL) ? "NULL" : "READ",
		    _nc_visbuf(acs_chars));
	_tracef("%s acs_chars %s",
		(acs_chars == NULL)
		? "NULL"
		: (strcmp(acs_chars, show)
		   ? "DIFF"
		   : "SAME"),
		_nc_visbuf(show));
	_nc_unlock_global(tracef);
    }
#endif  
#endif
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_init_acs(void)
{
    NCURSES_SP_NAME(_nc_init_acs) (CURRENT_SCREEN);
}
#endif

#if !NCURSES_WCWIDTH_GRAPHICS
NCURSES_EXPORT(int)
_nc_wacs_width(unsigned ch)
{
    int result;
    switch (ch) {
    case 0x00a3:		 
    case 0x00b0:		 
    case 0x00b1:		 
    case 0x00b7:		 
    case 0x03c0:		 
    case 0x2190:		 
    case 0x2191:		 
    case 0x2192:		 
    case 0x2193:		 
    case 0x2260:		 
    case 0x2264:		 
    case 0x2265:		 
    case 0x23ba:		 
    case 0x23bb:		 
    case 0x23bc:		 
    case 0x23bd:		 
    case 0x2500:		 
    case 0x2501:		 
    case 0x2502:		 
    case 0x2503:		 
    case 0x250c:		 
    case 0x250f:		 
    case 0x2510:		 
    case 0x2513:		 
    case 0x2514:		 
    case 0x2517:		 
    case 0x2518:		 
    case 0x251b:		 
    case 0x251c:		 
    case 0x2523:		 
    case 0x2524:		 
    case 0x252b:		 
    case 0x252c:		 
    case 0x2533:		 
    case 0x2534:		 
    case 0x253b:		 
    case 0x253c:		 
    case 0x254b:		 
    case 0x2550:		 
    case 0x2551:		 
    case 0x2554:		 
    case 0x2557:		 
    case 0x255a:		 
    case 0x255d:		 
    case 0x2560:		 
    case 0x2563:		 
    case 0x2566:		 
    case 0x2569:		 
    case 0x256c:		 
    case 0x2592:		 
    case 0x25ae:		 
    case 0x25c6:		 
    case 0x2603:		 
	result = 1;
	break;
    default:
	result = wcwidth(ch);
	break;
    }
    return result;
}
#endif  
