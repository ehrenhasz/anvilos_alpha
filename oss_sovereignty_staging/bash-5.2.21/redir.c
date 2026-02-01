 

 

#include "config.h"

#if !defined (__GNUC__) && !defined (HAVE_ALLOCA_H) && defined (_AIX)
  #pragma alloca
#endif  

#include <stdio.h>
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

#if !defined (errno)
extern int errno;
#endif

#include "bashansi.h"
#include "bashintl.h"
#include "memalloc.h"

#define NEED_FPURGE_DECL

#include "shell.h"
#include "flags.h"
#include "execute_cmd.h"
#include "redir.h"
#include "trap.h"

#if defined (BUFFERED_INPUT)
#  include "input.h"
#endif

#include "builtins/pipesize.h"

 
#if __FreeBSD__ && !defined (HEREDOC_PIPESIZE)
#  define HEREDOC_PIPESIZE 4096
#endif

 
#ifndef PIPESIZE
#  ifdef PIPE_BUF
#    define PIPESIZE PIPE_BUF
#  else
#    define PIPESIZE 4096
#  endif
#endif

#ifndef HEREDOC_PIPESIZE
#  define HEREDOC_PIPESIZE PIPESIZE
#endif

#if defined (HEREDOC_PIPEMAX)
#  if HEREDOC_PIPESIZE > HEREDOC_PIPEMAX
#    define HEREDOC_PIPESIZE HEREDOC_PIPEMAX
#  endif
#endif

#define SHELL_FD_BASE	10

int expanding_redir;
int varassign_redir_autoclose = 0;

extern REDIRECT *redirection_undo_list;
extern REDIRECT *exec_redirection_undo_list;

 
static void add_exec_redirect PARAMS((REDIRECT *));
static int add_undo_redirect PARAMS((int, enum r_instruction, int));
static int add_undo_close_redirect PARAMS((int));
static int expandable_redirection_filename PARAMS((REDIRECT *));
static int stdin_redirection PARAMS((enum r_instruction, int));
static int undoablefd PARAMS((int));
static int do_redirection_internal PARAMS((REDIRECT *, int, char **));

static char *heredoc_expand PARAMS((WORD_DESC *, enum r_instruction, size_t *));
static int heredoc_write PARAMS((int, char *, size_t));
static int here_document_to_fd PARAMS((WORD_DESC *, enum r_instruction));

static int redir_special_open PARAMS((int, char *, int, int, enum r_instruction));
static int noclobber_open PARAMS((char *, int, int, enum r_instruction));
static int redir_open PARAMS((char *, int, int, enum r_instruction));

static int redir_varassign PARAMS((REDIRECT *, int));
static int redir_varvalue PARAMS((REDIRECT *));

 
static REDIRECTEE rd;

 
static int heredoc_errno;

#define REDIRECTION_ERROR(r, e, fd) \
do { \
  if ((r) < 0) \
    { \
      if (fd >= 0) \
	close (fd); \
      set_exit_status (EXECUTION_FAILURE);\
      return ((e) == 0 ? EINVAL : (e));\
    } \
} while (0)

