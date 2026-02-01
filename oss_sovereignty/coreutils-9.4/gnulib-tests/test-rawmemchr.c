 
#define RAWMEMCHR (char *) rawmemchr

int
main (void)
{
  size_t n = 0x100000;
  char *input = malloc (n + 1);
  ASSERT (input);

  input[0] = 'a';
  input[1] = 'b';
  memset (input + 2, 'c', 1024);
  memset (input + 1026, 'd', n - 1028);
  input[n - 2] = 'e';
  input[n - 1] = 'a';
  input[n] = '\0';

   
  ASSERT (RAWMEMCHR (input, 'a') == input);
  ASSERT (RAWMEMCHR (input, 'b') == input + 1);
  ASSERT (RAWMEMCHR (input, 'c') == input + 2);
  ASSERT (RAWMEMCHR (input, 'd') == input + 1026);

  ASSERT (RAWMEMCHR (input + 1, 'a') == input + n - 1);
  ASSERT (RAWMEMCHR (input + 1, 'e') == input + n - 2);
  ASSERT (RAWMEMCHR (input + 1, 0x789abc00 | 'e') == input + n - 2);

  ASSERT (RAWMEMCHR (input, '\0') == input + n);

   
  {
    int i, j;
    for (i = 0; i < 32; i++)
      {
        for (j = 0; j < 256; j++)
          input[i + j] = j;
        for (j = 0; j < 256; j++)
          {
            ASSERT (RAWMEMCHR (input + i, j) == input + i + j);
          }
      }
  }

   
  {
    char *page_boundary = (char *) zerosize_ptr ();
    size_t i;

    if (!page_boundary)
      page_boundary = input + 4096;
    memset (page_boundary - 512, '1', 511);
    page_boundary[-1] = '2';
    for (i = 1; i <= 512; i++)
      ASSERT (RAWMEMCHR (page_boundary - i, (i * 0x01010100) | '2')
              == page_boundary - 1);
  }

  free (input);

  return 0;
}
