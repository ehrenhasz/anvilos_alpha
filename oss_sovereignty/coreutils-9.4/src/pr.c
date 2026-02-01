 

 

#include <config.h>

#include <getopt.h>
#include <stdckdint.h>
#include <sys/types.h>
#include "system.h"
#include "fadvise.h"
#include "hard-locale.h"
#include "mbswidth.h"
#include "quote.h"
#include "stat-time.h"
#include "stdio--.h"
#include "strftime.h"
#include "xstrtol.h"
#include "xstrtol-error.h"
#include "xdectoint.h"

 
#define PROGRAM_NAME "pr"

#define AUTHORS \
  proper_name ("Pete TerMaat"), \
  proper_name ("Roland Huebner")

 
#define ANYWHERE	0

 

 

struct COLUMN;
struct COLUMN
  {
    FILE *fp;			 
    char const *name;		 
    enum
      {
        OPEN,
        FF_FOUND,		 
        ON_HOLD,		 
        CLOSED
      }
    status;			 

     
    bool (*print_func) (struct COLUMN *);

     
    void (*char_func) (char);

    int current_line;		 
    int lines_stored;		 
    int lines_to_print;		 
    int start_position;		 
    bool numbered;
    bool full_page_printed;	 

     
  };

typedef struct COLUMN COLUMN;

static int char_to_clump (char c);
static bool read_line (COLUMN *p);
static bool print_page (void);
static bool print_stored (COLUMN *p);
static bool open_file (char *name, COLUMN *p);
static bool skip_to_page (uintmax_t page);
static void print_header (void);
static void pad_across_to (int position);
static void add_line_number (COLUMN *p);
static void getoptnum (char const *n_str, int min, int *num,
                       char const *errfmt);
static void getoptarg (char *arg, char switch_char, char *character,
                       int *number);
static void print_files (int number_of_files, char **av);
static void init_parameters (int number_of_files);
static void init_header (char const *filename, int desc);
static bool init_fps (int number_of_files, char **av);
static void init_funcs (void);
static void init_store_cols (void);
static void store_columns (void);
static void balance (int total_stored);
static void store_char (char c);
static void pad_down (unsigned int lines);
static void read_rest_of_line (COLUMN *p);
static void skip_read (COLUMN *p, int column_number);
static void print_char (char c);
static void cleanup (void);
static void print_sep_string (void);
static void separator_string (char const *optarg_S);

 
static COLUMN *column_vector;

 
static char *buff;

 
static unsigned int buff_current;

 
static size_t buff_allocated;

 
static int *line_vector;

 
static int *end_vector;

 
static bool parallel_files = false;

 
static bool align_empty_cols;

 
static bool empty_line;

 
static bool FF_only;

 
static bool explicit_columns = false;

 
static bool extremities = true;

 
static bool keep_FF = false;
static bool print_a_FF = false;

 
static bool print_a_header;

 
static bool use_form_feed = false;

 
static bool have_read_stdin = false;

 
static bool print_across_flag = false;

 
static bool storing_columns = true;

 
 
static bool balance_columns = false;

 
static int lines_per_page = 66;

 
enum { lines_per_header = 5 };
static int lines_per_body;
enum { lines_per_footer = 5 };

 
static int chars_per_line = 72;

 
static bool truncate_lines = false;

 
static bool join_lines = false;

 
static int chars_per_column;

 
static bool untabify_input = false;

 
static char input_tab_char = '\t';

 
static int chars_per_input_tab = 8;

 
static bool tabify_output = false;

 
static char output_tab_char = '\t';

 
static int chars_per_output_tab = 8;

 
static int spaces_not_printed;

 
static int chars_per_margin = 0;

 
static int output_position;

 
static int input_position;

 
static bool failed_opens = false;

 
#define TAB_WIDTH(c_, h_) ((c_) - ((h_) % (c_)))

 
#define POS_AFTER_TAB(c_, h_) ((h_) + TAB_WIDTH (c_, h_))

 
static int columns = 1;

 
static uintmax_t first_page_number = 0;
static uintmax_t last_page_number = UINTMAX_MAX;

 
static int files_ready_to_read = 0;

 
static uintmax_t page_number;

 
static int line_number;

 
static bool numbered_lines = false;

 
static char number_separator = '\t';

 
static int line_count = 1;

 
static bool skip_count = true;

 
static int start_line_num = 1;

 
static int chars_per_number = 5;

 
static int number_width;

 
static char *number_buff;

 
static bool use_esc_sequence = false;

 
static bool use_cntrl_prefix = false;

 
static bool double_space = false;

 
static int total_files = 0;

 
static bool ignore_failed_opens = false;

 
static bool use_col_separator = false;

 
static char const *col_sep_string = "";
static int col_sep_length = 0;
static char *column_separator = (char *) " ";
static char *line_separator = (char *) "\t";

 
static int separators_not_printed;

 
static int padding_not_printed;

 
static bool pad_vertically;

 
static char *custom_header;

 
static char const *date_format;

 
static timezone_t localtz;

 
static char *date_text;
static char const *file_text;

 
static int header_width_available;

static char *clump_buff;

 
static bool last_line = false;

 
enum
{
  COLUMNS_OPTION = CHAR_MAX + 1,
  PAGES_OPTION
};

static char const short_options[] =
  "-0123456789D:FJN:S::TW:abcde::fh:i::l:mn::o:rs::tvw:";

