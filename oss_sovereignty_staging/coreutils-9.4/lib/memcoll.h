 

#ifndef MEMCOLL_H_
# define MEMCOLL_H_ 1

# include <stddef.h>

int memcoll (char *restrict, size_t, char *restrict, size_t);
int memcoll0 (char const *, size_t, char const *, size_t);

#endif  
