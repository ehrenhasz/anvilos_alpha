 

int
main ()
{
   
  {
    static const UNIT src[] = { 'c', 'l', 'i', 'm', 'a', 't', 'e' };
    size_t n;

    for (n = 0; n <= SIZEOF (src); n++)
      {
        UNIT dest[1 + SIZEOF (src) + 1] =
          { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
        UNIT *ret;
        size_t i;

        ret = U_CPY (dest + 1, src, n);
        ASSERT (ret == dest + 1);
        ASSERT (dest[0] == MAGIC);
        for (i = 0; i < n; i++)
          ASSERT (dest[1 + i] == src[i]);
        ASSERT (dest[1 + n] == MAGIC);
      }
  }

  return 0;
}
