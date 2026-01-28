

#ifndef RA_RTC_H_
#define RA_RTC_H_

#include <stdint.h>
#include <stdbool.h>

#define RTC_PERIOD_MINUTE 0x00
#define RTC_PERIOD_SECOND 0x01

typedef struct {
    unsigned short year;
    unsigned char month;
    unsigned char date;
    unsigned char weekday;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
} ra_rtc_t;

typedef void (*ra_rtc_cb_t)(void *);

int ra_rtc_get_year(void);
int ra_rtc_get_month(void);
int ra_rtc_get_date(void);
int ra_rtc_get_hour(void);
int ra_rtc_get_minute(void);
int ra_rtc_get_second(void);
int ra_rtc_get_weekday(void);
void ra_rtc_period_on();
void ra_rtc_period_off();
void ra_rtc_set_period_time(uint32_t period);
void ra_rtc_set_period_func(void *cb, void *param);
void ra_rtc_alarm_on(void);
void ra_rtc_alarm_off(void);
void ra_rtc_set_alarm_time(int hour, int min, int week_flag);
void ra_rtc_set_alarm_func(void *cb, void *param);
void ra_rtc_set_adjustment(int adj, int aadjp);
uint8_t ra_rtc_get_adjustment(void);
bool ra_rtc_set_time(ra_rtc_t *time);
bool ra_rtc_get_time(ra_rtc_t *time);
bool ra_rtc_init(uint8_t source);
bool ra_rtc_deinit(void);
void rtc_alarm_periodic_isr(void);
void rtc_carry_isr(void);

#endif 
