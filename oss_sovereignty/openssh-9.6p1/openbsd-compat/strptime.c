 
 

 

 

#include "includes.h"

#ifndef HAVE_STRPTIME

#define TM_YEAR_BASE 1900	 

#include <ctype.h>
#include <locale.h>
#include <string.h>
#include <time.h>

 

 
#define _ALT_E			0x01
#define _ALT_O			0x02
#define	_LEGAL_ALT(x)		{ if (alt_format & ~(x)) return (0); }


static	int _conv_num(const unsigned char **, int *, int, int);
static	char *_strptime(const char *, const char *, struct tm *, int);


char *
strptime(const char *buf, const char *fmt, struct tm *tm)
{
	return(_strptime(buf, fmt, tm, 1));
}

static char *
_strptime(const char *buf, const char *fmt, struct tm *tm, int initialize)
{
	unsigned char c;
	const unsigned char *bp;
	size_t len;
	int alt_format, i;
	static int century, relyear;

	if (initialize) {
		century = TM_YEAR_BASE;
		relyear = -1;
	}

	bp = (unsigned char *)buf;
	while ((c = *fmt) != '\0') {
		 
		alt_format = 0;

		 
		if (isspace(c)) {
			while (isspace(*bp))
				bp++;

			fmt++;
			continue;
		}
				
		if ((c = *fmt++) != '%')
			goto literal;


again:		switch (c = *fmt++) {
		case '%':	 
literal:
		if (c != *bp++)
			return (NULL);

		break;

		 
		case 'E':	 
			_LEGAL_ALT(0);
			alt_format |= _ALT_E;
			goto again;

		case 'O':	 
			_LEGAL_ALT(0);
			alt_format |= _ALT_O;
			goto again;
			
		 
#if 0
		case 'c':	 
			_LEGAL_ALT(_ALT_E);
			if (!(bp = _strptime(bp, _ctloc(d_t_fmt), tm, 0)))
				return (NULL);
			break;
#endif
		case 'D':	 
			_LEGAL_ALT(0);
			if (!(bp = _strptime(bp, "%m/%d/%y", tm, 0)))
				return (NULL);
			break;
	
		case 'R':	 
			_LEGAL_ALT(0);
			if (!(bp = _strptime(bp, "%H:%M", tm, 0)))
				return (NULL);
			break;

		case 'r':	 
			_LEGAL_ALT(0);
			if (!(bp = _strptime(bp, "%I:%M:%S %p", tm, 0)))
				return (NULL);
			break;

		case 'T':	 
			_LEGAL_ALT(0);
			if (!(bp = _strptime(bp, "%H:%M:%S", tm, 0)))
				return (NULL);
			break;
#if 0
		case 'X':	 
			_LEGAL_ALT(_ALT_E);
			if (!(bp = _strptime(bp, _ctloc(t_fmt), tm, 0)))
				return (NULL);
			break;

		case 'x':	 
			_LEGAL_ALT(_ALT_E);
			if (!(bp = _strptime(bp, _ctloc(d_fmt), tm, 0)))
				return (NULL);
			break;
#endif
		 
#if 0
		case 'A':	 
		case 'a':
			_LEGAL_ALT(0);
			for (i = 0; i < 7; i++) {
				 
				len = strlen(_ctloc(day[i]));
				if (strncasecmp(_ctloc(day[i]), bp, len) == 0)
					break;

				 
				len = strlen(_ctloc(abday[i]));
				if (strncasecmp(_ctloc(abday[i]), bp, len) == 0)
					break;
			}

			 
			if (i == 7)
				return (NULL);

			tm->tm_wday = i;
			bp += len;
			break;

		case 'B':	 
		case 'b':
		case 'h':
			_LEGAL_ALT(0);
			for (i = 0; i < 12; i++) {
				 
				len = strlen(_ctloc(mon[i]));
				if (strncasecmp(_ctloc(mon[i]), bp, len) == 0)
					break;

				 
				len = strlen(_ctloc(abmon[i]));
				if (strncasecmp(_ctloc(abmon[i]), bp, len) == 0)
					break;
			}

			 
			if (i == 12)
				return (NULL);

			tm->tm_mon = i;
			bp += len;
			break;
#endif

		case 'C':	 
			_LEGAL_ALT(_ALT_E);
			if (!(_conv_num(&bp, &i, 0, 99)))
				return (NULL);

			century = i * 100;
			break;

		case 'd':	 
		case 'e':
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_mday, 1, 31)))
				return (NULL);
			break;

		case 'k':	 
			_LEGAL_ALT(0);
			 
		case 'H':
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_hour, 0, 23)))
				return (NULL);
			break;

		case 'l':	 
			_LEGAL_ALT(0);
			 
		case 'I':
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_hour, 1, 12)))
				return (NULL);
			break;

		case 'j':	 
			_LEGAL_ALT(0);
			if (!(_conv_num(&bp, &tm->tm_yday, 1, 366)))
				return (NULL);
			tm->tm_yday--;
			break;

		case 'M':	 
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_min, 0, 59)))
				return (NULL);
			break;

		case 'm':	 
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_mon, 1, 12)))
				return (NULL);
			tm->tm_mon--;
			break;

