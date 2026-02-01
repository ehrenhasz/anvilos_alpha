 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_wacs.c,v 1.20 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT_VAR(cchar_t) * _nc_wacs = 0;

NCURSES_EXPORT(void)
_nc_init_wacs(void)
{
     
    static const struct {
	unsigned map;
	int	value[2];
    } table[] = {
	 
	{ 'l',	{ '+',	0x250c }},	 
	{ 'm',	{ '+',	0x2514 }},	 
	{ 'k',	{ '+',	0x2510 }},	 
	{ 'j',	{ '+',	0x2518 }},	 
	{ 't',	{ '+',	0x251c }},	 
	{ 'u',	{ '+',	0x2524 }},	 
	{ 'v',	{ '+',	0x2534 }},	 
	{ 'w',	{ '+',	0x252c }},	 
	{ 'q',	{ '-',	0x2500 }},	 
	{ 'x',	{ '|',	0x2502 }},	 
	{ 'n',	{ '+',	0x253c }},	 
	{ 'o',	{ '~',	0x23ba }},	 
	{ 's',	{ '_',	0x23bd }},	 
	{ '`',	{ '+',	0x25c6 }},	 
	{ 'a',	{ ':',	0x2592 }},	 
	{ 'f',	{ '\'',	0x00b0 }},	 
	{ 'g',	{ '#',	0x00b1 }},	 
	{ '~',	{ 'o',	0x00b7 }},	 
	 
	{ ',',	{ '<',	0x2190 }},	 
	{ '+',	{ '>',	0x2192 }},	 
	{ '.',	{ 'v',	0x2193 }},	 
	{ '-',	{ '^',	0x2191 }},	 
	{ 'h',	{ '#',	0x2592 }},	 
	{ 'i',	{ '#',	0x2603 }},	 
	{ '0',	{ '#',	0x25ae }},	 
	 
	{ 'p',	{ '-',	0x23bb }},	 
	{ 'r',	{ '-',	0x23bc }},	 
	{ 'y',	{ '<',	0x2264 }},	 
	{ 'z',	{ '>',	0x2265 }},	 
	{ '{',	{ '*',	0x03c0 }},	 
	{ '|',	{ '!',	0x2260 }},	 
	{ '}',	{ 'f',	0x00a3 }},	 
	 
	{ 'L',	{ '+',	0x250f }},	 
	{ 'M',	{ '+',	0x2517 }},	 
	{ 'K',	{ '+',	0x2513 }},	 
	{ 'J',	{ '+',	0x251b }},	 
	{ 'T',	{ '+',	0x2523 }},	 
	{ 'U',	{ '+',	0x252b }},	 
	{ 'V',	{ '+',	0x253b }},	 
	{ 'W',	{ '+',	0x2533 }},	 
	{ 'Q',	{ '-',	0x2501 }},	 
	{ 'X',	{ '|',	0x2503 }},	 
	{ 'N',	{ '+',	0x254b }},	 
	 
	{ 'C',	{ '+',	0x2554 }},	 
	{ 'D',	{ '+',	0x255a }},	 
	{ 'B',	{ '+',	0x2557 }},	 
	{ 'A',	{ '+',	0x255d }},	 
	{ 'G',	{ '+',	0x2563 }},	 
	{ 'F',	{ '+',	0x2560 }},	 
	{ 'H',	{ '+',	0x2569 }},	 
	{ 'I',	{ '+',	0x2566 }},	 
	{ 'R',	{ '-',	0x2550 }},	 
	{ 'Y',	{ '|',	0x2551 }},	 
	{ 'E',	{ '+',	0x256c }},	 
    };
     

    int active = _nc_unicode_locale();

     
    T(("initializing WIDE-ACS map (Unicode is%s active)",
       active ? "" : " not"));

    if ((_nc_wacs = typeCalloc(cchar_t, ACS_LEN)) != 0) {
	unsigned n;

	for (n = 0; n < SIZEOF(table); ++n) {
	    unsigned m;
#if NCURSES_WCWIDTH_GRAPHICS
	    int wide = wcwidth((wchar_t) table[n].value[active]);
#else
	    int wide = 1;
#endif

	    m = table[n].map;
	    if (active && (wide == 1)) {
		SetChar(_nc_wacs[m], table[n].value[1], A_NORMAL);
	    } else if (acs_map[m] & A_ALTCHARSET) {
		SetChar(_nc_wacs[m], m, A_ALTCHARSET);
	    } else {
		SetChar(_nc_wacs[m], table[n].value[0], A_NORMAL);
	    }

	    T(("#%d, width:%d SetChar(%c, %s) = %s",
	       n, wide, m,
	       _tracechar(table[n].value[active]),
	       _tracecchar_t(&_nc_wacs[m])));
	}
    }
}
