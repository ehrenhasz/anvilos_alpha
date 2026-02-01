 

 

#include "config.h"

#include <stdio.h>
#include "chartypes.h"
#include "bashtypes.h"
#if !defined (_MINIX) && defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif
#include "filecntl.h"
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif
#include <errno.h>

#include "bashansi.h"

#include "memalloc.h"
#include "shell.h"
#include "execute_cmd.h"
#include "flags.h"
#include "hashlib.h"
#include "pathexp.h"
#include "hashcmd.h"
#include "findcmd.h"	 

#include <glob/strmatch.h>

#if !defined (errno)
extern int errno;
#endif

 
static char *_find_user_command_internal PARAMS((const char *, int));
static char *find_user_command_internal PARAMS((const char *, int));
static char *find_user_command_in_path PARAMS((const char *, char *, int, int *));
static char *find_in_path_element PARAMS((const char *, char *, int, int, struct stat *, int *));
static char *find_absolute_program PARAMS((const char *, int));

static char *get_next_path_element PARAMS((char *, int *));

 
static char *file_to_lose_on;

 
int check_hashed_filenames = CHECKHASH_DEFAULT;

 
int dot_found_in_search = 0;

 
static struct ignorevar execignore =
{
  "EXECIGNORE",
  NULL,
  0,
  NULL,
  NULL
};

void
setup_exec_ignore (varname)
     char *varname;
{
  setup_ignore_patterns (&execignore);
}

static int
exec_name_should_ignore (name)
     const char *name;
{
  struct ign *p;

  for (p = execignore.ignores; p && p->val; p++)
    if (strmatch (p->val, (char *)name, FNMATCH_EXTFLAG|FNM_CASEFOLD) != FNM_NOMATCH)
      return 1;
  return 0;
}

 
int
file_status (name)
     const char *name;
{
  struct stat finfo;
  int r;

   
  if (stat (name, &finfo) < 0)
    return (0);

   
  if (S_ISDIR (finfo.st_mode))
    return (FS_EXISTS|FS_DIRECTORY);

  r = FS_EXISTS;

#if defined (HAVE_EACCESS)
   
  if (exec_name_should_ignore (name) == 0 && eaccess (name, X_OK) == 0)
    r |= FS_EXECABLE;
  if (eaccess (name, R_OK) == 0)
    r |= FS_READABLE;

  return r;
#elif defined (AFS)
   
  if (exec_name_should_ignore (name) == 0 && access (name, X_OK) == 0)
    r |= FS_EXECABLE;
  if (access (name, R_OK) == 0)
    r |= FS_READABLE;

  return r;
#else  

   

   
  if (current_user.euid == (uid_t)0)
    {
      r |= FS_READABLE;
      if (exec_name_should_ignore (name) == 0 && (finfo.st_mode & S_IXUGO))
	r |= FS_EXECABLE;
      return r;
    }

   
  if (current_user.euid == finfo.st_uid)
    {
      if (exec_name_should_ignore (name) == 0 && (finfo.st_mode & S_IXUSR))
	r |= FS_EXECABLE;
      if (finfo.st_mode & S_IRUSR)
	r |= FS_READABLE;
    }

   
  else if (group_member (finfo.st_gid))
    {
      if (exec_name_should_ignore (name) == 0 && (finfo.st_mode & S_IXGRP))
	r |= FS_EXECABLE;
      if (finfo.st_mode & S_IRGRP)
	r |= FS_READABLE;
    }

   
  else
    {
      if (exec_name_should_ignore (name) == 0 && finfo.st_mode & S_IXOTH)
	r |= FS_EXECABLE;
      if (finfo.st_mode & S_IROTH)
	r |= FS_READABLE;
    }

  return r;
#endif  
}

 
int
executable_file (file)
     const char *file;
{
  int s;

  s = file_status (file);
#if defined (EISDIR)
  if (s & FS_DIRECTORY)
    errno = EISDIR;	 
#endif
  return ((s & FS_EXECABLE) && ((s & FS_DIRECTORY) == 0));
}

int
is_directory (file)
     const char *file;
{
  return (file_status (file) & FS_DIRECTORY);
}

int
executable_or_directory (file)
     const char *file;
{
  int s;

  s = file_status (file);
  return ((s & FS_EXECABLE) || (s & FS_DIRECTORY));
}

 
char *
find_user_command (name)
     const char *name;
{
  return (find_user_command_internal (name, FS_EXEC_PREFERRED|FS_NODIRS));
}

 
char *
find_path_file (name)
     const char *name;
{
  return (find_user_command_internal (name, FS_READABLE));
}

