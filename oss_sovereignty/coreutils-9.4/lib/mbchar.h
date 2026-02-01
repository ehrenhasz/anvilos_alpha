 

 

#ifndef _MBCHAR_H
#define _MBCHAR_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <string.h>
#include <uchar.h>

_GL_INLINE_HEADER_BEGIN
#ifndef MBCHAR_INLINE
# define MBCHAR_INLINE _GL_INLINE
#endif

 
#define MBCHAR_BUF_SIZE 4

struct mbchar
{
  const char *ptr;       
  size_t bytes;          
  bool wc_valid;         
  char32_t wc;           
#if defined GNULIB_MBFILE
  char buf[MBCHAR_BUF_SIZE];  
#endif
};

 

typedef struct mbchar mbchar_t;

 
#define mb_ptr(mbc) ((mbc).ptr)
#define mb_len(mbc) ((mbc).bytes)

 
#define mb_iseq(mbc, sc) ((mbc).wc_valid && (mbc).wc == (sc))
#define mb_isnul(mbc) ((mbc).wc_valid && (mbc).wc == 0)
#define mb_cmp(mbc1, mbc2) \
  ((mbc1).wc_valid                                                      \
   ? ((mbc2).wc_valid                                                   \
      ? _GL_CMP ((mbc1).wc, (mbc2).wc)                                  \
      : -1)                                                             \
   : ((mbc2).wc_valid                                                   \
      ? 1                                                               \
      : (mbc1).bytes == (mbc2).bytes                                    \
        ? memcmp ((mbc1).ptr, (mbc2).ptr, (mbc1).bytes)                 \
        : (mbc1).bytes < (mbc2).bytes                                   \
          ? (memcmp ((mbc1).ptr, (mbc2).ptr, (mbc1).bytes) > 0 ? 1 : -1) \
          : (memcmp ((mbc1).ptr, (mbc2).ptr, (mbc2).bytes) >= 0 ? 1 : -1)))
#define mb_casecmp(mbc1, mbc2) \
  ((mbc1).wc_valid                                                      \
   ? ((mbc2).wc_valid                                                   \
      ? _GL_CMP (c32tolower ((mbc1).wc), c32tolower ((mbc2).wc))        \
      : -1)                                                             \
   : ((mbc2).wc_valid                                                   \
      ? 1                                                               \
      : (mbc1).bytes == (mbc2).bytes                                    \
        ? memcmp ((mbc1).ptr, (mbc2).ptr, (mbc1).bytes)                 \
        : (mbc1).bytes < (mbc2).bytes                                   \
          ? (memcmp ((mbc1).ptr, (mbc2).ptr, (mbc1).bytes) > 0 ? 1 : -1) \
          : (memcmp ((mbc1).ptr, (mbc2).ptr, (mbc2).bytes) >= 0 ? 1 : -1)))
#define mb_equal(mbc1, mbc2) \
  ((mbc1).wc_valid && (mbc2).wc_valid                                   \
   ? (mbc1).wc == (mbc2).wc                                             \
   : (mbc1).bytes == (mbc2).bytes                                       \
     && memcmp ((mbc1).ptr, (mbc2).ptr, (mbc1).bytes) == 0)
#define mb_caseequal(mbc1, mbc2) \
  ((mbc1).wc_valid && (mbc2).wc_valid                                   \
   ? c32tolower ((mbc1).wc) == c32tolower ((mbc2).wc)                   \
   : (mbc1).bytes == (mbc2).bytes                                       \
     && memcmp ((mbc1).ptr, (mbc2).ptr, (mbc1).bytes) == 0)

 
#define mb_isascii(mbc) \
  ((mbc).wc_valid && (mbc).wc >= 0 && (mbc).wc <= 127)
#define mb_isalnum(mbc) ((mbc).wc_valid && c32isalnum ((mbc).wc))
#define mb_isalpha(mbc) ((mbc).wc_valid && c32isalpha ((mbc).wc))
#define mb_isblank(mbc) ((mbc).wc_valid && c32isblank ((mbc).wc))
#define mb_iscntrl(mbc) ((mbc).wc_valid && c32iscntrl ((mbc).wc))
#define mb_isdigit(mbc) ((mbc).wc_valid && c32isdigit ((mbc).wc))
#define mb_isgraph(mbc) ((mbc).wc_valid && c32isgraph ((mbc).wc))
#define mb_islower(mbc) ((mbc).wc_valid && c32islower ((mbc).wc))
#define mb_isprint(mbc) ((mbc).wc_valid && c32isprint ((mbc).wc))
#define mb_ispunct(mbc) ((mbc).wc_valid && c32ispunct ((mbc).wc))
#define mb_isspace(mbc) ((mbc).wc_valid && c32isspace ((mbc).wc))
#define mb_isupper(mbc) ((mbc).wc_valid && c32isupper ((mbc).wc))
#define mb_isxdigit(mbc) ((mbc).wc_valid && c32isxdigit ((mbc).wc))

 

 
#define MB_UNPRINTABLE_WIDTH 1

