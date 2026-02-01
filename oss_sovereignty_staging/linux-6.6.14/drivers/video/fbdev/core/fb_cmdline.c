 

#include <linux/export.h>
#include <linux/fb.h>
#include <linux/string.h>

#include <video/cmdline.h>

 
int fb_get_options(const char *name, char **option)
{
	const char *options = NULL;
	bool is_of = false;
	bool enabled;

	if (name)
		is_of = strncmp(name, "offb", 4);

	enabled = __video_get_options(name, &options, is_of);

	if (options) {
		if (!strncmp(options, "off", 3))
			enabled = false;
	}

	if (option) {
		if (options)
			*option = kstrdup(options, GFP_KERNEL);
		else
			*option = NULL;
	}

	return enabled ? 0 : 1; 
}
EXPORT_SYMBOL(fb_get_options);
