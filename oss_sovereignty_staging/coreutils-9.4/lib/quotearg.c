 

 
#if (__GNUC__ == 4 && 6 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

#include <config.h>

#include "quotearg.h"
#include "quote.h"

#include "attribute.h"
#include "minmax.h"
#include "xalloc.h"
#include "c-strcaseeq.h"
#include "localcharset.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

#define INT_BITS (sizeof (int) * CHAR_BIT)

struct quoting_options
{
   
  enum quoting_style style;

   
  int flags;

   
  unsigned int quote_these_too[(UCHAR_MAX / INT_BITS) + 1];

   
  char const *left_quote;

   
  char const *right_quote;
};

 
char const *const quoting_style_args[] =
{
  "literal",
  "shell",
  "shell-always",
  "shell-escape",
  "shell-escape-always",
  "c",
  "c-maybe",
  "escape",
  "locale",
  "clocale",
  0
};

 
enum quoting_style const quoting_style_vals[] =
{
  literal_quoting_style,
  shell_quoting_style,
  shell_always_quoting_style,
  shell_escape_quoting_style,
  shell_escape_always_quoting_style,
  c_quoting_style,
  c_maybe_quoting_style,
  escape_quoting_style,
  locale_quoting_style,
  clocale_quoting_style
};

 
static struct quoting_options default_quoting_options;

 
struct quoting_options *
clone_quoting_options (struct quoting_options *o)
{
  int e = errno;
  struct quoting_options *p = xmemdup (o ? o : &default_quoting_options,
                                       sizeof *o);
  errno = e;
  return p;
}

 
enum quoting_style
get_quoting_style (struct quoting_options const *o)
{
  return (o ? o : &default_quoting_options)->style;
}

 
void
set_quoting_style (struct quoting_options *o, enum quoting_style s)
{
  (o ? o : &default_quoting_options)->style = s;
}

 
int
set_char_quoting (struct quoting_options *o, char c, int i)
{
  unsigned char uc = c;
  unsigned int *p =
    (o ? o : &default_quoting_options)->quote_these_too + uc / INT_BITS;
  int shift = uc % INT_BITS;
  int r = (*p >> shift) & 1;
  *p ^= ((i & 1) ^ r) << shift;
  return r;
}

 
int
set_quoting_flags (struct quoting_options *o, int i)
{
  int r;
  if (!o)
    o = &default_quoting_options;
  r = o->flags;
  o->flags = i;
  return r;
}

void
set_custom_quoting (struct quoting_options *o,
                    char const *left_quote, char const *right_quote)
{
  if (!o)
    o = &default_quoting_options;
  o->style = custom_quoting_style;
  if (!left_quote || !right_quote)
    abort ();
  o->left_quote = left_quote;
  o->right_quote = right_quote;
}

 
static struct quoting_options  
quoting_options_from_style (enum quoting_style style)
{
  struct quoting_options o = { literal_quoting_style, 0, { 0 }, NULL, NULL };
  if (style == custom_quoting_style)
    abort ();
  o.style = style;
  return o;
}

 
static char const *
gettext_quote (char const *msgid, enum quoting_style s)
{
  char const *translation = _(msgid);
  char const *locale_code;

  if (translation != msgid)
    return translation;

   
  locale_code = locale_charset ();
  if (STRCASEEQ (locale_code, "UTF-8", 'U','T','F','-','8',0,0,0,0))
    return msgid[0] == '`' ? "\xe2\x80\x98": "\xe2\x80\x99";
  if (STRCASEEQ (locale_code, "GB18030", 'G','B','1','8','0','3','0',0,0))
    return msgid[0] == '`' ? "\xa1\ae": "\xa1\xaf";

  return (s == clocale_quoting_style ? "\"" : "'");
}

 

