 
 
 
#include <config.h>

#include <sys/types.h>

#include <stdio.h>
#include <ctype.h>
#include <posixtime.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>

 
#define SUNOS_EXT	1	 
#define VMS_EXT		1	 
#define HPUX_EXT	1	 
#define POSIX_SEMANTICS	1	 
#define POSIX_2008	1	 

#undef strchr	 

#if !defined (errno)
extern int errno;
#endif

#if defined (SHELL)
extern char *get_string_value (const char *);
#endif

extern void tzset(void);
static int weeknumber(const struct tm *timeptr, int firstweekday);
static int iso8601wknum(const struct tm *timeptr);

#ifndef inline
#ifdef __GNUC__
#define inline	__inline__
#else
#define inline	 
#endif
#endif

#define range(low, item, hi)	max(low, min(item, hi))

 
#if !defined(OS2) && !defined(MSDOS) && !defined(__CYGWIN__) && defined(HAVE_TZNAME)
extern char *tzname[2];
extern int daylight;
#if defined(SOLARIS) || defined(mips) || defined (M_UNIX)
extern long int timezone, altzone;
#else
#  if defined (HPUX) || defined(__hpux)
extern long int timezone;
#  else
#    if !defined(__CYGWIN__)
extern int timezone, altzone;
#    endif
#  endif
#endif
#endif

#undef min	 

 

static inline int
min(int a, int b)
{
	return (a < b ? a : b);
}

#undef max	 

 

static inline int
max(int a, int b)
{
	return (a > b ? a : b);
}

#ifdef POSIX_2008
 

static void
iso_8601_2000_year(char *buf, int year, size_t fw)
{
	int extra;
	char sign = '\0';

	if (year >= -9999 && year <= 9999) {
		sprintf(buf, "%0*d", (int) fw, year);
		return;
	}

	 
	if (year > 9999) {
		sign = '+';
	} else {
		sign = '-';
		year = -year;
	}

	extra = year / 10000;
	year %= 10000;
	sprintf(buf, "%c_%04d_%d", sign, extra, year);
}
#endif  

 

