
#include <linux/drbd_config.h>
#include <linux/module.h>

const char *drbd_buildtag(void)
{
	 

	static char buildtag[38] = "\0uilt-in";

	if (buildtag[0] == 0) {
#ifdef MODULE
		sprintf(buildtag, "srcversion: %-24s", THIS_MODULE->srcversion);
#else
		buildtag[0] = 'b';
#endif
	}

	return buildtag;
}