static struct option const long_options[] =
{
  {"pages", required_argument, nullptr, PAGES_OPTION},
  {"columns", required_argument, nullptr, COLUMNS_OPTION},
  {"across", no_argument, nullptr, 'a'},
  {"show-control-chars", no_argument, nullptr, 'c'},
  {"double-space", no_argument, nullptr, 'd'},
  {"date-format", required_argument, nullptr, 'D'},
  {"expand-tabs", optional_argument, nullptr, 'e'},
  {"form-feed", no_argument, nullptr, 'f'},
  {"header", required_argument, nullptr, 'h'},
  {"output-tabs", optional_argument, nullptr, 'i'},
  {"join-lines", no_argument, nullptr, 'J'},
  {"length", required_argument, nullptr, 'l'},
  {"merge", no_argument, nullptr, 'm'},
  {"number-lines", optional_argument, nullptr, 'n'},
  {"first-line-number", required_argument, nullptr, 'N'},
  {"indent", required_argument, nullptr, 'o'},
  {"no-file-warnings", no_argument, nullptr, 'r'},
  {"separator", optional_argument, nullptr, 's'},
  {"sep-string", optional_argument, nullptr, 'S'},
  {"omit-header", no_argument, nullptr, 't'},
  {"omit-pagination", no_argument, nullptr, 'T'},
  {"show-nonprinting", no_argument, nullptr, 'v'},
  {"width", required_argument, nullptr, 'w'},
  {"page-width", required_argument, nullptr, 'W'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

static _Noreturn void
integer_overflow (void)
{
  error (EXIT_FAILURE, 0, _("integer overflow"));
}

 

ATTRIBUTE_PURE
static unsigned int
cols_ready_to_print (void)
{
  COLUMN *q;
  unsigned int i;
  unsigned int n;

  n = 0;
  for (q = column_vector, i = 0; i < columns; ++q, ++i)
    if (q->status == OPEN
        || q->status == FF_FOUND	 
        || (storing_columns && q->lines_stored > 0 && q->lines_to_print > 0))
      ++n;
  return n;
}

 

static bool
first_last_page (int oi, char c, char const *pages)
{
  char *p;
  uintmax_t first;
  uintmax_t last = UINTMAX_MAX;
  strtol_error err = xstrtoumax (pages, &p, 10, &first, "");
  if (err != LONGINT_OK && err != LONGINT_INVALID_SUFFIX_CHAR)
    xstrtol_fatal (err, oi, c, long_options, pages);

  if (p == pages || !first)
    return false;

  if (*p == ':')
    {
      char const *p1 = p + 1;
      err = xstrtoumax (p1, &p, 10, &last, "");
      if (err != LONGINT_OK)
        xstrtol_fatal (err, oi, c, long_options, pages);
      if (p1 == p || last < first)
        return false;
    }

  if (*p)
    return false;

  first_page_number = first;
  last_page_number = last;
  return true;
}

 

static void
parse_column_count (char const *s)
{
  getoptnum (s, 1, &columns, _("invalid number of columns"));
  explicit_columns = true;
}

 

static void
separator_string (char const *optarg_S)
{
  size_t len = strlen (optarg_S);
  if (INT_MAX < len)
    integer_overflow ();
  col_sep_length = len;
  col_sep_string = optarg_S;
}

int
main (int argc, char **argv)
{
  unsigned int n_files;
  bool old_options = false;
  bool old_w = false;
  bool old_s = false;
  char **file_names;

   
  char *column_count_string = nullptr;
  size_t n_digits = 0;
  size_t n_alloc = 0;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  n_files = 0;
  file_names = (argc > 1
                ? xnmalloc (argc - 1, sizeof (char *))
                : nullptr);

  while (true)
    {
      int oi = -1;
      int c = getopt_long (argc, argv, short_options, long_options, &oi);
      if (c == -1)
        break;

      if (ISDIGIT (c))
        {
           
          if (n_digits + 1 >= n_alloc)
            column_count_string
              = X2REALLOC (column_count_string, &n_alloc);
          column_count_string[n_digits++] = c;
          column_count_string[n_digits] = '\0';
          continue;
        }

      n_digits = 0;

      switch (c)
        {
        case 1:			 
           
          if (! (first_page_number == 0
                 && *optarg == '+' && first_last_page (-2, '+', optarg + 1)))
            file_names[n_files++] = optarg;
          break;

        case PAGES_OPTION:	 
          {			 
            if (! optarg)
              error (EXIT_FAILURE, 0,
                     _("'--pages=FIRST_PAGE[:LAST_PAGE]' missing argument"));
            else if (! first_last_page (oi, 0, optarg))
              error (EXIT_FAILURE, 0, _("invalid page range %s"),
                     quote (optarg));
            break;
          }

        case COLUMNS_OPTION:	 
          {
            parse_column_count (optarg);

             
            free (column_count_string);
            column_count_string = nullptr;
            n_alloc = 0;
            break;
          }

        case 'a':
          print_across_flag = true;
          storing_columns = false;
          break;
        case 'b':
          balance_columns = true;
          break;
        case 'c':
          use_cntrl_prefix = true;
          break;
        case 'd':
          double_space = true;
          break;
        case 'D':
          date_format = optarg;
          break;
        case 'e':
          if (optarg)
            getoptarg (optarg, 'e', &input_tab_char,
                       &chars_per_input_tab);
           
          untabify_input = true;
          break;
        case 'f':
        case 'F':
          use_form_feed = true;
          break;
        case 'h':
          custom_header = optarg;
          break;
        case 'i':
          if (optarg)
            getoptarg (optarg, 'i', &output_tab_char,
                       &chars_per_output_tab);
           
          tabify_output = true;
          break;
        case 'J':
          join_lines = true;
          break;
        case 'l':
          getoptnum (optarg, 1, &lines_per_page,
                     _("'-l PAGE_LENGTH' invalid number of lines"));
          break;
        case 'm':
          parallel_files = true;
          storing_columns = false;
          break;
        case 'n':
          numbered_lines = true;
          if (optarg)
            getoptarg (optarg, 'n', &number_separator,
                       &chars_per_number);
          break;
        case 'N':
          skip_count = false;
          getoptnum (optarg, INT_MIN, &start_line_num,
                     _("'-N NUMBER' invalid starting line number"));
          break;
        case 'o':
          getoptnum (optarg, 0, &chars_per_margin,
                     _("'-o MARGIN' invalid line offset"));
          break;
        case 'r':
          ignore_failed_opens = true;
          break;
        case 's':
          old_options = true;
          old_s = true;
          if (!use_col_separator && optarg)
            separator_string (optarg);
          break;
        case 'S':
          old_s = false;
           
          col_sep_string = "";
          col_sep_length = 0;
          use_col_separator = true;
          if (optarg)
            separator_string (optarg);
          break;
        case 't':
          extremities = false;
          keep_FF = true;
          break;
        case 'T':
          extremities = false;
          keep_FF = false;
          break;
        case 'v':
          use_esc_sequence = true;
          break;
        case 'w':
          old_options = true;
          old_w = true;
          {
            int tmp_cpl;
            getoptnum (optarg, 1, &tmp_cpl,
                       _("'-w PAGE_WIDTH' invalid number of characters"));
            if (! truncate_lines)
              chars_per_line = tmp_cpl;
          }
          break;
        case 'W':
          old_w = false;			 
          truncate_lines = true;
          getoptnum (optarg, 1, &chars_per_line,
                     _("'-W PAGE_WIDTH' invalid number of characters"));
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
          break;
        }
    }

  if (column_count_string)
    {
      parse_column_count (column_count_string);
      free (column_count_string);
    }

  if (! date_format)
    date_format = (getenv ("POSIXLY_CORRECT") && !hard_locale (LC_TIME)
                   ? "%b %e %H:%M %Y"
                   : "%Y-%m-%d %H:%M");

  localtz = tzalloc (getenv ("TZ"));

   
  if (first_page_number == 0)
    first_page_number = 1;

  if (parallel_files && explicit_columns)
    error (EXIT_FAILURE, 0,
           _("cannot specify number of columns when printing in parallel"));

  if (parallel_files && print_across_flag)
    error (EXIT_FAILURE, 0,
           _("cannot specify both printing across and printing in parallel"));

 

  if (old_options)
    {
      if (old_w)
        {
          if (parallel_files || explicit_columns)
            {
               
              truncate_lines = true;
              if (old_s)
                 
                use_col_separator = true;
            }
          else
             
            join_lines = true;
        }
      else if (!use_col_separator)
        {
           
          if (old_s && (parallel_files || explicit_columns))
            {
              if (!truncate_lines)
                {
                   
                  join_lines = true;
                  if (col_sep_length > 0)
                     
                    use_col_separator = true;
                }
              else
                 
                 
                use_col_separator = true;
            }
        }
    }

  for (; optind < argc; optind++)
    {
      file_names[n_files++] = argv[optind];
    }

  if (n_files == 0)
    {
       
      print_files (0, nullptr);
    }
  else
    {
      if (parallel_files)
        print_files (n_files, file_names);
      else
        {
          for (unsigned int i = 0; i < n_files; i++)
            print_files (1, &file_names[i]);
        }
    }

  cleanup ();

  if (have_read_stdin && fclose (stdin) == EOF)
    error (EXIT_FAILURE, errno, _("standard input"));
  main_exit (failed_opens ? EXIT_FAILURE : EXIT_SUCCESS);
}

 

static void
getoptnum (char const *n_str, int min, int *num, char const *err)
{
  intmax_t tnum = xdectoimax (n_str, min, INT_MAX, "", err, 0);
  *num = tnum;
}

 

static void
getoptarg (char *arg, char switch_char, char *character, int *number)
{
  if (!*arg)
    {
      error (0, 0, _("'-%c': Invalid argument: %s"), switch_char, quote (arg));
      usage (EXIT_FAILURE);
    }

  if (!ISDIGIT (*arg))
    *character = *arg++;
  if (*arg)
    {
      long int tmp_long;
      strtol_error e = xstrtol (arg, nullptr, 10, &tmp_long, "");
      if (e == LONGINT_OK)
        {
          if (tmp_long <= 0)
            e = LONGINT_INVALID;
          else if (INT_MAX < tmp_long)
            e = LONGINT_OVERFLOW;
        }
      if (e != LONGINT_OK)
        {
          error (0, e & LONGINT_OVERFLOW ? EOVERFLOW : 0,
             _("'-%c' extra characters or invalid number in the argument: %s"),
                 switch_char, quote (arg));
          usage (EXIT_FAILURE);
        }
      *number = tmp_long;
    }
}

 

static void
init_parameters (int number_of_files)
{
  int chars_used_by_number = 0;

  lines_per_body = lines_per_page - lines_per_header - lines_per_footer;
  if (lines_per_body <= 0)
    {
      extremities = false;
      keep_FF = true;
    }
  if (extremities == false)
    lines_per_body = lines_per_page;

  if (double_space)
    lines_per_body = MAX (1, lines_per_body / 2);

   
  if (number_of_files == 0)
    parallel_files = false;

  if (parallel_files)
    columns = number_of_files;

   
  if (storing_columns)
    balance_columns = true;

   
  if (columns > 1)
    {
      if (!use_col_separator)
        {
           
          if (join_lines)
            col_sep_string = line_separator;
          else
            col_sep_string = column_separator;

          col_sep_length = 1;
          use_col_separator = true;
        }
       
      else if (!join_lines && col_sep_length == 1 && *col_sep_string == '\t')
        col_sep_string = column_separator;

      truncate_lines = true;
      if (! (col_sep_length == 1 && *col_sep_string == '\t'))
        untabify_input = true;
      tabify_output = true;
    }
  else
    storing_columns = false;

   
  if (join_lines)
    truncate_lines = false;

  if (numbered_lines)
    {
      int chars_per_default_tab = 8;

      line_count = start_line_num;

       

       
      if (number_separator == '\t')
        number_width = (chars_per_number
                        + TAB_WIDTH (chars_per_default_tab, chars_per_number));
      else
        number_width = chars_per_number + 1;

       
      if (parallel_files)
        chars_used_by_number = number_width;
    }

  int sep_chars, useful_chars;
  if (ckd_mul (&sep_chars, columns - 1, col_sep_length))
    sep_chars = INT_MAX;
  if (ckd_sub (&useful_chars, chars_per_line - chars_used_by_number,
               sep_chars))
    useful_chars = 0;
  chars_per_column = useful_chars / columns;

  if (chars_per_column < 1)
    error (EXIT_FAILURE, 0, _("page width too narrow"));

  if (numbered_lines)
    {
      free (number_buff);
      number_buff = xmalloc (MAX (chars_per_number,
                                  INT_STRLEN_BOUND (line_number)) + 1);
    }

   
  free (clump_buff);
  clump_buff = xmalloc (MAX (8, chars_per_input_tab));
}

 

static bool
init_fps (int number_of_files, char **av)
{
  COLUMN *p;

  total_files = 0;

  free (column_vector);
  column_vector = xnmalloc (columns, sizeof (COLUMN));

  if (parallel_files)
    {
      int files_left = number_of_files;
      for (p = column_vector; files_left--; ++p, ++av)
        {
          if (! open_file (*av, p))
            {
              --p;
              --columns;
            }
        }
      if (columns == 0)
        return false;
      init_header ("", -1);
    }
  else
    {
      p = column_vector;
      if (number_of_files > 0)
        {
          if (! open_file (*av, p))
            return false;
          init_header (*av, fileno (p->fp));
          p->lines_stored = 0;
        }
      else
        {
          p->name = _("standard input");
          p->fp = stdin;
          have_read_stdin = true;
          p->status = OPEN;
          p->full_page_printed = false;
          ++total_files;
          init_header ("", -1);
          p->lines_stored = 0;
        }

      char const *firstname = p->name;
      FILE *firstfp = p->fp;
      int i;
      for (i = columns - 1, ++p; i; --i, ++p)
        {
          p->name = firstname;
          p->fp = firstfp;
          p->status = OPEN;
          p->full_page_printed = false;
          p->lines_stored = 0;
        }
    }
  files_ready_to_read = total_files;
  return true;
}

 

static void
init_funcs (void)
{
  int i, h, h_next;
  COLUMN *p;

  h = chars_per_margin;

  if (!truncate_lines)
    h_next = ANYWHERE;
  else
    {
       
      if (parallel_files && numbered_lines)
        h_next = h + chars_per_column + number_width;
      else
        h_next = h + chars_per_column;
    }

   
  h = h + col_sep_length;

   

  for (p = column_vector, i = 1; i < columns; ++p, ++i)
    {
      if (storing_columns)	 
        {
          p->char_func = store_char;
          p->print_func = print_stored;
        }
      else
         
        {
          p->char_func = print_char;
          p->print_func = read_line;
        }

       
      p->numbered = numbered_lines && (!parallel_files || i == 1);
      p->start_position = h;

       

      if (!truncate_lines)
        {
          h = ANYWHERE;
          h_next = ANYWHERE;
        }
      else
        {
          h = h_next + col_sep_length;
          h_next = h + chars_per_column;
        }
    }

   
  if (storing_columns && balance_columns)
    {
      p->char_func = store_char;
      p->print_func = print_stored;
    }
  else
    {
      p->char_func = print_char;
      p->print_func = read_line;
    }

  p->numbered = numbered_lines && (!parallel_files || i == 1);
  p->start_position = h;
}

 

static bool
open_file (char *name, COLUMN *p)
{
  if (STREQ (name, "-"))
    {
      p->name = _("standard input");
      p->fp = stdin;
      have_read_stdin = true;
    }
  else
    {
      p->name = name;
      p->fp = fopen (name, "r");
    }
  if (p->fp == nullptr)
    {
      failed_opens = true;
      if (!ignore_failed_opens)
        error (0, errno, "%s", quotef (name));
      return false;
    }
  fadvise (p->fp, FADVISE_SEQUENTIAL);
  p->status = OPEN;
  p->full_page_printed = false;
  ++total_files;
  return true;
}

 

static void
close_file (COLUMN *p)
{
  COLUMN *q;
  int i;

  if (p->status == CLOSED)
    return;

  int err = errno;
  if (!ferror (p->fp))
    err = 0;
  if (fileno (p->fp) == STDIN_FILENO)
    clearerr (p->fp);
  else if (fclose (p->fp) != 0 && !err)
    err = errno;
  if (err)
    error (EXIT_FAILURE, err, "%s", quotef (p->name));

  if (!parallel_files)
    {
      for (q = column_vector, i = columns; i; ++q, --i)
        {
          q->status = CLOSED;
          if (q->lines_stored == 0)
            {
              q->lines_to_print = 0;
            }
        }
    }
  else
    {
      p->status = CLOSED;
      p->lines_to_print = 0;
    }

  --files_ready_to_read;
}

 

static void
hold_file (COLUMN *p)
{
  COLUMN *q;
  int i;

  if (!parallel_files)
    for (q = column_vector, i = columns; i; ++q, --i)
      {
        if (storing_columns)
          q->status = FF_FOUND;
        else
          q->status = ON_HOLD;
      }
  else
    p->status = ON_HOLD;

  p->lines_to_print = 0;
  --files_ready_to_read;
}

 

static void
reset_status (void)
{
  int i = columns;
  COLUMN *p;

  for (p = column_vector; i; --i, ++p)
    if (p->status == ON_HOLD)
      {
        p->status = OPEN;
        files_ready_to_read++;
      }

  if (storing_columns)
    {
      if (column_vector->status == CLOSED)
         
        files_ready_to_read = 0;
      else
        files_ready_to_read = 1;
    }
}

 

static void
print_files (int number_of_files, char **av)
{
  init_parameters (number_of_files);
  if (! init_fps (number_of_files, av))
    return;
  if (storing_columns)
    init_store_cols ();

  if (first_page_number > 1)
    {
      if (!skip_to_page (first_page_number))
        return;
      else
        page_number = first_page_number;
    }
  else
    page_number = 1;

  init_funcs ();

  line_number = line_count;
  while (print_page ())
    ;
}

 

static void
init_header (char const *filename, int desc)
{
  char *buf = nullptr;
  struct stat st;
  struct timespec t;
  int ns;
  struct tm tm;

   
  if (STREQ (filename, "-"))
    desc = -1;
  if (0 <= desc && fstat (desc, &st) == 0)
    t = get_stat_mtime (&st);
  else
    {
      static struct timespec timespec;
      if (! timespec.tv_sec)
        gettime (&timespec);
      t = timespec;
    }

  ns = t.tv_nsec;
  if (localtime_rz (localtz, &t.tv_sec, &tm))
    {
      size_t bufsize
        = nstrftime (nullptr, SIZE_MAX, date_format, &tm, localtz, ns) + 1;
      buf = xmalloc (bufsize);
      nstrftime (buf, bufsize, date_format, &tm, localtz, ns);
    }
  else
    {
      char secbuf[INT_BUFSIZE_BOUND (intmax_t)];
      buf = xmalloc (sizeof secbuf + MAX (10, INT_BUFSIZE_BOUND (int)));
      sprintf (buf, "%s.%09d", timetostr (t.tv_sec, secbuf), ns);
    }

  free (date_text);
  date_text = buf;
  file_text = custom_header ? custom_header : desc < 0 ? "" : filename;
  header_width_available = (chars_per_line
                            - mbswidth (date_text, 0)
                            - mbswidth (file_text, 0));
}

 

static void
init_page (void)
{
  int j;
  COLUMN *p;

  if (storing_columns)
    {
      store_columns ();
      for (j = columns - 1, p = column_vector; j; --j, ++p)
        {
          p->lines_to_print = p->lines_stored;
        }

       
      if (balance_columns)
        {
          p->lines_to_print = p->lines_stored;
        }
       
      else
        {
          if (p->status == OPEN)
            {
              p->lines_to_print = lines_per_body;
            }
          else
            p->lines_to_print = 0;
        }
    }
  else
    for (j = columns, p = column_vector; j; --j, ++p)
      if (p->status == OPEN)
        {
          p->lines_to_print = lines_per_body;
        }
      else
        p->lines_to_print = 0;
}

 

static void
align_column (COLUMN *p)
{
  padding_not_printed = p->start_position;
  if (col_sep_length < padding_not_printed)
    {
      pad_across_to (padding_not_printed - col_sep_length);
      padding_not_printed = ANYWHERE;
    }

  if (use_col_separator)
    print_sep_string ();

  if (p->numbered)
    add_line_number (p);
}

 

static bool
print_page (void)
{
  int j;
  int lines_left_on_page;
  COLUMN *p;

   
  bool pv;

  init_page ();

  if (cols_ready_to_print () == 0)
    return false;

  if (extremities)
    print_a_header = true;

   
  pad_vertically = false;
  pv = false;

  lines_left_on_page = lines_per_body;
  if (double_space)
    lines_left_on_page *= 2;

  while (lines_left_on_page > 0 && cols_ready_to_print () > 0)
    {
      output_position = 0;
      spaces_not_printed = 0;
      separators_not_printed = 0;
      pad_vertically = false;
      align_empty_cols = false;
      empty_line = true;

      for (j = 1, p = column_vector; j <= columns; ++j, ++p)
        {
          input_position = 0;
          if (p->lines_to_print > 0 || p->status == FF_FOUND)
            {
              FF_only = false;
              padding_not_printed = p->start_position;
              if (!(p->print_func) (p))
                read_rest_of_line (p);
              pv |= pad_vertically;

              --p->lines_to_print;
              if (p->lines_to_print <= 0)
                {
                  if (cols_ready_to_print () == 0)
                    break;
                }

               
              if (parallel_files && p->status != OPEN)
                {
                  if (empty_line)
                    align_empty_cols = true;
                  else if (p->status == CLOSED
                           || (p->status == ON_HOLD && FF_only))
                    align_column (p);
                }
            }
          else if (parallel_files)
            {
               
              if (empty_line)
                align_empty_cols = true;
              else
                align_column (p);
            }

           
          if (use_col_separator)
            ++separators_not_printed;
        }

      if (pad_vertically)
        {
          putchar ('\n');
          --lines_left_on_page;
        }

      if (cols_ready_to_print () == 0 && !extremities)
        break;

      if (double_space && pv)
        {
          putchar ('\n');
          --lines_left_on_page;
        }
    }

  if (lines_left_on_page == 0)
    for (j = 1, p = column_vector; j <= columns; ++j, ++p)
      if (p->status == OPEN)
        p->full_page_printed = true;

  pad_vertically = pv;

  if (pad_vertically && extremities)
    pad_down (lines_left_on_page + lines_per_footer);
  else if (keep_FF && print_a_FF)
    {
      putchar ('\f');
      print_a_FF = false;
    }

  if (last_page_number < ++page_number)
    return false;		 

  reset_status ();		 

  return true;			 
}

 

static void
init_store_cols (void)
{
  int total_lines, total_lines_1, chars_per_column_1, chars_if_truncate;
  if (ckd_mul (&total_lines, lines_per_body, columns)
      || ckd_add (&total_lines_1, total_lines, 1)
      || ckd_add (&chars_per_column_1, chars_per_column, 1)
      || ckd_mul (&chars_if_truncate, total_lines, chars_per_column_1))
    integer_overflow ();

  free (line_vector);
   
  line_vector = xnmalloc (total_lines_1, sizeof *line_vector);

  free (end_vector);
  end_vector = xnmalloc (total_lines, sizeof *end_vector);

  free (buff);
  buff = xnmalloc (chars_if_truncate, use_col_separator + 1);
  buff_allocated = chars_if_truncate;   
  buff_allocated *= use_col_separator + 1;
}

 

static void
store_columns (void)
{
  int i, j;
  unsigned int line = 0;
  unsigned int buff_start;
  int last_col;		 
  COLUMN *p;

  buff_current = 0;
  buff_start = 0;

  if (balance_columns)
    last_col = columns;
  else
    last_col = columns - 1;

  for (i = 1, p = column_vector; i <= last_col; ++i, ++p)
    p->lines_stored = 0;

  for (i = 1, p = column_vector; i <= last_col && files_ready_to_read;
       ++i, ++p)
    {
      p->current_line = line;
      for (j = lines_per_body; j && files_ready_to_read; --j)

        if (p->status == OPEN)	 
          {
            input_position = 0;

            if (!read_line (p))
              read_rest_of_line (p);

            if (p->status == OPEN
                || buff_start != buff_current)
              {
                ++p->lines_stored;
                line_vector[line] = buff_start;
                end_vector[line++] = input_position;
                buff_start = buff_current;
              }
          }
    }

   
  line_vector[line] = buff_start;

  if (balance_columns)
    balance (line);
}

static void
balance (int total_stored)
{
  COLUMN *p;
  int i, lines;
  int first_line = 0;

  for (i = 1, p = column_vector; i <= columns; ++i, ++p)
    {
      lines = total_stored / columns;
      if (i <= total_stored % columns)
        ++lines;

      p->lines_stored = lines;
      p->current_line = first_line;

      first_line += lines;
    }
}

 

static void
store_char (char c)
{
  if (buff_current >= buff_allocated)
    {
       
      buff = X2REALLOC (buff, &buff_allocated);
    }
  buff[buff_current++] = c;
}

static void
add_line_number (COLUMN *p)
{
  int i;
  char *s;
  int num_width;

   
  num_width = sprintf (number_buff, "%*d", chars_per_number, line_number);
  line_number++;
  s = number_buff + (num_width - chars_per_number);
  for (i = chars_per_number; i > 0; i--)
    (p->char_func) (*s++);

  if (columns > 1)
    {
       
      if (number_separator == '\t')
        {
          i = number_width - chars_per_number;
          while (i-- > 0)
            (p->char_func) (' ');
        }
      else
        (p->char_func) (number_separator);
    }
  else
     
    {
      (p->char_func) (number_separator);
      if (number_separator == '\t')
        output_position = POS_AFTER_TAB (chars_per_output_tab,
                          output_position);
    }

  if (truncate_lines && !parallel_files)
    input_position += number_width;
}

 

static void
pad_across_to (int position)
{
  int h = output_position;

  if (tabify_output)
    spaces_not_printed = position - output_position;
  else
    {
      while (++h <= position)
        putchar (' ');
      output_position = position;
    }
}

 

static void
pad_down (unsigned int lines)
{
  if (use_form_feed)
    putchar ('\f');
  else
    for (unsigned int i = lines; i; --i)
      putchar ('\n');
}

 

static void
read_rest_of_line (COLUMN *p)
{
  int c;
  FILE *f = p->fp;

  while ((c = getc (f)) != '\n')
    {
      if (c == '\f')
        {
          if ((c = getc (f)) != '\n')
            ungetc (c, f);
          if (keep_FF)
            print_a_FF = true;
          hold_file (p);
          break;
        }
      else if (c == EOF)
        {
          close_file (p);
          break;
        }
    }
}

 

static void
skip_read (COLUMN *p, int column_number)
{
  int c;
  FILE *f = p->fp;
  int i;
  bool single_ff = false;
  COLUMN *q;

   
  if ((c = getc (f)) == '\f' && p->full_page_printed)
     
    if ((c = getc (f)) == '\n')
      c = getc (f);

  p->full_page_printed = false;

   
  if (c == '\f')
    single_ff = true;

   
  if (last_line)
    p->full_page_printed = true;

  while (c != '\n')
    {
      if (c == '\f')
        {
           
          if (last_line)
            {
              if (!parallel_files)
                for (q = column_vector, i = columns; i; ++q, --i)
                  q->full_page_printed = false;
              else
                p->full_page_printed = false;
            }

          if ((c = getc (f)) != '\n')
            ungetc (c, f);
          hold_file (p);
          break;
        }
      else if (c == EOF)
        {
          close_file (p);
          break;
        }
      c = getc (f);
    }

  if (skip_count)
    if ((!parallel_files || column_number == 1) && !single_ff)
      ++line_count;
}

 

static void
print_white_space (void)
{
  int h_new;
  int h_old = output_position;
  int goal = h_old + spaces_not_printed;

  while (goal - h_old > 1
         && (h_new = POS_AFTER_TAB (chars_per_output_tab, h_old)) <= goal)
    {
      putchar (output_tab_char);
      h_old = h_new;
    }
  while (++h_old <= goal)
    putchar (' ');

  output_position = goal;
  spaces_not_printed = 0;
}

 

static void
print_sep_string (void)
{
  char const *s = col_sep_string;
  int l = col_sep_length;

  if (separators_not_printed <= 0)
    {
       
      if (spaces_not_printed > 0)
        print_white_space ();
    }
  else
    {
      for (; separators_not_printed > 0; --separators_not_printed)
        {
          while (l-- > 0)
            {
               
              if (*s == ' ')
                {
                   
                  s++;
                  ++spaces_not_printed;
                }
              else
                {
                  if (spaces_not_printed > 0)
                    print_white_space ();
                  putchar (*s++);
                  ++output_position;
                }
            }
           
          if (spaces_not_printed > 0)
            print_white_space ();
        }
    }
}

 

static void
print_clump (COLUMN *p, int n, char *clump)
{
  while (n--)
    (p->char_func) (*clump++);
}

 

static void
print_char (char c)
{
  if (tabify_output)
    {
      if (c == ' ')
        {
          ++spaces_not_printed;
          return;
        }
      else if (spaces_not_printed > 0)
        print_white_space ();

       
      if (! isprint (to_uchar (c)))
        {
          if (c == '\b')
            --output_position;
        }
      else
        ++output_position;
    }
  putchar (c);
}

 

static bool
skip_to_page (uintmax_t page)
{
  for (uintmax_t n = 1; n < page; ++n)
    {
      COLUMN *p;
      int j;

      for (int i = 1; i < lines_per_body; ++i)
        {
          for (j = 1, p = column_vector; j <= columns; ++j, ++p)
            if (p->status == OPEN)
              skip_read (p, j);
        }
      last_line = true;
      for (j = 1, p = column_vector; j <= columns; ++j, ++p)
        if (p->status == OPEN)
          skip_read (p, j);

      if (storing_columns)	 
        for (j = 1, p = column_vector; j <= columns; ++j, ++p)
          if (p->status != CLOSED)
            p->status = ON_HOLD;

      reset_status ();
      last_line = false;

      if (files_ready_to_read < 1)
        {
           
          error (0, 0,
                 _("starting page number %"PRIuMAX
                   " exceeds page count %"PRIuMAX),
                 page, n);
          break;
        }
    }
  return files_ready_to_read > 0;
}

 

static void
print_header (void)
{
  char page_text[256 + INT_STRLEN_BOUND (page_number)];
  int available_width;
  int lhs_spaces;
  int rhs_spaces;

  output_position = 0;
  pad_across_to (chars_per_margin);
  print_white_space ();

  if (page_number == 0)
    error (EXIT_FAILURE, 0, _("page number overflow"));

   
  sprintf (page_text, _("Page %"PRIuMAX), page_number);
  available_width = header_width_available - mbswidth (page_text, 0);
  available_width = MAX (0, available_width);
  lhs_spaces = available_width >> 1;
  rhs_spaces = available_width - lhs_spaces;

  printf ("\n\n%*s%s%*s%s%*s%s\n\n\n",
          chars_per_margin, "",
          date_text, lhs_spaces, " ",
          file_text, rhs_spaces, " ", page_text);

  print_a_header = false;
  output_position = 0;
}

 

static bool
read_line (COLUMN *p)
{
  int c;
  int chars;
  int last_input_position;
  int j, k;
  COLUMN *q;

   
  c = getc (p->fp);

  last_input_position = input_position;

  if (c == '\f' && p->full_page_printed)
    if ((c = getc (p->fp)) == '\n')
      c = getc (p->fp);
  p->full_page_printed = false;

  switch (c)
    {
    case '\f':
      if ((c = getc (p->fp)) != '\n')
        ungetc (c, p->fp);
      FF_only = true;
      if (print_a_header && !storing_columns)
        {
          pad_vertically = true;
          print_header ();
        }
      else if (keep_FF)
        print_a_FF = true;
      hold_file (p);
      return true;
    case EOF:
      close_file (p);
      return true;
    case '\n':
      break;
    default:
      chars = char_to_clump (c);
    }

  if (truncate_lines && input_position > chars_per_column)
    {
      input_position = last_input_position;
      return false;
    }

  if (p->char_func != store_char)
    {
      pad_vertically = true;

      if (print_a_header && !storing_columns)
        print_header ();

      if (parallel_files && align_empty_cols)
        {
           
          k = separators_not_printed;
          separators_not_printed = 0;
          for (j = 1, q = column_vector; j <= k; ++j, ++q)
            {
              align_column (q);
              separators_not_printed += 1;
            }
          padding_not_printed = p->start_position;
          if (truncate_lines)
            spaces_not_printed = chars_per_column;
          else
            spaces_not_printed = 0;
          align_empty_cols = false;
        }

      if (col_sep_length < padding_not_printed)
        {
          pad_across_to (padding_not_printed - col_sep_length);
          padding_not_printed = ANYWHERE;
        }

      if (use_col_separator)
        print_sep_string ();
    }

  if (p->numbered)
    add_line_number (p);

  empty_line = false;
  if (c == '\n')
    return true;

  print_clump (p, chars, clump_buff);

  while (true)
    {
      c = getc (p->fp);

      switch (c)
        {
        case '\n':
          return true;
        case '\f':
          if ((c = getc (p->fp)) != '\n')
            ungetc (c, p->fp);
          if (keep_FF)
            print_a_FF = true;
          hold_file (p);
          return true;
        case EOF:
          close_file (p);
          return true;
        }

      last_input_position = input_position;
      chars = char_to_clump (c);
      if (truncate_lines && input_position > chars_per_column)
        {
          input_position = last_input_position;
          return false;
        }

      print_clump (p, chars, clump_buff);
    }
}

 

static bool
print_stored (COLUMN *p)
{
  COLUMN *q;

  int line = p->current_line++;
  char *first = &buff[line_vector[line]];
   
  char *last = &buff[line_vector[line + 1]];

  pad_vertically = true;

  if (print_a_header)
    print_header ();

  if (p->status == FF_FOUND)
    {
      int i;
      for (i = 1, q = column_vector; i <= columns; ++i, ++q)
        q->status = ON_HOLD;
      if (column_vector->lines_to_print <= 0)
        {
          if (!extremities)
            pad_vertically = false;
          return true;		 
        }
    }

  if (col_sep_length < padding_not_printed)
    {
      pad_across_to (padding_not_printed - col_sep_length);
      padding_not_printed = ANYWHERE;
    }

  if (use_col_separator)
    print_sep_string ();

  while (first != last)
    print_char (*first++);

  if (spaces_not_printed == 0)
    {
      output_position = p->start_position + end_vector[line];
      if (p->start_position - col_sep_length == chars_per_margin)
        output_position -= col_sep_length;
    }

  return true;
}

 

static int
char_to_clump (char c)
{
  unsigned char uc = c;
  char *s = clump_buff;
  int i;
  char esc_buff[4];
  int width;
  int chars;
  int chars_per_c = 8;

  if (c == input_tab_char)
    chars_per_c = chars_per_input_tab;

  if (c == input_tab_char || c == '\t')
    {
      width = TAB_WIDTH (chars_per_c, input_position);

      if (untabify_input)
        {
          for (i = width; i; --i)
            *s++ = ' ';
          chars = width;
        }
      else
        {
          *s = c;
          chars = 1;
        }

    }
  else if (! isprint (uc))
    {
      if (use_esc_sequence)
        {
          width = 4;
          chars = 4;
          *s++ = '\\';
          sprintf (esc_buff, "%03o", uc);
          for (i = 0; i <= 2; ++i)
            *s++ = esc_buff[i];
        }
      else if (use_cntrl_prefix)
        {
          if (uc < 0200)
            {
              width = 2;
              chars = 2;
              *s++ = '^';
              *s = c ^ 0100;
            }
          else
            {
              width = 4;
              chars = 4;
              *s++ = '\\';
              sprintf (esc_buff, "%03o", uc);
              for (i = 0; i <= 2; ++i)
                *s++ = esc_buff[i];
            }
        }
      else if (c == '\b')
        {
          width = -1;
          chars = 1;
          *s = c;
        }
      else
        {
          width = 0;
          chars = 1;
          *s = c;
        }
    }
  else
    {
      width = 1;
      chars = 1;
      *s = c;
    }

   
  if (width < 0 && input_position == 0)
    {
      chars = 0;
      input_position = 0;
    }
  else if (width < 0 && input_position <= -width)
    input_position = 0;
  else
    input_position += width;

  return chars;
}

 

static void
cleanup (void)
{
  free (number_buff);
  free (clump_buff);
  free (column_vector);
  free (line_vector);
  free (end_vector);
  free (buff);
}

 

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
              program_name);

