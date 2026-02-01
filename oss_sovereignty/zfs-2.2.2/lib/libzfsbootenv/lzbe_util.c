 
 

#include <sys/types.h>
#include <string.h>
#include <libzfs.h>
#include <libzfsbootenv.h>

 
int
lzbe_bootenv_print(const char *pool, const char *nvlist, FILE *of)
{
	nvlist_t *nv;
	int rv = -1;

	if (pool == NULL || *pool == '\0' || of == NULL)
		return (rv);

	rv = lzbe_nvlist_get(pool, nvlist, (void **)&nv);
	if (rv == 0) {
		nvlist_print(of, nv);
		nvlist_free(nv);
	}

	return (rv);
}
