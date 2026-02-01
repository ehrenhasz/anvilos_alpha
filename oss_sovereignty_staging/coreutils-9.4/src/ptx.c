 

#include <config.h>

#include <getopt.h>
#include <sys/types.h>
#include "system.h"
#include <regex.h>
#include "argmatch.h"
#include "fadvise.h"
#include "quote.h"
#include "read-file.h"
#include "stdio--.h"
#include "xstrtol.h"

 
#define PROGRAM_NAME "ptx"

 
#define AUTHORS proper_name_lite ("F. Pinard", "Fran\xc3\xa7ois Pinard")

 
#define CHAR_SET_SIZE 256

#define ISODIGIT(C) ((C) >= '0' && (C) <= '7')
#define HEXTOBIN(C) ((C) >= 'a' && (C) <= 'f' ? (C)-'a'+10 \
                     : (C) >= 'A' && (C) <= 'F' ? (C)-'A'+10 : (C)-'0')
#define OCTTOBIN(C) ((C) - '0')

 

#if WITH_DMALLOC
# define MALLOC_FUNC_CHECK 1
# include <dmalloc.h>
#endif

 

 

 

enum Format
{
  UNKNOWN_FORMAT,		 
  DUMB_FORMAT,			 
  ROFF_FORMAT,			 
  TEX_FORMAT			 
};

static bool gnu_extensions = true;	 
static bool auto_reference = false;	 
static bool input_reference = false;	 
static bool right_reference = false;	 
static ptrdiff_t line_width = 72;	 
static ptrdiff_t gap_size = 3;	 
static char const *truncation_string = "/";
                                 
static char const *macro_name = "xx";	 
static enum Format output_format = UNKNOWN_FORMAT;
                                 

static bool ignore_case = false;	 
static char const *break_file = nullptr;  
static char const *only_file = nullptr;	 
static char const *ignore_file = nullptr;  

 
struct regex_data
{
   
  char const *string;

   
  struct re_pattern_buffer pattern;
  char fastmap[UCHAR_MAX + 1];
};

static struct regex_data context_regex;	 
static struct regex_data word_regex;	 

 

typedef struct
  {
    char *start;		 
    char *end;			 
  }
BLOCK;

typedef struct
  {
    char *start;		 
    ptrdiff_t size;		 
  }
WORD;

typedef struct
  {
    WORD *start;		 
    size_t alloc;		 
    ptrdiff_t length;		 
  }
WORD_TABLE;

 

 
static unsigned char folded_chars[CHAR_SET_SIZE];

 
static struct re_registers context_regs;

 
static struct re_registers word_regs;

 
static char word_fastmap[CHAR_SET_SIZE];

 
static ptrdiff_t maximum_word_length;

 
static ptrdiff_t reference_max_width;

 

static WORD_TABLE ignore_table;	 
static WORD_TABLE only_table;		 

 

static int number_input_files;	 
static intmax_t total_line_count;	 
static char const **input_file_name;	 
static intmax_t *file_line_count;	 

static BLOCK *text_buffers;	 

 

#define SKIP_NON_WHITE(cursor, limit) \
  while (cursor < limit && ! isspace (to_uchar (*cursor)))		\
    cursor++

#define SKIP_WHITE(cursor, limit) \
  while (cursor < limit && isspace (to_uchar (*cursor)))		\
    cursor++

#define SKIP_WHITE_BACKWARDS(cursor, start) \
  while (cursor > start && isspace (to_uchar (cursor[-1])))		\
    cursor--

#define SKIP_SOMETHING(cursor, limit) \
  if (word_regex.string)						\
    {									\
      regoff_t count;							\
      count = re_match (&word_regex.pattern, cursor, limit - cursor,	\
                        0, nullptr);					\
      if (count == -2)							\
        matcher_error ();						\
      cursor += count == -1 ? 1 : count;				\
    }									\
  else if (word_fastmap[to_uchar (*cursor)])				\
    while (cursor < limit && word_fastmap[to_uchar (*cursor)])		\
      cursor++;								\
  else									\
    cursor++

 

typedef struct
  {
    WORD key;			 
    ptrdiff_t left;		 
    ptrdiff_t right;		 
    intmax_t reference;		 
    int file_index;		 
  }
OCCURS;

 

static OCCURS *occurs_table[1];	 
static size_t occurs_alloc[1];	 
static ptrdiff_t number_of_occurs[1];  


 

 
static char edited_flag[CHAR_SET_SIZE];

 
static ptrdiff_t half_line_width;

 
static ptrdiff_t before_max_width;

 
static ptrdiff_t keyafter_max_width;

 
static ptrdiff_t truncation_string_length;

 

static BLOCK tail;		 
static bool tail_truncation;	 