size_t
strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr)
{
	char *endp = s + maxsize;
	char *start = s;
	auto char tbuf[100];
	long off;
	int i, w, oerrno;
	long y;
	static short first = 1;
#ifdef POSIX_SEMANTICS
	static char *savetz = NULL;
	static int savetzlen = 0;
	char *tz;
#endif  
#ifndef HAVE_TM_ZONE
#ifndef HAVE_TM_NAME
#ifndef HAVE_TZNAME
#ifndef __CYGWIN__
	extern char *timezone();
	struct timeval tv;
	struct timezone zone;
#endif  
#endif  
#endif  
#endif  
#ifdef POSIX_2008
	int pad;
	size_t fw;
	char flag;
#endif  

	 
	static const char *days_a[] = {
		"Sun", "Mon", "Tue", "Wed",
		"Thu", "Fri", "Sat",
	};
	static const char *days_l[] = {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday",
	};
	static const char *months_a[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	};
	static const char *months_l[] = {
		"January", "February", "March", "April",
		"May", "June", "July", "August", "September",
		"October", "November", "December",
	};
	static const char *ampm[] = { "AM", "PM", };

	oerrno = errno;

	if (s == NULL || format == NULL || timeptr == NULL || maxsize == 0)
		return 0;

	 
	if (strchr(format, '%') == NULL && strlen(format) + 1 >= maxsize)
		return 0;

#ifndef POSIX_SEMANTICS
	if (first) {
		tzset();
		first = 0;
	}
#else	 
#if defined (SHELL)
	tz = get_string_value ("TZ");
#else
	tz = getenv("TZ");
#endif
	if (first) {
		if (tz != NULL) {
			int tzlen = strlen(tz);

			savetz = (char *) malloc(tzlen + 1);
			if (savetz != NULL) {
				savetzlen = tzlen + 1;
				strcpy(savetz, tz);
			}
		}
		tzset();
		first = 0;
	}
	 
	if (tz && savetz && (tz[0] != savetz[0] || strcmp(tz, savetz) != 0)) {
		i = strlen(tz) + 1;
		if (i > savetzlen) {
			savetz = (char *) realloc(savetz, i);
			if (savetz) {
				savetzlen = i;
				strcpy(savetz, tz);
			}
		} else
			strcpy(savetz, tz);
		tzset();
	}
#endif	 

	for (; *format && s < endp - 1; format++) {
		tbuf[0] = '\0';
		if (*format != '%') {
			*s++ = *format;
			continue;
		}
#ifdef POSIX_2008
		pad = '\0';
		fw = 0;
		flag = '\0';
		switch (*++format) {
		case '+':
			flag = '+';
			 
		case '0':
			pad = '0';
			format++;
			break;

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			break;

		default:
			format--;
			goto again;
		}
		for (; isdigit(*format); format++) {
			fw = fw * 10 + (*format - '0');
		}
		format--;
#endif  

	again:
		switch (*++format) {
		case '\0':
			*s++ = '%';
			goto out;

		case '%':
			*s++ = '%';
			continue;

		case 'a':	 
			if (timeptr->tm_wday < 0 || timeptr->tm_wday > 6)
				strcpy(tbuf, "?");
			else
				strcpy(tbuf, days_a[timeptr->tm_wday]);
			break;

		case 'A':	 
			if (timeptr->tm_wday < 0 || timeptr->tm_wday > 6)
				strcpy(tbuf, "?");
			else
				strcpy(tbuf, days_l[timeptr->tm_wday]);
			break;

		case 'b':	 
		short_month:
			if (timeptr->tm_mon < 0 || timeptr->tm_mon > 11)
				strcpy(tbuf, "?");
			else
				strcpy(tbuf, months_a[timeptr->tm_mon]);
			break;

		case 'B':	 
			if (timeptr->tm_mon < 0 || timeptr->tm_mon > 11)
				strcpy(tbuf, "?");
			else
				strcpy(tbuf, months_l[timeptr->tm_mon]);
			break;

		case 'c':	 
			 
			strftime(tbuf, sizeof tbuf, "%A %B %d %T %Y", timeptr);
			break;

		case 'C':
#ifdef POSIX_2008
			if (pad != '\0' && fw > 0) {
				size_t min_fw = (flag ? 3 : 2);

				fw = max(fw, min_fw);
				sprintf(tbuf, flag
						? "%+0*ld"
						: "%0*ld", (int) fw,
						(timeptr->tm_year + 1900L) / 100);
			} else
#endif  
		century:
				sprintf(tbuf, "%02ld", (timeptr->tm_year + 1900L) / 100);
			break;

		case 'd':	 
			i = range(1, timeptr->tm_mday, 31);
			sprintf(tbuf, "%02d", i);
			break;

		case 'D':	 
			strftime(tbuf, sizeof tbuf, "%m/%d/%y", timeptr);
			break;

		case 'e':	 
			sprintf(tbuf, "%2d", range(1, timeptr->tm_mday, 31));
			break;

		case 'E':
			 
			goto again;

		case 'F':	 
		{
#ifdef POSIX_2008
			 
			char m_d[10];
			strftime(m_d, sizeof m_d, "-%m-%d", timeptr);
			size_t min_fw = 10;

			if (pad != '\0' && fw > 0) {
				fw = max(fw, min_fw);
			} else {
				fw = min_fw;
			}

			fw -= 6;	 

			iso_8601_2000_year(tbuf, timeptr->tm_year + 1900, fw);
			strcat(tbuf, m_d);
#else
			strftime(tbuf, sizeof tbuf, "%Y-%m-%d", timeptr);
#endif  
		}
			break;

		case 'g':
		case 'G':
			 
			w = iso8601wknum(timeptr);
			if (timeptr->tm_mon == 11 && w == 1)
				y = 1900L + timeptr->tm_year + 1;
			else if (timeptr->tm_mon == 0 && w >= 52)
				y = 1900L + timeptr->tm_year - 1;
			else
				y = 1900L + timeptr->tm_year;

			if (*format == 'G') {
#ifdef POSIX_2008
				if (pad != '\0' && fw > 0) {
					size_t min_fw = 4;

					fw = max(fw, min_fw);
					sprintf(tbuf, flag
							? "%+0*ld"
							: "%0*ld", (int) fw,
							y);
				} else
#endif  
					sprintf(tbuf, "%ld", y);
			}
			else
				sprintf(tbuf, "%02ld", y % 100);
			break;

		case 'h':	 
			goto short_month;

		case 'H':	 
			i = range(0, timeptr->tm_hour, 23);
			sprintf(tbuf, "%02d", i);
			break;

		case 'I':	 
			i = range(0, timeptr->tm_hour, 23);
			if (i == 0)
				i = 12;
			else if (i > 12)
				i -= 12;
			sprintf(tbuf, "%02d", i);
			break;

		case 'j':	 
			sprintf(tbuf, "%03d", timeptr->tm_yday + 1);
			break;

		case 'm':	 
			i = range(0, timeptr->tm_mon, 11);
			sprintf(tbuf, "%02d", i + 1);
			break;

		case 'M':	 
			i = range(0, timeptr->tm_min, 59);
			sprintf(tbuf, "%02d", i);
			break;

		case 'n':	 
			tbuf[0] = '\n';
			tbuf[1] = '\0';
			break;

		case 'O':
			 
			goto again;

		case 'p':	 
			i = range(0, timeptr->tm_hour, 23);
			if (i < 12)
				strcpy(tbuf, ampm[0]);
			else
				strcpy(tbuf, ampm[1]);
			break;

		case 'r':	 
			strftime(tbuf, sizeof tbuf, "%I:%M:%S %p", timeptr);
			break;

		case 'R':	 
			strftime(tbuf, sizeof tbuf, "%H:%M", timeptr);
			break;

#if defined(HAVE_MKTIME)
		case 's':	 
		{
			struct tm non_const_timeptr;

			non_const_timeptr = *timeptr;
			sprintf(tbuf, "%ld", mktime(& non_const_timeptr));
			break;
		}
#endif  

		case 'S':	 
			i = range(0, timeptr->tm_sec, 60);
			sprintf(tbuf, "%02d", i);
			break;

		case 't':	 
			tbuf[0] = '\t';
			tbuf[1] = '\0';
			break;

		case 'T':	 
		the_time:
			strftime(tbuf, sizeof tbuf, "%H:%M:%S", timeptr);
			break;

		case 'u':
		 
			sprintf(tbuf, "%d", timeptr->tm_wday == 0 ? 7 :
					timeptr->tm_wday);
			break;

		case 'U':	 
			sprintf(tbuf, "%02d", weeknumber(timeptr, 0));
			break;

		case 'V':	 
			sprintf(tbuf, "%02d", iso8601wknum(timeptr));
			break;

		case 'w':	 
			i = range(0, timeptr->tm_wday, 6);
			sprintf(tbuf, "%d", i);
			break;

		case 'W':	 
			sprintf(tbuf, "%02d", weeknumber(timeptr, 1));
			break;

		case 'x':	 
			strftime(tbuf, sizeof tbuf, "%A %B %d %Y", timeptr);
			break;

		case 'X':	 
			goto the_time;
			break;

		case 'y':	 
		year:
			i = timeptr->tm_year % 100;
			sprintf(tbuf, "%02d", i);
			break;

		case 'Y':	 
#ifdef POSIX_2008
			if (pad != '\0' && fw > 0) {
				size_t min_fw = 4;

				fw = max(fw, min_fw);
				sprintf(tbuf, flag
						? "%+0*ld"
						: "%0*ld", (int) fw,
						1900L + timeptr->tm_year);
			} else
#endif  
			sprintf(tbuf, "%ld", 1900L + timeptr->tm_year);
			break;

		 
 		case 'z':	 
 			if (timeptr->tm_isdst < 0)
 				break;
#ifdef HAVE_TM_NAME
			 
			off = -timeptr->tm_tzadj / 60;
#else  
#ifdef HAVE_TM_ZONE
			 
			off = timeptr->tm_gmtoff / 60;
#else  
#if HAVE_TZNAME
			 
#  if defined(__hpux) || defined (HPUX) || defined(__CYGWIN__)
			off = -timezone / 60;
#  else
			 
			off = -(daylight ? altzone : timezone) / 60;
#  endif
#else  
			gettimeofday(& tv, & zone);
			off = -zone.tz_minuteswest;
#endif  
#endif  
#endif  
			if (off < 0) {
				tbuf[0] = '-';
				off = -off;
			} else {
				tbuf[0] = '+';
			}
			sprintf(tbuf+1, "%02ld%02ld", off/60, off%60);
			break;

		case 'Z':	 
#ifdef HAVE_TZNAME
			i = (daylight && timeptr->tm_isdst > 0);  
			strcpy(tbuf, tzname[i]);
#else
#ifdef HAVE_TM_ZONE
			strcpy(tbuf, timeptr->tm_zone);
#else
#ifdef HAVE_TM_NAME
			strcpy(tbuf, timeptr->tm_name);
#else
			gettimeofday(& tv, & zone);
			strcpy(tbuf, timezone(zone.tz_minuteswest,
						timeptr->tm_isdst > 0));
#endif  
#endif  
#endif  
			break;

#ifdef SUNOS_EXT
		case 'k':	 
			sprintf(tbuf, "%2d", range(0, timeptr->tm_hour, 23));
			break;

		case 'l':	 
			i = range(0, timeptr->tm_hour, 23);
			if (i == 0)
				i = 12;
			else if (i > 12)
				i -= 12;
			sprintf(tbuf, "%2d", i);
			break;
#endif

#ifdef HPUX_EXT
		case 'N':	 
			 
			goto century;	 

		case 'o':	 
			goto year;	 
#endif  


#ifdef VMS_EXT
		case 'v':	 
			sprintf(tbuf, "%2d-%3.3s-%4ld",
				range(1, timeptr->tm_mday, 31),
				months_a[range(0, timeptr->tm_mon, 11)],
				timeptr->tm_year + 1900L);
			for (i = 3; i < 6; i++)
				if (islower(tbuf[i]))
					tbuf[i] = toupper(tbuf[i]);
			break;
#endif

		default:
			tbuf[0] = '%';
			tbuf[1] = *format;
			tbuf[2] = '\0';
			break;
		}
		i = strlen(tbuf);
		if (i) {
			if (s + i < endp - 1) {
				strcpy(s, tbuf);
				s += i;
			} else
				return 0;
		}
	}
out:
	if (s < endp && *format == '\0') {
		*s = '\0';
		if (s == start)
			errno = oerrno;
		return (s - start);
	} else
		return 0;
}

 

