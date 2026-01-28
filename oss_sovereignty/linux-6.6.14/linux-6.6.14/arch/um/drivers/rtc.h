#ifndef __UM_RTC_H__
#define __UM_RTC_H__
int uml_rtc_start(bool timetravel);
int uml_rtc_enable_alarm(unsigned long long delta_seconds);
void uml_rtc_disable_alarm(void);
void uml_rtc_stop(bool timetravel);
void uml_rtc_send_timetravel_alarm(void);
#endif  
