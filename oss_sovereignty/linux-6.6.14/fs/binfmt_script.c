
 

#include <linux/module.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/binfmts.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/err.h>
#include <linux/fs.h>

static inline bool spacetab(char c) { return c == ' ' || c == '\t'; }
static inline const char *next_non_spacetab(const char *first, const char *last)
{
	for (; first <= last; first++)
		if (!spacetab(*first))
			return first;
	return NULL;
}
static inline const char *next_terminator(const char *first, const char *last)
{
	for (; first <= last; first++)
		if (spacetab(*first) || !*first)
			return first;
	return NULL;
}

static int load_script(struct linux_binprm *bprm)
{
	const char *i_name, *i_sep, *i_arg, *i_end, *buf_end;
	struct file *file;
	int retval;

	 
	if ((bprm->buf[0] != '#') || (bprm->buf[1] != '!'))
		return -ENOEXEC;

	 
	buf_end = bprm->buf + sizeof(bprm->buf) - 1;
	i_end = strnchr(bprm->buf, sizeof(bprm->buf), '\n');
	if (!i_end) {
		i_end = next_non_spacetab(bprm->buf + 2, buf_end);
		if (!i_end)
			return -ENOEXEC;  
		 
		if (!next_terminator(i_end, buf_end))
			return -ENOEXEC;
		i_end = buf_end;
	}
	 
	while (spacetab(i_end[-1]))
		i_end--;

	 
	i_name = next_non_spacetab(bprm->buf+2, i_end);
	if (!i_name || (i_name == i_end))
		return -ENOEXEC;  

	 
	i_arg = NULL;
	i_sep = next_terminator(i_name, i_end);
	if (i_sep && (*i_sep != '\0'))
		i_arg = next_non_spacetab(i_sep, i_end);

	 
	if (bprm->interp_flags & BINPRM_FLAGS_PATH_INACCESSIBLE)
		return -ENOENT;

	 
	retval = remove_arg_zero(bprm);
	if (retval)
		return retval;
	retval = copy_string_kernel(bprm->interp, bprm);
	if (retval < 0)
		return retval;
	bprm->argc++;
	*((char *)i_end) = '\0';
	if (i_arg) {
		*((char *)i_sep) = '\0';
		retval = copy_string_kernel(i_arg, bprm);
		if (retval < 0)
			return retval;
		bprm->argc++;
	}
	retval = copy_string_kernel(i_name, bprm);
	if (retval)
		return retval;
	bprm->argc++;
	retval = bprm_change_interp(i_name, bprm);
	if (retval < 0)
		return retval;

	 
	file = open_exec(i_name);
	if (IS_ERR(file))
		return PTR_ERR(file);

	bprm->interpreter = file;
	return 0;
}

static struct linux_binfmt script_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_script,
};

static int __init init_script_binfmt(void)
{
	register_binfmt(&script_format);
	return 0;
}

static void __exit exit_script_binfmt(void)
{
	unregister_binfmt(&script_format);
}

core_initcall(init_script_binfmt);
module_exit(exit_script_binfmt);
MODULE_LICENSE("GPL");
