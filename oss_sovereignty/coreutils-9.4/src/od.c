 

#include <config.h>

#include <stdckdint.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include "system.h"
#include "argmatch.h"
#include "assure.h"
#include "ftoastr.h"
#include "quote.h"
#include "stat-size.h"
#include "xbinary-io.h"
#include "xprintf.h"
#include "xstrtol.h"
#include "xstrtol-error.h"

 
#define PROGRAM_NAME "od"

#define AUTHORS proper_name ("Jim Meyering")

 
#define DEFAULT_BYTES_PER_BLOCK 16

#if HAVE_UNSIGNED_LONG_LONG_INT
typedef unsigned long long int unsigned_long_long_int;
#else
 
typedef unsigned long int unsigned_long_long_int;
#endif

enum size_spec
  {
    NO_SIZE,
    CHAR,
    SHORT,
    INT,
    LONG,
    LONG_LONG,
     
    FLOAT_SINGLE,
    FLOAT_DOUBLE,
    FLOAT_LONG_DOUBLE,
    N_SIZE_SPECS
  };

enum output_format
  {
    SIGNED_DECIMAL,
    UNSIGNED_DECIMAL,
    OCTAL,
    HEXADECIMAL,
    FLOATING_POINT,
    NAMED_CHARACTER,
    CHARACTER
  };

#define MAX_INTEGRAL_TYPE_SIZE sizeof (unsigned_long_long_int)

 
enum
  {
    FMT_BYTES_ALLOCATED =
           (sizeof "%*.99" + 1
            + MAX (sizeof "ld",
                   MAX (sizeof PRIdMAX,
                        MAX (sizeof PRIoMAX,
                             MAX (sizeof PRIuMAX,
                                  sizeof PRIxMAX)))))
  };

 
static_assert (MAX_INTEGRAL_TYPE_SIZE * CHAR_BIT / 3 <= 99);

 
struct tspec
  {
    enum output_format fmt;
    enum size_spec size;  
     
    void (*print_function) (size_t fields, size_t blank, void const *data,
                            char const *fmt, int width, int pad);
    char fmt_string[FMT_BYTES_ALLOCATED];  
    bool hexl_mode_trailer;
    int field_width;  
    int pad_width;  
  };

 

static char const bytes_to_oct_digits[] =
{0, 3, 6, 8, 11, 14, 16, 19, 22, 25, 27, 30, 32, 35, 38, 41, 43};

static char const bytes_to_signed_dec_digits[] =
{1, 4, 6, 8, 11, 13, 16, 18, 20, 23, 25, 28, 30, 33, 35, 37, 40};

static char const bytes_to_unsigned_dec_digits[] =
{0, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25, 27, 29, 32, 34, 37, 39};

static char const bytes_to_hex_digits[] =
{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32};

 
static_assert (MAX_INTEGRAL_TYPE_SIZE
               < ARRAY_CARDINALITY (bytes_to_hex_digits));

 
static_assert (sizeof bytes_to_oct_digits == sizeof bytes_to_signed_dec_digits);
static_assert (sizeof bytes_to_oct_digits
               == sizeof bytes_to_unsigned_dec_digits);
static_assert (sizeof bytes_to_oct_digits == sizeof bytes_to_hex_digits);

 
static const int width_bytes[] =
{
  -1,
  sizeof (char),
  sizeof (short int),
  sizeof (int),
  sizeof (long int),
  sizeof (unsigned_long_long_int),
  sizeof (float),
  sizeof (double),
  sizeof (long double)
};

 
static_assert (ARRAY_CARDINALITY (width_bytes) == N_SIZE_SPECS);

 
static char const charname[33][4] =
{
  "nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
  "bs", "ht", "nl", "vt", "ff", "cr", "so", "si",
  "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
  "can", "em", "sub", "esc", "fs", "gs", "rs", "us",
  "sp"
};

 
static int address_base;

 
#define MAX_ADDRESS_LENGTH \
  ((sizeof (uintmax_t) * CHAR_BIT + CHAR_BIT - 1) / 3)

 
static int address_pad_len;

 
static size_t string_min;

 
static bool flag_dump_strings;

 
static bool traditional;

 
static bool flag_pseudo_start;

 
static uintmax_t pseudo_offset;

 
static void (*format_address) (uintmax_t, char);

 
static uintmax_t n_bytes_to_skip = 0;

 
static bool limit_bytes_to_format = false;

 
static uintmax_t max_bytes_to_format;

 
static uintmax_t end_offset;

 
static bool abbreviate_duplicate_blocks = true;

 
static struct tspec *spec;

 
static size_t n_specs;

 
static size_t n_specs_allocated;

 
static size_t bytes_per_block;

 
static char const *input_filename;

 
static char const *const *file_list;

 
static char const *const default_file_list[] = {"-", nullptr};

 
static FILE *in_stream;

 
static bool have_read_stdin;

 
static enum size_spec integral_type_size[MAX_INTEGRAL_TYPE_SIZE + 1];

#define MAX_FP_TYPE_SIZE sizeof (long double)
static enum size_spec fp_type_size[MAX_FP_TYPE_SIZE + 1];

#ifndef WORDS_BIGENDIAN
# define WORDS_BIGENDIAN 0
#endif

 
static bool input_swap;

static char const short_options[] = "A:aBbcDdeFfHhIij:LlN:OoS:st:vw::Xx";

 
enum
{
  TRADITIONAL_OPTION = CHAR_MAX + 1,
  ENDIAN_OPTION,
};

enum endian_type
{
  endian_little,
  endian_big
};