void
redirection_error (temp, error, fn)
     REDIRECT *temp;
     int error;
     char *fn;		 
{
  char *filename, *allocname;
  int oflags;

  allocname = 0;
  if ((temp->rflags & REDIR_VARASSIGN) && error < 0)
    filename = allocname = savestring (temp->redirector.filename->word);
  else if ((temp->rflags & REDIR_VARASSIGN) == 0 && temp->redirector.dest < 0)
     
    filename = _("file descriptor out of range");
#ifdef EBADF
   
  else if (error != NOCLOBBER_REDIRECT && temp->redirector.dest >= 0 && error == EBADF)
    {
       
      switch (temp->instruction)
        {
        case r_duplicating_input:
        case r_duplicating_output:
        case r_move_input:
        case r_move_output:
	  filename = allocname = itos (temp->redirectee.dest);
	  break;
	case r_duplicating_input_word:
	  if (temp->redirector.dest == 0)	 
	    filename = temp->redirectee.filename->word;	 
	  else
	    filename = allocname = itos (temp->redirector.dest);
	  break;
	case r_duplicating_output_word:
	  if (temp->redirector.dest == 1)	 
	    filename = temp->redirectee.filename->word;	 
	  else
	    filename = allocname = itos (temp->redirector.dest);
	  break;
	default:
	  filename = allocname = itos (temp->redirector.dest);
	  break;
        }
    }
#endif
  else if (fn)
    filename = fn;
  else if (expandable_redirection_filename (temp))
    {
      oflags = temp->redirectee.filename->flags;
      if (posixly_correct && interactive_shell == 0)
	temp->redirectee.filename->flags |= W_NOGLOB;
      temp->redirectee.filename->flags |= W_NOCOMSUB;
      filename = allocname = redirection_expand (temp->redirectee.filename);
      temp->redirectee.filename->flags = oflags;
      if (filename == 0)
	filename = temp->redirectee.filename->word;
    }
  else if (temp->redirectee.dest < 0)
    filename = _("file descriptor out of range");
  else
    filename = allocname = itos (temp->redirectee.dest);

  switch (error)
    {
    case AMBIGUOUS_REDIRECT:
      internal_error (_("%s: ambiguous redirect"), filename);
      break;

    case NOCLOBBER_REDIRECT:
      internal_error (_("%s: cannot overwrite existing file"), filename);
      break;

#if defined (RESTRICTED_SHELL)
    case RESTRICTED_REDIRECT:
      internal_error (_("%s: restricted: cannot redirect output"), filename);
      break;
#endif  

    case HEREDOC_REDIRECT:
      internal_error (_("cannot create temp file for here-document: %s"), strerror (heredoc_errno));
      break;

    case BADVAR_REDIRECT:
      internal_error (_("%s: cannot assign fd to variable"), filename);
      break;

    default:
      internal_error ("%s: %s", filename, strerror (error));
      break;
    }

  FREE (allocname);
}

 
int
do_redirections (list, flags)
     REDIRECT *list;
     int flags;
{
  int error;
  REDIRECT *temp;
  char *fn;

  if (flags & RX_UNDOABLE)
    {
      if (redirection_undo_list)
	{
	  dispose_redirects (redirection_undo_list);
	  redirection_undo_list = (REDIRECT *)NULL;
	}
      if (exec_redirection_undo_list)
	dispose_exec_redirects ();
    }

  for (temp = list; temp; temp = temp->next)
    {
      fn = 0;
      error = do_redirection_internal (temp, flags, &fn);
      if (error)
	{
	  redirection_error (temp, error, fn);
	  FREE (fn);
	  return (error);
	}
      FREE (fn);
    }
  return (0);
}

 
static int
expandable_redirection_filename (redirect)
     REDIRECT *redirect;
{
  switch (redirect->instruction)
    {
    case r_output_direction:
    case r_appending_to:
    case r_input_direction:
    case r_inputa_direction:
    case r_err_and_out:
    case r_append_err_and_out:
    case r_input_output:
    case r_output_force:
    case r_duplicating_input_word:
    case r_duplicating_output_word:
    case r_move_input_word:
    case r_move_output_word:
      return 1;

    default:
      return 0;
    }
}

 
char *
redirection_expand (word)
     WORD_DESC *word;
{
  char *result;
  WORD_LIST *tlist1, *tlist2;
  WORD_DESC *w;
  int old;

  w = copy_word (word);
  if (posixly_correct)
    w->flags |= W_NOSPLIT;

  tlist1 = make_word_list (w, (WORD_LIST *)NULL);
  expanding_redir = 1;
   
  sv_ifs ("IFS");
  tlist2 = expand_words_no_vars (tlist1);
  expanding_redir = 0;
   
  old = executing_builtin;
  executing_builtin = 1;
  sv_ifs ("IFS");
  executing_builtin = old;
  dispose_words (tlist1);

  if (tlist2 == 0 || tlist2->next)
    {
       
      if (tlist2)
	dispose_words (tlist2);
      return ((char *)NULL);
    }
  result = string_list (tlist2);   
  dispose_words (tlist2);
  return (result);
}

 
static char *
heredoc_expand (redirectee, ri, lenp)
     WORD_DESC *redirectee;
     enum r_instruction ri;
     size_t *lenp;
{
  char *document;
  size_t dlen;
  int old;

  if (redirectee->word == 0 || redirectee->word[0] == '\0')
    {
      if (lenp)
        *lenp = 0;
      return (redirectee->word);
    }

   
  if (ri != r_reading_string && (redirectee->flags & W_QUOTED))
    {
      if (lenp)
        *lenp = STRLEN (redirectee->word);
      return (redirectee->word);
    }
  
  expanding_redir = 1;
   
  sv_ifs ("IFS");
  document = (ri == r_reading_string) ? expand_assignment_string_to_string (redirectee->word, 0)
  				      : expand_string_to_string (redirectee->word, Q_HERE_DOCUMENT);
  expanding_redir = 0;
   
  old = executing_builtin;
  executing_builtin = 1;
  sv_ifs ("IFS");
  executing_builtin = old;

  dlen = STRLEN (document);
   
  if (ri == r_reading_string)
    {
      document = xrealloc (document, dlen + 2);
      document[dlen++] = '\n';
      document[dlen] = '\0';
    }
  if (lenp)
    *lenp = dlen;    

  return document;
}

 
static int
heredoc_write (fd, heredoc, herelen)
     int fd;
     char *heredoc;
     size_t herelen;
{
  ssize_t nw;
  int e;

  errno = 0;
  nw = write (fd, heredoc, herelen);
  e = errno;
  if (nw != herelen)
    {
      if (e == 0)
	e = ENOSPC;
      return e;
    }
  return 0;
}

 
static int
here_document_to_fd (redirectee, ri)
     WORD_DESC *redirectee;
     enum r_instruction ri;
{
  char *filename;
  int r, fd, fd2, herepipe[2];
  char *document;
  size_t document_len;
#if HEREDOC_PARANOID
  struct stat st1, st2;
#endif

   
  document = heredoc_expand (redirectee, ri, &document_len);

   
  if (document_len == 0)
    {
      fd = open ("/dev/null", O_RDONLY);
      r = errno;
      if (document != redirectee->word)
	FREE (document);
      errno = r;
      return fd;
    }

