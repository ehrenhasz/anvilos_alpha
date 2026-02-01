 

 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include "rlconf.h"

#if defined __TANDEM
#  define _XOPEN_SOURCE_EXTENDED 1
#  define _TANDEM_SOURCE 1
#  include <sys/types.h>
#  include <sys/stat.h>
#endif

#include <stdio.h>

#include "posixstat.h"  
#include <fcntl.h>  

#ifndef S_ISDIR
#  define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif

 
#if defined (HAVE_STRING_H)
#  include <string.h>
#else  
#  include <strings.h>
#endif  

 
#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include "readline.h"
#include "rldefs.h"

#ifdef COLOR_SUPPORT

#include "xmalloc.h"
#include "colors.h"

static bool is_colored (enum indicator_no type);
static void restore_default_color (void);

#define RL_COLOR_PREFIX_EXTENSION	"readline-colored-completion-prefix"

COLOR_EXT_TYPE *_rl_color_ext_list = 0;

 
void
_rl_put_indicator (const struct bin_str *ind)
{
  fwrite (ind->string, ind->len, 1, rl_outstream);
}

static bool
is_colored (enum indicator_no colored_filetype)
{
  size_t len = _rl_color_indicator[colored_filetype].len;
  char const *s = _rl_color_indicator[colored_filetype].string;
  return ! (len == 0
            || (len == 1 && strncmp (s, "0", 1) == 0)
            || (len == 2 && strncmp (s, "00", 2) == 0));
}

static void
restore_default_color (void)
{
  _rl_put_indicator (&_rl_color_indicator[C_LEFT]);
  _rl_put_indicator (&_rl_color_indicator[C_RIGHT]);
}

void
_rl_set_normal_color (void)
{
  if (is_colored (C_NORM))
    {
      _rl_put_indicator (&_rl_color_indicator[C_LEFT]);
      _rl_put_indicator (&_rl_color_indicator[C_NORM]);
      _rl_put_indicator (&_rl_color_indicator[C_RIGHT]);
    }
}

static struct bin_str *
_rl_custom_readline_prefix (void)
{
  size_t len;
  COLOR_EXT_TYPE *ext;

  len = strlen (RL_COLOR_PREFIX_EXTENSION);
  for (ext = _rl_color_ext_list; ext; ext = ext->next)
    if (ext->ext.len == len && STREQN (ext->ext.string, RL_COLOR_PREFIX_EXTENSION, len))
      return (&ext->seq);
  return (NULL);
}

