 

#include <config.h>

#include "selinux-at.h"
#include "openat.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "save-cwd.h"

#include "openat-priv.h"

#define AT_FUNC_NAME getfileconat
#define AT_FUNC_F1 getfilecon
#define AT_FUNC_POST_FILE_PARAM_DECLS , char **con
#define AT_FUNC_POST_FILE_ARGS        , con
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS

#define AT_FUNC_NAME lgetfileconat
#define AT_FUNC_F1 lgetfilecon
#define AT_FUNC_POST_FILE_PARAM_DECLS , char **con
#define AT_FUNC_POST_FILE_ARGS        , con
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS

#define AT_FUNC_NAME setfileconat
#define AT_FUNC_F1 setfilecon
#define AT_FUNC_POST_FILE_PARAM_DECLS , char const *con
#define AT_FUNC_POST_FILE_ARGS        , con
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS

#define AT_FUNC_NAME lsetfileconat
#define AT_FUNC_F1 lsetfilecon
#define AT_FUNC_POST_FILE_PARAM_DECLS , char const *con
#define AT_FUNC_POST_FILE_ARGS        , con
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS
