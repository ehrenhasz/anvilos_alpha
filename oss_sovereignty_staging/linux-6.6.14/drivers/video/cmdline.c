
 

#include <linux/fb.h>  
#include <linux/init.h>

#include <video/cmdline.h>

 
static const char *video_options[FB_MAX] __read_mostly;
static const char *video_option __read_mostly;
static int video_of_only __read_mostly;

static const char *__video_get_option_string(const char *name)
{
	const char *options = NULL;
	size_t name_len = 0;

	if (name)
		name_len = strlen(name);

	if (name_len) {
		unsigned int i;
		const char *opt;

		for (i = 0; i < ARRAY_SIZE(video_options); ++i) {
			if (!video_options[i])
				continue;
			if (video_options[i][0] == '\0')
				continue;
			opt = video_options[i];
			if (!strncmp(opt, name, name_len) && opt[name_len] == ':')
				options = opt + name_len + 1;
		}
	}

	 
	if (!options)
		options = video_option;

	return options;
}

 
const char *video_get_options(const char *name)
{
	return __video_get_option_string(name);
}
EXPORT_SYMBOL(video_get_options);

bool __video_get_options(const char *name, const char **options, bool is_of)
{
	bool enabled = true;
	const char *opt = NULL;

	if (video_of_only && !is_of)
		enabled = false;

	opt = __video_get_option_string(name);

	if (options)
		*options = opt;

	return enabled;
}
EXPORT_SYMBOL(__video_get_options);

 
static int __init video_setup(char *options)
{
	if (!options || !*options)
		goto out;

	if (!strncmp(options, "ofonly", 6)) {
		video_of_only = true;
		goto out;
	}

	if (strchr(options, ':')) {
		 
		size_t i;

		for (i = 0; i < ARRAY_SIZE(video_options); i++) {
			if (!video_options[i]) {
				video_options[i] = options;
				break;
			}
		}
	} else {
		 
		video_option = options;
	}

out:
	return 1;
}
__setup("video=", video_setup);