static size_t
quotearg_buffer_restyled (char *buffer, size_t buffersize,
                          char const *arg, size_t argsize,
                          enum quoting_style quoting_style, int flags,
                          unsigned int const *quote_these_too,
                          char const *left_quote,
                          char const *right_quote)
{
  size_t i;
  size_t len = 0;
  size_t orig_buffersize = 0;
  char const *quote_string = 0;
  size_t quote_string_len = 0;
  bool backslash_escapes = false;
  bool unibyte_locale = MB_CUR_MAX == 1;
  bool elide_outer_quotes = (flags & QA_ELIDE_OUTER_QUOTES) != 0;
  bool pending_shell_escape_end = false;
  bool encountered_single_quote = false;
  bool all_c_and_shell_quote_compat = true;

#define STORE(c) \
    do \
      { \
        if (len < buffersize) \
          buffer[len] = (c); \
        len++; \
      } \
    while (0)

#define START_ESC() \
    do \
      { \
        if (elide_outer_quotes) \
          goto force_outer_quoting_style; \
        escaping = true; \
        if (quoting_style == shell_always_quoting_style \
            && ! pending_shell_escape_end) \
          { \
            STORE ('\''); \
            STORE ('$'); \
            STORE ('\''); \
            pending_shell_escape_end = true; \
          } \
        STORE ('\\'); \
      } \
    while (0)

#define END_ESC() \
    do \
      { \
        if (pending_shell_escape_end && ! escaping) \
          { \
            STORE ('\''); \
            STORE ('\''); \
            pending_shell_escape_end = false; \
          } \
      } \
    while (0)

 process_input:

  switch (quoting_style)
    {
    case c_maybe_quoting_style:
      quoting_style = c_quoting_style;
      elide_outer_quotes = true;
      FALLTHROUGH;
    case c_quoting_style:
      if (!elide_outer_quotes)
        STORE ('"');
      backslash_escapes = true;
      quote_string = "\"";
      quote_string_len = 1;
      break;

    case escape_quoting_style:
      backslash_escapes = true;
      elide_outer_quotes = false;
      break;

    case locale_quoting_style:
    case clocale_quoting_style:
    case custom_quoting_style:
      {
        if (quoting_style != custom_quoting_style)
          {
             
            left_quote = gettext_quote (N_("`"), quoting_style);
            right_quote = gettext_quote (N_("'"), quoting_style);
          }
        if (!elide_outer_quotes)
          for (quote_string = left_quote; *quote_string; quote_string++)
            STORE (*quote_string);
        backslash_escapes = true;
        quote_string = right_quote;
        quote_string_len = strlen (quote_string);
      }
      break;

    case shell_escape_quoting_style:
      backslash_escapes = true;
      FALLTHROUGH;
    case shell_quoting_style:
      elide_outer_quotes = true;
      FALLTHROUGH;
    case shell_escape_always_quoting_style:
      if (!elide_outer_quotes)
        backslash_escapes = true;
      FALLTHROUGH;
    case shell_always_quoting_style:
      quoting_style = shell_always_quoting_style;
      if (!elide_outer_quotes)
        STORE ('\'');
      quote_string = "'";
      quote_string_len = 1;
      break;

    case literal_quoting_style:
      elide_outer_quotes = false;
      break;

    default:
      abort ();
    }

  for (i = 0;  ! (argsize == SIZE_MAX ? arg[i] == '\0' : i == argsize);  i++)
    {
      unsigned char c;
      unsigned char esc;
      bool is_right_quote = false;
      bool escaping = false;
      bool c_and_shell_quote_compat = false;

      if (backslash_escapes
          && quoting_style != shell_always_quoting_style
          && quote_string_len
          && (i + quote_string_len
              <= (argsize == SIZE_MAX && 1 < quote_string_len
                   
                  ? (argsize = strlen (arg)) : argsize))
          && memcmp (arg + i, quote_string, quote_string_len) == 0)
        {
          if (elide_outer_quotes)
            goto force_outer_quoting_style;
          is_right_quote = true;
        }

      c = arg[i];
      switch (c)
        {
        case '\0':
          if (backslash_escapes)
            {
              START_ESC ();
               
              if (quoting_style != shell_always_quoting_style
                  && i + 1 < argsize && '0' <= arg[i + 1] && arg[i + 1] <= '9')
                {
                  STORE ('0');
                  STORE ('0');
                }
              c = '0';
               
            }
          else if (flags & QA_ELIDE_NULL_BYTES)
            continue;
          break;

        case '?':
          switch (quoting_style)
            {
            case shell_always_quoting_style:
              if (elide_outer_quotes)
                goto force_outer_quoting_style;
              break;

            case c_quoting_style:
              if ((flags & QA_SPLIT_TRIGRAPHS)
                  && i + 2 < argsize && arg[i + 1] == '?')
                switch (arg[i + 2])
                  {
                  case '!': case '\'':
                  case '(': case ')': case '-': case '/':
                  case '<': case '=': case '>':
                     
                    if (elide_outer_quotes)
                      goto force_outer_quoting_style;
                    c = arg[i + 2];
                    i += 2;
                    STORE ('?');
                    STORE ('"');
                    STORE ('"');
                    STORE ('?');
                    break;

                  default:
                    break;
                  }
              break;

            default:
              break;
            }
          break;

        case '\a': esc = 'a'; goto c_escape;
        case '\b': esc = 'b'; goto c_escape;
        case '\f': esc = 'f'; goto c_escape;
        case '\n': esc = 'n'; goto c_and_shell_escape;
        case '\r': esc = 'r'; goto c_and_shell_escape;
        case '\t': esc = 't'; goto c_and_shell_escape;
        case '\v': esc = 'v'; goto c_escape;
        case '\\': esc = c;
           
          if (quoting_style == shell_always_quoting_style)
            {
              if (elide_outer_quotes)
                goto force_outer_quoting_style;
              goto store_c;
            }

           
          if (backslash_escapes && elide_outer_quotes && quote_string_len)
            goto store_c;

        c_and_shell_escape:
          if (quoting_style == shell_always_quoting_style
              && elide_outer_quotes)
            goto force_outer_quoting_style;
           
        c_escape:
          if (backslash_escapes)
            {
              c = esc;
              goto store_escape;
            }
          break;

        case '{': case '}':  
          if (! (argsize == SIZE_MAX ? arg[1] == '\0' : argsize == 1))
            break;
          FALLTHROUGH;
        case '#': case '~':
          if (i != 0)
            break;
          FALLTHROUGH;
        case ' ':
          c_and_shell_quote_compat = true;
          FALLTHROUGH;
        case '!':  
        case '"': case '$': case '&':
        case '(': case ')': case '*': case ';':
        case '<':
        case '=':  
        case '>': case '[':
        case '^':  
        case '`': case '|':
           
          if (quoting_style == shell_always_quoting_style
              && elide_outer_quotes)
            goto force_outer_quoting_style;
          break;

        case '\'':
          encountered_single_quote = true;
          c_and_shell_quote_compat = true;
          if (quoting_style == shell_always_quoting_style)
            {
              if (elide_outer_quotes)
                goto force_outer_quoting_style;

              if (buffersize && ! orig_buffersize)
                {
                   
                  orig_buffersize = buffersize;
                  buffersize = 0;
                }

              STORE ('\'');
              STORE ('\\');
              STORE ('\'');
              pending_shell_escape_end = false;
            }
          break;

        case '%': case '+': case ',': case '-': case '.': case '/':
        case '0': case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': case ':':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z': case ']': case '_': case 'a': case 'b':
        case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
        case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
           
          c_and_shell_quote_compat = true;
          break;

        default:
           
          {
             
            size_t m;

            bool printable;

            if (unibyte_locale)
              {
                m = 1;
                printable = isprint (c) != 0;
              }
            else
              {
                mbstate_t mbstate;
                mbszero (&mbstate);

                m = 0;
                printable = true;
                if (argsize == SIZE_MAX)
                  argsize = strlen (arg);

                for (;;)
                  {
                    char32_t w;
                    size_t bytes = mbrtoc32 (&w, &arg[i + m],
                                             argsize - (i + m), &mbstate);
                    if (bytes == 0)
                      break;
                    else if (bytes == (size_t) -1)
                      {
                        printable = false;
                        break;
                      }
                    else if (bytes == (size_t) -2)
                      {
                        printable = false;
                        while (i + m < argsize && arg[i + m])
                          m++;
                        break;
                      }
                    else
                      {
                        #if !GNULIB_MBRTOC32_REGULAR
                        if (bytes == (size_t) -3)
                          bytes = 0;
                        #endif
                         
                        if ('[' == 0x5b && elide_outer_quotes
                            && quoting_style == shell_always_quoting_style)
                          {
                            size_t j;
                            for (j = 1; j < bytes; j++)
                              switch (arg[i + m + j])
                                {
                                case '[': case '\\': case '^':
                                case '`': case '|':
                                  goto force_outer_quoting_style;

                                default:
                                  break;
                                }
                          }

                        if (! c32isprint (w))
                          printable = false;
                        m += bytes;
                      }
                    #if !GNULIB_MBRTOC32_REGULAR
                    if (mbsinit (&mbstate))
                    #endif
                      break;
                  }
              }

            c_and_shell_quote_compat = printable;

            if (1 < m || (backslash_escapes && ! printable))
              {
                 
                size_t ilim = i + m;

                for (;;)
                  {
                    if (backslash_escapes && ! printable)
                      {
                        START_ESC ();
                        STORE ('0' + (c >> 6));
                        STORE ('0' + ((c >> 3) & 7));
                        c = '0' + (c & 7);
                      }
                    else if (is_right_quote)
                      {
                        STORE ('\\');
                        is_right_quote = false;
                      }
                    if (ilim <= i + 1)
                      break;
                    END_ESC ();
                    STORE (c);
                    c = arg[++i];
                  }

                goto store_c;
              }
          }
        }

      if (! (((backslash_escapes && quoting_style != shell_always_quoting_style)
              || elide_outer_quotes)
             && quote_these_too
             && quote_these_too[c / INT_BITS] >> (c % INT_BITS) & 1)
          && !is_right_quote)
        goto store_c;

    store_escape:
      START_ESC ();

    store_c:
      END_ESC ();
      STORE (c);

      if (! c_and_shell_quote_compat)
        all_c_and_shell_quote_compat = false;
    }

  if (len == 0 && quoting_style == shell_always_quoting_style
      && elide_outer_quotes)
    goto force_outer_quoting_style;

   
  if (quoting_style == shell_always_quoting_style && ! elide_outer_quotes
      && encountered_single_quote)
    {
      if (all_c_and_shell_quote_compat)
        return quotearg_buffer_restyled (buffer, orig_buffersize, arg, argsize,
                                         c_quoting_style,
                                         flags, quote_these_too,
                                         left_quote, right_quote);
      else if (! buffersize && orig_buffersize)
        {
           
          buffersize = orig_buffersize;
          len = 0;
          goto process_input;
        }
    }

  if (quote_string && !elide_outer_quotes)
    for (; *quote_string; quote_string++)
      STORE (*quote_string);

  if (len < buffersize)
    buffer[len] = '\0';
  return len;

 force_outer_quoting_style:
   
  if (quoting_style == shell_always_quoting_style && backslash_escapes)
    quoting_style = shell_escape_always_quoting_style;
  return quotearg_buffer_restyled (buffer, buffersize, arg, argsize,
                                   quoting_style,
                                   flags & ~QA_ELIDE_OUTER_QUOTES, NULL,
                                   left_quote, right_quote);
}

 
size_t
quotearg_buffer (char *buffer, size_t buffersize,
                 char const *arg, size_t argsize,
                 struct quoting_options const *o)
{
  struct quoting_options const *p = o ? o : &default_quoting_options;
  int e = errno;
  size_t r = quotearg_buffer_restyled (buffer, buffersize, arg, argsize,
                                       p->style, p->flags, p->quote_these_too,
                                       p->left_quote, p->right_quote);
  errno = e;
  return r;
}