static BLOCK before;		 
static bool before_truncation;	 

static BLOCK keyafter;		 
static bool keyafter_truncation;  

static BLOCK head;		 
static bool head_truncation;	 

static BLOCK reference;		 

 

 

static void
matcher_error (void)
{
  error (EXIT_FAILURE, errno, _("error in regular expression matcher"));
}

 

static void
unescape_string (char *string)
{
  char *cursor;			 
  int value;			 
  int length;			 

  cursor = string;

  while (*string)
    {
      if (*string == '\\')
        {
          string++;
          switch (*string)
            {
            case 'x':		 
              value = 0;
              for (length = 0, string++;
                   length < 3 && isxdigit (to_uchar (*string));
                   length++, string++)
                value = value * 16 + HEXTOBIN (*string);
              if (length == 0)
                {
                  *cursor++ = '\\';
                  *cursor++ = 'x';
                }
              else
                *cursor++ = value;
              break;

            case '0':		 
              value = 0;
              for (length = 0, string++;
                   length < 3 && ISODIGIT (*string);
                   length++, string++)
                value = value * 8 + OCTTOBIN (*string);
              *cursor++ = value;
              break;

            case 'a':		 
#if __STDC__
              *cursor++ = '\a';
#else
              *cursor++ = 7;
#endif
              string++;
              break;

            case 'b':		 
              *cursor++ = '\b';
              string++;
              break;

            case 'c':		 
              while (*string)
                string++;
              break;

            case 'f':		 
              *cursor++ = '\f';
              string++;
              break;

            case 'n':		 
              *cursor++ = '\n';
              string++;
              break;

            case 'r':		 
              *cursor++ = '\r';
              string++;
              break;

            case 't':		 
              *cursor++ = '\t';
              string++;
              break;

            case 'v':		 
#if __STDC__
              *cursor++ = '\v';
#else
              *cursor++ = 11;
#endif
              string++;
              break;

            case '\0':		 
               
              break;

            default:
              *cursor++ = '\\';
              *cursor++ = *string++;
              break;
            }
        }
      else
        *cursor++ = *string++;
    }

  *cursor = '\0';
}

 

static void
compile_regex (struct regex_data *regex)
{
  struct re_pattern_buffer *pattern = &regex->pattern;
  char const *string = regex->string;
  char const *message;

  pattern->buffer = nullptr;
  pattern->allocated = 0;
  pattern->fastmap = regex->fastmap;
  pattern->translate = ignore_case ? folded_chars : nullptr;

  message = re_compile_pattern (string, strlen (string), pattern);
  if (message)
    error (EXIT_FAILURE, 0, _("%s (for regexp %s)"), message, quote (string));

   

  re_compile_fastmap (pattern);
}

 

static void
initialize_regex (void)
{
  int character;		 

   

  if (ignore_case)
    for (character = 0; character < CHAR_SET_SIZE; character++)
      folded_chars[character] = toupper (character);

   

  if (context_regex.string)
    {
      if (!*context_regex.string)
        context_regex.string = nullptr;
    }
  else if (gnu_extensions && !input_reference)
    context_regex.string = "[.?!][]\"')}]*\\($\\|\t\\|  \\)[ \t\n]*";
  else
    context_regex.string = "\n";

  if (context_regex.string)
    compile_regex (&context_regex);

   

  if (word_regex.string)
    compile_regex (&word_regex);
  else if (!break_file)
    {
      if (gnu_extensions)
        {

           

          for (character = 0; character < CHAR_SET_SIZE; character++)
            word_fastmap[character] = !! isalpha (character);
        }
      else
        {

           

          memset (word_fastmap, 1, CHAR_SET_SIZE);
          word_fastmap[' '] = 0;
          word_fastmap['\t'] = 0;
          word_fastmap['\n'] = 0;
        }
    }
}

 

static void
swallow_file_in_memory (char const *file_name, BLOCK *block)
{
  size_t used_length;		 

   
  bool using_stdin = !file_name || !*file_name || STREQ (file_name, "-");
  if (using_stdin)
    block->start = fread_file (stdin, 0, &used_length);
  else
    block->start = read_file (file_name, 0, &used_length);

  if (!block->start)
    error (EXIT_FAILURE, errno, "%s", quotef (using_stdin ? "-" : file_name));

  if (using_stdin)
    clearerr (stdin);

  block->end = block->start + used_length;
}

 

 

