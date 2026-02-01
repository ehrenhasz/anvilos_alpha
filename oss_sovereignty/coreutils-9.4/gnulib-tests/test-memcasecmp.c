 
  void *page_boundary1 = zerosize_ptr ();
  void *page_boundary2 = zerosize_ptr ();
  if (page_boundary1 && page_boundary2)
    ASSERT (memcasecmp (page_boundary1, page_boundary2, 0) == 0);
  ASSERT (memcasecmp ("foo", "foobar", 2) == 0);
  ASSERT (memcasecmp ("foo", "foobar", 3) == 0);
  ASSERT (memcasecmp ("foo", "foobar", 4) != 0);
  ASSERT (memcasecmp ("foo", "bar", 1) != 0);
  ASSERT (memcasecmp ("foo", "bar", 3) != 0);

   
  ASSERT (memcasecmp ("foo", "moo", 4) < 0);
  ASSERT (memcasecmp ("moo", "foo", 4) > 0);
  ASSERT (memcasecmp ("oomph", "oops", 3) < 0);
  ASSERT (memcasecmp ("oops", "oomph", 3) > 0);
  ASSERT (memcasecmp ("foo", "foobar", 4) < 0);
  ASSERT (memcasecmp ("foobar", "foo", 4) > 0);

   
  ASSERT (memcasecmp ("1\0", "2\0", 2) < 0);
  ASSERT (memcasecmp ("2\0", "1\0", 2) > 0);
  ASSERT (memcasecmp ("x\0""1", "x\0""2", 3) < 0);
  ASSERT (memcasecmp ("x\0""2", "x\0""1", 3) > 0);

   
  {
    char foo[21];
    char bar[21];
    int i;
    for (i = 0; i < 4; i++)
      {
        char *a = foo + i;
        char *b = bar + i;
        strcpy (a, "--------01111111");
        strcpy (b, "--------10000000");
        ASSERT (memcasecmp (a, b, 16) < 0);
      }
  }

  return 0;
}
