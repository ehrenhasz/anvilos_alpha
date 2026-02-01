
 

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/inet.h>
#include <linux/ctype.h>
#include "cifsglob.h"
#include "cifsproto.h"

 
char *extract_hostname(const char *unc)
{
	const char *src;
	char *dst, *delim;
	unsigned int len;

	 
	 
	if (strlen(unc) < 3)
		return ERR_PTR(-EINVAL);
	for (src = unc; *src && *src == '\\'; src++)
		;
	if (!*src)
		return ERR_PTR(-EINVAL);

	 
	delim = strchr(src, '\\');
	if (!delim)
		return ERR_PTR(-EINVAL);

	len = delim - src;
	dst = kmalloc((len + 1), GFP_KERNEL);
	if (dst == NULL)
		return ERR_PTR(-ENOMEM);

	memcpy(dst, src, len);
	dst[len] = '\0';

	return dst;
}

char *extract_sharename(const char *unc)
{
	const char *src;
	char *delim, *dst;

	 
	src = unc + 2;

	 
	delim = strchr(src, '\\');
	if (!delim)
		return ERR_PTR(-EINVAL);
	delim++;

	 
	dst = kstrdup(delim, GFP_KERNEL);
	if (!dst)
		return ERR_PTR(-ENOMEM);

	return dst;
}
