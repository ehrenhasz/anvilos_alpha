 

 

 

#include <curses.priv.h>

#if !HAVE_VSSCANF

MODULE_ID("$Id: vsscanf.c,v 1.21 2020/02/02 23:34:34 tom Exp $")

#if !(HAVE_VFSCANF || HAVE__DOSCAN)

#include <ctype.h>

#define L_SQUARE '['
#define R_SQUARE ']'

typedef enum {
    cUnknown
    ,cError			 
    ,cAssigned
    ,cChar
    ,cInt
    ,cFloat
    ,cDouble
    ,cPointer
    ,cLong
    ,cShort
    ,cRange
    ,cString
} ChunkType;

typedef enum {
    oUnknown
    ,oShort
    ,oLong
} OtherType;

typedef enum {
    sUnknown
    ,sPercent			 
    ,sNormal			 
    ,sLeft			 
    ,sRange			 
    ,sFinal			 
} ScanState;

static ChunkType
final_ch(int ch, OtherType other)
{
    ChunkType result = cUnknown;

    switch (ch) {
    case 'c':
	if (other == oUnknown)
	    result = cChar;
	else
	    result = cError;
	break;
    case 'd':
    case 'i':
    case 'X':
    case 'x':
	switch (other) {
	case oUnknown:
	    result = cInt;
	    break;
	case oShort:
	    result = cShort;
	    break;
	case oLong:
	    result = cLong;
	    break;
	}
	break;
    case 'E':
    case 'e':
    case 'f':
    case 'g':
	switch (other) {
	case oUnknown:
	    result = cFloat;
	    break;
	case oShort:
	    result = cError;
	    break;
	case oLong:
	    result = cDouble;
	    break;
	}
	break;
    case 'n':
	if (other == oUnknown)
	    result = cAssigned;
	else
	    result = cError;
	break;
    case 'p':
	if (other == oUnknown)
	    result = cPointer;
	else
	    result = cError;
	break;
    case 's':
	if (other == oUnknown)
	    result = cString;
	else
	    result = cError;
	break;
    }
    return result;
}

static OtherType
other_ch(int ch)
{
    OtherType result = oUnknown;
    switch (ch) {
    case 'h':
	result = oShort;
	break;
    case 'l':
	result = oLong;
	break;
    }
    return result;
}
#endif

 
NCURSES_EXPORT(int)
vsscanf(const char *str, const char *format, va_list ap)
{
#if HAVE_VFSCANF || HAVE__DOSCAN
     
    FILE strbuf;

    strbuf._flag = _IOREAD;
    strbuf._ptr = strbuf._base = (unsigned char *) str;
    strbuf._cnt = strlen(str);
    strbuf._file = _NFILE;

#if HAVE_VFSCANF
    return (vfscanf(&strbuf, format, ap));
#else
    return (_doscan(&strbuf, format, ap));
#endif
#else
    static int can_convert = -1;

    int assigned = 0;
    int consumed = 0;

    T((T_CALLED("vsscanf(%s,%s,...)"),
       _nc_visbuf2(1, str),
       _nc_visbuf2(2, format)));

     
    if (can_convert < 0) {
	int check1;
	int check2;
	if (sscanf("123", "%d%n", &check1, &check2) > 0
	    && check1 == 123
	    && check2 == 3) {
	    can_convert = 1;
	} else {
	    can_convert = 0;
	}
    }

    if (can_convert) {
	size_t len_fmt = strlen(format) + 32;
	char *my_fmt = malloc(len_fmt);
	ChunkType chunk, ctest;
	OtherType other, otest;
	ScanState state;
	unsigned n;
	int eaten;
	void *pointer;

	if (my_fmt != 0) {
	     
	    while (*format != '\0') {
		 
		state = sUnknown;
		chunk = cUnknown;
		other = oUnknown;
		pointer = 0;
		for (n = 0; format[n] != 0 && state != sFinal; ++n) {
		    my_fmt[n] = format[n];
		    switch (state) {
		    case sUnknown:
			if (format[n] == '%')
			    state = sPercent;
			break;
		    case sPercent:
			if (format[n] == '%') {
			    state = sUnknown;
			} else if (format[n] == L_SQUARE) {
			    state = sLeft;
			} else {
			    state = sNormal;
			    --n;
			}
			break;
		    case sLeft:
			state = sRange;
			if (format[n] == '^') {
			    ++n;
			    my_fmt[n] = format[n];
			}
			break;
		    case sRange:
			if (format[n] == R_SQUARE) {
			    state = sFinal;
			    chunk = cRange;
			}
			break;
		    case sNormal:
			if (format[n] == '*') {
			    state = sUnknown;
			} else {
			    if ((ctest = final_ch(format[n], other)) != cUnknown) {
				state = sFinal;
				chunk = ctest;
			    } else if ((otest = other_ch(format[n])) != oUnknown) {
				other = otest;
			    } else if (isalpha(UChar(format[n]))) {
				state = sFinal;
				chunk = cError;
			    }
			}
			break;
		    case sFinal:
			break;
		    }
		}
		my_fmt[n] = '\0';
		format += n;

		if (chunk == cUnknown
		    || chunk == cError) {
		    if (assigned == 0)
			assigned = EOF;
		    break;
		}

		 
		if (chunk != cAssigned) {
		    _nc_STRCAT(my_fmt, "%n", len_fmt);
		}

		switch (chunk) {
		case cAssigned:
		    _nc_STRCAT(my_fmt, "%n", len_fmt);
		    pointer = &eaten;
		    break;
		case cInt:
		    pointer = va_arg(ap, int *);
		    break;
		case cShort:
		    pointer = va_arg(ap, short *);
		    break;
		case cFloat:
		    pointer = va_arg(ap, float *);
		    break;
		case cDouble:
		    pointer = va_arg(ap, double *);
		    break;
		case cLong:
		    pointer = va_arg(ap, long *);
		    break;
		case cPointer:
		    pointer = va_arg(ap, void *);
		    break;
		case cChar:
		case cRange:
		case cString:
		    pointer = va_arg(ap, char *);
		    break;
		case cError:
		case cUnknown:
		    break;
		}
		 
		T(("...converting chunk #%d type %d(%s,%s)",
		   assigned + 1, chunk,
		   _nc_visbuf2(1, str + consumed),
		   _nc_visbuf2(2, my_fmt)));
		if (sscanf(str + consumed, my_fmt, pointer, &eaten) > 0)
		    consumed += eaten;
		else
		    break;
		++assigned;
	    }
	    free(my_fmt);
	}
    }
    returnCode(assigned);
#endif
}
#else
extern
NCURSES_EXPORT(void)
_nc_vsscanf(void);		 
NCURSES_EXPORT(void)
_nc_vsscanf(void)
{
}				 
#endif  
