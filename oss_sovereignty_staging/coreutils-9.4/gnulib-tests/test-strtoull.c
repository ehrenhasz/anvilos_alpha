 
  {
    const char input[] = "";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " ";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " -";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }

   
  {
    const char input[] = "0";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+0";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "23";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = " 23";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+23";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 23);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-23";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == - 23ULL);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "2147483647";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 2147483647);
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-2147483648";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == - 2147483648ULL);
    ASSERT (ptr == input + 11);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "4294967295";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 4294967295U);
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "0x2A";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x2A";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 16);
    ASSERT (result == 42ULL);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x2A";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 0);
    ASSERT (result == 42ULL);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 16);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 0);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "0b111010";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b111010";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 2);
    ASSERT (result == 58ULL);
    ASSERT (ptr == input + 8);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b111010";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 0);
    ASSERT (result == 58ULL);
    ASSERT (ptr == input + 8);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 10);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 2);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0b";
    char *ptr;
    unsigned long long result;
    errno = 0;
    result = strtoull (input, &ptr, 0);
    ASSERT (result == 0ULL);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }

  return 0;
}
