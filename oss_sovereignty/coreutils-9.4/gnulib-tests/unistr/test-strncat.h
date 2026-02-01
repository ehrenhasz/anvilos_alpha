 

static void
check_single (const UNIT *input, size_t length, size_t n)
{
  static const UNIT base[] = { 'C', 'h', 'a', 'n', 'g', 'i', 'n', 'g', 0 };
  size_t m;

  for (m = 0; m < SIZEOF (base); m++)
    {
      UNIT *dest;
      UNIT *result;
      size_t i;

      dest = (UNIT *) malloc ((1 + m + n + 2) * sizeof (UNIT));
      ASSERT (dest != NULL);

      dest[0] = MAGIC;
      for (i = 0; i < m; i++)
        dest[1 + i] = base[i];
      dest[1 + m] = 0;
      for (i = 1; i < n + 2; i++)
        dest[1 + m + i] = MAGIC;

      result = U_STRNCAT (dest + 1, input, n);
      ASSERT (result == dest + 1);

      ASSERT (dest[0] == MAGIC);
      for (i = 0; i < m; i++)
        ASSERT (dest[1 + i] == base[i]);
      for (i = 0; i < (n <= length ? n : length); i++)
        ASSERT (dest[1 + m + i] == input[i]);
      ASSERT (dest[1 + m + i] == 0);
      ASSERT (dest[1 + m + i + 1] == MAGIC);

      free (dest);
    }
}

static void
check (const UNIT *input, size_t input_length)
{
  size_t length;
  size_t n;

  ASSERT (input_length > 0);
  ASSERT (input[input_length - 1] == 0);
  length = input_length - 1;  

  for (n = 0; n <= 2 * length + 2; n++)
    check_single (input, length, n);

   
  {
    char *page_boundary = (char *) zerosize_ptr ();

    if (page_boundary != NULL)
      {
        for (n = 0; n <= 2 * length + 2; n++)
          {
            size_t n_to_copy = (n <= length ? n : length + 1);
            UNIT *copy;
            size_t i;

            copy = (UNIT *) page_boundary - n_to_copy;
            for (i = 0; i < n_to_copy; i++)
              copy[i] = input[i];

            check_single (copy, length, n);
          }
      }
  }
}
