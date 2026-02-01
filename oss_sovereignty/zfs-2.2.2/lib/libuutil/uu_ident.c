 
 



#include "libuutil_common.h"

#include <string.h>

 

#define	IS_ALPHA(c) \
	(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

#define	IS_DIGIT(c) \
	((c) >= '0' && (c) <= '9')

static int
is_valid_ident(const char *s, const char *e, int allowdot)
{
	char c;

	if (s >= e)
		return (0);		 

	c = *s++;
	if (!IS_ALPHA(c))
		return (0);		 

	while (s < e && (c = *s++) != 0) {
		if (IS_ALPHA(c) || IS_DIGIT(c) || c == '-' || c == '_' ||
		    (allowdot && c == '.'))
			continue;
		return (0);		 
	}
	return (1);
}

static int
is_valid_component(const char *b, const char *e, uint_t flags)
{
	char *sp;

	if (flags & UU_NAME_DOMAIN) {
		sp = strchr(b, ',');
		if (sp != NULL && sp < e) {
			if (!is_valid_ident(b, sp, 1))
				return (0);
			b = sp + 1;
		}
	}

	return (is_valid_ident(b, e, 0));
}

int
uu_check_name(const char *name, uint_t flags)
{
	const char *end = name + strlen(name);
	const char *p;

	if (flags & ~(UU_NAME_DOMAIN | UU_NAME_PATH)) {
		uu_set_error(UU_ERROR_UNKNOWN_FLAG);
		return (-1);
	}

	if (!(flags & UU_NAME_PATH)) {
		if (!is_valid_component(name, end, flags))
			goto bad;
		return (0);
	}

	while ((p = strchr(name, '/')) != NULL) {
		if (!is_valid_component(name, p - 1, flags))
			goto bad;
		name = p + 1;
	}
	if (!is_valid_component(name, end, flags))
		goto bad;

	return (0);

bad:
	uu_set_error(UU_ERROR_INVALID_ARGUMENT);
	return (-1);
}