  if (shell_compatibility_level <= 50)
    goto use_tempfile;

#if HEREDOC_PIPESIZE
   
  if (document_len <= HEREDOC_PIPESIZE)
    {
      if (pipe (herepipe) < 0)
	{
	   
	  r = errno;
	  if (document != redirectee->word)
	    free (document);
	  errno = r;
	  return (-1);
	}

#if defined (F_GETPIPE_SZ)
      if (fcntl (herepipe[1], F_GETPIPE_SZ, 0) < document_len)
	goto use_tempfile;
#endif

      r = heredoc_write (herepipe[1], document, document_len);
      if (document != redirectee->word)
	free (document);
      close (herepipe[1]);
      if (r)			 
	{
	  close (herepipe[0]);
	  errno = r;
	  return (-1);
	}
      return (herepipe[0]);
    }
#endif

use_tempfile:

  fd = sh_mktmpfd ("sh-thd", MT_USERANDOM|MT_USETMPDIR, &filename);

   
  if (fd < 0)
    {
      r = errno;
      FREE (filename);
      if (document != redirectee->word)
	FREE (document);
      errno = r;
      return (fd);
    }

  fchmod (fd, S_IRUSR | S_IWUSR);
  SET_CLOSE_ON_EXEC (fd);

  errno = r = 0;		 
  r = heredoc_write (fd, document, document_len);
  if (document != redirectee->word)
    FREE (document);

  if (r)
    {
      close (fd);
      unlink (filename);
      free (filename);
      errno = r;
      return (-1);
    }

   
   
  fd2 = open (filename, O_RDONLY|O_BINARY, 0600);

  if (fd2 < 0)
    {
      r = errno;
      unlink (filename);
      free (filename);
      close (fd);
      errno = r;
      return -1;
    }

#if HEREDOC_PARANOID
   
  if (fstat (fd, &st1) < 0 || S_ISREG (st1.st_mode) == 0 ||
      fstat (fd2, &st2) < 0 || S_ISREG (st2.st_mode) == 0 ||
      same_file (filename, filename, &st1, &st2) == 0)
    {
      unlink (filename);
      free (filename);
      close (fd);
      close (fd2);
      errno = EEXIST;
      return -1;
    }
#endif

  close (fd);
  if (unlink (filename) < 0)
    {
      r = errno;
      close (fd2);
      free (filename);
      errno = r;
      return (-1);
    }

  free (filename);

  fchmod (fd2, S_IRUSR);
  return (fd2);
}

#define RF_DEVFD	1
#define RF_DEVSTDERR	2
#define RF_DEVSTDIN	3
#define RF_DEVSTDOUT	4
#define RF_DEVTCP	5
#define RF_DEVUDP	6

 
static STRING_INT_ALIST _redir_special_filenames[] = {
#if !defined (HAVE_DEV_FD)
  { "/dev/fd/[0-9]*", RF_DEVFD },
#endif
#if !defined (HAVE_DEV_STDIN)
  { "/dev/stderr", RF_DEVSTDERR },
  { "/dev/stdin", RF_DEVSTDIN },
  { "/dev/stdout", RF_DEVSTDOUT },
#endif
#if defined (NETWORK_REDIRECTIONS)
  { "/dev/tcp/*/*", RF_DEVTCP },
  { "/dev/udp/*/*", RF_DEVUDP },
#endif
  { (char *)NULL, -1 }
};

static int
redir_special_open (spec, filename, flags, mode, ri)
     int spec;
     char *filename;
     int flags, mode;
     enum r_instruction ri;
{
  int fd;
#if !defined (HAVE_DEV_FD)
  intmax_t lfd;
#endif