static int
compare_words (const void *void_first, const void *void_second)
{
#define first ((const WORD *) void_first)
#define second ((const WORD *) void_second)
  ptrdiff_t length;		 
  ptrdiff_t counter;		 
  int value;			 

  length = first->size < second->size ? first->size : second->size;

  if (ignore_case)
    {
      for (counter = 0; counter < length; counter++)
        {
          value = (folded_chars [to_uchar (first->start[counter])]
                   - folded_chars [to_uchar (second->start[counter])]);
          if (value != 0)
            return value;
        }
    }
  else
    {
      for (counter = 0; counter < length; counter++)
        {
          value = (to_uchar (first->start[counter])
                   - to_uchar (second->start[counter]));
          if (value != 0)
            return value;
        }
    }

  return (first->size > second->size) - (first->size < second->size);
#undef first
#undef second
}

 

static int
compare_occurs (const void *void_first, const void *void_second)
{
#define first ((const OCCURS *) void_first)
#define second ((const OCCURS *) void_second)
  int value;

  value = compare_words (&first->key, &second->key);
  return (value ? value
          : ((first->key.start > second->key.start)
             - (first->key.start < second->key.start)));
#undef first
#undef second
}

 

ATTRIBUTE_PURE
static bool
search_table (WORD *word, WORD_TABLE *table)
{
  ptrdiff_t lowest;		 
  ptrdiff_t highest;		 
  ptrdiff_t middle;		 
  int value;			 

  lowest = 0;
  highest = table->length - 1;
  while (lowest <= highest)
    {
      middle = (lowest + highest) / 2;
      value = compare_words (word, table->start + middle);
      if (value < 0)
        highest = middle - 1;
      else if (value > 0)
        lowest = middle + 1;
      else
        return true;
    }
  return false;
}

 

static void
sort_found_occurs (void)
{

   
  if (number_of_occurs[0])
    qsort (occurs_table[0], number_of_occurs[0], sizeof **occurs_table,
           compare_occurs);
}

 

 

static void
digest_break_file (char const *file_name)
{
  BLOCK file_contents;		 
  char *cursor;			 

  swallow_file_in_memory (file_name, &file_contents);

   

  memset (word_fastmap, 1, CHAR_SET_SIZE);
  for (cursor = file_contents.start; cursor < file_contents.end; cursor++)
    word_fastmap[to_uchar (*cursor)] = 0;

  if (!gnu_extensions)
    {

       

      word_fastmap[' '] = 0;
      word_fastmap['\t'] = 0;
      word_fastmap['\n'] = 0;
    }

   

  free (file_contents.start);
}

 

static void
digest_word_file (char const *file_name, WORD_TABLE *table)
{
  BLOCK file_contents;		 
  char *cursor;			 
  char *word_start;		 

  swallow_file_in_memory (file_name, &file_contents);

  table->start = nullptr;
  table->alloc = 0;
  table->length = 0;

   

  cursor = file_contents.start;
  while (cursor < file_contents.end)
    {

       

      word_start = cursor;
      while (cursor < file_contents.end && *cursor != '\n')
        cursor++;

       

      if (cursor > word_start)
        {
          if (table->length == table->alloc)
            table->start = x2nrealloc (table->start, &table->alloc,
                                       sizeof *table->start);
          table->start[table->length].start = word_start;
          table->start[table->length].size = cursor - word_start;
          table->length++;
        }

       

      if (cursor < file_contents.end)
        cursor++;
    }

   

  qsort (table->start, table->length, sizeof table->start[0], compare_words);
}

 

 

