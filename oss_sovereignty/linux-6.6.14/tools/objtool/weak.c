
 

#include <stdbool.h>
#include <errno.h>
#include <objtool/objtool.h>

#define UNSUPPORTED(name)						\
({									\
	fprintf(stderr, "error: objtool: " name " not implemented\n");	\
	return ENOSYS;							\
})

int __weak orc_dump(const char *_objname)
{
	UNSUPPORTED("ORC");
}

int __weak orc_create(struct objtool_file *file)
{
	UNSUPPORTED("ORC");
}
