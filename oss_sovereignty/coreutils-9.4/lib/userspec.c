 

#include <config.h>

 
#include "userspec.h"

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "intprops.h"
#include "inttostr.h"
#include "xalloc.h"
#include "xstrtol.h"

#include "gettext.h"
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#ifndef HAVE_ENDGRENT
# define endgrent() ((void) 0)
#endif

#ifndef HAVE_ENDPWENT
# define endpwent() ((void) 0)
#endif

#ifndef UID_T_MAX
# define UID_T_MAX TYPE_MAXIMUM (uid_t)
#endif

#ifndef GID_T_MAX
# define GID_T_MAX TYPE_MAXIMUM (gid_t)
#endif

 
#ifndef MAXUID
# define MAXUID UID_T_MAX
#endif
#ifndef MAXGID
# define MAXGID GID_T_MAX
#endif

#ifdef __DJGPP__

 
# define ISDIGIT(c) ((unsigned int) (c) - '0' <= 9)

 

static bool
is_number (const char *str)
{
  do
    {
      if (!ISDIGIT (*str))
        return false;
    }
  while (*++str);

  return true;
}
#endif

static char const *
parse_with_separator (char const *spec, char const *separator,
                      uid_t *uid, gid_t *gid,
                      char **username, char **groupname)
{
  const char *error_msg;
  struct passwd *pwd;
  struct group *grp;
  char *u;
  char const *g;
  char *gname = NULL;
  uid_t unum = *uid;
  gid_t gnum = gid ? *gid : -1;

  error_msg = NULL;
  if (username)
    *username = NULL;
  if (groupname)
    *groupname = NULL;

   

  u = NULL;
  if (separator == NULL)
    {
      if (*spec)
        u = xstrdup (spec);
    }
  else
    {
      idx_t ulen = separator - spec;
      if (ulen != 0)
        {
          u = ximemdup (spec, ulen + 1);
          u[ulen] = '\0';
        }
    }

  g = (separator == NULL || *(separator + 1) == '\0'
       ? NULL
       : separator + 1);

#ifdef __DJGPP__
   
  if (u && !is_number (u))
    setenv ("USER", u, 1);
  if (g && !is_number (g))
    setenv ("GROUP", g, 1);
#endif

  if (u != NULL)
    {
       
      pwd = (*u == '+' ? NULL : getpwnam (u));
      if (pwd == NULL)
        {
          username = NULL;
          bool use_login_group = (separator != NULL && g == NULL);
          if (use_login_group)
            {
               
              error_msg = N_("invalid spec");
            }
          else
            {
              unsigned long int tmp;
              if (xstrtoul (u, NULL, 10, &tmp, "") == LONGINT_OK
                  && tmp <= MAXUID && (uid_t) tmp != (uid_t) -1)
                unum = tmp;
              else
                error_msg = N_("invalid user");
            }
        }
      else
        {
          unum = pwd->pw_uid;
          if (g == NULL && separator != NULL)
            {
               
              char buf[INT_BUFSIZE_BOUND (uintmax_t)];
              gnum = pwd->pw_gid;
              grp = getgrgid (gnum);
              gname = xstrdup (grp ? grp->gr_name : umaxtostr (gnum, buf));
              endgrent ();
            }
        }
      endpwent ();
    }

  if (g != NULL && error_msg == NULL)
    {
       
       
      grp = (*g == '+' ? NULL : getgrnam (g));
      if (grp == NULL)
        {
          groupname = NULL;
          unsigned long int tmp;
          if (xstrtoul (g, NULL, 10, &tmp, "") == LONGINT_OK
              && tmp <= MAXGID && (gid_t) tmp != (gid_t) -1)
            gnum = tmp;
          else
            error_msg = N_("invalid group");
        }
      else
        gnum = grp->gr_gid;
      endgrent ();               
      gname = xstrdup (g);
    }

  if (error_msg == NULL)
    {
      *uid = unum;
      if (gid)
        *gid = gnum;
      if (username)
        {
          *username = u;
          u = NULL;
        }
      if (groupname)
        {
          *groupname = gname;
          gname = NULL;
        }
    }

  free (u);
  free (gname);
  return error_msg ? _(error_msg) : NULL;
}

 

char const *
parse_user_spec_warn (char const *spec, uid_t *uid, gid_t *gid,
                      char **username, char **groupname, bool *pwarn)
{
  char const *colon = gid ? strchr (spec, ':') : NULL;
  char const *error_msg =
    parse_with_separator (spec, colon, uid, gid, username, groupname);
  bool warn = false;

  if (gid && !colon && error_msg)
    {
       

      char const *dot = strchr (spec, '.');
      if (dot
          && ! parse_with_separator (spec, dot, uid, gid, username, groupname))
        {
          warn = true;
          error_msg = pwarn ? N_("warning: '.' should be ':'") : NULL;
        }
    }

  if (pwarn)
    *pwarn = warn;
  return error_msg;
}

 

char const *
parse_user_spec (char const *spec, uid_t *uid, gid_t *gid,
                 char **username, char **groupname)
{
  return parse_user_spec_warn (spec, uid, gid, username, groupname, NULL);
}

#ifdef TEST

# define NULL_CHECK(s) ((s) == NULL ? "(null)" : (s))

int
main (int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++)
    {
      const char *e;
      char *username, *groupname;
      uid_t uid;
      gid_t gid;
      char *tmp;

      tmp = strdup (argv[i]);
      e = parse_user_spec (tmp, &uid, &gid, &username, &groupname);
      free (tmp);
      printf ("%s: %lu %lu %s %s %s\n",
              argv[i],
              (unsigned long int) uid,
              (unsigned long int) gid,
              NULL_CHECK (username),
              NULL_CHECK (groupname),
              NULL_CHECK (e));
    }

  exit (0);
}

#endif

 
