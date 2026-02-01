 
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  {
    const char input[] = "1,";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ",5";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result == 0.5L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1,5";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result == 1.5L);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.5";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
     
    ASSERT ((ptr == input + 1 && result == 1.0L)
            || (ptr == input + 3 && result == 1.5L));
    ASSERT (errno == 0);
  }
  {
    const char input[] = "123.456,789";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
     
    ASSERT ((ptr == input + 3 && result == 123.0L)
            || (ptr == input + 7 && result > 123.45L && result < 123.46L));
    ASSERT (errno == 0);
  }
  {
    const char input[] = "123,456.789";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result > 123.45L && result < 123.46L);
    ASSERT (ptr == input + 7);
    ASSERT (errno == 0);
  }

  return 0;
}
