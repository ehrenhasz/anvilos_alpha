 
 

#include <stdio.h>
#include <time.h>
#include <langinfo.h>
#include "statcommon.h"

#ifndef _DATE_FMT
#ifdef D_T_FMT
#define	_DATE_FMT D_T_FMT
#else  
#define	_DATE_FMT "%+"
#endif  
#endif  

 
void
print_timestamp(uint_t timestamp_fmt)
{
	time_t t = time(NULL);
	static const char *fmt = NULL;

	 
	if (fmt == NULL)
		fmt = nl_langinfo(_DATE_FMT);

	if (timestamp_fmt == UDATE) {
		(void) printf("%lld\n", (longlong_t)t);
	} else if (timestamp_fmt == DDATE) {
		char dstr[64];
		struct tm tm;
		int len;

		len = strftime(dstr, sizeof (dstr), fmt, localtime_r(&t, &tm));
		if (len > 0)
			(void) printf("%s\n", dstr);
	}
}
