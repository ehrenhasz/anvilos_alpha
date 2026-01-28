#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <libzutil.h>
static struct {
	const char *arg0;
	char *base, *end;
	char *nul;
	boolean_t warned;
	boolean_t reset;
	int error;
} SPT;
#define	LIBBSD_IS_PATHNAME_SEPARATOR(c) ((c) == '/')
#define	SPT_MAXTITLE 255
extern const char *__progname;
static const char *
getprogname(void)
{
	return (__progname);
}
static void
setprogname(const char *progname)
{
	size_t i;
	for (i = strlen(progname); i > 0; i--) {
		if (LIBBSD_IS_PATHNAME_SEPARATOR(progname[i - 1])) {
			__progname = progname + i;
			return;
		}
	}
	__progname = progname;
}
static inline size_t
spt_min(size_t a, size_t b)
{
	return ((a < b) ? a : b);
}
static int
spt_clearenv(void)
{
	char **tmp;
	tmp = malloc(sizeof (*tmp));
	if (tmp == NULL)
		return (errno);
	tmp[0] = NULL;
	environ = tmp;
	return (0);
}
static int
spt_copyenv(int envc, char *envp[])
{
	char **envcopy;
	char *eq;
	int envsize;
	int i, error;
	if (environ != envp)
		return (0);
	envsize = (envc + 1) * sizeof (char *);
	envcopy = malloc(envsize);
	if (envcopy == NULL)
		return (errno);
	memcpy(envcopy, envp, envsize);
	error = spt_clearenv();
	if (error) {
		environ = envp;
		free(envcopy);
		return (error);
	}
	for (i = 0; envcopy[i]; i++) {
		eq = strchr(envcopy[i], '=');
		if (eq == NULL)
			continue;
		*eq = '\0';
		if (setenv(envcopy[i], eq + 1, 1) < 0)
			error = errno;
		*eq = '=';
		if (error) {
			environ = envp;
			free(envcopy);
			return (error);
		}
	}
	free(envcopy);
	return (0);
}
static int
spt_copyargs(int argc, char *argv[])
{
	char *tmp;
	int i;
	for (i = 1; i < argc || (i >= argc && argv[i]); i++) {
		if (argv[i] == NULL)
			continue;
		tmp = strdup(argv[i]);
		if (tmp == NULL)
			return (errno);
		argv[i] = tmp;
	}
	return (0);
}
void
zfs_setproctitle_init(int argc, char *argv[], char *envp[])
{
	char *base, *end, *nul, *tmp;
	int i, envc, error;
	if (argc < 0)
		return;
	base = argv[0];
	if (base == NULL)
		return;
	nul = base + strlen(base);
	end = nul + 1;
	for (i = 0; i < argc || (i >= argc && argv[i]); i++) {
		if (argv[i] == NULL || argv[i] != end)
			continue;
		end = argv[i] + strlen(argv[i]) + 1;
	}
	for (i = 0; envp[i]; i++) {
		if (envp[i] != end)
			continue;
		end = envp[i] + strlen(envp[i]) + 1;
	}
	envc = i;
	SPT.arg0 = strdup(argv[0]);
	if (SPT.arg0 == NULL) {
		SPT.error = errno;
		return;
	}
	tmp = strdup(getprogname());
	if (tmp == NULL) {
		SPT.error = errno;
		return;
	}
	setprogname(tmp);
	error = spt_copyenv(envc, envp);
	if (error) {
		SPT.error = error;
		return;
	}
	error = spt_copyargs(argc, argv);
	if (error) {
		SPT.error = error;
		return;
	}
	SPT.nul  = nul;
	SPT.base = base;
	SPT.end  = end;
}
void
zfs_setproctitle(const char *fmt, ...)
{
	char buf[SPT_MAXTITLE + 1];
	va_list ap;
	char *nul;
	int len;
	if (SPT.base == NULL) {
		if (!SPT.warned) {
			warnx("setproctitle not initialized, please"
			    "call zfs_setproctitle_init()");
			SPT.warned = B_TRUE;
		}
		return;
	}
	if (fmt) {
		if (fmt[0] == '-') {
			fmt++;
			len = 0;
		} else {
			snprintf(buf, sizeof (buf), "%s: ", getprogname());
			len = strlen(buf);
		}
		va_start(ap, fmt);
		len += vsnprintf(buf + len, sizeof (buf) - len, fmt, ap);
		va_end(ap);
	} else {
		len = snprintf(buf, sizeof (buf), "%s", SPT.arg0);
	}
	if (len <= 0) {
		SPT.error = errno;
		return;
	}
	if (!SPT.reset) {
		memset(SPT.base, 0, SPT.end - SPT.base);
		SPT.reset = B_TRUE;
	} else {
		memset(SPT.base, 0, spt_min(sizeof (buf), SPT.end - SPT.base));
	}
	len = spt_min(len, spt_min(sizeof (buf), SPT.end - SPT.base) - 1);
	memcpy(SPT.base, buf, len);
	nul = SPT.base + len;
	if (nul < SPT.nul) {
		*SPT.nul = '.';
	} else if (nul == SPT.nul && nul + 1 < SPT.end) {
		*SPT.nul = ' ';
		*++nul = '\0';
	}
}
