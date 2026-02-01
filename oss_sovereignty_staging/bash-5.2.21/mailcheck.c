 

 

#include "config.h"

#include <stdio.h>
#include "bashtypes.h"
#include "posixstat.h"
#if defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif
#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif
#include "posixtime.h"
#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "execute_cmd.h"
#include "mailcheck.h"
#include <tilde/tilde.h>

 
#define MBOX_INITIALIZED	0x01

extern time_t shell_start_time;

extern int mailstat PARAMS((const char *, struct stat *));

typedef struct _fileinfo {
  char *name;
  char *msg;
  time_t access_time;
  time_t mod_time;
  off_t file_size;
  int flags;
} FILEINFO;

 
static FILEINFO **mailfiles = (FILEINFO **)NULL;

 
static int mailfiles_count;

 
static time_t last_time_mail_checked = 0;

 
int mail_warning;

static int find_mail_file PARAMS((char *));
static void init_mail_file PARAMS((int));
static void update_mail_file PARAMS((int));
static int add_mail_file PARAMS((char *, char *));

static FILEINFO *alloc_mail_file PARAMS((char *, char *));
static void dispose_mail_file PARAMS((FILEINFO *));

static int file_mod_date_changed PARAMS((int));
static int file_access_date_changed PARAMS((int));
static int file_has_grown PARAMS((int));

static char *parse_mailpath_spec PARAMS((char *));

 
int
time_to_check_mail ()
{
  char *temp;
  time_t now;
  intmax_t seconds;

  temp = get_string_value ("MAILCHECK");

   
  if (temp == 0 || legal_number (temp, &seconds) == 0 || seconds < 0)
    return (0);

  now = NOW;
   
  return (seconds == 0 || ((now - last_time_mail_checked) >= seconds));
}

 
void
reset_mail_timer ()
{
  last_time_mail_checked = NOW;
}

 
static int
find_mail_file (file)
     char *file;
{
  register int i;

  for (i = 0; i < mailfiles_count; i++)
    if (STREQ (mailfiles[i]->name, file))
      return i;

  return -1;
}

#define RESET_MAIL_FILE(i) \
  do \
    { \
      mailfiles[i]->access_time = mailfiles[i]->mod_time = 0; \
      mailfiles[i]->file_size = 0; \
      mailfiles[i]->flags = 0; \
    } \
  while (0)

#define UPDATE_MAIL_FILE(i, finfo) \
  do \
    { \
      mailfiles[i]->access_time = finfo.st_atime; \
      mailfiles[i]->mod_time = finfo.st_mtime; \
      mailfiles[i]->file_size = finfo.st_size; \
      mailfiles[i]->flags |= MBOX_INITIALIZED; \
    } \
  while (0)

static void
init_mail_file (i)
     int i;
{
  mailfiles[i]->access_time = mailfiles[i]->mod_time = last_time_mail_checked ? last_time_mail_checked : shell_start_time;
  mailfiles[i]->file_size = 0;
  mailfiles[i]->flags = 0;
}

static void
update_mail_file (i)
     int i;
{
  char *file;
  struct stat finfo;

  file = mailfiles[i]->name;
  if (mailstat (file, &finfo) == 0)
    UPDATE_MAIL_FILE (i, finfo);
  else
    RESET_MAIL_FILE (i);
}

 
static int
add_mail_file (file, msg)
     char *file, *msg;
{
  struct stat finfo;
  char *filename;
  int i;

  filename = full_pathname (file);
  i = find_mail_file (filename);
  if (i >= 0)
    {
      if (mailstat (filename, &finfo) == 0)
	UPDATE_MAIL_FILE (i, finfo);

      free (filename);
      return i;
    }

  i = mailfiles_count++;
  mailfiles = (FILEINFO **)xrealloc
		(mailfiles, mailfiles_count * sizeof (FILEINFO *));

  mailfiles[i] = alloc_mail_file (filename, msg);
  init_mail_file (i);

  return i;
}

 
void
reset_mail_files ()
{
  register int i;

  for (i = 0; i < mailfiles_count; i++)
    RESET_MAIL_FILE (i);
}

static FILEINFO *
alloc_mail_file (filename, msg)
     char *filename, *msg;
{
  FILEINFO *mf;

  mf = (FILEINFO *)xmalloc (sizeof (FILEINFO));
  mf->name = filename;
  mf->msg = msg ? savestring (msg) : (char *)NULL;
  mf->flags = 0;

  return mf;
}

static void
dispose_mail_file (mf)
     FILEINFO *mf;
{
  free (mf->name);
  FREE (mf->msg);
  free (mf);
}

 
void
free_mail_files ()
{
  register int i;

  for (i = 0; i < mailfiles_count; i++)
    dispose_mail_file (mailfiles[i]);

  if (mailfiles)
    free (mailfiles);

  mailfiles_count = 0;
  mailfiles = (FILEINFO **)NULL;
}

