 

 

 
#ifndef TPARM_TYPE_H
#define TPARM_TYPE_H 1

#define USE_LIBTINFO
#include <progs.priv.h>

typedef enum {
    Other = -1
    ,Numbers = 0
    ,Num_Str
    ,Num_Str_Str
} TParams;

extern TParams tparm_type(const char *name);
extern TParams guess_tparm_type(int nparam, char **p_is_s);

#endif  