static void
find_occurs_in_text (int file_index)
{
  char *cursor;			 
  char *scan;			 
  char *line_start;		 
  char *line_scan;		 
  ptrdiff_t reference_length;	 
  WORD possible_key;		 
  OCCURS *occurs_cursor;	 

  char *context_start;		 
  char *context_end;		 
  char *word_start;		 
  char *word_end;		 
  char *next_context_start;	 

  const BLOCK *text_buffer = &text_buffers[file_index];

   

  reference_length = 0;

   

  line_start = text_buffer->start;
  line_scan = line_start;
  if (input_reference)
    {
      SKIP_NON_WHITE (line_scan, text_buffer->end);
      reference_length = line_scan - line_start;
      SKIP_WHITE (line_scan, text_buffer->end);
    }

   

  for (cursor = text_buffer->start;
       cursor < text_buffer->end;
       cursor = next_context_start)
    {

       

      context_start = cursor;

       

      next_context_start = text_buffer->end;
      if (context_regex.string)
        switch (re_search (&context_regex.pattern, cursor,
                           text_buffer->end - cursor,
                           0, text_buffer->end - cursor, &context_regs))
          {
          case -2:
            matcher_error ();

          case -1:
            break;

          case 0:
            error (EXIT_FAILURE, 0,
                   _("error: regular expression has a match of length zero:"
                     " %s"),
                   quote (context_regex.string));

          default:
            next_context_start = cursor + context_regs.end[0];
            break;
          }

       

      context_end = next_context_start;
      SKIP_WHITE_BACKWARDS (context_end, context_start);

       

      while (true)
        {
          if (word_regex.string)

             

            {
              regoff_t r = re_search (&word_regex.pattern, cursor,
                                      context_end - cursor,
                                      0, context_end - cursor, &word_regs);
              if (r == -2)
                matcher_error ();
              if (r == -1)
                break;
              word_start = cursor + word_regs.start[0];
              word_end = cursor + word_regs.end[0];
            }
          else

             

            {
              scan = cursor;
              while (scan < context_end
                     && !word_fastmap[to_uchar (*scan)])
                scan++;

              if (scan == context_end)
                break;

              word_start = scan;

              while (scan < context_end
                     && word_fastmap[to_uchar (*scan)])
                scan++;

              word_end = scan;
            }

           

          cursor = word_start;

           

          if (word_end == word_start)
            {
              cursor++;
              continue;
            }

           

          possible_key.start = cursor;
          possible_key.size = word_end - word_start;
          cursor += possible_key.size;

          if (possible_key.size > maximum_word_length)
            maximum_word_length = possible_key.size;

           

          if (input_reference)
            {
              while (line_scan < possible_key.start)
                if (*line_scan == '\n')
                  {
                    total_line_count++;
                    line_scan++;
                    line_start = line_scan;
                    SKIP_NON_WHITE (line_scan, text_buffer->end);
                    reference_length = line_scan - line_start;
                  }
                else
                  line_scan++;
              if (line_scan > possible_key.start)
                continue;
            }

           

          if (ignore_file && search_table (&possible_key, &ignore_table))
            continue;
          if (only_file && !search_table (&possible_key, &only_table))
            continue;

           

          if (number_of_occurs[0] == occurs_alloc[0])
            occurs_table[0] = x2nrealloc (occurs_table[0],
                                          &occurs_alloc[0],
                                          sizeof *occurs_table[0]);
          occurs_cursor = occurs_table[0] + number_of_occurs[0];

           

          if (auto_reference)
            {

               

              while (line_scan < possible_key.start)
                if (*line_scan == '\n')
                  {
                    total_line_count++;
                    line_scan++;
                    line_start = line_scan;
                    SKIP_NON_WHITE (line_scan, text_buffer->end);
                  }
                else
                  line_scan++;

              occurs_cursor->reference = total_line_count;
            }
          else if (input_reference)
            {

               

              occurs_cursor->reference = line_start - possible_key.start;
              if (reference_length > reference_max_width)
                reference_max_width = reference_length;
            }

           

          if (input_reference && line_start == context_start)
            {
              SKIP_NON_WHITE (context_start, context_end);
              SKIP_WHITE (context_start, context_end);
            }

           

          occurs_cursor->key = possible_key;
          occurs_cursor->left = context_start - possible_key.start;
          occurs_cursor->right = context_end - possible_key.start;
          occurs_cursor->file_index = file_index;

          number_of_occurs[0]++;
        }
    }
}

 

 

static void
print_spaces (ptrdiff_t number)
{
  for (ptrdiff_t counter = number; counter > 0; counter--)
    putchar (' ');
}

 

static void
print_field (BLOCK field)
{
  char *cursor;			 

   

  for (cursor = field.start; cursor < field.end; cursor++)
    {
      unsigned char character = *cursor;
      if (edited_flag[character])
        {
           

          switch (character)
            {
            case '"':
               
              putchar ('"');
              putchar ('"');
              break;

            case '$':
            case '%':
            case '&':
            case '#':
            case '_':
               
              putchar ('\\');
              putchar (character);
              break;

            case '{':
            case '}':
               
              printf ("$\\%c$", character);
              break;

            case '\\':
               
              fputs ("\\backslash{}", stdout);
              break;

            default:
               
              putchar (' ');
            }
        }
      else
        putchar (*cursor);
    }
}

 

 

