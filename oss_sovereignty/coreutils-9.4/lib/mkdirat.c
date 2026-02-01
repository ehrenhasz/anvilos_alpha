 

#include <config.h>

 
#include <sys/stat.h>

#include <stdlib.h>
#include <unistd.h>

 

#define AT_FUNC_NAME mkdirat
#define AT_FUNC_F1 mkdir
#define AT_FUNC_POST_FILE_PARAM_DECLS , mode_t mode
#define AT_FUNC_POST_FILE_ARGS        , mode
#include "at-func.c"