bool
_rl_print_prefix_color (void)
{
  struct bin_str *s;

   
  s = _rl_custom_readline_prefix ();
  if (s == 0)
    s = &_rl_color_indicator[C_PREFIX];
  if (s->string != NULL)
    {
      if (is_colored (C_NORM))
	restore_default_color ();
      _rl_put_indicator (&_rl_color_indicator[C_LEFT]);
      _rl_put_indicator (s);
      _rl_put_indicator (&_rl_color_indicator[C_RIGHT]);
      return 0;
    }
  else
    return 1;
}
  
 
bool
_rl_print_color_indicator (const char *f)
{
  enum indicator_no colored_filetype;
  COLOR_EXT_TYPE *ext;	 
  size_t len;		 

  const char* name;
  char *filename;
  struct stat astat, linkstat;
  mode_t mode;
  int linkok;	 
  int stat_ok;

  name = f;

   
  filename = 0;
  if (rl_filename_stat_hook)
    {
      filename = savestring (f);
      (*rl_filename_stat_hook) (&filename);
      name = filename;
    }

#if defined (HAVE_LSTAT)
  stat_ok = lstat(name, &astat);
#else
  stat_ok = stat(name, &astat);
#endif
  if (stat_ok == 0)
    {
      mode = astat.st_mode;
#if defined (HAVE_LSTAT)
      if (S_ISLNK (mode))
	{
	  linkok = stat (name, &linkstat) == 0;
	  if (linkok && strncmp (_rl_color_indicator[C_LINK].string, "target", 6) == 0)
	    mode = linkstat.st_mode;
	}
      else
#endif
	linkok = 1;
    }
  else
    linkok = -1;

   

  if (linkok == -1 && _rl_color_indicator[C_MISSING].string != NULL)
    colored_filetype = C_MISSING;
  else if (linkok == 0 && _rl_color_indicator[C_ORPHAN].string != NULL)
    colored_filetype = C_ORPHAN;	 
  else if(stat_ok != 0)
    {
      static enum indicator_no filetype_indicator[] = FILETYPE_INDICATORS;
      colored_filetype = filetype_indicator[normal]; 
    }
  else
    {
      if (S_ISREG (mode))
        {
          colored_filetype = C_FILE;

#if defined (S_ISUID)
          if ((mode & S_ISUID) != 0 && is_colored (C_SETUID))
            colored_filetype = C_SETUID;
          else
#endif
#if defined (S_ISGID)
          if ((mode & S_ISGID) != 0 && is_colored (C_SETGID))
            colored_filetype = C_SETGID;
          else
#endif
          if (is_colored (C_CAP) && 0) 
            colored_filetype = C_CAP;
          else if ((mode & S_IXUGO) != 0 && is_colored (C_EXEC))
            colored_filetype = C_EXEC;
          else if ((1 < astat.st_nlink) && is_colored (C_MULTIHARDLINK))
            colored_filetype = C_MULTIHARDLINK;
        }
      else if (S_ISDIR (mode))
        {
          colored_filetype = C_DIR;

#if defined (S_ISVTX)
          if ((mode & S_ISVTX) && (mode & S_IWOTH)
              && is_colored (C_STICKY_OTHER_WRITABLE))
            colored_filetype = C_STICKY_OTHER_WRITABLE;
          else
#endif
          if ((mode & S_IWOTH) != 0 && is_colored (C_OTHER_WRITABLE))
            colored_filetype = C_OTHER_WRITABLE;
#if defined (S_ISVTX)
          else if ((mode & S_ISVTX) != 0 && is_colored (C_STICKY))
            colored_filetype = C_STICKY;
#endif
        }
#if defined (S_ISLNK)
      else if (S_ISLNK (mode))
        colored_filetype = C_LINK;
#endif
      else if (S_ISFIFO (mode))
        colored_filetype = C_FIFO;
#if defined (S_ISSOCK)
      else if (S_ISSOCK (mode))
        colored_filetype = C_SOCK;
#endif
#if defined (S_ISBLK)
      else if (S_ISBLK (mode))
        colored_filetype = C_BLK;
#endif
      else if (S_ISCHR (mode))
        colored_filetype = C_CHR;
      else
        {
           
          colored_filetype = C_ORPHAN;
        }
    }

   
  ext = NULL;
  if (colored_filetype == C_FILE)
    {
       
      len = strlen (name);
      name += len;		 
      for (ext = _rl_color_ext_list; ext != NULL; ext = ext->next)
        {
          if (ext->ext.len <= len
              && strncmp (name - ext->ext.len, ext->ext.string,
                          ext->ext.len) == 0)
            break;
        }
    }

  free (filename);	 

  {
    const struct bin_str *const s
      = ext ? &(ext->seq) : &_rl_color_indicator[colored_filetype];
    if (s->string != NULL)
      {
         
        if (is_colored (C_NORM))
	  restore_default_color ();
        _rl_put_indicator (&_rl_color_indicator[C_LEFT]);
        _rl_put_indicator (s);
        _rl_put_indicator (&_rl_color_indicator[C_RIGHT]);
        return 0;
      }
    else
      return 1;
  }
}

void
_rl_prep_non_filename_text (void)
{
  if (_rl_color_indicator[C_END].string != NULL)
    _rl_put_indicator (&_rl_color_indicator[C_END]);
  else
    {
      _rl_put_indicator (&_rl_color_indicator[C_LEFT]);
      _rl_put_indicator (&_rl_color_indicator[C_RESET]);
      _rl_put_indicator (&_rl_color_indicator[C_RIGHT]);
    }
}
#endif  