static void
fix_output_parameters (void)
{
  size_t file_index;		 
  intmax_t line_ordinal;	 
  ptrdiff_t reference_width;	 
  int character;		 
  char const *cursor;		 

   

  if (auto_reference)
    {
      reference_max_width = 0;
      for (file_index = 0; file_index < number_input_files; file_index++)
        {
          line_ordinal = file_line_count[file_index] + 1;
          if (file_index > 0)
            line_ordinal -= file_line_count[file_index - 1];
          char ordinal_string[INT_BUFSIZE_BOUND (intmax_t)];
          reference_width = sprintf (ordinal_string, "%"PRIdMAX, line_ordinal);
          if (input_file_name[file_index])
            reference_width += strlen (input_file_name[file_index]);
          if (reference_width > reference_max_width)
            reference_max_width = reference_width;
        }
      reference_max_width++;
      reference.start = xmalloc (reference_max_width + 1);
    }

   

  if ((auto_reference || input_reference) && !right_reference)
    line_width -= reference_max_width + gap_size;
  if (line_width < 0)
    line_width = 0;

   

  half_line_width = line_width / 2;
  before_max_width = half_line_width - gap_size;
  keyafter_max_width = half_line_width;

   

  if (truncation_string && *truncation_string)
    truncation_string_length = strlen (truncation_string);
  else
    truncation_string = nullptr;

  if (gnu_extensions)
    {

       

      before_max_width -= 2 * truncation_string_length;
      if (before_max_width < 0)
        before_max_width = 0;
      keyafter_max_width -= 2 * truncation_string_length;
    }
  else
    {

       

      keyafter_max_width -= 2 * truncation_string_length + 1;
    }

   

  for (character = 0; character < CHAR_SET_SIZE; character++)
    edited_flag[character] = !! isspace (character);
  edited_flag['\f'] = 1;

   

  switch (output_format)
    {
    case UNKNOWN_FORMAT:
       

    case DUMB_FORMAT:
      break;

    case ROFF_FORMAT:

       

      edited_flag['"'] = 1;
      break;

    case TEX_FORMAT:

       

      for (cursor = "$%&#_{}\\"; *cursor; cursor++)
        edited_flag[to_uchar (*cursor)] = 1;

      break;
    }
}

 

static void
define_all_fields (OCCURS *occurs)
{
  ptrdiff_t tail_max_width;	 
  ptrdiff_t head_max_width;	 
  char *cursor;			 
  char *left_context_start;	 
  char *right_context_end;	 
  char *left_field_start;	 
  char const *file_name;	 
  intmax_t line_ordinal;	 
  char const *buffer_start;	 
  char const *buffer_end;	 

   

  keyafter.start = occurs->key.start;
  keyafter.end = keyafter.start + occurs->key.size;
  left_context_start = keyafter.start + occurs->left;
  right_context_end = keyafter.start + occurs->right;

  buffer_start = text_buffers[occurs->file_index].start;
  buffer_end = text_buffers[occurs->file_index].end;

  cursor = keyafter.end;
  while (cursor < right_context_end
         && cursor <= keyafter.start + keyafter_max_width)
    {
      keyafter.end = cursor;
      SKIP_SOMETHING (cursor, right_context_end);
    }
  if (cursor <= keyafter.start + keyafter_max_width)
    keyafter.end = cursor;

  keyafter_truncation = truncation_string && keyafter.end < right_context_end;

  SKIP_WHITE_BACKWARDS (keyafter.end, keyafter.start);

   

  if (-occurs->left > half_line_width + maximum_word_length)
    {
      left_field_start
        = keyafter.start - (half_line_width + maximum_word_length);
      SKIP_SOMETHING (left_field_start, keyafter.start);
    }
  else
    left_field_start = keyafter.start + occurs->left;

   

  before.start = left_field_start;
  before.end = keyafter.start;
  SKIP_WHITE_BACKWARDS (before.end, before.start);

  while (before.start + before_max_width < before.end)
    SKIP_SOMETHING (before.start, before.end);

  if (truncation_string)
    {
      cursor = before.start;
      SKIP_WHITE_BACKWARDS (cursor, buffer_start);
      before_truncation = cursor > left_context_start;
    }
  else
    before_truncation = false;

  SKIP_WHITE (before.start, buffer_end);

   

  tail_max_width
    = before_max_width - (before.end - before.start) - gap_size;

  if (tail_max_width > 0)
    {
      tail.start = keyafter.end;
      SKIP_WHITE (tail.start, buffer_end);

      tail.end = tail.start;
      cursor = tail.end;
      while (cursor < right_context_end
             && cursor < tail.start + tail_max_width)
        {
          tail.end = cursor;
          SKIP_SOMETHING (cursor, right_context_end);
        }

      if (cursor < tail.start + tail_max_width)
        tail.end = cursor;

      if (tail.end > tail.start)
        {
          keyafter_truncation = false;
          tail_truncation = truncation_string && tail.end < right_context_end;
        }
      else
        tail_truncation = false;

      SKIP_WHITE_BACKWARDS (tail.end, tail.start);
    }
  else
    {

       

      tail.start = nullptr;
      tail.end = nullptr;
      tail_truncation = false;
    }

   

  head_max_width
    = keyafter_max_width - (keyafter.end - keyafter.start) - gap_size;

  if (head_max_width > 0)
    {
      head.end = before.start;
      SKIP_WHITE_BACKWARDS (head.end, buffer_start);

      head.start = left_field_start;
      while (head.start + head_max_width < head.end)
        SKIP_SOMETHING (head.start, head.end);

      if (head.end > head.start)
        {
          before_truncation = false;
          head_truncation = (truncation_string
                             && head.start > left_context_start);
        }
      else
        head_truncation = false;

      SKIP_WHITE (head.start, head.end);
    }
  else
    {

       

      head.start = nullptr;
      head.end = nullptr;
      head_truncation = false;
    }

  if (auto_reference)
    {

       

      file_name = input_file_name[occurs->file_index];
      if (!file_name)
        file_name = "";

      line_ordinal = occurs->reference + 1;
      if (occurs->file_index > 0)
        line_ordinal -= file_line_count[occurs->file_index - 1];

      char *file_end = stpcpy (reference.start, file_name);
      reference.end = file_end + sprintf (file_end, ":%"PRIdMAX, line_ordinal);
    }
  else if (input_reference)
    {

       

      reference.start = keyafter.start + occurs->reference;
      reference.end = reference.start;
      SKIP_NON_WHITE (reference.end, right_context_end);
    }
}

 

 

