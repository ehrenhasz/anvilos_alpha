 

#include <config.h>

#include <getopt.h>
#include <sys/types.h>
#include <signal.h>
#include <stdckdint.h>

#include "system.h"

#include <regex.h>

#include "fd-reopen.h"
#include "quote.h"
#include "safe-read.h"
#include "stdio--.h"
#include "xdectoint.h"
#include "xstrtol.h"

 
#define PROGRAM_NAME "csplit"

#define AUTHORS \
  proper_name ("Stuart Kemp"), \
  proper_name ("David MacKenzie")

 
#define DEFAULT_PREFIX	"xx"

 
struct control
{
  intmax_t offset;		 
  intmax_t lines_required;	 
  intmax_t repeat;		 
  int argnum;			 
  bool repeat_forever;		 
  bool ignore;			 
  bool regexpr;			 
  struct re_pattern_buffer re_compiled;	 
};

 
#define START_SIZE	8191

 
#define CTRL_SIZE	80

#ifdef DEBUG
 
# define START_SIZE	200
# define CTRL_SIZE	1
#endif

 
struct cstring
{
  idx_t len;
  char *str;
};

 
struct line
{
  idx_t used;			 
  idx_t insert_index;		 
  idx_t retrieve_index;	 
  struct cstring starts[CTRL_SIZE];  
  struct line *next;		 
};

 
struct buffer_record
{
  idx_t bytes_alloc;		 
  idx_t bytes_used;		 
  intmax_t start_line;		 
  intmax_t first_available;	 
  idx_t num_lines;		 
  char *buffer;			 
  struct line *line_start;	 
  struct line *curr_line;	 
  struct buffer_record *next;
};

static void close_output_file (void);
static void create_output_file (void);
static void delete_all_files (bool);
static void save_line_to_file (const struct cstring *line);

 
static struct buffer_record *head = nullptr;

 
static char *hold_area = nullptr;

 
static idx_t hold_count = 0;

 
static intmax_t last_line_number = 0;

 
static intmax_t current_line = 0;

 
static bool have_read_eof = false;

 
static char *volatile filename_space = nullptr;

 
static char const *volatile prefix = nullptr;

 
static char *volatile suffix = nullptr;

 
static int volatile digits = 2;

 
static int volatile files_created = 0;

 
static intmax_t bytes_written;

 
static FILE *output_stream = nullptr;

 
static char *output_filename = nullptr;

 
static char **global_argv;

 
static bool suppress_count;

 
static bool volatile remove_files;

 
static bool elide_empty_files;

 
static bool suppress_matched;

 
static struct control *controls;

 
static idx_t control_used;

 
static sigset_t caught_signals;

 
enum
{
  SUPPRESS_MATCHED_OPTION = CHAR_MAX + 1
};