static int
isleap(long year)
{
	return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}


 

static int
iso8601wknum(const struct tm *timeptr)
{
	 

	int weeknum, jan1day, diff;

	 
	weeknum = weeknumber(timeptr, 1);

	 
	jan1day = timeptr->tm_wday - (timeptr->tm_yday % 7);
	if (jan1day < 0)
		jan1day += 7;

	 
	switch (jan1day) {
	case 1:		 
		break;
	case 2:		 
	case 3:		 
	case 4:		 
		weeknum++;
		break;
	case 5:		 
	case 6:		 
	case 0:		 
		if (weeknum == 0) {
#ifdef USE_BROKEN_XPG4
			 
			weeknum = 53;
#else
			 
			struct tm dec31ly;	 
			dec31ly = *timeptr;
			dec31ly.tm_year--;
			dec31ly.tm_mon = 11;
			dec31ly.tm_mday = 31;
			dec31ly.tm_wday = (jan1day == 0) ? 6 : jan1day - 1;
			dec31ly.tm_yday = 364 + isleap(dec31ly.tm_year + 1900L);
			weeknum = iso8601wknum(& dec31ly);
#endif
		}
		break;
	}

	if (timeptr->tm_mon == 11) {
		 
		int wday, mday;

		wday = timeptr->tm_wday;
		mday = timeptr->tm_mday;
		if (   (wday == 1 && (mday >= 29 && mday <= 31))
		    || (wday == 2 && (mday == 30 || mday == 31))
		    || (wday == 3 &&  mday == 31))
			weeknum = 1;
	}

	return weeknum;
}

 

 

