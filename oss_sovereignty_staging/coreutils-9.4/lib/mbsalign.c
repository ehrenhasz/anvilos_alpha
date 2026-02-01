 

#include <config.h>
#include "mbsalign.h"

#include "minmax.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>

 

static bool
wc_ensure_printable (wchar_t *wchars)
{
  bool replaced = false;
  wchar_t *wc = wchars;
  while (*wc)
    {
      if (!iswprint ((wint_t) *wc))
        {
          *wc = 0xFFFD;  
          replaced = true;
        }
      wc++;
    }
  return replaced;
}

 

static size_t
wc_truncate (wchar_t *wc, size_t width)
{
  size_t cells = 0;
  int next_cells = 0;

  while (*wc)
    {
      next_cells = wcwidth (*wc);
      if (next_cells == -1)  
        {
          *wc = 0xFFFD;  
          next_cells = 1;
        }
      if (cells + next_cells > width)
        break;
      cells += next_cells;
      wc++;
    }
  *wc = L'\0';
  return cells;
}

 

static char *
mbs_align_pad (char *dest, char const *dest_end, size_t n_spaces)
{
   
  while (n_spaces-- && (dest < dest_end))
    *dest++ = ' ';
  *dest = '\0';
  return dest;
}

 

size_t
mbsalign (char const *src, char *dest, size_t dest_size,
          size_t *width, mbs_align_t align, int flags)
{
  size_t ret = SIZE_MAX;
  size_t src_size = strlen (src) + 1;
  char *newstr = nullptr;
  wchar_t *str_wc = nullptr;
  char const *str_to_print = src;
  size_t n_cols = src_size - 1;
  size_t n_used_bytes = n_cols;  
  size_t n_spaces = 0;
  bool conversion = false;
  bool wc_enabled = false;

   
  if (!(flags & MBA_UNIBYTE_ONLY) && MB_CUR_MAX > 1)
    {
      size_t src_chars = mbstowcs (nullptr, src, 0);
      if (src_chars == SIZE_MAX)
        {
          if (flags & MBA_UNIBYTE_FALLBACK)
            goto mbsalign_unibyte;
          else
            goto mbsalign_cleanup;
        }
      src_chars += 1;  
      str_wc = malloc (src_chars * sizeof (wchar_t));
      if (str_wc == nullptr)
        {
          if (flags & MBA_UNIBYTE_FALLBACK)
            goto mbsalign_unibyte;
          else
            goto mbsalign_cleanup;
        }
      if (mbstowcs (str_wc, src, src_chars) != 0)
        {
          str_wc[src_chars - 1] = L'\0';
          wc_enabled = true;
          conversion = wc_ensure_printable (str_wc);
          n_cols = wcswidth (str_wc, src_chars);
        }
    }

   
  if (wc_enabled && (conversion || (n_cols > *width)))
    {
        if (conversion)
          {
              
            src_size = wcstombs (nullptr, str_wc, 0) + 1;
          }
        newstr = malloc (src_size);
        if (newstr == nullptr)
        {
          if (flags & MBA_UNIBYTE_FALLBACK)
            goto mbsalign_unibyte;
          else
            goto mbsalign_cleanup;
        }
        str_to_print = newstr;
        n_cols = wc_truncate (str_wc, *width);
        n_used_bytes = wcstombs (newstr, str_wc, src_size);
    }

mbsalign_unibyte:

  if (n_cols > *width)  
    {
      n_cols = *width;
      n_used_bytes = n_cols;
    }

  if (*width > n_cols)  
    n_spaces = *width - n_cols;

   
  *width = n_cols;

  {
    size_t start_spaces, end_spaces;

    switch (align)
      {
      case MBS_ALIGN_LEFT:
        start_spaces = 0;
        end_spaces = n_spaces;
        break;
      case MBS_ALIGN_RIGHT:
        start_spaces = n_spaces;
        end_spaces = 0;
        break;
      case MBS_ALIGN_CENTER:
      default:
        start_spaces = n_spaces / 2 + n_spaces % 2;
        end_spaces = n_spaces / 2;
        break;
      }

      if (flags & MBA_NO_LEFT_PAD)
        start_spaces = 0;
      if (flags & MBA_NO_RIGHT_PAD)
        end_spaces = 0;

       
      if (dest_size != 0)
        {
          size_t space_left;
          char *dest_end = dest + dest_size - 1;

          dest = mbs_align_pad (dest, dest_end, start_spaces);
          space_left = dest_end - dest;
          dest = mempcpy (dest, str_to_print, MIN (n_used_bytes, space_left));
          mbs_align_pad (dest, dest_end, end_spaces);
        }

     
    ret = n_used_bytes + ((start_spaces + end_spaces) * 1);
  }

mbsalign_cleanup:

  free (str_wc);
  free (newstr);

  return ret;
}

 

char *
ambsalign (char const *src, size_t *width, mbs_align_t align, int flags)
{
  size_t orig_width = *width;
  size_t size = *width;          
  size_t req = size;
  char *buf = nullptr;

  while (req >= size)
    {
      char *nbuf;
      size = req + 1;            
      nbuf = realloc (buf, size);
      if (nbuf == nullptr)
        {
          free (buf);
          buf = nullptr;
          break;
        }
      buf = nbuf;
      *width = orig_width;
      req = mbsalign (src, buf, size, width, align, flags);
      if (req == SIZE_MAX)
        {
          free (buf);
          buf = nullptr;
          break;
        }
    }

  return buf;
}