      fputs (_("\
Paginate or columnate FILE(s) for printing.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
  +FIRST_PAGE[:LAST_PAGE], --pages=FIRST_PAGE[:LAST_PAGE]\n\
                    begin [stop] printing with page FIRST_[LAST_]PAGE\n\
  -COLUMN, --columns=COLUMN\n\
                    output COLUMN columns and print columns down,\n\
                    unless -a is used. Balance number of lines in the\n\
                    columns on each page\n\
"), stdout);
      fputs (_("\
  -a, --across      print columns across rather than down, used together\n\
                    with -COLUMN\n\
  -c, --show-control-chars\n\
                    use hat notation (^G) and octal backslash notation\n\
  -d, --double-space\n\
                    double space the output\n\
"), stdout);
      fputs (_("\
  -D, --date-format=FORMAT\n\
                    use FORMAT for the header date\n\
  -e[CHAR[WIDTH]], --expand-tabs[=CHAR[WIDTH]]\n\
                    expand input CHARs (TABs) to tab WIDTH (8)\n\
  -F, -f, --form-feed\n\
                    use form feeds instead of newlines to separate pages\n\
                    (by a 3-line page header with -F or a 5-line header\n\
                    and trailer without -F)\n\
"), stdout);
      fputs (_("\
  -h, --header=HEADER\n\
                    use a centered HEADER instead of filename in page header,\n\
                    -h \"\" prints a blank line, don't use -h\"\"\n\
  -i[CHAR[WIDTH]], --output-tabs[=CHAR[WIDTH]]\n\
                    replace spaces with CHARs (TABs) to tab WIDTH (8)\n\
  -J, --join-lines  merge full lines, turns off -W line truncation, no column\n\
                    alignment, --sep-string[=STRING] sets separators\n\
"), stdout);
      fputs (_("\
  -l, --length=PAGE_LENGTH\n\
                    set the page length to PAGE_LENGTH (66) lines\n\
                    (default number of lines of text 56, and with -F 63).\n\
                    implies -t if PAGE_LENGTH <= 10\n\
"), stdout);
      fputs (_("\
  -m, --merge       print all files in parallel, one in each column,\n\
                    truncate lines, but join lines of full length with -J\n\
"), stdout);
      fputs (_("\
  -n[SEP[DIGITS]], --number-lines[=SEP[DIGITS]]\n\
                    number lines, use DIGITS (5) digits, then SEP (TAB),\n\
                    default counting starts with 1st line of input file\n\
  -N, --first-line-number=NUMBER\n\
                    start counting with NUMBER at 1st line of first\n\
                    page printed (see +FIRST_PAGE)\n\
"), stdout);
      fputs (_("\
  -o, --indent=MARGIN\n\
                    offset each line with MARGIN (zero) spaces, do not\n\
                    affect -w or -W, MARGIN will be added to PAGE_WIDTH\n\
  -r, --no-file-warnings\n\
                    omit warning when a file cannot be opened\n\
"), stdout);
      fputs (_("\
  -s[CHAR], --separator[=CHAR]\n\
                    separate columns by a single character, default for CHAR\n\
                    is the <TAB> character without -w and \'no char\' with -w.\
\n\
                    -s[CHAR] turns off line truncation of all 3 column\n\
                    options (-COLUMN|-a -COLUMN|-m) except -w is set\n\
"), stdout);
      fputs (_("\
  -S[STRING], --sep-string[=STRING]\n\
                    separate columns by STRING,\n\
                    without -S: Default separator <TAB> with -J and <space>\n\
                    otherwise (same as -S\" \"), no effect on column options\n\
"), stdout);
      fputs (_("\
  -t, --omit-header  omit page headers and trailers;\n\
                     implied if PAGE_LENGTH <= 10\n\
"), stdout);
      fputs (_("\
  -T, --omit-pagination\n\
                    omit page headers and trailers, eliminate any pagination\n\
                    by form feeds set in input files\n\
  -v, --show-nonprinting\n\
                    use octal backslash notation\n\
  -w, --width=PAGE_WIDTH\n\
                    set page width to PAGE_WIDTH (72) characters for\n\
                    multiple text-column output only, -s[char] turns off (72)\n\
"), stdout);
      fputs (_("\
  -W, --page-width=PAGE_WIDTH\n\
                    set page width to PAGE_WIDTH (72) characters always,\n\
                    truncate lines, except -J option is set, no interference\n\
                    with -S or -s\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}
