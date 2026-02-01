 
  for (idx_t alignment = 1; alignment <= 16 * 1024 * 1024; alignment *= 2)
    for (idx_t size = 1; size <= 1024; size *= 2)
      {
        test_alignalloc (alignment, size - 1);
        test_alignalloc (alignment, size);
        test_alignalloc (alignment, size + 1);
      }

   
  alignfree (NULL);

  return 0;
}