char *
quotearg_alloc (char const *arg, size_t argsize,
                struct quoting_options const *o)
{
  return quotearg_alloc_mem (arg, argsize, NULL, o);
}

 
char *
quotearg_alloc_mem (char const *arg, size_t argsize, size_t *size,
                    struct quoting_options const *o)
{
  struct quoting_options const *p = o ? o : &default_quoting_options;
  int e = errno;
   
  int flags = p->flags | (size ? 0 : QA_ELIDE_NULL_BYTES);
  size_t bufsize = quotearg_buffer_restyled (0, 0, arg, argsize, p->style,
                                             flags, p->quote_these_too,
                                             p->left_quote,
                                             p->right_quote) + 1;
  char *buf = xcharalloc (bufsize);
  quotearg_buffer_restyled (buf, bufsize, arg, argsize, p->style, flags,
                            p->quote_these_too,
                            p->left_quote, p->right_quote);
  errno = e;
  if (size)
    *size = bufsize - 1;
  return buf;
}

 
struct slotvec
{
  size_t size;
  char *val;
};

 
static char slot0[256];
static int nslots = 1;
static struct slotvec slotvec0 = {sizeof slot0, slot0};
static struct slotvec *slotvec = &slotvec0;

void
quotearg_free (void)
{
  struct slotvec *sv = slotvec;
  int i;
  for (i = 1; i < nslots; i++)
    free (sv[i].val);
  if (sv[0].val != slot0)
    {
      free (sv[0].val);
      slotvec0.size = sizeof slot0;
      slotvec0.val = slot0;
    }
  if (sv != &slotvec0)
    {
      free (sv);
      slotvec = &slotvec0;
    }
  nslots = 1;
}

 
static char *
quotearg_n_options (int n, char const *arg, size_t argsize,
                    struct quoting_options const *options)
{
  int e = errno;

