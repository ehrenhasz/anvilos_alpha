 
  {
    const char input[] = "";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " ";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " -";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }

   
  {
    const char input[] = "0";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+0";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "23";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = " 23";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+23";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-23";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == -23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "2147483647";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 2147483647);
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-2147483648";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == -2147483647 - 1);
    ASSERT (ptr == input + 11);
    ASSERT (errno == 0);
  }
  if (sizeof (long long) > sizeof (int))
    {
      const char input[] = "4294967295";
      char *ptr;
      long long result;
      errno = 0;
      result = strtoll (input, &ptr, 10);
      ASSERT (result == 65535LL * 65537LL);
      ASSERT (ptr == input + 10);
      ASSERT (errno == 0);
    }

   
  {
    const char input[] = "0x2A";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x2A";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 16);
    ASSERT (result == 42LL);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x2A";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 0);
    ASSERT (result == 42LL);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 16);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 0);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "0b111010";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b111010";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 2);
    ASSERT (result == 58LL);
    ASSERT (ptr == input + 8);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b111010";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 0);
    ASSERT (result == 58LL);
    ASSERT (ptr == input + 8);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 10);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 2);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b";
    char *ptr;
    long long result;
    errno = 0;
    result = strtoll (input, &ptr, 0);
    ASSERT (result == 0LL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }

  return 0;
}
