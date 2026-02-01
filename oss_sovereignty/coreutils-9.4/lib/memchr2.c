 
void *
memchr2 (void const *s, int c1_in, int c2_in, size_t n)
{
   
  typedef unsigned long int longword;

  const unsigned char *char_ptr;
  void const *void_ptr;
  const longword *longword_ptr;
  longword repeated_one;
  longword repeated_c1;
  longword repeated_c2;
  unsigned char c1;
  unsigned char c2;

  c1 = (unsigned char) c1_in;
  c2 = (unsigned char) c2_in;

  if (c1 == c2)
    return memchr (s, c1, n);

   
  for (void_ptr = s;
       n > 0 && (uintptr_t) void_ptr % sizeof (longword) != 0;
       --n)
    {
      char_ptr = void_ptr;
      if (*char_ptr == c1 || *char_ptr == c2)
        return (void *) void_ptr;
      void_ptr = char_ptr + 1;
    }

  longword_ptr = void_ptr;

   

   
  repeated_one = 0x01010101;
  repeated_c1 = c1 | (c1 << 8);
  repeated_c2 = c2 | (c2 << 8);
  repeated_c1 |= repeated_c1 << 16;
  repeated_c2 |= repeated_c2 << 16;
  if (0xffffffffU < (longword) -1)
    {
      repeated_one |= repeated_one << 31 << 1;
      repeated_c1 |= repeated_c1 << 31 << 1;
      repeated_c2 |= repeated_c2 << 31 << 1;
      if (8 < sizeof (longword))
        {
          size_t i;

          for (i = 64; i < sizeof (longword) * 8; i *= 2)
            {
              repeated_one |= repeated_one << i;
              repeated_c1 |= repeated_c1 << i;
              repeated_c2 |= repeated_c2 << i;
            }
        }
    }

   

  while (n >= sizeof (longword))
    {
      longword longword1 = *longword_ptr ^ repeated_c1;
      longword longword2 = *longword_ptr ^ repeated_c2;

      if (((((longword1 - repeated_one) & ~longword1)
            | ((longword2 - repeated_one) & ~longword2))
           & (repeated_one << 7)) != 0)
        break;
      longword_ptr++;
      n -= sizeof (longword);
    }

  char_ptr = (const unsigned char *) longword_ptr;

   

  for (; n > 0; --n, ++char_ptr)
    {
      if (*char_ptr == c1 || *char_ptr == c2)
        return (void *) char_ptr;
    }

  return NULL;
}