  fd = -1;
  switch (spec)
    {
#if !defined (HAVE_DEV_FD)
    case RF_DEVFD:
      if (all_digits (filename+8) && legal_number (filename+8, &lfd) && lfd == (int)lfd)
	{
	  fd = lfd;
	  fd = fcntl (fd, F_DUPFD, SHELL_FD_BASE);
	}
      else
	fd = AMBIGUOUS_REDIRECT;
      break;
#endif

#if !defined (HAVE_DEV_STDIN)
    case RF_DEVSTDIN:
      fd = fcntl (0, F_DUPFD, SHELL_FD_BASE);
      break;
    case RF_DEVSTDOUT:
      fd = fcntl (1, F_DUPFD, SHELL_FD_BASE);
      break;
    case RF_DEVSTDERR:
      fd = fcntl (2, F_DUPFD, SHELL_FD_BASE);
      break;
#endif

#if defined (NETWORK_REDIRECTIONS)
    case RF_DEVTCP:
    case RF_DEVUDP:
#if defined (RESTRICTED_SHELL)
      if (restricted)
	return (RESTRICTED_REDIRECT);
#endif
#if defined (HAVE_NETWORK)
      fd = netopen (filename);
#else
      internal_warning (_("/dev/(tcp|udp)/host/port not supported without networking"));
      fd = open (filename, flags, mode);
#endif
      break;
#endif  
    }

  return fd;
}
      
 
static int
noclobber_open (filename, flags, mode, ri)
     char *filename;
     int flags, mode;
     enum r_instruction ri;
{
  int r, fd;
  struct stat finfo, finfo2;

   
  r = stat (filename, &finfo);
  if (r == 0 && (S_ISREG (finfo.st_mode)))
    return (NOCLOBBER_REDIRECT);

   
  flags &= ~O_TRUNC;
  if (r != 0)
    {
      fd = open (filename, flags|O_EXCL, mode);
      return ((fd < 0 && errno == EEXIST) ? NOCLOBBER_REDIRECT : fd);
    }
  fd = open (filename, flags, mode);

   
  if (fd < 0)
    return (errno == EEXIST ? NOCLOBBER_REDIRECT : fd);

   

   
  if ((fstat (fd, &finfo2) == 0) && (S_ISREG (finfo2.st_mode) == 0) &&
      r == 0 && (S_ISREG (finfo.st_mode) == 0) &&
      same_file (filename, filename, &finfo, &finfo2))
    return fd;

   
  close (fd);  
  errno = EEXIST;
  return (NOCLOBBER_REDIRECT);
}

static int
redir_open (filename, flags, mode, ri)
     char *filename;
     int flags, mode;
     enum r_instruction ri;
{
  int fd, r, e;

  r = find_string_in_alist (filename, _redir_special_filenames, 1);
  if (r >= 0)
    return (redir_special_open (r, filename, flags, mode, ri));

   
  if (noclobber && CLOBBERING_REDIRECT (ri))
    {
      fd = noclobber_open (filename, flags, mode, ri);
      if (fd == NOCLOBBER_REDIRECT)
	return (NOCLOBBER_REDIRECT);
    }
  else
    {
      do
	{
	  fd = open (filename, flags, mode);
	  e = errno;
	  if (fd < 0 && e == EINTR)
	    {
	      QUIT;
	      run_pending_traps ();
	    }
	  errno = e;
	}
      while (fd < 0 && errno == EINTR);

#if defined (AFS)
      if ((fd < 0) && (errno == EACCES))
	{
	  fd = open (filename, flags & ~O_CREAT, mode);
	  errno = EACCES;	 
	}
#endif  
    }

  return fd;
}