  struct slotvec *sv = slotvec;

  int nslots_max = MIN (INT_MAX, IDX_MAX);
  if (! (0 <= n && n < nslots_max))
    abort ();

  if (nslots <= n)
    {
      bool preallocated = (sv == &slotvec0);
      idx_t new_nslots = nslots;

      slotvec = sv = xpalloc (preallocated ? NULL : sv, &new_nslots,
                              n - nslots + 1, nslots_max, sizeof *sv);
      if (preallocated)
        *sv = slotvec0;
      memset (sv + nslots, 0, (new_nslots - nslots) * sizeof *sv);
      nslots = new_nslots;
    }

  {
    size_t size = sv[n].size;
    char *val = sv[n].val;
     
    int flags = options->flags | QA_ELIDE_NULL_BYTES;
    size_t qsize = quotearg_buffer_restyled (val, size, arg, argsize,
                                             options->style, flags,
                                             options->quote_these_too,
                                             options->left_quote,
                                             options->right_quote);

    if (size <= qsize)
      {
        sv[n].size = size = qsize + 1;
        if (val != slot0)
          free (val);
        sv[n].val = val = xcharalloc (size);
        quotearg_buffer_restyled (val, size, arg, argsize, options->style,
                                  flags, options->quote_these_too,
                                  options->left_quote,
                                  options->right_quote);
      }

    errno = e;
    return val;
  }
}

