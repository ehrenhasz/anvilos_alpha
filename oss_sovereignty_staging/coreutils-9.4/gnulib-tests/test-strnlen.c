 
  ASSERT (strnlen ("a", 0) == 0);
  ASSERT (strnlen ("a", 1) == 1);
  ASSERT (strnlen ("a", 2) == 1);
  ASSERT (strnlen ("", 0x100000) == 0);

   
  for (i = 0; i < 512; i++)
    {
      char *start = page_boundary - i;
      size_t j = i;
      memset (start, 'x', i);
      do
        {
          if (i != j)
            {
              start[j] = 0;
              ASSERT (strnlen (start, i + j) == j);
            }
          ASSERT (strnlen (start, i) == j);
          ASSERT (strnlen (start, j) == j);
        }
      while (j--);
    }

  return 0;
}
