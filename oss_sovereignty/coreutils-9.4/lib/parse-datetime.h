 
#define PARSE_DATETIME_DEBUG 1

 
bool parse_datetime2 (struct timespec *restrict,
                      char const *, struct timespec const *,
                      unsigned int flags, timezone_t, char const *);
