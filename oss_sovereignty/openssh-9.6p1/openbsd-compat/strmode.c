 
 

 

#include "includes.h"
#ifndef HAVE_STRMODE

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

 

void
strmode(int mode, char *p)
{
	  
	switch (mode & S_IFMT) {
	case S_IFDIR:			 
		*p++ = 'd';
		break;
	case S_IFCHR:			 
		*p++ = 'c';
		break;
	case S_IFBLK:			 
		*p++ = 'b';
		break;
	case S_IFREG:			 
		*p++ = '-';
		break;
	case S_IFLNK:			 
		*p++ = 'l';
		break;
#ifdef S_IFSOCK
	case S_IFSOCK:			 
		*p++ = 's';
		break;
#endif
#ifdef S_IFIFO
	case S_IFIFO:			 
		*p++ = 'p';
		break;
#endif
	default:			 
		*p++ = '?';
		break;
	}
	 
	if (mode & S_IRUSR)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWUSR)
		*p++ = 'w';
	else
		*p++ = '-';
	switch (mode & (S_IXUSR | S_ISUID)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXUSR:
		*p++ = 'x';
		break;
	case S_ISUID:
		*p++ = 'S';
		break;
	case S_IXUSR | S_ISUID:
		*p++ = 's';
		break;
	}
	 
	if (mode & S_IRGRP)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWGRP)
		*p++ = 'w';
	else
		*p++ = '-';
	switch (mode & (S_IXGRP | S_ISGID)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXGRP:
		*p++ = 'x';
		break;
	case S_ISGID:
		*p++ = 'S';
		break;
	case S_IXGRP | S_ISGID:
		*p++ = 's';
		break;
	}
	 
	if (mode & S_IROTH)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWOTH)
		*p++ = 'w';
	else
		*p++ = '-';
	switch (mode & (S_IXOTH | S_ISVTX)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXOTH:
		*p++ = 'x';
		break;
	case S_ISVTX:
		*p++ = 'T';
		break;
	case S_IXOTH | S_ISVTX:
		*p++ = 't';
		break;
	}
	*p++ = ' ';		 
	*p = '\0';
}
#endif
