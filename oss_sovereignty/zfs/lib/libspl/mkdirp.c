#include <sys/types.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
static char *simplify(const char *str);
int
mkdirp(const char *d, mode_t mode)
{
	char  *endptr, *ptr, *slash, *str;
	str = simplify(d);
	if (str == NULL)
		return (-1);
	if (mkdir(str, mode) == 0) {
		free(str);
		return (0);
	}
	if (errno != ENOENT) {
		free(str);
		return (-1);
	}
	endptr = strrchr(str, '\0');
	slash = strrchr(str, '/');
	while (slash != NULL) {
		ptr = slash;
		*ptr = '\0';
		if (access(str, F_OK) == 0)
			break;
		else {
			slash = strrchr(str, '/');
			if (slash == NULL || slash == str) {
				if (mkdir(str, mode) != 0 && errno != EEXIST) {
					free(str);
					return (-1);
				}
				break;
			}
		}
	}
	while ((ptr = strchr(str, '\0')) != endptr) {
		*ptr = '/';
		if (mkdir(str, mode) != 0 && errno != EEXIST) {
			free(str);
			return (-1);
		}
	}
	free(str);
	return (0);
}
static char *
simplify(const char *str)
{
	int i;
	size_t mbPathlen;	 
	size_t wcPathlen;	 
	wchar_t *wptr;		 
	wchar_t *wcPath;	 
	char *mbPath;		 
	if (!str) {
		errno = ENOENT;
		return (NULL);
	}
	if ((mbPath = strdup(str)) == NULL) {
		return (NULL);
	}
	mbPathlen = strlen(mbPath);
	if ((wcPath = calloc(mbPathlen+1, sizeof (wchar_t))) == NULL) {
		free(mbPath);
		return (NULL);
	}
	if ((wcPathlen = mbstowcs(wcPath, mbPath, mbPathlen)) == (size_t)-1) {
		free(mbPath);
		free(wcPath);
		return (NULL);
	}
	for (wptr = wcPath, i = 0; i < wcPathlen; i++) {
		*wptr++ = wcPath[i];
		if (wcPath[i] == '/') {
			i++;
			while (wcPath[i] == '/') {
				i++;
			}
			i--;
		}
	}
	*wptr = '\0';
	if (wcstombs(mbPath, wcPath, mbPathlen) == (size_t)-1) {
		free(mbPath);
		free(wcPath);
		return (NULL);
	}
	free(wcPath);
	return (mbPath);
}