#if 0
		case 'p':	 
			_LEGAL_ALT(0);
			 
			len = strlen(_ctloc(am_pm[0]));
			if (strncasecmp(_ctloc(am_pm[0]), bp, len) == 0) {
				if (tm->tm_hour > 12)	 
					return (NULL);
				else if (tm->tm_hour == 12)
					tm->tm_hour = 0;

				bp += len;
				break;
			}
			 
			len = strlen(_ctloc(am_pm[1]));
			if (strncasecmp(_ctloc(am_pm[1]), bp, len) == 0) {
				if (tm->tm_hour > 12)	 
					return (NULL);
				else if (tm->tm_hour < 12)
					tm->tm_hour += 12;

				bp += len;
				break;
			}

			 
			return (NULL);
#endif
		case 'S':	 
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_sec, 0, 61)))
				return (NULL);
			break;

		case 'U':	 
		case 'W':	 
			_LEGAL_ALT(_ALT_O);
			 
			 if (!(_conv_num(&bp, &i, 0, 53)))
				return (NULL);
			 break;

		case 'w':	 
			_LEGAL_ALT(_ALT_O);
			if (!(_conv_num(&bp, &tm->tm_wday, 0, 6)))
				return (NULL);
			break;

		case 'Y':	 
			_LEGAL_ALT(_ALT_E);
			if (!(_conv_num(&bp, &i, 0, 9999)))
				return (NULL);

			relyear = -1;
			tm->tm_year = i - TM_YEAR_BASE;
			break;

		case 'y':	 
			_LEGAL_ALT(_ALT_E | _ALT_O);
			if (!(_conv_num(&bp, &relyear, 0, 99)))
				return (NULL);
			break;

		 
		case 'n':	 
		case 't':
			_LEGAL_ALT(0);
			while (isspace(*bp))
				bp++;
			break;


		default:	 
			return (NULL);
		}


	}

	 
	if (relyear != -1) {
		if (century == TM_YEAR_BASE) {
			if (relyear <= 68)
				tm->tm_year = relyear + 2000 - TM_YEAR_BASE;
			else
				tm->tm_year = relyear + 1900 - TM_YEAR_BASE;
		} else {
			tm->tm_year = relyear + century - TM_YEAR_BASE;
		}
	}

	return ((char *)bp);
}


static int
_conv_num(const unsigned char **buf, int *dest, int llim, int ulim)
{
	int result = 0;
	int rulim = ulim;

	if (**buf < '0' || **buf > '9')
		return (0);

	 
	do {
		result *= 10;
		result += *(*buf)++ - '0';
		rulim /= 10;
	} while ((result * 10 <= ulim) && rulim && **buf >= '0' && **buf <= '9');

	if (result < llim || result > ulim)
		return (0);

	*dest = result;
	return (1);
}

#endif  