static int
undoablefd (fd)
     int fd;
{
  int clexec;

  clexec = fcntl (fd, F_GETFD, 0);
  if (clexec == -1 || (fd >= SHELL_FD_BASE && clexec == 1))
    return 0;
  return 1;
}

 
static int
do_redirection_internal (redirect, flags, fnp)
     REDIRECT *redirect;
     int flags;
     char **fnp;
{
  WORD_DESC *redirectee;
  int redir_fd, fd, redirector, r, oflags;
  intmax_t lfd;
  char *redirectee_word;
  enum r_instruction ri;
  REDIRECT *new_redirect;
  REDIRECTEE sd;

  redirectee = redirect->redirectee.filename;
  redir_fd = redirect->redirectee.dest;
  redirector = redirect->redirector.dest;
  ri = redirect->instruction;

  if (redirect->flags & RX_INTERNAL)
    flags |= RX_INTERNAL;

  if (TRANSLATE_REDIRECT (ri))
    {
       
      redirectee_word = redirection_expand (redirectee);

       
      if ((ri == r_move_input_word || ri == r_move_output_word) && redirectee_word == 0)
	{
	  sd = redirect->redirector;
	  rd.dest = 0;
	  new_redirect = make_redirection (sd, r_close_this, rd, 0);
	}
      else if (redirectee_word == 0)
	return (AMBIGUOUS_REDIRECT);
      else if (redirectee_word[0] == '-' && redirectee_word[1] == '\0')
	{
	  sd = redirect->redirector;
	  rd.dest = 0;
	  new_redirect = make_redirection (sd, r_close_this, rd, 0);
	}
      else if (all_digits (redirectee_word))
	{
	  sd = redirect->redirector;
	  if (legal_number (redirectee_word, &lfd) && (int)lfd == lfd)
	    rd.dest = lfd;
	  else
	    rd.dest = -1;	 
	  switch (ri)
	    {
	    case r_duplicating_input_word:
	      new_redirect = make_redirection (sd, r_duplicating_input, rd, 0);
	      break;
	    case r_duplicating_output_word:
	      new_redirect = make_redirection (sd, r_duplicating_output, rd, 0);
	      break;
	    case r_move_input_word:
	      new_redirect = make_redirection (sd, r_move_input, rd, 0);
	      break;
	    case r_move_output_word:
	      new_redirect = make_redirection (sd, r_move_output, rd, 0);
	      break;
	    default:
	      break;	 
	    }
	}
      else if (ri == r_duplicating_output_word && (redirect->rflags & REDIR_VARASSIGN) == 0 && redirector == 1)
	{
	  sd = redirect->redirector;
	  rd.filename = make_bare_word (redirectee_word);
	  new_redirect = make_redirection (sd, r_err_and_out, rd, 0);
	}
      else
	{
	  free (redirectee_word);
	  return (AMBIGUOUS_REDIRECT);
	}

      free (redirectee_word);

       
      if (new_redirect->instruction == r_err_and_out)
	{
	  char *alloca_hack;

	   
	  redirectee = (WORD_DESC *)alloca (sizeof (WORD_DESC));
	  xbcopy ((char *)new_redirect->redirectee.filename,
		 (char *)redirectee, sizeof (WORD_DESC));

	  alloca_hack = (char *)
	    alloca (1 + strlen (new_redirect->redirectee.filename->word));
	  redirectee->word = alloca_hack;
	  strcpy (redirectee->word, new_redirect->redirectee.filename->word);
	}
      else
	 
	redirectee = new_redirect->redirectee.filename;

      redir_fd = new_redirect->redirectee.dest;
      redirector = new_redirect->redirector.dest;
      ri = new_redirect->instruction;

       
      redirect->flags = new_redirect->flags;
      dispose_redirects (new_redirect);
    }

  switch (ri)
    {
    case r_output_direction:
    case r_appending_to:
    case r_input_direction:
    case r_inputa_direction:
    case r_err_and_out:		 
    case r_append_err_and_out:	 
    case r_input_output:
    case r_output_force:
      if (posixly_correct && interactive_shell == 0)
	{
	  oflags = redirectee->flags;
	  redirectee->flags |= W_NOGLOB;
	}
      redirectee_word = redirection_expand (redirectee);
      if (posixly_correct && interactive_shell == 0)
	redirectee->flags = oflags;

      if (redirectee_word == 0)
	return (AMBIGUOUS_REDIRECT);

#if defined (RESTRICTED_SHELL)
      if (restricted && (WRITE_REDIRECT (ri)))
	{
	  free (redirectee_word);
	  return (RESTRICTED_REDIRECT);
	}
#endif  

      fd = redir_open (redirectee_word, redirect->flags, 0666, ri);
      if (fnp)
	*fnp = redirectee_word;
      else
	free (redirectee_word);

      if (fd == NOCLOBBER_REDIRECT || fd == RESTRICTED_REDIRECT)
	return (fd);

      if (fd < 0)
	return (errno);

      if (flags & RX_ACTIVE)
	{
	  if (redirect->rflags & REDIR_VARASSIGN)
	    {
	      redirector = fcntl (fd, F_DUPFD, SHELL_FD_BASE);		 
	      r = errno;
	      if (redirector < 0)
		sys_error (_("redirection error: cannot duplicate fd"));
	      REDIRECTION_ERROR (redirector, r, fd);
	    }

	  if ((flags & RX_UNDOABLE) && ((redirect->rflags & REDIR_VARASSIGN) == 0 || varassign_redir_autoclose))
	    {
	       		 
	      if (fd != redirector && (redirect->rflags & REDIR_VARASSIGN) && varassign_redir_autoclose)
		r = add_undo_close_redirect (redirector);	      
	      else if ((fd != redirector) && (fcntl (redirector, F_GETFD, 0) != -1))
		r = add_undo_redirect (redirector, ri, -1);
	      else
		r = add_undo_close_redirect (redirector);
	      REDIRECTION_ERROR (r, errno, fd);
	    }

#if defined (BUFFERED_INPUT)
	   
	  if (redirector != 0 || (subshell_environment & SUBSHELL_ASYNC) == 0)
	    check_bash_input (redirector);
#endif

	   
	  if (redirector == 1 && fileno (stdout) == redirector)
	    {
	      fflush (stdout);
	      fpurge (stdout);
	    }
	  else if (redirector == 2 && fileno (stderr) == redirector)
	    {
	      fflush (stderr);
	      fpurge (stderr);
	    }

	  if (redirect->rflags & REDIR_VARASSIGN)
	    {
	      if ((r = redir_varassign (redirect, redirector)) < 0)
		{
		  close (redirector);
		  close (fd);
		  return (r);	 
		}
	    }
	  else if ((fd != redirector) && (dup2 (fd, redirector) < 0))
	    {
	      close (fd);	 
	      return (errno);
	    }

#if defined (BUFFERED_INPUT)
	   
	  if (ri == r_input_direction || ri == r_input_output)
	    duplicate_buffered_stream (fd, redirector);
#endif  

	   
	  if ((flags & RX_CLEXEC) && (redirector > 2))
	    SET_CLOSE_ON_EXEC (redirector);
	}

      if (fd != redirector)
	{
#if defined (BUFFERED_INPUT)
	  if (INPUT_REDIRECT (ri))
	    close_buffered_fd (fd);
	  else
#endif  
	    close (fd);		 
	}

       
      if (ri == r_err_and_out || ri == r_append_err_and_out)
	{
	  if (flags & RX_ACTIVE)
	    {
	      if (flags & RX_UNDOABLE)
		add_undo_redirect (2, ri, -1);
	      if (dup2 (1, 2) < 0)
		return (errno);
	    }
	}
      break;

    case r_reading_until:
    case r_deblank_reading_until:
    case r_reading_string:
       
      if (redirectee)
	{
	  fd = here_document_to_fd (redirectee, ri);

	  if (fd < 0)
	    {
	      heredoc_errno = errno;
	      return (HEREDOC_REDIRECT);
	    }

	  if (redirect->rflags & REDIR_VARASSIGN)
	    {
	      redirector = fcntl (fd, F_DUPFD, SHELL_FD_BASE);		 
	      r = errno;
	      if (redirector < 0)
		sys_error (_("redirection error: cannot duplicate fd"));
	      REDIRECTION_ERROR (redirector, r, fd);
	    }

	  if (flags & RX_ACTIVE)
	    {
	      if ((flags & RX_UNDOABLE) && ((redirect->rflags & REDIR_VARASSIGN) == 0 || varassign_redir_autoclose))
	        {
		   
		  if (fd != redirector && (redirect->rflags & REDIR_VARASSIGN) && varassign_redir_autoclose)
		    r = add_undo_close_redirect (redirector);	      
		  else if ((fd != redirector) && (fcntl (redirector, F_GETFD, 0) != -1))
		    r = add_undo_redirect (redirector, ri, -1);
		  else
		    r = add_undo_close_redirect (redirector);
		  REDIRECTION_ERROR (r, errno, fd);
	        }

#if defined (BUFFERED_INPUT)
	      check_bash_input (redirector);
#endif
	      if (redirect->rflags & REDIR_VARASSIGN)
		{
		  if ((r = redir_varassign (redirect, redirector)) < 0)
		    {
		      close (redirector);
		      close (fd);
		      return (r);	 
		    }
		}
	      else if (fd != redirector && dup2 (fd, redirector) < 0)
		{
		  r = errno;
		  close (fd);
		  return (r);
		}

#if defined (BUFFERED_INPUT)
	      duplicate_buffered_stream (fd, redirector);
#endif

	      if ((flags & RX_CLEXEC) && (redirector > 2))
		SET_CLOSE_ON_EXEC (redirector);
	    }

	  if (fd != redirector)
#if defined (BUFFERED_INPUT)
	    close_buffered_fd (fd);
#else
	    close (fd);
#endif
	}
      break;

    case r_duplicating_input:
    case r_duplicating_output:
    case r_move_input:
    case r_move_output:
      if ((flags & RX_ACTIVE) && (redirect->rflags & REDIR_VARASSIGN))
        {
	  redirector = fcntl (redir_fd, F_DUPFD, SHELL_FD_BASE);		 
	  r = errno;
	  if (redirector < 0)
	    sys_error (_("redirection error: cannot duplicate fd"));
	  REDIRECTION_ERROR (redirector, r, -1);
        }

      if ((flags & RX_ACTIVE) && (redir_fd != redirector))
	{
	  if ((flags & RX_UNDOABLE) && ((redirect->rflags & REDIR_VARASSIGN) == 0 || varassign_redir_autoclose))
	    {
	       
	      if ((redirect->rflags & REDIR_VARASSIGN) && varassign_redir_autoclose)
		r = add_undo_close_redirect (redirector);	      
	      else if (fcntl (redirector, F_GETFD, 0) != -1)
		r = add_undo_redirect (redirector, ri, redir_fd);
	      else
		r = add_undo_close_redirect (redirector);
	      REDIRECTION_ERROR (r, errno, -1);
	    }
	  if ((flags & RX_UNDOABLE) && (ri == r_move_input || ri == r_move_output))
	    {
	       
	      if (fcntl (redirector, F_GETFD, 0) != -1)
		{
		  r = add_undo_redirect (redir_fd, r_close_this, -1);
		  REDIRECTION_ERROR (r, errno, -1);
		}
	    }
#if defined (BUFFERED_INPUT)
	   
	  if (redirector != 0 || (subshell_environment & SUBSHELL_ASYNC) == 0)
	    check_bash_input (redirector);
#endif
	  if (redirect->rflags & REDIR_VARASSIGN)
	    {
	      if ((r = redir_varassign (redirect, redirector)) < 0)
		{
		  close (redirector);
		  return (r);	 
		}
	    }
	   
	  else if (dup2 (redir_fd, redirector) < 0)
	    return (errno);

#if defined (BUFFERED_INPUT)
	  if (ri == r_duplicating_input || ri == r_move_input)
	    duplicate_buffered_stream (redir_fd, redirector);
#endif  

	   
	   
#if 0
	  if (((fcntl (redir_fd, F_GETFD, 0) == 1) || redir_fd < 2 || (flags & RX_CLEXEC)) &&
	       (redirector > 2))
#else
	  if (((fcntl (redir_fd, F_GETFD, 0) == 1) || (redir_fd < 2 && (flags & RX_INTERNAL)) || (flags & RX_CLEXEC)) &&
	       (redirector > 2))
#endif
	    SET_CLOSE_ON_EXEC (redirector);

	   
	  if ((redirect->flags & RX_INTERNAL) && (redirect->flags & RX_SAVCLEXEC) && redirector >= 3 && (redir_fd >= SHELL_FD_BASE || (redirect->flags & RX_SAVEFD)))
	    SET_OPEN_ON_EXEC (redirector);
	    
	   
	  if (ri == r_move_input || ri == r_move_output)
	    {
	      xtrace_fdchk (redir_fd);

	      close (redir_fd);
#if defined (COPROCESS_SUPPORT)
	      coproc_fdchk (redir_fd);	 
#endif
	    }
	}
      break;

    case r_close_this:
      if (flags & RX_ACTIVE)
	{
	  if (redirect->rflags & REDIR_VARASSIGN)
	    {
	      redirector = redir_varvalue (redirect);
	      if (redirector < 0)
		return AMBIGUOUS_REDIRECT;
	    }

	  r = 0;
	  if (flags & RX_UNDOABLE)
	    {
	      if (fcntl (redirector, F_GETFD, 0) != -1)
		r = add_undo_redirect (redirector, ri, -1);
	      else
		r = add_undo_close_redirect (redirector);
	      REDIRECTION_ERROR (r, errno, redirector);
	    }

#if defined (COPROCESS_SUPPORT)
	  coproc_fdchk (redirector);
#endif
	  xtrace_fdchk (redirector);

#if defined (BUFFERED_INPUT)
	   
	  if (redirector != 0 || (subshell_environment & SUBSHELL_ASYNC) == 0)
	    check_bash_input (redirector);
	  r = close_buffered_fd (redirector);
#else  
	  r = close (redirector);
#endif  

	  if (r < 0 && (flags & RX_INTERNAL) && (errno == EIO || errno == ENOSPC))
	    REDIRECTION_ERROR (r, errno, -1);
	}
      break;

    case r_duplicating_input_word:
    case r_duplicating_output_word:
    case r_move_input_word:
    case r_move_output_word:
      break;
    }
  return (0);
}

 
static int
add_undo_redirect (fd, ri, fdbase)
     int fd;
     enum r_instruction ri;
     int fdbase;
{
  int new_fd, clexec_flag, savefd_flag;
  REDIRECT *new_redirect, *closer, *dummy_redirect;
  REDIRECTEE sd;

  savefd_flag = 0;
  new_fd = fcntl (fd, F_DUPFD, (fdbase < SHELL_FD_BASE) ? SHELL_FD_BASE : fdbase+1);
  if (new_fd < 0)
    new_fd = fcntl (fd, F_DUPFD, SHELL_FD_BASE);
  if (new_fd < 0)
    {
      new_fd = fcntl (fd, F_DUPFD, 0);
      savefd_flag = 1;
    }

  if (new_fd < 0)
    {
      sys_error (_("redirection error: cannot duplicate fd"));
      return (-1);
    }

  clexec_flag = fcntl (fd, F_GETFD, 0);

  sd.dest = new_fd;
  rd.dest = 0;
  closer = make_redirection (sd, r_close_this, rd, 0);
  closer->flags |= RX_INTERNAL;
  dummy_redirect = copy_redirects (closer);

  sd.dest = fd;
  rd.dest = new_fd;
  if (fd == 0)
    new_redirect = make_redirection (sd, r_duplicating_input, rd, 0);
  else
    new_redirect = make_redirection (sd, r_duplicating_output, rd, 0);
  new_redirect->flags |= RX_INTERNAL;
  if (savefd_flag)
    new_redirect->flags |= RX_SAVEFD;
  if (clexec_flag == 0 && fd >= 3 && (new_fd >= SHELL_FD_BASE || savefd_flag))
    new_redirect->flags |= RX_SAVCLEXEC;
  new_redirect->next = closer;

  closer->next = redirection_undo_list;
  redirection_undo_list = new_redirect;

   
  add_exec_redirect (dummy_redirect);

   
  if (fd >= SHELL_FD_BASE && ri != r_close_this && clexec_flag)
    {
      sd.dest = fd;
      rd.dest = new_fd;
      new_redirect = make_redirection (sd, r_duplicating_output, rd, 0);
      new_redirect->flags |= RX_INTERNAL;

      add_exec_redirect (new_redirect);
    }

   
  if (clexec_flag || fd < 3)
    SET_CLOSE_ON_EXEC (new_fd);
  else if (redirection_undo_list->flags & RX_SAVCLEXEC)
    SET_CLOSE_ON_EXEC (new_fd);

  return (0);
}

 
static int
add_undo_close_redirect (fd)
     int fd;
{
  REDIRECT *closer;
  REDIRECTEE sd;

  sd.dest = fd;
  rd.dest = 0;
  closer = make_redirection (sd, r_close_this, rd, 0);
  closer->flags |= RX_INTERNAL;
  closer->next = redirection_undo_list;
  redirection_undo_list = closer;

  return 0;
}

