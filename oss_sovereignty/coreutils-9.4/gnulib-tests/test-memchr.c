 
#define MEMCHR (char *) memchr

int
main (void)
{
  size_t n = 0x100000;
  char *input = malloc (n);
  ASSERT (input);

  input[0] = 'a';
  input[1] = 'b';
  memset (input + 2, 'c', 1024);
  memset (input + 1026, 'd', n - 1028);
  input[n - 2] = 'e';
  input[n - 1] = 'a';

   
  ASSERT (MEMCHR (input, 'a', n) == input);

  ASSERT (MEMCHR (input, 'a', 0) == NULL);

  {
    void *page_boundary = zerosize_ptr ();
    if (page_boundary)
      ASSERT (MEMCHR (page_boundary, 'a', 0) == NULL);
  }

  ASSERT (MEMCHR (input, 'b', n) == input + 1);
  ASSERT (MEMCHR (input, 'c', n) == input + 2);
  ASSERT (MEMCHR (input, 'd', n) == input + 1026);

  ASSERT (MEMCHR (input + 1, 'a', n - 1) == input + n - 1);
  ASSERT (MEMCHR (input + 1, 'e', n - 1) == input + n - 2);
  ASSERT (MEMCHR (input + 1, 0x789abc00 | 'e', n - 1) == input + n - 2);

  ASSERT (MEMCHR (input, 'f', n) == NULL);
  ASSERT (MEMCHR (input, '\0', n) == NULL);

   
  {
    size_t repeat = 10000;
    for (; repeat > 0; repeat--)
      {
        ASSERT (MEMCHR (input, 'c', n) == input + 2);
      }
  }

   
  {
    int i, j;
    for (i = 0; i < 32; i++)
      {
        for (j = 0; j < 256; j++)
          input[i + j] = j;
        for (j = 0; j < 256; j++)
          {
            ASSERT (MEMCHR (input + i, j, 256) == input + i + j);
          }
      }
  }

   
  {
    char *page_boundary = (char *) zerosize_ptr ();
     
    int limit = 257;

    if (page_boundary != NULL)
      {
        for (n = 1; n <= limit; n++)
          {
            char *mem = page_boundary - n;
            memset (mem, 'X', n);
            ASSERT (MEMCHR (mem, 'U', n) == NULL);
            ASSERT (MEMCHR (mem, 0, n) == NULL);

            {
              size_t i;
              size_t k;

              for (i = 0; i < n; i++)
                {
                  mem[i] = 'U';
                  for (k = i + 1; k < n + limit; k++)
                    ASSERT (MEMCHR (mem, 'U', k) == mem + i);
                  mem[i] = 0;
                  for (k = i + 1; k < n + limit; k++)
                    ASSERT (MEMCHR (mem, 0, k) == mem + i);
                  mem[i] = 'X';
                }
            }
          }
      }
  }

  free (input);

  return 0;
}