static struct option const longopts[] =
{
  {"digits", required_argument, nullptr, 'n'},
  {"quiet", no_argument, nullptr, 'q'},
  {"silent", no_argument, nullptr, 's'},
  {"keep-files", no_argument, nullptr, 'k'},
  {"elide-empty-files", no_argument, nullptr, 'z'},
  {"prefix", required_argument, nullptr, 'f'},
  {"suffix-format", required_argument, nullptr, 'b'},
  {"suppress-matched", no_argument, nullptr, SUPPRESS_MATCHED_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 

static void
cleanup (void)
{
  sigset_t oldset;

  close_output_file ();

  sigprocmask (SIG_BLOCK, &caught_signals, &oldset);
  delete_all_files (false);
  sigprocmask (SIG_SETMASK, &oldset, nullptr);
}

static _Noreturn void
cleanup_fatal (void)
{
  cleanup ();
  exit (EXIT_FAILURE);
}

extern void
xalloc_die (void)
{
  error (0, 0, "%s", _("memory exhausted"));
  cleanup_fatal ();
}

static void
interrupt_handler (int sig)
{
  delete_all_files (true);
  signal (sig, SIG_DFL);
   
  raise (sig);
}

 

static void
save_to_hold_area (char *start, idx_t num)
{
  free (hold_area);
  hold_area = start;
  hold_count = num;
}

 

static idx_t
read_input (char *dest, idx_t max_n_bytes)
{
  idx_t bytes_read;

  if (max_n_bytes == 0)
    return 0;

  bytes_read = safe_read (STDIN_FILENO, dest, max_n_bytes);

  if (bytes_read == 0)
    have_read_eof = true;

  if (bytes_read == SAFE_READ_ERROR)
    {
      error (0, errno, _("read error"));
      cleanup_fatal ();
    }

  return bytes_read;
}

 

static void
clear_line_control (struct line *p)
{
  p->used = 0;
  p->insert_index = 0;
  p->retrieve_index = 0;
}

 

static struct line *
new_line_control (void)
{
  struct line *p = xmalloc (sizeof *p);

  p->next = nullptr;
  clear_line_control (p);

  return p;
}

 

static void
keep_new_line (struct buffer_record *b, char *line_start, idx_t line_len)
{
  struct line *l;

   
  if (b->line_start == nullptr)
    b->line_start = b->curr_line = new_line_control ();

   
  if (b->curr_line->used == CTRL_SIZE)
    {
      b->curr_line->next = new_line_control ();
      b->curr_line = b->curr_line->next;
    }

  l = b->curr_line;

   
  l->starts[l->insert_index].str = line_start;
  l->starts[l->insert_index].len = line_len;
  l->used++;
  l->insert_index++;
}

 

static idx_t
record_line_starts (struct buffer_record *b)
{
  char *line_start;		 
  idx_t lines;			 
  idx_t line_length;		 

  if (b->bytes_used == 0)
    return 0;

  lines = 0;
  line_start = b->buffer;
  char *buffer_end = line_start + b->bytes_used;
  *buffer_end = '\n';

  while (true)
    {
      char *line_end = rawmemchr (line_start, '\n');
      if (line_end == buffer_end)
        break;
      line_length = line_end - line_start + 1;
      keep_new_line (b, line_start, line_length);
      line_start = line_end + 1;
      lines++;
    }

   
  idx_t bytes_left = buffer_end - line_start;
  if (bytes_left)
    {
      if (have_read_eof)
        {
          keep_new_line (b, line_start, bytes_left);
          lines++;
        }
      else
        save_to_hold_area (ximemdup (line_start, bytes_left), bytes_left);
    }

  b->num_lines = lines;
  b->first_available = b->start_line = last_line_number + 1;
  last_line_number += lines;

  return lines;
}

 

static ATTRIBUTE_DEALLOC (free_buffer, 1)
struct buffer_record *
get_new_buffer (idx_t min_size)
{
  struct buffer_record *new_buffer = xmalloc (sizeof *new_buffer);
  new_buffer->bytes_alloc = 0;
  new_buffer->buffer = xpalloc (nullptr, &new_buffer->bytes_alloc, min_size,
                                -1, 1);
  new_buffer->bytes_used = 0;
  new_buffer->start_line = new_buffer->first_available = last_line_number + 1;
  new_buffer->num_lines = 0;
  new_buffer->line_start = new_buffer->curr_line = nullptr;
  new_buffer->next = nullptr;

  return new_buffer;
}

 

static void
save_buffer (struct buffer_record *buf)
{
  struct buffer_record *p;

  buf->next = nullptr;
  buf->curr_line = buf->line_start;

  if (head == nullptr)
    head = buf;
  else
    {
      for (p = head; p->next; p = p->next)
          ;
      p->next = buf;
    }
}

 

static bool
load_buffer (void)
{
  if (have_read_eof)
    return false;

   
  idx_t bytes_wanted = MAX (START_SIZE, hold_count + 1);

  while (true)
    {
      struct buffer_record *b = get_new_buffer (bytes_wanted);
      idx_t bytes_alloc = b->bytes_alloc;
      idx_t bytes_avail = bytes_alloc;
      char *p = b->buffer;

       
      if (hold_count)
        {
          p = mempcpy (p, hold_area, hold_count);
          b->bytes_used += hold_count;
          bytes_avail -= hold_count;
          hold_count = 0;
        }

      b->bytes_used += read_input (p, bytes_avail - 1);

      if (record_line_starts (b) != 0)
        {
          save_buffer (b);
          return true;
        }

      free_buffer (b);
      if (have_read_eof)
        return false;
      if (ckd_add (&bytes_wanted, bytes_alloc, bytes_alloc >> 1))
        xalloc_die ();
    }
}

 

static intmax_t
get_first_line_in_buffer (void)
{
  if (head == nullptr && !load_buffer ())
    error (EXIT_FAILURE, errno, _("input disappeared"));

  return head->first_available;
}

 

static struct cstring *
remove_line (void)
{
   
  static struct buffer_record *prev_buf = nullptr;

  struct cstring *line;		 
  struct line *l;		 

  if (prev_buf)
    {
      free_buffer (prev_buf);
      prev_buf = nullptr;
    }

  if (head == nullptr && !load_buffer ())
    return nullptr;

  if (current_line < head->first_available)
    current_line = head->first_available;

  ++(head->first_available);

  l = head->curr_line;

  line = &l->starts[l->retrieve_index];

   
  if (++l->retrieve_index == l->used)
    {
       
      head->curr_line = l->next;
      if (head->curr_line == nullptr || head->curr_line->used == 0)
        {
           
          prev_buf = head;
          head = head->next;
        }
    }

  return line;
}

 

static struct cstring *
find_line (intmax_t linenum)
{
  struct buffer_record *b;

  if (head == nullptr && !load_buffer ())
    return nullptr;

  if (linenum < head->start_line)
    return nullptr;

  for (b = head;;)
    {
      if (linenum < b->start_line + b->num_lines)
        {
           
          struct line *l;
          idx_t offset;	 

          l = b->line_start;
          offset = linenum - b->start_line;
           
          while (offset >= CTRL_SIZE)
            {
              l = l->next;
              offset -= CTRL_SIZE;
            }
          return &l->starts[offset];
        }
      if (b->next == nullptr && !load_buffer ())
        return nullptr;
      b = b->next;		 
    }
}

 

static bool
no_more_lines (void)
{
  return find_line (current_line + 1) == nullptr;
}

 

static void
set_input_file (char const *name)
{
  if (! STREQ (name, "-") && fd_reopen (STDIN_FILENO, name, O_RDONLY, 0) < 0)
    error (EXIT_FAILURE, errno, _("cannot open %s for reading"),
           quoteaf (name));
}

 

static void
write_to_file (intmax_t last_line, bool ignore, int argnum)
{
  struct cstring *line;
  intmax_t first_line;		 
  intmax_t lines;		 
  intmax_t i;

  first_line = get_first_line_in_buffer ();

  if (first_line > last_line)
    {
      error (0, 0, _("%s: line number out of range"),
             quote (global_argv[argnum]));
      cleanup_fatal ();
    }

  lines = last_line - first_line;

  for (i = 0; i < lines; i++)
    {
      line = remove_line ();
      if (line == nullptr)
        {
          error (0, 0, _("%s: line number out of range"),
                 quote (global_argv[argnum]));
          cleanup_fatal ();
        }
      if (!ignore)
        save_line_to_file (line);
    }
}

 

static void
dump_rest_of_file (void)
{
  struct cstring *line;

  while ((line = remove_line ()) != nullptr)
    save_line_to_file (line);
}

 

static void
handle_line_error (const struct control *p, intmax_t repetition)
{
  char buf[INT_BUFSIZE_BOUND (intmax_t)];

  fprintf (stderr, _("%s: %s: line number out of range"),
           program_name, quote (imaxtostr (p->lines_required, buf)));
  if (repetition)
    fprintf (stderr, _(" on repetition %s\n"), imaxtostr (repetition, buf));
  else
    fprintf (stderr, "\n");

  cleanup_fatal ();
}

 

static void
process_line_count (const struct control *p, intmax_t repetition)
{
  intmax_t linenum;
  intmax_t last_line_to_save = p->lines_required * (repetition + 1);

  create_output_file ();

   
  if (no_more_lines () && suppress_matched)
    handle_line_error (p, repetition);

  linenum = get_first_line_in_buffer ();
  while (linenum++ < last_line_to_save)
    {
      struct cstring *line = remove_line ();
      if (line == nullptr)
        handle_line_error (p, repetition);
      save_line_to_file (line);
    }

  close_output_file ();

  if (suppress_matched)
    remove_line ();

   
  if (no_more_lines () && !suppress_matched)
    handle_line_error (p, repetition);
}

static void
regexp_error (struct control *p, intmax_t repetition, bool ignore)
{
  fprintf (stderr, _("%s: %s: match not found"),
           program_name, quote (global_argv[p->argnum]));

  if (repetition)
    {
      char buf[INT_BUFSIZE_BOUND (intmax_t)];
      fprintf (stderr, _(" on repetition %s\n"), imaxtostr (repetition, buf));
    }
  else
    fprintf (stderr, "\n");

  if (!ignore)
    {
      dump_rest_of_file ();
      close_output_file ();
    }
  cleanup_fatal ();
}

 

static void
process_regexp (struct control *p, intmax_t repetition)
{
  struct cstring *line;		 
  idx_t line_len;		 
  intmax_t break_line;		 
  bool ignore = p->ignore;	 
  regoff_t ret;

  if (!ignore)
    create_output_file ();

   

  if (p->offset >= 0)
    {
      while (true)
        {
          line = find_line (++current_line);
          if (line == nullptr)
            {
              if (p->repeat_forever)
                {
                  if (!ignore)
                    {
                      dump_rest_of_file ();
                      close_output_file ();
                    }
                  exit (EXIT_SUCCESS);
                }
              else
                regexp_error (p, repetition, ignore);
            }
          line_len = line->len;
          if (line->str[line_len - 1] == '\n')
            line_len--;
          ret = re_search (&p->re_compiled, line->str, line_len,
                           0, line_len, nullptr);
          if (ret == -2)
            {
              error (0, 0, _("error in regular expression search"));
              cleanup_fatal ();
            }
          if (ret == -1)
            {
              line = remove_line ();
              if (!ignore)
                save_line_to_file (line);
            }
          else
            break;
        }
    }
  else
    {
       
      while (true)
        {
          line = find_line (++current_line);
          if (line == nullptr)
            {
              if (p->repeat_forever)
                {
                  if (!ignore)
                    {
                      dump_rest_of_file ();
                      close_output_file ();
                    }
                  exit (EXIT_SUCCESS);
                }
              else
                regexp_error (p, repetition, ignore);
            }
          line_len = line->len;
          if (line->str[line_len - 1] == '\n')
            line_len--;
          ret = re_search (&p->re_compiled, line->str, line_len,
                           0, line_len, nullptr);
          if (ret == -2)
            {
              error (0, 0, _("error in regular expression search"));
              cleanup_fatal ();
            }
          if (ret != -1)
            break;
        }
    }

   
  break_line = current_line + p->offset;

  write_to_file (break_line, ignore, p->argnum);

  if (!ignore)
    close_output_file ();

  if (p->offset > 0)
    current_line = break_line;

  if (suppress_matched)
    remove_line ();
}

 

static void
split_file (void)
{
  for (idx_t i = 0; i < control_used; i++)
    {
      intmax_t j;
      if (controls[i].regexpr)
        {
          for (j = 0; (controls[i].repeat_forever
                       || j <= controls[i].repeat); j++)
            process_regexp (&controls[i], j);
        }
      else
        {
          for (j = 0; (controls[i].repeat_forever
                       || j <= controls[i].repeat); j++)
            process_line_count (&controls[i], j);
        }
    }

  create_output_file ();
  dump_rest_of_file ();
  close_output_file ();
}

 

static char *
make_filename (int num)
{
  strcpy (filename_space, prefix);
  if (suffix)
    sprintf (filename_space + strlen (prefix), suffix, num);
  else
    sprintf (filename_space + strlen (prefix), "%0*d", digits, num);
  return filename_space;
}

 

static void
create_output_file (void)
{
  int nfiles = files_created;
  bool fopen_ok;
  int fopen_errno;

  output_filename = make_filename (nfiles);

  if (nfiles == INT_MAX)
    {
      fopen_ok = false;
      fopen_errno = EOVERFLOW;
    }
  else
    {
       
      sigset_t oldset;
      sigprocmask (SIG_BLOCK, &caught_signals, &oldset);
      output_stream = fopen (output_filename, "w");
      fopen_ok = (output_stream != nullptr);
      fopen_errno = errno;
      files_created = nfiles + fopen_ok;
      sigprocmask (SIG_SETMASK, &oldset, nullptr);
    }

  if (! fopen_ok)
    {
      error (0, fopen_errno, "%s", quotef (output_filename));
      cleanup_fatal ();
    }
  bytes_written = 0;
}

 

static void
delete_all_files (bool in_signal_handler)
{
  if (! remove_files)
    return;

  for (int i = files_created; 0 <= --i; )
    {
      char const *name = make_filename (i);
      if (unlink (name) != 0 && errno != ENOENT && !in_signal_handler)
        error (0, errno, "%s", quotef (name));
    }

  files_created = 0;
}

 

static void
close_output_file (void)
{
  if (output_stream)
    {
      if (ferror (output_stream))
        {
          error (0, 0, _("write error for %s"), quoteaf (output_filename));
          output_stream = nullptr;
          cleanup_fatal ();
        }
      if (fclose (output_stream) != 0)
        {
          error (0, errno, "%s", quotef (output_filename));
          output_stream = nullptr;
          cleanup_fatal ();
        }
      if (bytes_written == 0 && elide_empty_files)
        {
          sigset_t oldset;
          bool unlink_ok;
          int unlink_errno;

           
          sigprocmask (SIG_BLOCK, &caught_signals, &oldset);
          unlink_ok = (unlink (output_filename) == 0);
          unlink_errno = errno;
          files_created--;
          sigprocmask (SIG_SETMASK, &oldset, nullptr);

          if (! unlink_ok && unlink_errno != ENOENT)
            error (0, unlink_errno, "%s", quotef (output_filename));
        }
      else
        {
          if (!suppress_count)
            {
              char buf[INT_BUFSIZE_BOUND (intmax_t)];
              fprintf (stdout, "%s\n", imaxtostr (bytes_written, buf));
            }
        }
      output_stream = nullptr;
    }
}

 

static void
save_line_to_file (const struct cstring *line)
{
  idx_t l = fwrite (line->str, sizeof (char), line->len, output_stream);
  if (l != line->len)
    {
      error (0, errno, _("write error for %s"), quoteaf (output_filename));
      output_stream = nullptr;
      cleanup_fatal ();
    }
  bytes_written += line->len;
}

 

static struct control *
new_control_record (void)
{
  static idx_t control_allocated = 0;  
  struct control *p;

  if (control_used == control_allocated)
    controls = xpalloc (controls, &control_allocated, 1, -1, sizeof *controls);
  p = &controls[control_used++];
  p->regexpr = false;
  p->repeat = 0;
  p->repeat_forever = false;
  p->lines_required = 0;
  p->offset = 0;
  return p;
}

 

static void
check_for_offset (struct control *p, char const *str, char const *num)
{
  if (xstrtoimax (num, nullptr, 10, &p->offset, "") != LONGINT_OK)
    error (EXIT_FAILURE, 0, _("%s: integer expected after delimiter"),
           quote (str));
}

 

static void
parse_repeat_count (int argnum, struct control *p, char *str)
{
  char *end;

  end = str + strlen (str) - 1;
  if (*end != '}')
    error (EXIT_FAILURE, 0, _("%s: '}' is required in repeat count"),
           quote (str));
  *end = '\0';

  if (str + 1 == end - 1 && *(str + 1) == '*')
    p->repeat_forever = true;
  else
    {
      uintmax_t val;
      if (xstrtoumax (str + 1, nullptr, 10, &val, "") != LONGINT_OK
          || INTMAX_MAX < val)
        {
          error (EXIT_FAILURE, 0,
                 _("%s}: integer required between '{' and '}'"),
                 quote (global_argv[argnum]));
        }
      p->repeat = val;
    }

  *end = '}';
}

 

static struct control *
extract_regexp (int argnum, bool ignore, char const *str)
{
  idx_t len;			 
  char delim = *str;
  char const *closing_delim;
  struct control *p;
  char const *err;

  closing_delim = strrchr (str + 1, delim);
  if (closing_delim == nullptr)
    error (EXIT_FAILURE, 0,
           _("%s: closing delimiter '%c' missing"), str, delim);

  len = closing_delim - str - 1;
  p = new_control_record ();
  p->argnum = argnum;
  p->ignore = ignore;

  p->regexpr = true;
  p->re_compiled.buffer = nullptr;
  p->re_compiled.allocated = 0;
  p->re_compiled.fastmap = xmalloc (UCHAR_MAX + 1);
  p->re_compiled.translate = nullptr;
  re_syntax_options =
    RE_SYNTAX_POSIX_BASIC & ~RE_CONTEXT_INVALID_DUP & ~RE_NO_EMPTY_RANGES;
  err = re_compile_pattern (str + 1, len, &p->re_compiled);
  if (err)
    {
      error (0, 0, _("%s: invalid regular expression: %s"), quote (str), err);
      cleanup_fatal ();
    }

  if (closing_delim[1])
    check_for_offset (p, str, closing_delim + 1);

  return p;
}

 

static void
parse_patterns (int argc, int start, char **argv)
{
  struct control *p;		 
  static intmax_t last_val = 0;

  for (int i = start; i < argc; i++)
    {
      if (*argv[i] == '/' || *argv[i] == '%')
        {
          p = extract_regexp (i, *argv[i] == '%', argv[i]);
        }
      else
        {
          p = new_control_record ();
          p->argnum = i;

          uintmax_t val;
          if (xstrtoumax (argv[i], nullptr, 10, &val, "") != LONGINT_OK
              || INTMAX_MAX < val)
            error (EXIT_FAILURE, 0, _("%s: invalid pattern"), quote (argv[i]));
          if (val == 0)
            error (EXIT_FAILURE, 0,
                   _("%s: line number must be greater than zero"), argv[i]);
          if (val < last_val)
            {
              char buf[INT_BUFSIZE_BOUND (intmax_t)];
              error (EXIT_FAILURE, 0,
                     _("line number %s is smaller than preceding line number,"
                       " %s"),
                     quote (argv[i]), imaxtostr (last_val, buf));
            }

          if (val == last_val)
            error (0, 0,
           _("warning: line number %s is the same as preceding line number"),
                   quote (argv[i]));

          last_val = val;

          p->lines_required = val;
        }

      if (i + 1 < argc && *argv[i + 1] == '{')
        {
           
          i++;
          parse_repeat_count (i, p, argv[i]);
        }
    }
}



 
enum { FLAG_THOUSANDS = 1, FLAG_ALTERNATIVE = 2 };

 
static idx_t
get_format_flags (char const *format, int *flags_ptr)
{
  int flags = 0;

  for (idx_t count = 0; ; count++)
    {
      switch (format[count])
        {
        case '-':
        case '0':
          break;

        case '\'':
          flags |= FLAG_THOUSANDS;
          break;

        case '#':
          flags |= FLAG_ALTERNATIVE;
          break;

        default:
          *flags_ptr = flags;
          return count;
        }
    }
}

 
static void
check_format_conv_type (char *format, int flags)
{
  unsigned char ch = *format;
  int compatible_flags = FLAG_THOUSANDS;

  switch (ch)
    {
    case 'd':
    case 'i':
      break;

    case 'u':
      *format = 'd';
      break;

    case 'o':
    case 'x':
    case 'X':
      compatible_flags = FLAG_ALTERNATIVE;
      break;

    case 0:
      error (EXIT_FAILURE, 0, _("missing conversion specifier in suffix"));

    default:
      if (isprint (ch))
        error (EXIT_FAILURE, 0,
               _("invalid conversion specifier in suffix: %c"), ch);
      else
        error (EXIT_FAILURE, 0,
               _("invalid conversion specifier in suffix: \\%.3o"), ch);
    }

  if (flags & ~ compatible_flags)
    error (EXIT_FAILURE, 0,
           _("invalid flags in conversion specification: %%%c%c"),
           (flags & ~ compatible_flags & FLAG_ALTERNATIVE ? '#' : '\''), ch);
}

 
static idx_t
max_out (char *format)
{
  bool percent = false;

  for (char *f = format; *f; f++)
    if (*f == '%' && *++f != '%')
      {
        if (percent)
          error (EXIT_FAILURE, 0,
                 _("too many %% conversion specifications in suffix"));
        percent = true;
        int flags;
        f += get_format_flags (f, &flags);
        while (ISDIGIT (*f))
          f++;
        if (*f == '.')
          while (ISDIGIT (*++f))
            continue;
        check_format_conv_type (f, flags);
      }

  if (! percent)
    error (EXIT_FAILURE, 0,
           _("missing %% conversion specification in suffix"));

  int maxlen = snprintf (nullptr, 0, format, INT_MAX);
  if (maxlen < 0)
    xalloc_die ();
  return maxlen;
}

int
main (int argc, char **argv)
{
  int optc;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  global_argv = argv;
  controls = nullptr;
  control_used = 0;
  suppress_count = false;
  remove_files = true;
  suppress_matched = false;
  prefix = DEFAULT_PREFIX;

  while ((optc = getopt_long (argc, argv, "f:b:kn:sqz", longopts, nullptr))
         != -1)
    switch (optc)
      {
      case 'f':
        prefix = optarg;
        break;

      case 'b':
        suffix = optarg;
        break;

      case 'k':
        remove_files = false;
        break;

      case 'n':
        digits = xdectoimax (optarg, 0, MIN (INT_MAX, IDX_MAX), "",
                             _("invalid number"), 0);
        break;

      case 's':
      case 'q':
        suppress_count = true;
        break;

      case 'z':
        elide_empty_files = true;
        break;

      case SUPPRESS_MATCHED_OPTION:
        suppress_matched = true;
        break;

      case_GETOPT_HELP_CHAR;

      case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

      default:
        usage (EXIT_FAILURE);
      }

  if (argc - optind < 2)
    {
      if (argc <= optind)
        error (0, 0, _("missing operand"));
      else
        error (0, 0, _("missing operand after %s"), quote (argv[argc - 1]));
      usage (EXIT_FAILURE);
    }

  idx_t prefix_len = strlen (prefix);
  idx_t max_digit_string_len
    = (suffix
       ? max_out (suffix)
       : MAX (INT_STRLEN_BOUND (int), digits));
  idx_t filename_size;
  if (ckd_add (&filename_size, prefix_len, max_digit_string_len + 1))
    xalloc_die ();
  filename_space = ximalloc (filename_size);

  set_input_file (argv[optind++]);

  parse_patterns (argc, optind, argv);

  {
    int i;
    static int const sig[] =
      {
         
        SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
#ifdef SIGPOLL
        SIGPOLL,
#endif
#ifdef SIGPROF
        SIGPROF,
#endif
#ifdef SIGVTALRM
        SIGVTALRM,
#endif
#ifdef SIGXCPU
        SIGXCPU,
#endif
#ifdef SIGXFSZ
        SIGXFSZ,
#endif
      };
    enum { nsigs = ARRAY_CARDINALITY (sig) };

    struct sigaction act;

    sigemptyset (&caught_signals);
    for (i = 0; i < nsigs; i++)
      {
        sigaction (sig[i], nullptr, &act);
        if (act.sa_handler != SIG_IGN)
          sigaddset (&caught_signals, sig[i]);
      }

    act.sa_handler = interrupt_handler;
    act.sa_mask = caught_signals;
    act.sa_flags = 0;

    for (i = 0; i < nsigs; i++)
      if (sigismember (&caught_signals, sig[i]))
        sigaction (sig[i], &act, nullptr);
  }

  split_file ();

  if (close (STDIN_FILENO) != 0)
    {
      error (0, errno, _("read error"));
      cleanup_fatal ();
    }

  return EXIT_SUCCESS;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... FILE PATTERN...\n\
"),
              program_name);
      fputs (_("\
Output pieces of FILE separated by PATTERN(s) to files 'xx00', 'xx01', ...,\n\
and output byte counts of each piece to standard output.\n\
"), stdout);
       fputs (_("\
\n\
Read standard input if FILE is -\n\
"), stdout);

      emit_mandatory_arg_note ();

      fputs (_("\
  -b, --suffix-format=FORMAT  use sprintf FORMAT instead of %02d\n\
  -f, --prefix=PREFIX        use PREFIX instead of 'xx'\n\
  -k, --keep-files           do not remove output files on errors\n\
"), stdout);
      fputs (_("\
      --suppress-matched     suppress the lines matching PATTERN\n\
"), stdout);
      fputs (_("\
  -n, --digits=DIGITS        use specified number of digits instead of 2\n\
  -s, --quiet, --silent      do not print counts of output file sizes\n\
  -z, --elide-empty-files    suppress empty output files\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
Each PATTERN may be:\n\
  INTEGER            copy up to but not including specified line number\n\
  /REGEXP/[OFFSET]   copy up to but not including a matching line\n\
  %REGEXP%[OFFSET]   skip to, but not including a matching line\n\
  {INTEGER}          repeat the previous pattern specified number of times\n\
  {*}                repeat the previous pattern as many times as possible\n\
\n\
A line OFFSET is an integer optionally preceded by '+' or '-'\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}
