 
#if 4 < __GNUC__ + (7 <= __GNUC_MINOR__) && __GNUC__ < 12
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

 
#define MEMRCHR (char *) memrchr

int
main (void)
{
  size_t n = 0x100000;
  char *input = malloc (n);
  ASSERT (input);

  input[n - 1] = 'a';
  input[n - 2] = 'b';
  memset (input + n - 1026, 'c', 1024);
  memset (input + 2, 'd', n - 1028);
  input[1] = 'e';
  input[0] = 'a';

   
  ASSERT (MEMRCHR (input, 'a', n) == input + n - 1);

  ASSERT (MEMRCHR (input, 'a', 0) == NULL);
  void *page_boundary = zerosize_ptr ();
  if (page_boundary)
    ASSERT (MEMRCHR (page_boundary, 'a', 0) == NULL);

  ASSERT (MEMRCHR (input, 'b', n) == input + n - 2);
  ASSERT (MEMRCHR (input, 'c', n) == input + n - 3);
  ASSERT (MEMRCHR (input, 'd', n) == input + n - 1027);

  ASSERT (MEMRCHR (input, 'a', n - 1) == input);
  ASSERT (MEMRCHR (input, 'e', n - 1) == input + 1);

  ASSERT (MEMRCHR (input, 'f', n) == NULL);
  ASSERT (MEMRCHR (input, '\0', n) == NULL);

   
  {
    size_t repeat = 10000;
    for (; repeat > 0; repeat--)
      {
        ASSERT (MEMRCHR (input, 'c', n) == input + n - 3);
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
            ASSERT (MEMRCHR (input + i, j, 256) == input + i + j);
          }
      }
  }

  free (input);

  return 0;
}
