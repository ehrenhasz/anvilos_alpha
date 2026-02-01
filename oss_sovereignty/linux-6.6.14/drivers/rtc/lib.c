
 

#include <linux/export.h>
#include <linux/rtc.h>

static const unsigned char rtc_days_in_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const unsigned short rtc_ydays[2][13] = {
	 
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	 
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

 
int rtc_month_days(unsigned int month, unsigned int year)
{
	return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}
EXPORT_SYMBOL(rtc_month_days);

 
int rtc_year_days(unsigned int day, unsigned int month, unsigned int year)
{
	return rtc_ydays[is_leap_year(year)][month] + day - 1;
}
EXPORT_SYMBOL(rtc_year_days);

 
void rtc_time64_to_tm(time64_t time, struct rtc_time *tm)
{
	unsigned int secs;
	int days;

	u64 u64tmp;
	u32 u32tmp, udays, century, day_of_century, year_of_century, year,
		day_of_year, month, day;
	bool is_Jan_or_Feb, is_leap_year;

	 
	days = div_s64_rem(time, 86400, &secs);

	 
	tm->tm_wday = (days + 4) % 7;

	 

	udays		= ((u32) days) + 719468;

	u32tmp		= 4 * udays + 3;
	century		= u32tmp / 146097;
	day_of_century	= u32tmp % 146097 / 4;

	u32tmp		= 4 * day_of_century + 3;
	u64tmp		= 2939745ULL * u32tmp;
	year_of_century	= upper_32_bits(u64tmp);
	day_of_year	= lower_32_bits(u64tmp) / 2939745 / 4;

	year		= 100 * century + year_of_century;
	is_leap_year	= year_of_century != 0 ?
		year_of_century % 4 == 0 : century % 4 == 0;

	u32tmp		= 2141 * day_of_year + 132377;
	month		= u32tmp >> 16;
	day		= ((u16) u32tmp) / 2141;

	 
	is_Jan_or_Feb	= day_of_year >= 306;

	 
	year		= year + is_Jan_or_Feb;
	month		= is_Jan_or_Feb ? month - 12 : month;
	day		= day + 1;

	day_of_year	= is_Jan_or_Feb ?
		day_of_year - 306 : day_of_year + 31 + 28 + is_leap_year;

	 
	tm->tm_year	= (int) (year - 1900);
	tm->tm_mon	= (int) month;
	tm->tm_mday	= (int) day;
	tm->tm_yday	= (int) day_of_year + 1;

	tm->tm_hour = secs / 3600;
	secs -= tm->tm_hour * 3600;
	tm->tm_min = secs / 60;
	tm->tm_sec = secs - tm->tm_min * 60;

	tm->tm_isdst = 0;
}
EXPORT_SYMBOL(rtc_time64_to_tm);

 
int rtc_valid_tm(struct rtc_time *tm)
{
	if (tm->tm_year < 70 ||
	    tm->tm_year > (INT_MAX - 1900) ||
	    ((unsigned int)tm->tm_mon) >= 12 ||
	    tm->tm_mday < 1 ||
	    tm->tm_mday > rtc_month_days(tm->tm_mon,
					 ((unsigned int)tm->tm_year + 1900)) ||
	    ((unsigned int)tm->tm_hour) >= 24 ||
	    ((unsigned int)tm->tm_min) >= 60 ||
	    ((unsigned int)tm->tm_sec) >= 60)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(rtc_valid_tm);

 
time64_t rtc_tm_to_time64(struct rtc_time *tm)
{
	return mktime64(((unsigned int)tm->tm_year + 1900), tm->tm_mon + 1,
			tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}
EXPORT_SYMBOL(rtc_tm_to_time64);

 
ktime_t rtc_tm_to_ktime(struct rtc_time tm)
{
	return ktime_set(rtc_tm_to_time64(&tm), 0);
}
EXPORT_SYMBOL_GPL(rtc_tm_to_ktime);

 
struct rtc_time rtc_ktime_to_tm(ktime_t kt)
{
	struct timespec64 ts;
	struct rtc_time ret;

	ts = ktime_to_timespec64(kt);
	 
	if (ts.tv_nsec)
		ts.tv_sec++;
	rtc_time64_to_tm(ts.tv_sec, &ret);
	return ret;
}
EXPORT_SYMBOL_GPL(rtc_ktime_to_tm);