static void
output_one_roff_line (void)
{
   

  printf (".%s \"", macro_name);
  print_field (tail);
  if (tail_truncation)
    fputs (truncation_string, stdout);
  putchar ('"');

   

  fputs (" \"", stdout);
  if (before_truncation)
    fputs (truncation_string, stdout);
  print_field (before);
  putchar ('"');

   

  fputs (" \"", stdout);
  print_field (keyafter);
  if (keyafter_truncation)
    fputs (truncation_string, stdout);
  putchar ('"');

   

  fputs (" \"", stdout);
  if (head_truncation)
    fputs (truncation_string, stdout);
  print_field (head);
  putchar ('"');

   

  if (auto_reference || input_reference)
    {
      fputs (" \"", stdout);
      print_field (reference);
      putchar ('"');
    }

  putchar ('\n');
}

 

static void
output_one_tex_line (void)
{
  BLOCK key;			 
  BLOCK after;			 
  char *cursor;			 

  printf ("\\%s ", macro_name);
  putchar ('{');
  print_field (tail);
  fputs ("}{", stdout);
  print_field (before);
  fputs ("}{", stdout);
  key.start = keyafter.start;
  after.end = keyafter.end;
  cursor = keyafter.start;
  SKIP_SOMETHING (cursor, keyafter.end);
  key.end = cursor;
  after.start = cursor;
  print_field (key);
  fputs ("}{", stdout);
  print_field (after);
  fputs ("}{", stdout);
  print_field (head);
  putchar ('}');
  if (auto_reference || input_reference)
    {
      putchar ('{');
      print_field (reference);
      putchar ('}');
    }
  putchar ('\n');
}

 

static void
output_one_dumb_line (void)
{
  if (!right_reference)
    {
      if (auto_reference)
        {

           

          print_field (reference);
          putchar (':');
          print_spaces (reference_max_width
                        + gap_size
                        - (reference.end - reference.start)
                        - 1);
        }
      else
        {

           

          print_field (reference);
          print_spaces (reference_max_width
                        + gap_size
                        - (reference.end - reference.start));
        }
    }

  if (tail.start < tail.end)
    {
       

      print_field (tail);
      if (tail_truncation)
        fputs (truncation_string, stdout);

      print_spaces (half_line_width - gap_size
                    - (before.end - before.start)
                    - (before_truncation ? truncation_string_length : 0)
                    - (tail.end - tail.start)
                    - (tail_truncation ? truncation_string_length : 0));
    }
  else
    print_spaces (half_line_width - gap_size
                  - (before.end - before.start)
                  - (before_truncation ? truncation_string_length : 0));

   

  if (before_truncation)
    fputs (truncation_string, stdout);
  print_field (before);

  print_spaces (gap_size);

   

  print_field (keyafter);
  if (keyafter_truncation)
    fputs (truncation_string, stdout);

  if (head.start < head.end)
    {
       

      print_spaces (half_line_width
                    - (keyafter.end - keyafter.start)
                    - (keyafter_truncation ? truncation_string_length : 0)
                    - (head.end - head.start)
                    - (head_truncation ? truncation_string_length : 0));
      if (head_truncation)
        fputs (truncation_string, stdout);
      print_field (head);
    }
  else

    if ((auto_reference || input_reference) && right_reference)
      print_spaces (half_line_width
                    - (keyafter.end - keyafter.start)
                    - (keyafter_truncation ? truncation_string_length : 0));

  if ((auto_reference || input_reference) && right_reference)
    {
       

      print_spaces (gap_size);
      print_field (reference);
    }

  putchar ('\n');
}

 

