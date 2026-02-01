 
  if (strncmp (p, "lt-", 3) == 0)
    p += 3;

   
#if defined __CYGWIN__
   
  assert (STREQ (p, "test-getprogname"));
#else
  assert (STREQ (p, "test-getprogname" EXEEXT));
#endif

  return 0;
}