static char const *const endian_args[] =
{
  "little", "big", nullptr
};

static enum endian_type const endian_types[] =
{
  endian_little, endian_big
};

static struct option const long_options[] =
{
  {"skip-bytes", required_argument, nullptr, 'j'},
  {"address-radix", required_argument, nullptr, 'A'},
  {"read-bytes", required_argument, nullptr, 'N'},
  {"format", required_argument, nullptr, 't'},
  {"output-duplicates", no_argument, nullptr, 'v'},
  {"strings", optional_argument, nullptr, 'S'},
  {"traditional", no_argument, nullptr, TRADITIONAL_OPTION},
  {"width", optional_argument, nullptr, 'w'},
  {"endian", required_argument, nullptr, ENDIAN_OPTION },

  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
  or:  %s [-abcdfilosx]... [FILE] [[+]OFFSET[.][b]]\n\
  or:  %s --traditional [OPTION]... [FILE] [[+]OFFSET[.][b] [+][LABEL][.][b]]\n\
"),
              program_name, program_name, program_name);
      fputs (_("\n\
Write an unambiguous representation, octal bytes by default,\n\
of FILE to standard output.  With more than one FILE argument,\n\
concatenate them in the listed order to form the input.\n\
"), stdout);

      emit_stdin_note ();

      fputs (_("\
\n\
If first and second call formats both apply, the second format is assumed\n\
if the last operand begins with + or (if there are 2 operands) a digit.\n\
An OFFSET operand means -j OFFSET.  LABEL is the pseudo-address\n\
at first byte printed, incremented when dump is progressing.\n\
For OFFSET and LABEL, a 0x or 0X prefix indicates hexadecimal;\n\
suffixes may be . for octal and b for multiply by 512.\n\
"), stdout);

      emit_mandatory_arg_note ();

      fputs (_("\
  -A, --address-radix=RADIX   output format for file offsets; RADIX is one\n\
                                of [doxn], for Decimal, Octal, Hex or None\n\
      --endian={big|little}   swap input bytes according the specified order\n\
  -j, --skip-bytes=BYTES      skip BYTES input bytes first\n\
"), stdout);
      fputs (_("\
  -N, --read-bytes=BYTES      limit dump to BYTES input bytes\n\
  -S BYTES, --strings[=BYTES]  show only NUL terminated strings\n\
                                of at least BYTES (3) printable characters\n\
  -t, --format=TYPE           select output format or formats\n\
  -v, --output-duplicates     do not use * to mark line suppression\n\
  -w[BYTES], --width[=BYTES]  output BYTES bytes per output line;\n\
                                32 is implied when BYTES is not specified\n\
      --traditional           accept arguments in third form above\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
\n\
Traditional format specifications may be intermixed; they accumulate:\n\
  -a   same as -t a,  select named characters, ignoring high-order bit\n\
  -b   same as -t o1, select octal bytes\n\
  -c   same as -t c,  select printable characters or backslash escapes\n\
  -d   same as -t u2, select unsigned decimal 2-byte units\n\
"), stdout);
      fputs (_("\
  -f   same as -t fF, select floats\n\
  -i   same as -t dI, select decimal ints\n\
  -l   same as -t dL, select decimal longs\n\
  -o   same as -t o2, select octal 2-byte units\n\
  -s   same as -t d2, select decimal 2-byte units\n\
  -x   same as -t x2, select hexadecimal 2-byte units\n\
"), stdout);
      fputs (_("\
\n\
\n\
TYPE is made up of one or more of these specifications:\n\
  a          named character, ignoring high-order bit\n\
  c          printable character or backslash escape\n\
"), stdout);
      fputs (_("\
  d[SIZE]    signed decimal, SIZE bytes per integer\n\
  f[SIZE]    floating point, SIZE bytes per float\n\
  o[SIZE]    octal, SIZE bytes per integer\n\
  u[SIZE]    unsigned decimal, SIZE bytes per integer\n\
  x[SIZE]    hexadecimal, SIZE bytes per integer\n\
"), stdout);
      fputs (_("\
\n\
SIZE is a number.  For TYPE in [doux], SIZE may also be C for\n\
sizeof(char), S for sizeof(short), I for sizeof(int) or L for\n\
sizeof(long).  If TYPE is f, SIZE may also be F for sizeof(float), D\n\
for sizeof(double) or L for sizeof(long double).\n\
"), stdout);
      fputs (_("\
\n\
Adding a z suffix to any type displays printable characters at the end of\n\
each output line.\n\
"), stdout);
      fputs (_("\
\n\
\n\
BYTES is hex with 0x or 0X prefix, and may have a multiplier suffix:\n\
  b    512\n\
  KB   1000\n\
  K    1024\n\
  MB   1000*1000\n\
  M    1024*1024\n\
and so on for G, T, P, E, Z, Y, R, Q.\n\
Binary prefixes can be used, too: KiB=K, MiB=M, and so on.\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

 

#define PRINT_FIELDS(N, T, FMT_STRING_DECL, ACTION)                     \
static void                                                             \
N (size_t fields, size_t blank, void const *block,                      \
   FMT_STRING_DECL, int width, int pad)                                 \
{                                                                       \
  T const *p = block;                                                   \
  uintmax_t i;                                                          \
  int pad_remaining = pad;                                              \
  for (i = fields; blank < i; i--)                                      \
    {                                                                   \
      int next_pad = pad * (i - 1) / fields;                            \
      int adjusted_width = pad_remaining - next_pad + width;            \
      T x;                                                              \
      if (input_swap && sizeof (T) > 1)                                 \
        {                                                               \
          size_t j;                                                     \
          union {                                                       \
            T x;                                                        \
            char b[sizeof (T)];                                         \
          } u;                                                          \
          for (j = 0; j < sizeof (T); j++)                              \
            u.b[j] = ((char const *) p)[sizeof (T) - 1 - j];            \
          x = u.x;                                                      \
        }                                                               \
      else                                                              \
        x = *p;                                                         \
      p++;                                                              \
      ACTION;                                                           \
      pad_remaining = next_pad;                                         \
    }                                                                   \
}

#define PRINT_TYPE(N, T)                                                \
  PRINT_FIELDS (N, T, char const *fmt_string,                           \
                xprintf (fmt_string, adjusted_width, x))

#define PRINT_FLOATTYPE(N, T, FTOASTR, BUFSIZE)                         \
  PRINT_FIELDS (N, T, MAYBE_UNUSED char const *fmt_string,              \
                char buf[BUFSIZE];                                      \
                FTOASTR (buf, sizeof buf, 0, 0, x);                     \
                xprintf ("%*s", adjusted_width, buf))

PRINT_TYPE (print_s_char, signed char)
PRINT_TYPE (print_char, unsigned char)
PRINT_TYPE (print_s_short, short int)
PRINT_TYPE (print_short, unsigned short int)
PRINT_TYPE (print_int, unsigned int)
PRINT_TYPE (print_long, unsigned long int)
PRINT_TYPE (print_long_long, unsigned_long_long_int)

PRINT_FLOATTYPE (print_float, float, ftoastr, FLT_BUFSIZE_BOUND)
PRINT_FLOATTYPE (print_double, double, dtoastr, DBL_BUFSIZE_BOUND)
PRINT_FLOATTYPE (print_long_double, long double, ldtoastr, LDBL_BUFSIZE_BOUND)

#undef PRINT_TYPE
#undef PRINT_FLOATTYPE

static void
dump_hexl_mode_trailer (size_t n_bytes, char const *block)
{
  fputs ("  >", stdout);
  for (size_t i = n_bytes; i > 0; i--)
    {
      unsigned char c = *block++;
      unsigned char c2 = (isprint (c) ? c : '.');
      putchar (c2);
    }
  putchar ('<');
}

static void
print_named_ascii (size_t fields, size_t blank, void const *block,
                   MAYBE_UNUSED char const *unused_fmt_string,
                   int width, int pad)
{
  unsigned char const *p = block;
  uintmax_t i;
  int pad_remaining = pad;
  for (i = fields; blank < i; i--)
    {
      int next_pad = pad * (i - 1) / fields;
      int masked_c = *p++ & 0x7f;
      char const *s;
      char buf[2];

      if (masked_c == 127)
        s = "del";
      else if (masked_c <= 040)
        s = charname[masked_c];
      else
        {
          buf[0] = masked_c;
          buf[1] = 0;
          s = buf;
        }

      xprintf ("%*s", pad_remaining - next_pad + width, s);
      pad_remaining = next_pad;
    }
}

static void
print_ascii (size_t fields, size_t blank, void const *block,
             MAYBE_UNUSED char const *unused_fmt_string, int width,
             int pad)
{
  unsigned char const *p = block;
  uintmax_t i;
  int pad_remaining = pad;
  for (i = fields; blank < i; i--)
    {
      int next_pad = pad * (i - 1) / fields;
      unsigned char c = *p++;
      char const *s;
      char buf[4];

      switch (c)
        {
        case '\0':
          s = "\\0";
          break;

        case '\a':
          s = "\\a";
          break;

        case '\b':
          s = "\\b";
          break;

        case '\f':
          s = "\\f";
          break;

        case '\n':
          s = "\\n";
          break;

        case '\r':
          s = "\\r";
          break;

        case '\t':
          s = "\\t";
          break;

        case '\v':
          s = "\\v";
          break;

        default:
          sprintf (buf, (isprint (c) ? "%c" : "%03o"), c);
          s = buf;
        }

      xprintf ("%*s", pad_remaining - next_pad + width, s);
      pad_remaining = next_pad;
    }
}

 

static bool
simple_strtoi (char const *s, char const **p, int *val)
{
  int sum;

  for (sum = 0; ISDIGIT (*s); s++)
    if (ckd_mul (&sum, sum, 10) || ckd_add (&sum, sum, *s - '0'))
      return false;
  *p = s;
  *val = sum;
  return true;
}

 

static bool ATTRIBUTE_NONNULL ()
decode_one_format (char const *s_orig, char const *s, char const **next,
                   struct tspec *tspec)
{
  enum size_spec size_spec;
  int size;
  enum output_format fmt;
  void (*print_function) (size_t, size_t, void const *, char const *,
                          int, int);
  char const *p;
  char c;
  int field_width;

  switch (*s)
    {
    case 'd':
    case 'o':
    case 'u':
    case 'x':
      c = *s;
      ++s;
      switch (*s)
        {
        case 'C':
          ++s;
          size = sizeof (char);
          break;

        case 'S':
          ++s;
          size = sizeof (short int);
          break;

        case 'I':
          ++s;
          size = sizeof (int);
          break;

        case 'L':
          ++s;
          size = sizeof (long int);
          break;

        default:
          if (! simple_strtoi (s, &p, &size))
            {
               
              error (0, 0, _("invalid type string %s"), quote (s_orig));
              return false;
            }
          if (p == s)
            size = sizeof (int);
          else
            {
              if (MAX_INTEGRAL_TYPE_SIZE < size
                  || integral_type_size[size] == NO_SIZE)
                {
                  error (0, 0, _("invalid type string %s;\nthis system"
                                 " doesn't provide a %d-byte integral type"),
                         quote (s_orig), size);
                  return false;
                }
              s = p;
            }
          break;
        }

#define ISPEC_TO_FORMAT(Spec, Min_format, Long_format, Max_format)	\
  ((Spec) == LONG_LONG ? (Max_format)					\
   : ((Spec) == LONG ? (Long_format)					\
      : (Min_format)))							\

      size_spec = integral_type_size[size];

      switch (c)
        {
        case 'd':
          fmt = SIGNED_DECIMAL;
          field_width = bytes_to_signed_dec_digits[size];
          sprintf (tspec->fmt_string, "%%*%s",
                   ISPEC_TO_FORMAT (size_spec, "d", "ld", PRIdMAX));
          break;

        case 'o':
          fmt = OCTAL;
          sprintf (tspec->fmt_string, "%%*.%d%s",
                   (field_width = bytes_to_oct_digits[size]),
                   ISPEC_TO_FORMAT (size_spec, "o", "lo", PRIoMAX));
          break;

        case 'u':
          fmt = UNSIGNED_DECIMAL;
          field_width = bytes_to_unsigned_dec_digits[size];
          sprintf (tspec->fmt_string, "%%*%s",
                   ISPEC_TO_FORMAT (size_spec, "u", "lu", PRIuMAX));
          break;

        case 'x':
          fmt = HEXADECIMAL;
          sprintf (tspec->fmt_string, "%%*.%d%s",
                   (field_width = bytes_to_hex_digits[size]),
                   ISPEC_TO_FORMAT (size_spec, "x", "lx", PRIxMAX));
          break;

        default:
          unreachable ();
        }

      switch (size_spec)
        {
        case CHAR:
          print_function = (fmt == SIGNED_DECIMAL
                            ? print_s_char
                            : print_char);
          break;

        case SHORT:
          print_function = (fmt == SIGNED_DECIMAL
                            ? print_s_short
                            : print_short);
          break;

        case INT:
          print_function = print_int;
          break;

        case LONG:
          print_function = print_long;
          break;

        case LONG_LONG:
          print_function = print_long_long;
          break;

        default:
          affirm (false);
        }
      break;

    case 'f':
      fmt = FLOATING_POINT;
      ++s;
      switch (*s)
        {
        case 'F':
          ++s;
          size = sizeof (float);
          break;

        case 'D':
          ++s;
          size = sizeof (double);
          break;

        case 'L':
          ++s;
          size = sizeof (long double);
          break;

        default:
          if (! simple_strtoi (s, &p, &size))
            {
               
              error (0, 0, _("invalid type string %s"), quote (s_orig));
              return false;
            }
          if (p == s)
            size = sizeof (double);
          else
            {
              if (size > MAX_FP_TYPE_SIZE
                  || fp_type_size[size] == NO_SIZE)
                {
                  error (0, 0,
                         _("invalid type string %s;\n"
                           "this system doesn't provide a %d-byte"
                           " floating point type"),
                         quote (s_orig), size);
                  return false;
                }
              s = p;
            }
          break;
        }
      size_spec = fp_type_size[size];

      {
        struct lconv const *locale = localeconv ();
        size_t decimal_point_len =
          (locale->decimal_point[0] ? strlen (locale->decimal_point) : 1);

        switch (size_spec)
          {
          case FLOAT_SINGLE:
            print_function = print_float;
            field_width = FLT_STRLEN_BOUND_L (decimal_point_len);
            break;

          case FLOAT_DOUBLE:
            print_function = print_double;
            field_width = DBL_STRLEN_BOUND_L (decimal_point_len);
            break;

          case FLOAT_LONG_DOUBLE:
            print_function = print_long_double;
            field_width = LDBL_STRLEN_BOUND_L (decimal_point_len);
            break;

          default:
            affirm (false);
          }

        break;
      }

    case 'a':
      ++s;
      fmt = NAMED_CHARACTER;
      size_spec = CHAR;
      print_function = print_named_ascii;
      field_width = 3;
      break;

    case 'c':
      ++s;
      fmt = CHARACTER;
      size_spec = CHAR;
      print_function = print_ascii;
      field_width = 3;
      break;

    default:
      error (0, 0, _("invalid character '%c' in type string %s"),
             *s, quote (s_orig));
      return false;
    }

  tspec->size = size_spec;
  tspec->fmt = fmt;
  tspec->print_function = print_function;

  tspec->field_width = field_width;
  tspec->hexl_mode_trailer = (*s == 'z');
  if (tspec->hexl_mode_trailer)
    s++;

  *next = s;
  return true;
}

 

static bool
open_next_file (void)
{
  bool ok = true;

  do
    {
      input_filename = *file_list;
      if (input_filename == nullptr)
        return ok;
      ++file_list;

      if (STREQ (input_filename, "-"))
        {
          input_filename = _("standard input");
          in_stream = stdin;
          have_read_stdin = true;
          xset_binary_mode (STDIN_FILENO, O_BINARY);
        }
      else
        {
          in_stream = fopen (input_filename, (O_BINARY ? "rb" : "r"));
          if (in_stream == nullptr)
            {
              error (0, errno, "%s", quotef (input_filename));
              ok = false;
            }
        }
    }
  while (in_stream == nullptr);

  if (limit_bytes_to_format && !flag_dump_strings)
    setvbuf (in_stream, nullptr, _IONBF, 0);

  return ok;
}

 

static bool
check_and_close (int in_errno)
{
  bool ok = true;

  if (in_stream != nullptr)
    {
      if (!ferror (in_stream))
        in_errno = 0;
      if (STREQ (file_list[-1], "-"))
        clearerr (in_stream);
      else if (fclose (in_stream) != 0 && !in_errno)
        in_errno = errno;
      if (in_errno)
        {
          error (0, in_errno, "%s", quotef (input_filename));
          ok = false;
        }

      in_stream = nullptr;
    }

  if (ferror (stdout))
    {
      error (0, 0, _("write error"));
      ok = false;
    }

  return ok;
}

 

static bool ATTRIBUTE_NONNULL ()
decode_format_string (char const *s)
{
  char const *s_orig = s;

  while (*s != '\0')
    {
      char const *next;

      if (n_specs_allocated <= n_specs)
        spec = X2NREALLOC (spec, &n_specs_allocated);

      if (! decode_one_format (s_orig, s, &next, &spec[n_specs]))
        return false;

      affirm (s != next);
      s = next;
      ++n_specs;
    }

  return true;
}

 

static bool
skip (uintmax_t n_skip)
{
  bool ok = true;
  int in_errno = 0;

  if (n_skip == 0)
    return true;

  while (in_stream != nullptr)	 
    {
      struct stat file_stats;

       

      if (fstat (fileno (in_stream), &file_stats) == 0)
        {
          bool usable_size = usable_st_size (&file_stats);

           
          if (usable_size && ST_BLKSIZE (file_stats) < file_stats.st_size)
            {
              if ((uintmax_t) file_stats.st_size < n_skip)
                n_skip -= file_stats.st_size;
              else
                {
                  if (fseeko (in_stream, n_skip, SEEK_CUR) != 0)
                    {
                      in_errno = errno;
                      ok = false;
                    }
                  n_skip = 0;
                }
            }

          else if (!usable_size && fseeko (in_stream, n_skip, SEEK_CUR) == 0)
            n_skip = 0;

           

          else
            {
              char buf[BUFSIZ];
              size_t n_bytes_read, n_bytes_to_read = BUFSIZ;

              while (0 < n_skip)
                {
                  if (n_skip < n_bytes_to_read)
                    n_bytes_to_read = n_skip;
                  n_bytes_read = fread (buf, 1, n_bytes_to_read, in_stream);
                  n_skip -= n_bytes_read;
                  if (n_bytes_read != n_bytes_to_read)
                    {
                      if (ferror (in_stream))
                        {
                          in_errno = errno;
                          ok = false;
                          n_skip = 0;
                          break;
                        }
                      if (feof (in_stream))
                        break;
                    }
                }
            }

          if (n_skip == 0)
            break;
        }

      else    
        {
          error (0, errno, "%s", quotef (input_filename));
          ok = false;
        }

      ok &= check_and_close (in_errno);

      ok &= open_next_file ();
    }

  if (n_skip != 0)
    error (EXIT_FAILURE, 0, _("cannot skip past end of combined input"));

  return ok;
}

static void
format_address_none (MAYBE_UNUSED uintmax_t address,
                     MAYBE_UNUSED char c)
{
}

static void
format_address_std (uintmax_t address, char c)
{
  char buf[MAX_ADDRESS_LENGTH + 2];
  char *p = buf + sizeof buf;
  char const *pbound;

  *--p = '\0';
  *--p = c;
  pbound = p - address_pad_len;

   
  switch (address_base)
    {
    case 8:
      do
        *--p = '0' + (address & 7);
      while ((address >>= 3) != 0);
      break;

    case 10:
      do
        *--p = '0' + (address % 10);
      while ((address /= 10) != 0);
      break;

    case 16:
      do
        *--p = "0123456789abcdef"[address & 15];
      while ((address >>= 4) != 0);
      break;
    }

  while (pbound < p)
    *--p = '0';

  fputs (p, stdout);
}

static void
format_address_paren (uintmax_t address, char c)
{
  putchar ('(');
  format_address_std (address, ')');
  if (c)
    putchar (c);
}

static void
format_address_label (uintmax_t address, char c)
{
  format_address_std (address, ' ');
  format_address_paren (address + pseudo_offset, c);
}

 

static void
write_block (uintmax_t current_offset, size_t n_bytes,
             char const *prev_block, char const *curr_block)
{
  static bool first = true;
  static bool prev_pair_equal = false;

#define EQUAL_BLOCKS(b1, b2) (memcmp (b1, b2, bytes_per_block) == 0)

  if (abbreviate_duplicate_blocks
      && !first && n_bytes == bytes_per_block
      && EQUAL_BLOCKS (prev_block, curr_block))
    {
      if (prev_pair_equal)
        {
           
        }
      else
        {
          printf ("*\n");
          prev_pair_equal = true;
        }
    }
  else
    {
      prev_pair_equal = false;
      for (size_t i = 0; i < n_specs; i++)
        {
          int datum_width = width_bytes[spec[i].size];
          int fields_per_block = bytes_per_block / datum_width;
          int blank_fields = (bytes_per_block - n_bytes) / datum_width;
          if (i == 0)
            format_address (current_offset, '\0');
          else
            printf ("%*s", address_pad_len, "");
          (*spec[i].print_function) (fields_per_block, blank_fields,
                                     curr_block, spec[i].fmt_string,
                                     spec[i].field_width, spec[i].pad_width);
          if (spec[i].hexl_mode_trailer)
            {
               
              int field_width = spec[i].field_width;
              int pad_width = (spec[i].pad_width * blank_fields
                               / fields_per_block);
              printf ("%*s", blank_fields * field_width + pad_width, "");
              dump_hexl_mode_trailer (n_bytes, curr_block);
            }
          putchar ('\n');
        }
    }
  first = false;
}

 

static bool
read_char (int *c)
{
  bool ok = true;

  *c = EOF;

  while (in_stream != nullptr)	 
    {
      *c = fgetc (in_stream);

      if (*c != EOF)
        break;

      ok &= check_and_close (errno);

      ok &= open_next_file ();
    }

  return ok;
}

 

static bool
read_block (size_t n, char *block, size_t *n_bytes_in_buffer)
{
  bool ok = true;

  affirm (0 < n && n <= bytes_per_block);

  *n_bytes_in_buffer = 0;

  while (in_stream != nullptr)	 
    {
      size_t n_needed;
      size_t n_read;

      n_needed = n - *n_bytes_in_buffer;
      n_read = fread (block + *n_bytes_in_buffer, 1, n_needed, in_stream);

      *n_bytes_in_buffer += n_read;

      if (n_read == n_needed)
        break;

      ok &= check_and_close (errno);

      ok &= open_next_file ();
    }

  return ok;
}

 

ATTRIBUTE_PURE
static int
get_lcm (void)
{
  int l_c_m = 1;

  for (size_t i = 0; i < n_specs; i++)
    l_c_m = lcm (l_c_m, width_bytes[spec[i].size]);
  return l_c_m;
}

 

static bool
parse_old_offset (char const *s, uintmax_t *offset)
{
  int radix;

  if (*s == '\0')
    return false;

   
  if (s[0] == '+')
    ++s;

   
  if (strchr (s, '.') != nullptr)
    radix = 10;
  else
    {
      if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        radix = 16;
      else
        radix = 8;
    }

  return xstrtoumax (s, nullptr, radix, offset, "Bb") == LONGINT_OK;
}

 

static bool
dump (void)
{
  char *block[2];
  uintmax_t current_offset;
  bool idx = false;
  bool ok = true;
  size_t n_bytes_read;

  block[0] = xnmalloc (2, bytes_per_block);
  block[1] = block[0] + bytes_per_block;

  current_offset = n_bytes_to_skip;

  if (limit_bytes_to_format)
    {
      while (ok)
        {
          size_t n_needed;
          if (current_offset >= end_offset)
            {
              n_bytes_read = 0;
              break;
            }
          n_needed = MIN (end_offset - current_offset,
                          (uintmax_t) bytes_per_block);
          ok &= read_block (n_needed, block[idx], &n_bytes_read);
          if (n_bytes_read < bytes_per_block)
            break;
          affirm (n_bytes_read == bytes_per_block);
          write_block (current_offset, n_bytes_read,
                       block[!idx], block[idx]);
          if (ferror (stdout))
            ok = false;
          current_offset += n_bytes_read;
          idx = !idx;
        }
    }
  else
    {
      while (ok)
        {
          ok &= read_block (bytes_per_block, block[idx], &n_bytes_read);
          if (n_bytes_read < bytes_per_block)
            break;
          affirm (n_bytes_read == bytes_per_block);
          write_block (current_offset, n_bytes_read,
                       block[!idx], block[idx]);
          if (ferror (stdout))
            ok = false;
          current_offset += n_bytes_read;
          idx = !idx;
        }
    }

  if (n_bytes_read > 0)
    {
      int l_c_m;
      size_t bytes_to_write;

      l_c_m = get_lcm ();

       
      bytes_to_write = l_c_m * ((n_bytes_read + l_c_m - 1) / l_c_m);

      memset (block[idx] + n_bytes_read, 0, bytes_to_write - n_bytes_read);
      write_block (current_offset, n_bytes_read, block[!idx], block[idx]);
      current_offset += n_bytes_read;
    }

  format_address (current_offset, '\n');

  if (limit_bytes_to_format && current_offset >= end_offset)
    ok &= check_and_close (0);

  free (block[0]);

  return ok;
}

 

static bool
dump_strings (void)
{
  size_t bufsize = MAX (100, string_min);
  char *buf = xmalloc (bufsize);
  uintmax_t address = n_bytes_to_skip;
  bool ok = true;

  while (true)
    {
      size_t i;
      int c;

       
    tryline:

      if (limit_bytes_to_format
          && (end_offset < string_min || end_offset - string_min <= address))
        break;

      for (i = 0; i < string_min; i++)
        {
          ok &= read_char (&c);
          address++;
          if (c < 0)
            {
              free (buf);
              return ok;
            }
          if (! isprint (c))
             
            goto tryline;
          buf[i] = c;
        }

       
      while (!limit_bytes_to_format || address < end_offset)
        {
          if (i == bufsize)
            {
              buf = X2REALLOC (buf, &bufsize);
            }
          ok &= read_char (&c);
          address++;
          if (c < 0)
            {
              free (buf);
              return ok;
            }
          if (c == '\0')
            break;		 
          if (! isprint (c))
            goto tryline;	 
          buf[i++] = c;		 
        }

       
      buf[i] = 0;
      format_address (address - i - 1, ' ');

      for (i = 0; (c = buf[i]); i++)
        {
          switch (c)
            {
            case '\a':
              fputs ("\\a", stdout);
              break;

            case '\b':
              fputs ("\\b", stdout);
              break;

            case '\f':
              fputs ("\\f", stdout);
              break;

            case '\n':
              fputs ("\\n", stdout);
              break;

            case '\r':
              fputs ("\\r", stdout);
              break;

            case '\t':
              fputs ("\\t", stdout);
              break;

            case '\v':
              fputs ("\\v", stdout);
              break;

            default:
              putc (c, stdout);
            }
        }
      putchar ('\n');
    }

   

  free (buf);

  ok &= check_and_close (0);
  return ok;
}

int
main (int argc, char **argv)
{
  int n_files;
  size_t i;
  int l_c_m;
  idx_t desired_width IF_LINT ( = 0);
  bool modern = false;
  bool width_specified = false;
  bool ok = true;
  size_t width_per_block = 0;
  static char const multipliers[] = "bEGKkMmPQRTYZ0";

   
  uintmax_t pseudo_start IF_LINT ( = 0);

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  for (i = 0; i <= MAX_INTEGRAL_TYPE_SIZE; i++)
    integral_type_size[i] = NO_SIZE;

  integral_type_size[sizeof (char)] = CHAR;
  integral_type_size[sizeof (short int)] = SHORT;
  integral_type_size[sizeof (int)] = INT;
  integral_type_size[sizeof (long int)] = LONG;
#if HAVE_UNSIGNED_LONG_LONG_INT
   
  integral_type_size[sizeof (unsigned_long_long_int)] = LONG_LONG;
#endif

  for (i = 0; i <= MAX_FP_TYPE_SIZE; i++)
    fp_type_size[i] = NO_SIZE;

  fp_type_size[sizeof (float)] = FLOAT_SINGLE;
   
  fp_type_size[sizeof (long double)] = FLOAT_LONG_DOUBLE;
  fp_type_size[sizeof (double)] = FLOAT_DOUBLE;

  n_specs = 0;
  n_specs_allocated = 0;
  spec = nullptr;

  format_address = format_address_std;
  address_base = 8;
  address_pad_len = 7;
  flag_dump_strings = false;

  while (true)
    {
      uintmax_t tmp;
      enum strtol_error s_err;
      int oi = -1;
      int c = getopt_long (argc, argv, short_options, long_options, &oi);
      if (c == -1)
        break;

      switch (c)
        {
        case 'A':
          modern = true;
          switch (optarg[0])
            {
            case 'd':
              format_address = format_address_std;
              address_base = 10;
              address_pad_len = 7;
              break;
            case 'o':
              format_address = format_address_std;
              address_base = 8;
              address_pad_len = 7;
              break;
            case 'x':
              format_address = format_address_std;
              address_base = 16;
              address_pad_len = 6;
              break;
            case 'n':
              format_address = format_address_none;
              address_pad_len = 0;
              break;
            default:
              error (EXIT_FAILURE, 0,
                     _("invalid output address radix '%c';"
                       " it must be one character from [doxn]"),
                     optarg[0]);
              break;
            }
          break;

        case 'j':
          modern = true;
          s_err = xstrtoumax (optarg, nullptr, 0,
                              &n_bytes_to_skip, multipliers);
          if (s_err != LONGINT_OK)
            xstrtol_fatal (s_err, oi, c, long_options, optarg);
          break;

        case 'N':
          modern = true;
          limit_bytes_to_format = true;

          s_err = xstrtoumax (optarg, nullptr, 0, &max_bytes_to_format,
                              multipliers);
          if (s_err != LONGINT_OK)
            xstrtol_fatal (s_err, oi, c, long_options, optarg);
          break;

        case 'S':
          modern = true;
          if (optarg == nullptr)
            string_min = 3;
          else
            {
              s_err = xstrtoumax (optarg, nullptr, 0, &tmp, multipliers);
              if (s_err != LONGINT_OK)
                xstrtol_fatal (s_err, oi, c, long_options, optarg);

               
              if (SIZE_MAX < tmp)
                error (EXIT_FAILURE, 0, _("%s is too large"), quote (optarg));

              string_min = tmp;
            }
          flag_dump_strings = true;
          break;

        case 't':
          modern = true;
          ok &= decode_format_string (optarg);
          break;

        case 'v':
          modern = true;
          abbreviate_duplicate_blocks = false;
          break;

        case TRADITIONAL_OPTION:
          traditional = true;
          break;

        case ENDIAN_OPTION:
          switch (XARGMATCH ("--endian", optarg, endian_args, endian_types))
            {
              case endian_big:
                  input_swap = ! WORDS_BIGENDIAN;
                  break;
              case endian_little:
                  input_swap = WORDS_BIGENDIAN;
                  break;
            }
          break;

           

#define CASE_OLD_ARG(old_char,new_string)		\
        case old_char:					\
          ok &= decode_format_string (new_string);	\
          break

          CASE_OLD_ARG ('a', "a");
          CASE_OLD_ARG ('b', "o1");
          CASE_OLD_ARG ('c', "c");
          CASE_OLD_ARG ('D', "u4");  
          CASE_OLD_ARG ('d', "u2");
        case 'F':  
          CASE_OLD_ARG ('e', "fD");  
          CASE_OLD_ARG ('f', "fF");
        case 'X':  
          CASE_OLD_ARG ('H', "x4");  
          CASE_OLD_ARG ('i', "dI");
        case 'I': case 'L':  
          CASE_OLD_ARG ('l', "dL");
          CASE_OLD_ARG ('O', "o4");  
        case 'B':  
          CASE_OLD_ARG ('o', "o2");
          CASE_OLD_ARG ('s', "d2");
        case 'h':  
          CASE_OLD_ARG ('x', "x2");

#undef CASE_OLD_ARG

        case 'w':
          modern = true;
          width_specified = true;
          if (optarg == nullptr)
            {
              desired_width = 32;
            }
          else
            {
              intmax_t w_tmp;
              s_err = xstrtoimax (optarg, nullptr, 10, &w_tmp, "");
              if (s_err != LONGINT_OK || w_tmp <= 0)
                xstrtol_fatal (s_err, oi, c, long_options, optarg);
              if (ckd_add (&desired_width, w_tmp, 0))
                error (EXIT_FAILURE, 0, _("%s is too large"), quote (optarg));
            }
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
          break;
        }
    }

  if (!ok)
    return EXIT_FAILURE;

  if (flag_dump_strings && n_specs > 0)
    error (EXIT_FAILURE, 0,
           _("no type may be specified when dumping strings"));

  n_files = argc - optind;

   

  if (!modern || traditional)
    {
      uintmax_t o1;
      uintmax_t o2;

      switch (n_files)
        {
        case 1:
          if ((traditional || argv[optind][0] == '+')
              && parse_old_offset (argv[optind], &o1))
            {
              n_bytes_to_skip = o1;
              --n_files;
              ++argv;
            }
          break;

        case 2:
          if ((traditional || argv[optind + 1][0] == '+'
               || ISDIGIT (argv[optind + 1][0]))
              && parse_old_offset (argv[optind + 1], &o2))
            {
              if (traditional && parse_old_offset (argv[optind], &o1))
                {
                  n_bytes_to_skip = o1;
                  flag_pseudo_start = true;
                  pseudo_start = o2;
                  argv += 2;
                  n_files -= 2;
                }
              else
                {
                  n_bytes_to_skip = o2;
                  --n_files;
                  argv[optind + 1] = argv[optind];
                  ++argv;
                }
            }
          break;

        case 3:
          if (traditional
              && parse_old_offset (argv[optind + 1], &o1)
              && parse_old_offset (argv[optind + 2], &o2))
            {
              n_bytes_to_skip = o1;
              flag_pseudo_start = true;
              pseudo_start = o2;
              argv[optind + 2] = argv[optind];
              argv += 2;
              n_files -= 2;
            }
          break;
        }

      if (traditional && 1 < n_files)
        {
          error (0, 0, _("extra operand %s"), quote (argv[optind + 1]));
          error (0, 0, "%s",
                 _("compatibility mode supports at most one file"));
          usage (EXIT_FAILURE);
        }
    }

  if (flag_pseudo_start)
    {
      if (format_address == format_address_none)
        {
          address_base = 8;
          address_pad_len = 7;
          format_address = format_address_paren;
        }
      else
        format_address = format_address_label;
    }

  if (limit_bytes_to_format)
    {
      end_offset = n_bytes_to_skip + max_bytes_to_format;
      if (end_offset < n_bytes_to_skip)
        error (EXIT_FAILURE, 0, _("skip-bytes + read-bytes is too large"));
    }

  if (n_specs == 0)
    decode_format_string ("oS");

  if (n_files > 0)
    {
       

      file_list = (char const *const *) &argv[optind];
    }
  else
    {
       

      file_list = default_file_list;
    }

   
  ok = open_next_file ();
  if (in_stream == nullptr)
    goto cleanup;

   
  ok &= skip (n_bytes_to_skip);
  if (in_stream == nullptr)
    goto cleanup;

  pseudo_offset = (flag_pseudo_start ? pseudo_start - n_bytes_to_skip : 0);

   
  l_c_m = get_lcm ();

  if (width_specified)
    {
      if (desired_width != 0 && desired_width % l_c_m == 0)
        bytes_per_block = desired_width;
      else
        {
          error (0, 0, _("warning: invalid width %td; using %d instead"),
                 desired_width, l_c_m);
          bytes_per_block = l_c_m;
        }
    }
  else
    {
      if (l_c_m < DEFAULT_BYTES_PER_BLOCK)
        bytes_per_block = l_c_m * (DEFAULT_BYTES_PER_BLOCK / l_c_m);
      else
        bytes_per_block = l_c_m;
    }

   
  for (i = 0; i < n_specs; i++)
    {
      int fields_per_block = bytes_per_block / width_bytes[spec[i].size];
      int block_width = (spec[i].field_width + 1) * fields_per_block;
      if (width_per_block < block_width)
        width_per_block = block_width;
    }
  for (i = 0; i < n_specs; i++)
    {
      int fields_per_block = bytes_per_block / width_bytes[spec[i].size];
      int block_width = spec[i].field_width * fields_per_block;
      spec[i].pad_width = width_per_block - block_width;
    }

#ifdef DEBUG
  printf ("lcm=%d, width_per_block=%"PRIuMAX"\n", l_c_m,
          (uintmax_t) width_per_block);
  for (i = 0; i < n_specs; i++)
    {
      int fields_per_block = bytes_per_block / width_bytes[spec[i].size];
      affirm (bytes_per_block % width_bytes[spec[i].size] == 0);
      affirm (1 <= spec[i].pad_width / fields_per_block);
      printf ("%d: fmt=\"%s\" in_width=%d out_width=%d pad=%d\n",
              i, spec[i].fmt_string, width_bytes[spec[i].size],
              spec[i].field_width, spec[i].pad_width);
    }
#endif

  ok &= (flag_dump_strings ? dump_strings () : dump ());

cleanup:

  if (have_read_stdin && fclose (stdin) == EOF)
    error (EXIT_FAILURE, errno, _("standard input"));

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
