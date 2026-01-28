#ifndef BB_RTC_H
#define BB_RTC_H 1
#include "libbb.h"
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#if ENABLE_FEATURE_HWCLOCK_ADJTIME_FHS
# define ADJTIME_PATH "/var/lib/hwclock/adjtime"
#else
# define ADJTIME_PATH "/etc/adjtime"
#endif
int rtc_adjtime_is_utc(void) FAST_FUNC;
int rtc_xopen(const char **default_rtc, int flags) FAST_FUNC;
void rtc_read_tm(struct tm *ptm, int fd) FAST_FUNC;
time_t rtc_tm2time(struct tm *ptm, int utc) FAST_FUNC;
struct linux_rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};
struct linux_rtc_wkalrm {
	unsigned char enabled;   
	unsigned char pending;   
	struct linux_rtc_time time;   
};
#define RTC_AIE_ON      _IO('p', 0x01)   
#define RTC_AIE_OFF     _IO('p', 0x02)   
#define RTC_UIE_ON      _IO('p', 0x03)   
#define RTC_UIE_OFF     _IO('p', 0x04)   
#define RTC_PIE_ON      _IO('p', 0x05)   
#define RTC_PIE_OFF     _IO('p', 0x06)   
#define RTC_WIE_ON      _IO('p', 0x0f)   
#define RTC_WIE_OFF     _IO('p', 0x10)   
#define RTC_ALM_SET     _IOW('p', 0x07, struct linux_rtc_time)  
#define RTC_ALM_READ    _IOR('p', 0x08, struct linux_rtc_time)  
#define RTC_RD_TIME     _IOR('p', 0x09, struct linux_rtc_time)  
#define RTC_SET_TIME    _IOW('p', 0x0a, struct linux_rtc_time)  
#define RTC_IRQP_READ   _IOR('p', 0x0b, unsigned long)    
#define RTC_IRQP_SET    _IOW('p', 0x0c, unsigned long)    
#define RTC_EPOCH_READ  _IOR('p', 0x0d, unsigned long)    
#define RTC_EPOCH_SET   _IOW('p', 0x0e, unsigned long)    
#define RTC_WKALM_SET   _IOW('p', 0x0f, struct linux_rtc_wkalrm) 
#define RTC_WKALM_RD    _IOR('p', 0x10, struct linux_rtc_wkalrm) 
#define RTC_IRQF 0x80  
#define RTC_PF 0x40
#define RTC_AF 0x20
#define RTC_UF 0x10
POP_SAVED_FUNCTION_VISIBILITY
#endif