static void
add_exec_redirect (dummy_redirect)
     REDIRECT *dummy_redirect;
{
  dummy_redirect->next = exec_redirection_undo_list;
  exec_redirection_undo_list = dummy_redirect;
}

 
static int
stdin_redirection (ri, redirector)
     enum r_instruction ri;
     int redirector;
{
  switch (ri)
    {
    case r_input_direction:
    case r_inputa_direction:
    case r_input_output:
    case r_reading_until:
    case r_deblank_reading_until:
    case r_reading_string:
      return (1);
    case r_duplicating_input:
    case r_duplicating_input_word:
    case r_close_this:
      return (redirector == 0);
    case r_output_direction:
    case r_appending_to:
    case r_duplicating_output:
    case r_err_and_out:
    case r_append_err_and_out:
    case r_output_force:
    case r_duplicating_output_word:
    case r_move_input:
    case r_move_output:
    case r_move_input_word:
    case r_move_output_word:
      return (0);
    }
  return (0);
}

 
int
stdin_redirects (redirs)
     REDIRECT *redirs;
{
  REDIRECT *rp;
  int n;

  for (n = 0, rp = redirs; rp; rp = rp->next)
    if ((rp->rflags & REDIR_VARASSIGN) == 0)
      n += stdin_redirection (rp->instruction, rp->redirector.dest);
  return n;
}
 