void
init_mail_dates ()
{
  if (mailfiles == 0)
    remember_mail_dates ();
}

 
static int
file_mod_date_changed (i)
     int i;
{
  time_t mtime;
  struct stat finfo;
  char *file;

  file = mailfiles[i]->name;
  mtime = mailfiles[i]->mod_time;

  if (mailstat (file, &finfo) != 0)
    return (0);

  if (finfo.st_size > 0)
    return (mtime < finfo.st_mtime);

  if (finfo.st_size == 0 && mailfiles[i]->file_size > 0)
    UPDATE_MAIL_FILE (i, finfo);

  return (0);
}

 
static int
file_access_date_changed (i)
     int i;
{
  time_t atime;
  struct stat finfo;
  char *file;

  file = mailfiles[i]->name;
  atime = mailfiles[i]->access_time;

  if (mailstat (file, &finfo) != 0)
    return (0);

  if (finfo.st_size > 0)
    return (atime < finfo.st_atime);

  return (0);
}

 
static int
file_has_grown (i)
     int i;
{
  off_t size;
  struct stat finfo;
  char *file;

  file = mailfiles[i]->name;
  size = mailfiles[i]->file_size;

  return ((mailstat (file, &finfo) == 0) && (finfo.st_size > size));
}

 
static char *
parse_mailpath_spec (str)
     char *str;
{
  char *s;
  int pass_next;

  for (s = str, pass_next = 0; s && *s; s++)
    {
      if (pass_next)
	{
	  pass_next = 0;
	  continue;
	}
      if (*s == '\\')
	{
	  pass_next++;
	  continue;
	}
      if (*s == '?' || *s == '%')
	return s;
    }
  return ((char *)NULL);
}

char *
make_default_mailpath ()
{
#if defined (DEFAULT_MAIL_DIRECTORY)
  char *mp;

  get_current_user_info ();
  mp = (char *)xmalloc (2 + sizeof (DEFAULT_MAIL_DIRECTORY) + strlen (current_user.user_name));
  strcpy (mp, DEFAULT_MAIL_DIRECTORY);
  mp[sizeof(DEFAULT_MAIL_DIRECTORY) - 1] = '/';
  strcpy (mp + sizeof (DEFAULT_MAIL_DIRECTORY), current_user.user_name);
  return (mp);
#else
  return ((char *)NULL);
#endif
}

 

void
remember_mail_dates ()
{
  char *mailpaths;
  char *mailfile, *mp;
  int i = 0;

  mailpaths = get_string_value ("MAILPATH");

   
  if (mailpaths == 0 && (mailpaths = get_string_value ("MAIL")))
    {
      add_mail_file (mailpaths, (char *)NULL);
      return;
    }

  if (mailpaths == 0)
    {
      mailpaths = make_default_mailpath ();
      if (mailpaths)
	{
	  add_mail_file (mailpaths, (char *)NULL);
	  free (mailpaths);
	}
      return;
    }

  while (mailfile = extract_colon_unit (mailpaths, &i))
    {
      mp = parse_mailpath_spec (mailfile);
      if (mp && *mp)
	*mp++ = '\0';
      add_mail_file (mailfile, mp);
      free (mailfile);
    }
}

 

 
void
check_mail ()
{
  char *current_mail_file, *message;
  int i, use_user_notification;
  char *dollar_underscore, *temp;

  dollar_underscore = get_string_value ("_");
  if (dollar_underscore)
    dollar_underscore = savestring (dollar_underscore);

  for (i = 0; i < mailfiles_count; i++)
    {
      current_mail_file = mailfiles[i]->name;

      if (*current_mail_file == '\0')
	continue;

      if (file_mod_date_changed (i))
	{
	  int file_is_bigger;

	  use_user_notification = mailfiles[i]->msg != (char *)NULL;
	  message = mailfiles[i]->msg ? mailfiles[i]->msg : _("You have mail in $_");

	  bind_variable ("_", current_mail_file, 0);

#define atime mailfiles[i]->access_time
#define mtime mailfiles[i]->mod_time

	   
	  file_is_bigger = file_has_grown (i);

	  update_mail_file (i);

	   
	  if ((atime >= mtime) && !file_is_bigger)
	    continue;

	   
	  if (use_user_notification == 0 && (atime < mtime) && file_is_bigger)
	    message = _("You have new mail in $_");
#undef atime
#undef mtime

	  if (temp = expand_string_to_string (message, Q_DOUBLE_QUOTES))
	    {
	      puts (temp);
	      free (temp);
	    }
	  else
	    putchar ('\n');
	}

      if (mail_warning && file_access_date_changed (i))
	{
	  update_mail_file (i);
	  printf (_("The mail in %s has been read\n"), current_mail_file);
	}
    }

  if (dollar_underscore)
    {
      bind_variable ("_", dollar_underscore, 0);
      free (dollar_underscore);
    }
  else
    unbind_variable ("_");
}