static void
generate_all_output (void)
{
  ptrdiff_t occurs_index;	 
  OCCURS *occurs_cursor;	 

   

  tail.start = nullptr;
  tail.end = nullptr;
  tail_truncation = false;

  head.start = nullptr;
  head.end = nullptr;
  head_truncation = false;

   

  occurs_cursor = occurs_table[0];

  for (occurs_index = 0; occurs_index < number_of_occurs[0]; occurs_index++)
    {
       

      define_all_fields (occurs_cursor);

       

      switch (output_format)
        {
        case UNKNOWN_FORMAT:
           

        case DUMB_FORMAT:
          output_one_dumb_line ();
          break;

        case ROFF_FORMAT:
          output_one_roff_line ();
          break;

        case TEX_FORMAT:
          output_one_tex_line ();
          break;
        }

       

      occurs_cursor++;
    }
}

 

 

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... [INPUT]...   (without -G)\n\
  or:  %s -G [OPTION]... [INPUT [OUTPUT]]\n"),
              program_name, program_name);
      fputs (_("\
Output a permuted index, including context, of the words in the input files.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
  -A, --auto-reference           output automatically generated references\n\
  -G, --traditional              behave more like System V 'ptx'\n\
"), stdout);
      fputs (_("\
  -F, --flag-truncation=STRING   use STRING for flagging line truncations.\n\
                                 The default is '/'\n\
"), stdout);
      fputs (_("\
  -M, --macro-name=STRING        macro name to use instead of 'xx'\n\
  -O, --format=roff              generate output as roff directives\n\
  -R, --right-side-refs          put references at right, not counted in -w\n\
  -S, --sentence-regexp=REGEXP   for end of lines or end of sentences\n\
  -T, --format=tex               generate output as TeX directives\n\
"), stdout);
      fputs (_("\
  -W, --word-regexp=REGEXP       use REGEXP to match each keyword\n\
  -b, --break-file=FILE          word break characters in this FILE\n\
  -f, --ignore-case              fold lower case to upper case for sorting\n\
  -g, --gap-size=NUMBER          gap size in columns between output fields\n\
  -i, --ignore-file=FILE         read ignore word list from FILE\n\
  -o, --only-file=FILE           read only word list from this FILE\n\
"), stdout);
      fputs (_("\
  -r, --references               first field of each line is a reference\n\
  -t, --typeset-mode               - not implemented -\n\
  -w, --width=NUMBER             output width in columns, reference excluded\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/*----------------------------------------------------------------------.
| Main program.  Decode ARGC arguments passed through the ARGV array of |
| strings, then launch execution.				        |
`----------------------------------------------------------------------*/

/* Long options equivalences.  */
static struct option const long_options[] =
{
  {"auto-reference", no_argument, nullptr, 'A'},
  {"break-file", required_argument, nullptr, 'b'},
  {"flag-truncation", required_argument, nullptr, 'F'},
  {"ignore-case", no_argument, nullptr, 'f'},
  {"gap-size", required_argument, nullptr, 'g'},
  {"ignore-file", required_argument, nullptr, 'i'},
  {"macro-name", required_argument, nullptr, 'M'},
  {"only-file", required_argument, nullptr, 'o'},
  {"references", no_argument, nullptr, 'r'},
  {"right-side-refs", no_argument, nullptr, 'R'},
  {"format", required_argument, nullptr, 10},
  {"sentence-regexp", required_argument, nullptr, 'S'},
  {"traditional", no_argument, nullptr, 'G'},
  {"typeset-mode", no_argument, nullptr, 't'},
  {"width", required_argument, nullptr, 'w'},
  {"word-regexp", required_argument, nullptr, 'W'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0},
};

static char const *const format_args[] =
{
  "roff", "tex", nullptr
};

static enum Format const format_vals[] =
{
  ROFF_FORMAT, TEX_FORMAT
};

int
main (int argc, char **argv)
{
  int optchar;			/* argument character */
  int file_index;		/* index in text input file arrays */

  /* Decode program options.  */

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

#if HAVE_SETCHRCLASS
  setchrclass (nullptr);
#endif

  while (optchar = getopt_long (argc, argv, "AF:GM:ORS:TW:b:i:fg:o:trw:",
                                long_options, nullptr),
         optchar != EOF)
    {
      switch (optchar)
        {
        default:
          usage (EXIT_FAILURE);

        case 'G':
          gnu_extensions = false;
          break;

        case 'b':
          break_file = optarg;
          break;

        case 'f':
          ignore_case = true;
          break;

        case 'g':
          {
            intmax_t tmp;
            if (! (xstrtoimax (optarg, nullptr, 0, &tmp, "") == LONGINT_OK
                   && 0 < tmp && tmp <= PTRDIFF_MAX))
              error (EXIT_FAILURE, 0, _("invalid gap width: %s"),
                     quote (optarg));
            gap_size = tmp;
            break;
          }

        case 'i':
          ignore_file = optarg;
          break;

        case 'o':
          only_file = optarg;
          break;

        case 'r':
          input_reference = true;
          break;

        case 't':
          /* Yet to understand...  */
          break;

        case 'w':
          {
            intmax_t tmp;
            if (! (xstrtoimax (optarg, nullptr, 0, &tmp, "") == LONGINT_OK
                   && 0 < tmp && tmp <= PTRDIFF_MAX))
              error (EXIT_FAILURE, 0, _("invalid line width: %s"),
                     quote (optarg));
            line_width = tmp;
            break;
          }

        case 'A':
          auto_reference = true;
          break;

        case 'F':
          truncation_string = optarg;
          unescape_string (optarg);
          break;

        case 'M':
          macro_name = optarg;
          break;

        case 'O':
          output_format = ROFF_FORMAT;
          break;

        case 'R':
          right_reference = true;
          break;

        case 'S':
          context_regex.string = optarg;
          unescape_string (optarg);
          break;

        case 'T':
          output_format = TEX_FORMAT;
          break;

        case 'W':
          word_regex.string = optarg;
          unescape_string (optarg);
          if (!*word_regex.string)
            word_regex.string = nullptr;
          break;

        case 10:
          output_format = XARGMATCH ("--format", optarg,
                                     format_args, format_vals);
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        }
    }

  /* Process remaining arguments.  If GNU extensions are enabled, process
     all arguments as input parameters.  If disabled, accept at most two
     arguments, the second of which is an output parameter.  */

  if (optind == argc)
    {

      /* No more argument simply means: read standard input.  */

      input_file_name = xmalloc (sizeof *input_file_name);
      file_line_count = xmalloc (sizeof *file_line_count);
      text_buffers =    xmalloc (sizeof *text_buffers);
      number_input_files = 1;
      input_file_name[0] = nullptr;
    }
  else if (gnu_extensions)
    {
      number_input_files = argc - optind;
      input_file_name = xnmalloc (number_input_files, sizeof *input_file_name);
      file_line_count = xnmalloc (number_input_files, sizeof *file_line_count);
      text_buffers    = xnmalloc (number_input_files, sizeof *text_buffers);

      for (file_index = 0; file_index < number_input_files; file_index++)
        {
          if (!*argv[optind] || STREQ (argv[optind], "-"))
            input_file_name[file_index] = nullptr;
          else
            input_file_name[file_index] = argv[optind];
          optind++;
        }
    }
  else
    {

      /* There is one necessary input file.  */

      number_input_files = 1;
      input_file_name = xmalloc (sizeof *input_file_name);
      file_line_count = xmalloc (sizeof *file_line_count);
      text_buffers    = xmalloc (sizeof *text_buffers);
      if (!*argv[optind] || STREQ (argv[optind], "-"))
        input_file_name[0] = nullptr;
      else
        input_file_name[0] = argv[optind];
      optind++;

      /* Redirect standard output, only if requested.  */

      if (optind < argc)
        {
          if (! freopen (argv[optind], "w", stdout))
            error (EXIT_FAILURE, errno, "%s", quotef (argv[optind]));
          optind++;
        }

      /* Diagnose any other argument as an error.  */

      if (optind < argc)
        {
          error (0, 0, _("extra operand %s"), quote (argv[optind]));
          usage (EXIT_FAILURE);
        }
    }

   

  if (output_format == UNKNOWN_FORMAT)
    output_format = gnu_extensions ? DUMB_FORMAT : ROFF_FORMAT;

   

  initialize_regex ();

   

  if (break_file)
    digest_break_file (break_file);

   

  if (ignore_file)
    {
      digest_word_file (ignore_file, &ignore_table);
      if (ignore_table.length == 0)
        ignore_file = nullptr;
    }

  if (only_file)
    {
      digest_word_file (only_file, &only_table);
      if (only_table.length == 0)
        only_file = nullptr;
    }

   

  number_of_occurs[0] = 0;
  total_line_count = 0;
  maximum_word_length = 0;
  reference_max_width = 0;

  for (file_index = 0; file_index < number_input_files; file_index++)
    {
      BLOCK *text_buffer = text_buffers + file_index;

       

      swallow_file_in_memory (input_file_name[file_index], text_buffer);
      find_occurs_in_text (file_index);

       

      total_line_count++;
      file_line_count[file_index] = total_line_count;
    }

   

  sort_found_occurs ();
  fix_output_parameters ();
  generate_all_output ();

   

  return EXIT_SUCCESS;
}
