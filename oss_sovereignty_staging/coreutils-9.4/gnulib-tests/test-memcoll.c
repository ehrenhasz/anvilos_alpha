 
  ASSERT (memcoll0 ("", 1, "", 1) == 0);
  ASSERT (memcoll0 ("fo", 3, "fo", 3) == 0);
  ASSERT (memcoll0 ("foo", 4, "foo", 4) == 0);
  ASSERT (memcoll0 ("foo\0", 5, "foob", 5) != 0);
  ASSERT (memcoll0 ("f", 2, "b", 2) != 0);
  ASSERT (memcoll0 ("foo", 4, "bar", 4) != 0);

   
  ASSERT (memcoll0 ("foo\0", 5, "moo\0", 5) < 0);
  ASSERT (memcoll0 ("moo\0", 5, "foo\0", 5) > 0);
  ASSERT (memcoll0 ("oom", 4, "oop", 4) < 0);
  ASSERT (memcoll0 ("oop", 4, "oom", 4) > 0);
  ASSERT (memcoll0 ("foo\0", 5, "foob", 5) < 0);
  ASSERT (memcoll0 ("foob", 5, "foo\0", 5) > 0);

   
  ASSERT (memcoll0 ("1\0", 3, "2\0", 3) < 0);
  ASSERT (memcoll0 ("2\0", 3, "1\0", 3) > 0);
  ASSERT (memcoll0 ("x\0""1", 4, "x\0""2", 4) < 0);
  ASSERT (memcoll0 ("x\0""2", 4, "x\0""1", 4) > 0);

  return 0;
}
