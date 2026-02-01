 
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  {
    const char input[] = "1,";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ",5";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.5);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1,5";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.5);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.5";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
     
    ASSERT ((ptr == input + 1 && result == 1.0)
            || (ptr == input + 3 && result == 1.5));
    ASSERT (errno == 0);
  }
  {
    const char input[] = "123.456,789";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
     
    ASSERT ((ptr == input + 3 && result == 123.0)
            || (ptr == input + 7 && result > 123.45 && result < 123.46));
    ASSERT (errno == 0);
  }
  {
    const char input[] = "123,456.789";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result > 123.45 && result < 123.46);
    ASSERT (ptr == input + 7);
    ASSERT (errno == 0);
  }

  return 0;
}
