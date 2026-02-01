 

#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include "../../libspl_impl.h"

__attribute__((visibility("hidden"))) ssize_t
getexecname_impl(char *execname)
{
	return (readlink("/proc/self/exe", execname, PATH_MAX));
}