static int
weeknumber(const struct tm *timeptr, int firstweekday)
{
	int wday = timeptr->tm_wday;
	int ret;

	if (firstweekday == 1) {
		if (wday == 0)	 
			wday = 6;
		else
			wday--;
	}
	ret = ((timeptr->tm_yday + 7 - wday) / 7);
	if (ret < 0)
		ret = 0;
	return ret;
}

#if 0
 

Date:         Wed, 24 Apr 91 20:54:08 MDT
From: Michal Jaegermann <audfax!emory!vm.ucs.UAlberta.CA!NTOMCZAK>
To: arnold@audiofax.com

Hi Arnold,
in a process of fixing of strftime() in libraries on Atari ST I grabbed
some pieces of code from your own strftime.  When doing that it came
to mind that your weeknumber() function compiles a little bit nicer
in the following form:
 
{
    return (timeptr->tm_yday - timeptr->tm_wday +
	    (firstweekday ? (timeptr->tm_wday ? 8 : 1) : 7)) / 7;
}
How nicer it depends on a compiler, of course, but always a tiny bit.

   Cheers,
   Michal
   ntomczak@vm.ucs.ualberta.ca
#endif

#ifdef	TEST_STRFTIME

 

 

#ifndef NULL
#include	<stdio.h>
#endif
#include	<sys/time.h>
#include	<string.h>

