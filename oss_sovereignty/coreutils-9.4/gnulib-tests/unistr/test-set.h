 

int
main ()
{
  {
#define NMAX 7
    size_t n;

    for (n = 0; n <= NMAX; n++)
      {
        UNIT dest[1 + NMAX + 1] =
          { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
        UNIT *ret;
        size_t i;

        ret = U_SET (dest + 1, VALUE, n);
        ASSERT (ret == dest + 1);
        ASSERT (dest[0] == MAGIC);
        for (i = 0; i < n; i++)
          ASSERT (dest[1 + i] == VALUE);
        ASSERT (dest[1 + n] == MAGIC);
      }
#undef NMAX
  }

  return 0;
}