static int
redir_varassign (redir, fd)
     REDIRECT *redir;
     int fd;
{
  WORD_DESC *w;
  SHELL_VAR *v;

  w = redir->redirector.filename;
  v = bind_var_to_int (w->word, fd, 0);
  if (v == 0 || readonly_p (v) || noassign_p (v))
    return BADVAR_REDIRECT;

  stupidly_hack_special_variables (w->word);
  return 0;
}

 
static int
redir_varvalue (redir)
     REDIRECT *redir;
{
  SHELL_VAR *v;
  char *val, *w;
  intmax_t vmax;
  int i;
#if defined (ARRAY_VARS)
  char *sub;
  int len, vr;
#endif

  w = redir->redirector.filename->word;		 
   
#if defined (ARRAY_VARS)
  if (vr = valid_array_reference (w, 0))
    {
      v = array_variable_part (w, 0, &sub, &len);
    }
  else
#endif
    {
      v = find_variable (w);
#if defined (ARRAY_VARS)
      if (v == 0)
	{
	  v = find_variable_last_nameref (w, 0);
	  if (v && nameref_p (v))
	    {
	      w = nameref_cell (v);
	      if (vr = valid_array_reference (w, 0))
		v = array_variable_part (w, 0, &sub, &len);
	      else
	        v = find_variable (w);
	    }
	}
#endif
    }
	
  if (v == 0 || invisible_p (v))
    return -1;

#if defined (ARRAY_VARS)
   
  if (vr && (array_p (v) || assoc_p (v)))
    val = get_array_value (w, 0, (array_eltstate_t *)NULL);
  else
#endif
  val = get_variable_value (v);
  if (val == 0 || *val == 0)
    return -1;

  if (legal_number (val, &vmax) < 0)
    return -1;

  i = vmax;	 
  return i;
}