char *
quotearg_n (int n, char const *arg)
{
  return quotearg_n_options (n, arg, SIZE_MAX, &default_quoting_options);
}

char *
quotearg_n_mem (int n, char const *arg, size_t argsize)
{
  return quotearg_n_options (n, arg, argsize, &default_quoting_options);
}

char *
quotearg (char const *arg)
{
  return quotearg_n (0, arg);
}

char *
quotearg_mem (char const *arg, size_t argsize)
{
  return quotearg_n_mem (0, arg, argsize);
}

char *
quotearg_n_style (int n, enum quoting_style s, char const *arg)
{
  struct quoting_options const o = quoting_options_from_style (s);
  return quotearg_n_options (n, arg, SIZE_MAX, &o);
}

char *
quotearg_n_style_mem (int n, enum quoting_style s,
                      char const *arg, size_t argsize)
{
  struct quoting_options const o = quoting_options_from_style (s);
  return quotearg_n_options (n, arg, argsize, &o);
}

char *
quotearg_style (enum quoting_style s, char const *arg)
{
  return quotearg_n_style (0, s, arg);
}

char *
quotearg_style_mem (enum quoting_style s, char const *arg, size_t argsize)
{
  return quotearg_n_style_mem (0, s, arg, argsize);
}

char *
quotearg_char_mem (char const *arg, size_t argsize, char ch)
{
  struct quoting_options options;
  options = default_quoting_options;
  set_char_quoting (&options, ch, 1);
  return quotearg_n_options (0, arg, argsize, &options);
}

char *
quotearg_char (char const *arg, char ch)
{
  return quotearg_char_mem (arg, SIZE_MAX, ch);
}

char *
quotearg_colon (char const *arg)
{
  return quotearg_char (arg, ':');
}

char *
quotearg_colon_mem (char const *arg, size_t argsize)
{
  return quotearg_char_mem (arg, argsize, ':');
}

char *
quotearg_n_style_colon (int n, enum quoting_style s, char const *arg)
{
  struct quoting_options options;
  options = quoting_options_from_style (s);
  set_char_quoting (&options, ':', 1);
  return quotearg_n_options (n, arg, SIZE_MAX, &options);
}

char *
quotearg_n_custom (int n, char const *left_quote,
                   char const *right_quote, char const *arg)
{
  return quotearg_n_custom_mem (n, left_quote, right_quote, arg,
                                SIZE_MAX);
}

char *
quotearg_n_custom_mem (int n, char const *left_quote,
                       char const *right_quote,
                       char const *arg, size_t argsize)
{
  struct quoting_options o = default_quoting_options;
  set_custom_quoting (&o, left_quote, right_quote);
  return quotearg_n_options (n, arg, argsize, &o);
}

char *
quotearg_custom (char const *left_quote, char const *right_quote,
                 char const *arg)
{
  return quotearg_n_custom (0, left_quote, right_quote, arg);
}

char *
quotearg_custom_mem (char const *left_quote, char const *right_quote,
                     char const *arg, size_t argsize)
{
  return quotearg_n_custom_mem (0, left_quote, right_quote, arg,
                                argsize);
}


 
struct quoting_options quote_quoting_options =
  {
    locale_quoting_style,
    0,
    { 0 },
    NULL, NULL
  };

char const *
quote_n_mem (int n, char const *arg, size_t argsize)
{
  return quotearg_n_options (n, arg, argsize, &quote_quoting_options);
}

char const *
quote_mem (char const *arg, size_t argsize)
{
  return quote_n_mem (0, arg, argsize);
}

char const *
quote_n (int n, char const *arg)
{
  return quote_n_mem (n, arg, SIZE_MAX);
}

char const *
quote (char const *arg)
{
  return quote_n (0, arg);
}

 