static char *
_find_user_command_internal (name, flags)
     const char *name;
     int flags;
{
  char *path_list, *cmd;
  SHELL_VAR *var;

   
  if (var = find_variable_tempenv ("PATH"))	 
    path_list = value_cell (var);
  else
    path_list = (char *)NULL;

  if (path_list == 0 || *path_list == '\0')
    return (savestring (name));

  cmd = find_user_command_in_path (name, path_list, flags, (int *)0);

  return (cmd);
}

static char *
find_user_command_internal (name, flags)
     const char *name;
     int flags;
{
#ifdef __WIN32__
  char *res, *dotexe;

  dotexe = (char *)xmalloc (strlen (name) + 5);
  strcpy (dotexe, name);
  strcat (dotexe, ".exe");
  res = _find_user_command_internal (dotexe, flags);
  free (dotexe);
  if (res == 0)
    res = _find_user_command_internal (name, flags);
  return res;
#else
  return (_find_user_command_internal (name, flags));
#endif
}

 
static char *
get_next_path_element (path_list, path_index_pointer)
     char *path_list;
     int *path_index_pointer;
{
  char *path;

  path = extract_colon_unit (path_list, path_index_pointer);

  if (path == 0)
    return (path);

  if (*path == '\0')
    {
      free (path);
      path = savestring (".");
    }

  return (path);
}

 
char *
search_for_command (pathname, flags)
     const char *pathname;
     int flags;
{
  char *hashed_file, *command, *path_list;
  int temp_path, st;
  SHELL_VAR *path;

  hashed_file = command = (char *)NULL;

   
  path = find_variable_tempenv ("PATH");
  temp_path = path && tempvar_p (path);

   
  if (temp_path == 0 && (flags & CMDSRCH_STDPATH) == 0 && absolute_program (pathname) == 0)
    hashed_file = phash_search (pathname);

   

  if (hashed_file && (posixly_correct || check_hashed_filenames))
    {
      st = file_status (hashed_file);
      if ((st & (FS_EXISTS|FS_EXECABLE)) != (FS_EXISTS|FS_EXECABLE))
	{
	  phash_remove (pathname);
	  free (hashed_file);
	  hashed_file = (char *)NULL;
	}
    }

  if (hashed_file)
    command = hashed_file;
  else if (absolute_program (pathname))
     
    command = savestring (pathname);
  else
    {
      if (flags & CMDSRCH_STDPATH)
	path_list = conf_standard_path ();
      else if (temp_path || path)
	path_list = value_cell (path);
      else
	path_list = 0;

      command = find_user_command_in_path (pathname, path_list, FS_EXEC_PREFERRED|FS_NODIRS, &st);

      if (command && hashing_enabled && temp_path == 0 && (flags & CMDSRCH_HASH))
	{
	   
	  if (STREQ (command, pathname))
	    {
	      if (st & FS_EXECABLE)
	        phash_insert ((char *)pathname, command, dot_found_in_search, 1);
	    }
	   
	  else if (posixly_correct || check_hashed_filenames)
	    {
	      if (st & FS_EXECABLE)
	        phash_insert ((char *)pathname, command, dot_found_in_search, 1);
	    }
	  else
	    phash_insert ((char *)pathname, command, dot_found_in_search, 1);
	}

      if (flags & CMDSRCH_STDPATH)
	free (path_list);
    }

  return (command);
}

char *
user_command_matches (name, flags, state)
     const char *name;
     int flags, state;
{
  register int i;
  int  path_index, name_len;
  char *path_list, *path_element, *match;
  struct stat dotinfo;
  static char **match_list = NULL;
  static int match_list_size = 0;
  static int match_index = 0;

  if (state == 0)
    {
       
      if (match_list == 0)
	{
	  match_list_size = 5;
	  match_list = strvec_create (match_list_size);
	}

       
      for (i = 0; i < match_list_size; i++)
	match_list[i] = 0;

       
      match_index = 0;

      if (absolute_program (name))
	{
	  match_list[0] = find_absolute_program (name, flags);
	  match_list[1] = (char *)NULL;
	  path_list = (char *)NULL;
	}
      else
	{
	  name_len = strlen (name);
	  file_to_lose_on = (char *)NULL;
	  dot_found_in_search = 0;
	  if (stat (".", &dotinfo) < 0)
	    dotinfo.st_dev = dotinfo.st_ino = 0;	 
	  path_list = get_string_value ("PATH");
      	  path_index = 0;
	}

      while (path_list && path_list[path_index])
	{
	  path_element = get_next_path_element (path_list, &path_index);

	  if (path_element == 0)
	    break;

	  match = find_in_path_element (name, path_element, flags, name_len, &dotinfo, (int *)0);
	  free (path_element);

	  if (match == 0)
	    continue;

	  if (match_index + 1 == match_list_size)
	    {
	      match_list_size += 10;
	      match_list = strvec_resize (match_list, (match_list_size + 1));
	    }

	  match_list[match_index++] = match;
	  match_list[match_index] = (char *)NULL;
	  FREE (file_to_lose_on);
	  file_to_lose_on = (char *)NULL;
	}

       
      match_index = 0;
    }

  match = match_list[match_index];

  if (match)
    match_index++;

  return (match);
}

