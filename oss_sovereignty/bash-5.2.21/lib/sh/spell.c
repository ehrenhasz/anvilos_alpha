 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <bashtypes.h>
#include <posixdir.h>
#include <posixstat.h>
#if defined (HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#include <stdio.h>

#include <bashansi.h>
#include <maxpath.h>
#include <stdc.h>

static int mindist PARAMS((char *, char *, char *));
static int spdist PARAMS((char *, char *));

 

 
int
spname(oldname, newname)
     char *oldname;
     char *newname;
{
  char *op, *np, *p;
  char guess[PATH_MAX + 1], best[PATH_MAX + 1];

  op = oldname;
  np = newname;
  for (;;)
    {
      while (*op == '/')     
	*np++ = *op++;
      *np = '\0';

      if (*op == '\0')     
	{
	   
	  if (oldname[1] == '\0' && newname[1] == '\0' &&
		oldname[0] != '.' && newname[0] == '.')
	    return -1;
	  return strcmp(oldname, newname) != 0;
	}

       
      for (p = guess; *op != '/' && *op != '\0'; op++)
	if (p < guess + PATH_MAX)
	  *p++ = *op;
      *p = '\0';

      if (mindist(newname, guess, best) >= 3)
	return -1;   

       
      for (p = best; *np = *p++; np++)
	;
    }
}

 
static int
mindist(dir, guess, best)
     char *dir;
     char *guess;
     char *best;
{
  DIR *fd;
  struct dirent *dp;
  int dist, x;

  dist = 3;     
  if (*dir == '\0')
    dir = ".";

  if ((fd = opendir(dir)) == NULL)
    return dist;

  while ((dp = readdir(fd)) != NULL)
    {
       
      x = spdist(dp->d_name, guess);
      if (x <= dist && x != 3)
	{
	  strcpy(best, dp->d_name);
	  dist = x;
	  if (dist == 0)     
	    break;
	}
    }
  (void)closedir(fd);

   
  if (best[0] == '.' && best[1] == '\0')
    dist = 3;
  return dist;
}

 
static int
spdist(cur, new)
     char *cur, *new;
{
  while (*cur == *new)
    {
      if (*cur == '\0')
	return 0;     
      cur++;
      new++;
    }

  if (*cur)
    {
      if (*new)
	{
	  if (cur[1] && new[1] && cur[0] == new[1] && cur[1] == new[0] && strcmp (cur + 2, new + 2) == 0)
	    return 1;   

	  if (strcmp (cur + 1, new + 1) == 0)
	    return 2;   
	}

      if (strcmp(&cur[1], &new[0]) == 0)
	return 2;     
    }

  if (*new && strcmp(cur, new + 1) == 0)
    return 2;       

  return 3;
}

char *
dirspell (dirname)
     char *dirname;
{
  int n;
  char *guess;

  n = (strlen (dirname) * 3 + 1) / 2 + 1;
  guess = (char *)malloc (n);
  if (guess == 0)
    return 0;

  switch (spname (dirname, guess))
    {
    case -1:
    default:
      free (guess);
      return (char *)NULL;
    case 0:
    case 1:
      return guess;
    }
}
