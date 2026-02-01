 
  {
    const char input[] = "";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " ";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " -";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }

   
  {
    const char input[] = "0";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+0";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "23";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = " 23";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+23";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-23";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == -23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "2147483647";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == 2147483647);
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-2147483648";
    char *ptr;
    intmax_t result;
    errno = 0;
    result = strtoimax (input, &ptr, 10);
    ASSERT (result == -2147483647 - 1);
    ASSERT (ptr == input + 11);
    ASSERT (errno == 0);
  }
  if (sizeof (intmax_t) > sizeof (int))
    {
      const char input[] = "4294967295";
      char *ptr;
      intmax_t result;
      errno = 0;
      result = strtoimax (input, &ptr, 10);
      ASSERT (result == (intmax_t) 65535 * (intmax_t) 65537);
      ASSERT (ptr == input + 10);
      ASSERT (errno == 0);
    }

  return 0;
}