#define		MAXTIME		132

 

static char *array[] =
{
	"(%%A)      full weekday name, var length (Sunday..Saturday)  %A",
	"(%%B)       full month name, var length (January..December)  %B",
	"(%%C)                                               Century  %C",
	"(%%D)                                       date (%%m/%%d/%%y)  %D",
	"(%%E)                           Locale extensions (ignored)  %E",
	"(%%F)       full month name, var length (January..December)  %F",
	"(%%H)                          hour (24-hour clock, 00..23)  %H",
	"(%%I)                          hour (12-hour clock, 01..12)  %I",
	"(%%M)                                       minute (00..59)  %M",
	"(%%N)                                      Emperor/Era Name  %N",
	"(%%O)                           Locale extensions (ignored)  %O",
	"(%%R)                                 time, 24-hour (%%H:%%M)  %R",
	"(%%S)                                       second (00..60)  %S",
	"(%%T)                              time, 24-hour (%%H:%%M:%%S)  %T",
	"(%%U)    week of year, Sunday as first day of week (00..53)  %U",
	"(%%V)                    week of year according to ISO 8601  %V",
	"(%%W)    week of year, Monday as first day of week (00..53)  %W",
	"(%%X)     appropriate locale time representation (%H:%M:%S)  %X",
	"(%%Y)                           year with century (1970...)  %Y",
	"(%%Z) timezone (EDT), or blank if timezone not determinable  %Z",
	"(%%a)          locale's abbreviated weekday name (Sun..Sat)  %a",
	"(%%b)            locale's abbreviated month name (Jan..Dec)  %b",
	"(%%c)           full date (Sat Nov  4 12:02:33 1989)%n%t%t%t  %c",
	"(%%d)                             day of the month (01..31)  %d",
	"(%%e)               day of the month, blank-padded ( 1..31)  %e",
	"(%%h)                                should be same as (%%b)  %h",
	"(%%j)                            day of the year (001..366)  %j",
	"(%%k)               hour, 24-hour clock, blank pad ( 0..23)  %k",
	"(%%l)               hour, 12-hour clock, blank pad ( 0..12)  %l",
	"(%%m)                                        month (01..12)  %m",
	"(%%o)                                      Emperor/Era Year  %o",
	"(%%p)              locale's AM or PM based on 12-hour clock  %p",
	"(%%r)                   time, 12-hour (same as %%I:%%M:%%S %%p)  %r",
	"(%%u) ISO 8601: Weekday as decimal number [1 (Monday) - 7]   %u",
	"(%%v)                                VMS date (dd-bbb-YYYY)  %v",
	"(%%w)                       day of week (0..6, Sunday == 0)  %w",
	"(%%x)                appropriate locale date representation  %x",
	"(%%y)                      last two digits of year (00..99)  %y",
	"(%%z)      timezone offset east of GMT as HHMM (e.g. -0500)  %z",
	(char *) NULL
};

 

int
main(argc, argv)
int argc;
char **argv;
{
	long time();

	char *next;
	char string[MAXTIME];

	int k;
	int length;

	struct tm *tm;

	long clock;

	 

	clock = time((long *) 0);
	tm = localtime(&clock);

	for (k = 0; next = array[k]; k++) {
		length = strftime(string, MAXTIME, next, tm);
		printf("%s\n", string);
	}

	exit(0);
}
#endif	 
