 
# include <stdint.h>

# ifndef _STRTOL_ERROR
enum strtol_error
  {
    LONGINT_OK = 0,

     
    LONGINT_OVERFLOW = 1,
    LONGINT_INVALID_SUFFIX_CHAR = 2,

    LONGINT_INVALID_SUFFIX_CHAR_WITH_OVERFLOW = (LONGINT_INVALID_SUFFIX_CHAR
                                                 | LONGINT_OVERFLOW),
    LONGINT_INVALID = 4
  };
typedef enum strtol_error strtol_error;
# endif

# define _DECLARE_XSTRTOL(name, type) \
  strtol_error name (const char *, char **, int, type *, const char *);
_DECLARE_XSTRTOL (xstrtol, long int)
_DECLARE_XSTRTOL (xstrtoul, unsigned long int)
_DECLARE_XSTRTOL (xstrtoll, long long int)
_DECLARE_XSTRTOL (xstrtoull, unsigned long long int)
_DECLARE_XSTRTOL (xstrtoimax, intmax_t)
_DECLARE_XSTRTOL (xstrtoumax, uintmax_t)

#endif  