static char *
find_absolute_program (name, flags)
     const char *name;
     int flags;
{
  int st;

  st = file_status (name);

   
  if ((st & FS_EXISTS) == 0)
    return ((char *)NULL);

   
  if ((flags & FS_EXISTS) || ((flags & FS_EXEC_ONLY) && (st & FS_EXECABLE)))
    return (savestring (name));

  return (NULL);
}

static char *
find_in_path_element (name, path, flags, name_len, dotinfop, rflagsp)
     const char *name;
     char *path;
     int flags, name_len;
     struct stat *dotinfop;
     int *rflagsp;
{
  int status;
  char *full_path, *xpath;

  xpath = (posixly_correct == 0 && *path == '~') ? bash_tilde_expand (path, 0) : path;

   
   
  if (dot_found_in_search == 0 && *xpath == '.')
    dot_found_in_search = same_file (".", xpath, dotinfop, (struct stat *)NULL);

  full_path = sh_makepath (xpath, name, 0);

  status = file_status (full_path);

  if (xpath != path)
    free (xpath);

  if (rflagsp)
    *rflagsp = status;

  if ((status & FS_EXISTS) == 0)
    {
      free (full_path);
      return ((char *)NULL);
    }

   
  if (flags & FS_EXISTS)
    return (full_path);

   
  if ((flags & FS_READABLE) && (status & FS_READABLE))
    return (full_path);

   
  if ((status & FS_EXECABLE) && (flags & (FS_EXEC_ONLY|FS_EXEC_PREFERRED)) &&
      (((flags & FS_NODIRS) == 0) || ((status & FS_DIRECTORY) == 0)))
    {
      FREE (file_to_lose_on);
      file_to_lose_on = (char *)NULL;
      return (full_path);
    }

   
  if ((flags & FS_EXEC_PREFERRED) && file_to_lose_on == 0 && exec_name_should_ignore (full_path) == 0)
    file_to_lose_on = savestring (full_path);

   
  if ((flags & (FS_EXEC_ONLY|FS_EXEC_PREFERRED)) ||
      ((flags & FS_NODIRS) && (status & FS_DIRECTORY)) ||
      ((flags & FS_READABLE) && (status & FS_READABLE) == 0))
    {
      free (full_path);
      return ((char *)NULL);
    }
  else
    return (full_path);
}

 
static char *
find_user_command_in_path (name, path_list, flags, rflagsp)
     const char *name;
     char *path_list;
     int flags, *rflagsp;
{
  char *full_path, *path;
  int path_index, name_len, rflags;
  struct stat dotinfo;

   
  dot_found_in_search = 0;

  if (rflagsp)
    *rflagsp = 0;

  if (absolute_program (name))
    {
      full_path = find_absolute_program (name, flags);
      return (full_path);
    }

  if (path_list == 0 || *path_list == '\0')
    return (savestring (name));		 

  file_to_lose_on = (char *)NULL;
  name_len = strlen (name);
  if (stat (".", &dotinfo) < 0)
    dotinfo.st_dev = dotinfo.st_ino = 0;
  path_index = 0;

  while (path_list[path_index])
    {
       
      QUIT;

      path = get_next_path_element (path_list, &path_index);
      if (path == 0)
	break;

       
      full_path = find_in_path_element (name, path, flags, name_len, &dotinfo, &rflags);
      free (path);

       
      if (full_path && (rflags & FS_DIRECTORY))
	{
	  free (full_path);
	  continue;
	}

      if (full_path)
	{
	  if (rflagsp)
	    *rflagsp = rflags;
	  FREE (file_to_lose_on);
	  return (full_path);
	}
    }

   
  if (file_to_lose_on && (flags & FS_NODIRS) && file_isdir (file_to_lose_on))
    {
      free (file_to_lose_on);
      file_to_lose_on = (char *)NULL;
    }

  return (file_to_lose_on);
}

 
char *
find_in_path (name, path_list, flags)
     const char *name;
     char *path_list;
     int flags;
{
  return (find_user_command_in_path (name, path_list, flags, (int *)0));
}
