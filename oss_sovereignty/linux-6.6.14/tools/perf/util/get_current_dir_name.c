


#ifndef HAVE_GET_CURRENT_DIR_NAME
#include "get_current_dir_name.h"
#include <limits.h>
#include <string.h>
#include <unistd.h>

 

char *get_current_dir_name(void)
{
	char pwd[PATH_MAX];

	return getcwd(pwd, sizeof(pwd)) == NULL ? NULL : strdup(pwd);
}
#endif 
