 
#ifndef MICROPY_INCLUDED_LIB_TIMEUTILS_TIMEUTILS_H
#define MICROPY_INCLUDED_LIB_TIMEUTILS_TIMEUTILS_H



#define TIMEUTILS_SECONDS_1970_TO_2000 (946684800ULL)

typedef struct _timeutils_struct_time_t {
    uint16_t tm_year;       
    uint8_t tm_mon;         
    uint8_t tm_mday;        
    uint8_t tm_hour;        
    uint8_t tm_min;         
    uint8_t tm_sec;         
    uint8_t tm_wday;        
    uint16_t tm_yday;       
} timeutils_struct_time_t;

bool timeutils_is_leap_year(mp_uint_t year);
mp_uint_t timeutils_days_in_month(mp_uint_t year, mp_uint_t month);
mp_uint_t timeutils_year_day(mp_uint_t year, mp_uint_t month, mp_uint_t date);

void timeutils_seconds_since_2000_to_struct_time(mp_uint_t t,
    timeutils_struct_time_t *tm);

mp_uint_t timeutils_seconds_since_2000(mp_uint_t year, mp_uint_t month,
    mp_uint_t date, mp_uint_t hour, mp_uint_t minute, mp_uint_t second);

mp_uint_t timeutils_mktime_2000(mp_uint_t year, mp_int_t month, mp_int_t mday,
    mp_int_t hours, mp_int_t minutes, mp_int_t seconds);


#if MICROPY_EPOCH_IS_1970

static inline void timeutils_seconds_since_epoch_to_struct_time(uint64_t t, timeutils_struct_time_t *tm) {
    
    timeutils_seconds_since_2000_to_struct_time(t - TIMEUTILS_SECONDS_1970_TO_2000, tm);
}

static inline uint64_t timeutils_mktime(mp_uint_t year, mp_int_t month, mp_int_t mday, mp_int_t hours, mp_int_t minutes, mp_int_t seconds) {
    return timeutils_mktime_2000(year, month, mday, hours, minutes, seconds) + TIMEUTILS_SECONDS_1970_TO_2000;
}

static inline uint64_t timeutils_seconds_since_epoch(mp_uint_t year, mp_uint_t month,
    mp_uint_t date, mp_uint_t hour, mp_uint_t minute, mp_uint_t second) {
    
    return timeutils_seconds_since_2000(year, month, date, hour, minute, second) + TIMEUTILS_SECONDS_1970_TO_2000;
}

static inline mp_uint_t timeutils_seconds_since_epoch_from_nanoseconds_since_1970(uint64_t ns) {
    return ns / 1000000000ULL;
}

static inline uint64_t timeutils_nanoseconds_since_epoch_to_nanoseconds_since_1970(uint64_t ns) {
    return ns;
}

#else 

#define timeutils_seconds_since_epoch_to_struct_time timeutils_seconds_since_2000_to_struct_time
#define timeutils_seconds_since_epoch timeutils_seconds_since_2000
#define timeutils_mktime timeutils_mktime_2000

static inline uint64_t timeutils_seconds_since_epoch_to_nanoseconds_since_1970(mp_uint_t s) {
    return ((uint64_t)s + TIMEUTILS_SECONDS_1970_TO_2000) * 1000000000ULL;
}

static inline mp_uint_t timeutils_seconds_since_epoch_from_nanoseconds_since_1970(uint64_t ns) {
    return ns / 1000000000ULL - TIMEUTILS_SECONDS_1970_TO_2000;
}

static inline int64_t timeutils_nanoseconds_since_epoch_to_nanoseconds_since_1970(int64_t ns) {
    return ns + TIMEUTILS_SECONDS_1970_TO_2000 * 1000000000ULL;
}

#endif

int timeutils_calc_weekday(int y, int m, int d);

#endif 
