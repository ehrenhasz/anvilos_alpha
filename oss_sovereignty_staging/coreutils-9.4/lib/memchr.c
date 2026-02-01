 
void *
__memchr (void const *s, int c_in, size_t n)
{
   
  typedef unsigned long int longword;

  const unsigned char *char_ptr;
  const longword *longword_ptr;
  longword repeated_one;
  longword repeated_c;
  unsigned reg_char c;

  c = (unsigned char) c_in;

   
  for (char_ptr = (const unsigned char *) s;
       n > 0 && (size_t) char_ptr % sizeof (longword) != 0;
       --n, ++char_ptr)
    if (*char_ptr == c)
      return (void *) char_ptr;

  longword_ptr = (const longword *) char_ptr;

   

   
  repeated_one = 0x01010101;
  repeated_c = c | (c << 8);
  repeated_c |= repeated_c << 16;
  if (0xffffffffU < (longword) -1)
    {
      repeated_one |= repeated_one << 31 << 1;
      repeated_c |= repeated_c << 31 << 1;
      if (8 < sizeof (longword))
        {
          size_t i;

          for (i = 64; i < sizeof (longword) * 8; i *= 2)
            {
              repeated_one |= repeated_one << i;
              repeated_c |= repeated_c << i;
            }
        }
    }

   

  while (n >= sizeof (longword))
    {
      longword longword1 = *longword_ptr ^ repeated_c;

      if ((((longword1 - repeated_one) & ~longword1)
           & (repeated_one << 7)) != 0)
        break;
      longword_ptr++;
      n -= sizeof (longword);
    }

  char_ptr = (const unsigned char *) longword_ptr;

   

  for (; n > 0; --n, ++char_ptr)
    {
      if (*char_ptr == c)
        return (void *) char_ptr;
    }

  return NULL;
}
#ifdef weak_alias
weak_alias (__memchr, BP_SYM (memchr))
#endif
