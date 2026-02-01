 
#define MEMCHR2 (char *) memchr2

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

   
  ASSERT (MEMCHR2 (input, 'a', 'b', n) == input);
  ASSERT (MEMCHR2 (input, 'b', 'a', n) == input);

  ASSERT (MEMCHR2 (input, 'a', 'b', 0) == NULL);
  void *page_boundary = zerosize_ptr ();
  if (page_boundary)
    ASSERT (MEMCHR2 (page_boundary, 'a', 'b', 0) == NULL);

  ASSERT (MEMCHR2 (input, 'b', 'd', n) == input + 1);
  ASSERT (MEMCHR2 (input + 2, 'b', 'd', n - 2) == input + 1026);

  ASSERT (MEMCHR2 (input, 'd', 'e', n) == input + 1026);
  ASSERT (MEMCHR2 (input, 'e', 'd', n) == input + 1026);

  ASSERT (MEMCHR2 (input + 1, 'a', 'e', n - 1) == input + n - 2);
  ASSERT (MEMCHR2 (input + 1, 'e', 'a', n - 1) == input + n - 2);

  ASSERT (MEMCHR2 (input, 'f', 'g', n) == NULL);
  ASSERT (MEMCHR2 (input, 'f', '\0', n) == NULL);

  ASSERT (MEMCHR2 (input, 'a', 'a', n) == input);
  ASSERT (MEMCHR2 (input + 1, 'a', 'a', n - 1) == input + n - 1);
  ASSERT (MEMCHR2 (input, 'f', 'f', n) == NULL);

   
  {
    size_t repeat = 10000;
    for (; repeat > 0; repeat--)
      {
        ASSERT (MEMCHR2 (input, 'c', 'e', n) == input + 2);
        ASSERT (MEMCHR2 (input, 'e', 'c', n) == input + 2);
        ASSERT (MEMCHR2 (input, 'c', '\0', n) == input + 2);
        ASSERT (MEMCHR2 (input, '\0', 'c', n) == input + 2);
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
            ASSERT (MEMCHR2 (input + i, j, 0xff, 256) == input + i + j);
            ASSERT (MEMCHR2 (input + i, 0xff, j, 256) == input + i + j);
          }
      }
  }

  free (input);

  return 0;
}