MBCHAR_INLINE int
mb_width_aux (char32_t wc)
{
  int w = c32width (wc);
   
  return (w >= 0 ? w : c32iscntrl (wc) ? 0 : MB_UNPRINTABLE_WIDTH);
}

#define mb_width(mbc) \
  ((mbc).wc_valid ? mb_width_aux ((mbc).wc) : MB_UNPRINTABLE_WIDTH)

 
#define mb_putc(mbc, stream)  fwrite ((mbc).ptr, 1, (mbc).bytes, (stream))

#if defined GNULIB_MBFILE
 
# define mb_setascii(mbc, sc) \
   ((mbc)->ptr = (mbc)->buf, (mbc)->bytes = 1, (mbc)->wc_valid = 1, \
    (mbc)->wc = (mbc)->buf[0] = (sc))
#endif

 
MBCHAR_INLINE void
mb_copy (mbchar_t *new_mbc, const mbchar_t *old_mbc)
{
#if defined GNULIB_MBFILE
  if (old_mbc->ptr == &old_mbc->buf[0])
    {
      memcpy (&new_mbc->buf[0], &old_mbc->buf[0], old_mbc->bytes);
      new_mbc->ptr = &new_mbc->buf[0];
    }
  else
#endif
    new_mbc->ptr = old_mbc->ptr;
  new_mbc->bytes = old_mbc->bytes;
  if ((new_mbc->wc_valid = old_mbc->wc_valid))
    new_mbc->wc = old_mbc->wc;
}


 
#if (' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
    && ('$' == 36) && ('%' == 37) && ('&' == 38) && ('\'' == 39) \
    && ('(' == 40) && (')' == 41) && ('*' == 42) && ('+' == 43) \
    && (',' == 44) && ('-' == 45) && ('.' == 46) && ('/' == 47) \
    && ('0' == 48) && ('1' == 49) && ('2' == 50) && ('3' == 51) \
    && ('4' == 52) && ('5' == 53) && ('6' == 54) && ('7' == 55) \
    && ('8' == 56) && ('9' == 57) && (':' == 58) && (';' == 59) \
    && ('<' == 60) && ('=' == 61) && ('>' == 62) && ('?' == 63) \
    && ('@' == 64) && ('A' == 65) && ('B' == 66) && ('C' == 67) \
    && ('D' == 68) && ('E' == 69) && ('F' == 70) && ('G' == 71) \
    && ('H' == 72) && ('I' == 73) && ('J' == 74) && ('K' == 75) \
    && ('L' == 76) && ('M' == 77) && ('N' == 78) && ('O' == 79) \
    && ('P' == 80) && ('Q' == 81) && ('R' == 82) && ('S' == 83) \
    && ('T' == 84) && ('U' == 85) && ('V' == 86) && ('W' == 87) \
    && ('X' == 88) && ('Y' == 89) && ('Z' == 90) && ('[' == 91) \
    && ('\\' == 92) && (']' == 93) && ('^' == 94) && ('_' == 95) \
    && ('`' == 96) && ('a' == 97) && ('b' == 98) && ('c' == 99) \
    && ('d' == 100) && ('e' == 101) && ('f' == 102) && ('g' == 103) \
    && ('h' == 104) && ('i' == 105) && ('j' == 106) && ('k' == 107) \
    && ('l' == 108) && ('m' == 109) && ('n' == 110) && ('o' == 111) \
    && ('p' == 112) && ('q' == 113) && ('r' == 114) && ('s' == 115) \
    && ('t' == 116) && ('u' == 117) && ('v' == 118) && ('w' == 119) \
    && ('x' == 120) && ('y' == 121) && ('z' == 122) && ('{' == 123) \
    && ('|' == 124) && ('}' == 125) && ('~' == 126)
 
# define IS_BASIC_ASCII 1

 
# define is_basic(c) ((unsigned char) (c) < 0x80)

#else

MBCHAR_INLINE bool
is_basic (char c)
{
  switch (c)
    {
    case '\0':
    case '\007': case '\010':
    case '\t': case '\n': case '\v': case '\f': case '\r':
    case ' ': case '!': case '"': case '#': case '$': case '%':
    case '&': case '\'': case '(': case ')': case '*':
    case '+': case ',': case '-': case '.': case '/':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case ':': case ';': case '<': case '=': case '>':
    case '?': case '@':
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y':
    case 'Z':
    case '[': case '\\': case ']': case '^': case '_': case '`':
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y':
    case 'z': case '{': case '|': case '}': case '~':
      return 1;
    default:
      return 0;
    }
}

#endif

_GL_INLINE_HEADER_END

#endif  
