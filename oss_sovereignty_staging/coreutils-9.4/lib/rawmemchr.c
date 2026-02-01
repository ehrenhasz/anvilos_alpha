 
#include <string.h>

 
#if !HAVE_RAWMEMCHR

# include <limits.h>
# include <stdint.h>


 
void *
rawmemchr (const void *s, int c_in)
{
   
  typedef uintptr_t longword;
   
  static_assert (UINTPTR_WIDTH == UCHAR_WIDTH * sizeof (longword));

  const unsigned char *char_ptr;
  unsigned char c = c_in;

   
  for (char_ptr = (const unsigned char *) s;
       (uintptr_t) char_ptr % alignof (longword) != 0;
       ++char_ptr)
    if (*char_ptr == c)
      return (void *) char_ptr;

  longword const *longword_ptr = s = char_ptr;

   
  longword repeated_one = (longword) -1 / UCHAR_MAX;
  longword repeated_c = repeated_one * c;
  longword repeated_hibit = repeated_one * (UCHAR_MAX / 2 + 1);

   

  while (1)
    {
      longword longword1 = *longword_ptr ^ repeated_c;

      if ((((longword1 - repeated_one) & ~longword1) & repeated_hibit) != 0)
        break;
      longword_ptr++;
    }

  char_ptr = s = longword_ptr;

   

  while (*char_ptr != c)
    char_ptr++;
  return (void *) char_ptr;
}

#endif
