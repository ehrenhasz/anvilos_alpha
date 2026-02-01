 

#ifndef POSIXTM_H_
# define POSIXTM_H_

# include <time.h>

 
# define PDS_TRAILING_YEAR 1
# define PDS_CENTURY 2
# define PDS_SECONDS 4
# define PDS_PRE_2000 8

 
# define PDS_LEADING_YEAR 0

bool posixtime (time_t *p, const char *s, unsigned int syntax_bits);

#endif
