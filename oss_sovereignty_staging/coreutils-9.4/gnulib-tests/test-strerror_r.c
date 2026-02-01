 

  errno = 0;
  buf[0] = '\0';
  ASSERT (strerror_r (EACCES, buf, sizeof buf) == 0);
  ASSERT (buf[0] != '\0');
  ASSERT (errno == 0);
  ASSERT (strlen (buf) < sizeof buf);

  errno = 0;
  buf[0] = '\0';
  ASSERT (strerror_r (ETIMEDOUT, buf, sizeof buf) == 0);
  ASSERT (buf[0] != '\0');
  ASSERT (errno == 0);
  ASSERT (strlen (buf) < sizeof buf);

  errno = 0;
  buf[0] = '\0';
  ASSERT (strerror_r (EOVERFLOW, buf, sizeof buf) == 0);
  ASSERT (buf[0] != '\0');
  ASSERT (errno == 0);
  ASSERT (strlen (buf) < sizeof buf);

   
  errno = 0;
  buf[0] = '\0';
  ret = strerror_r (0, buf, sizeof buf);
  ASSERT (ret == 0);
  ASSERT (buf[0]);
  ASSERT (errno == 0);
  ASSERT (strstr (buf, "nknown") == NULL);
  ASSERT (strstr (buf, "ndefined") == NULL);

   
  errno = 0;
  buf[0] = '^';
  ret = strerror_r (-3, buf, sizeof buf);
  ASSERT (ret == 0 || ret == EINVAL);
  ASSERT (buf[0] != '^');
  ASSERT (*buf);
  ASSERT (errno == 0);
  ASSERT (strlen (buf) < sizeof buf);

   
  {
    int errs[] = { EACCES, 0, -3, };
    int j;

    buf[sizeof buf - 1] = '\0';
    for (j = 0; j < SIZEOF (errs); j++)
      {
        int err = errs[j];
        char buf2[sizeof buf] = "";
        size_t len;
        size_t i;

        strerror_r (err, buf2, sizeof buf2);
        len = strlen (buf2);
        ASSERT (len < sizeof buf);

        for (i = 0; i <= len; i++)
          {
            memset (buf, '^', sizeof buf - 1);
            errno = 0;
            ret = strerror_r (err, buf, i);
            ASSERT (errno == 0);
            if (j == 2)
              ASSERT (ret == ERANGE || ret == EINVAL);
            else
              ASSERT (ret == ERANGE);
            if (i)
              {
                ASSERT (strncmp (buf, buf2, i - 1) == 0);
                ASSERT (buf[i - 1] == '\0');
              }
            ASSERT (strspn (buf + i, "^") == sizeof buf - 1 - i);
          }

        strcpy (buf, "BADFACE");
        errno = 0;
        ret = strerror_r (err, buf, len + 1);
        ASSERT (ret != ERANGE);
        ASSERT (errno == 0);
        ASSERT (strcmp (buf, buf2) == 0);
      }
  }

#if GNULIB_STRERROR
   
  {
    const char *msg1;
    const char *msg2;
    const char *msg3;
    const char *msg4;
    char *str1;
    char *str2;
    char *str3;
    char *str4;

    msg1 = strerror (ENOENT);
    ASSERT (msg1);
    str1 = strdup (msg1);
    ASSERT (str1);

    msg2 = strerror (ERANGE);
    ASSERT (msg2);
    str2 = strdup (msg2);
    ASSERT (str2);

    msg3 = strerror (-4);
    ASSERT (msg3);
    str3 = strdup (msg3);
    ASSERT (str3);

    msg4 = strerror (1729576);
    ASSERT (msg4);
    str4 = strdup (msg4);
    ASSERT (str4);

    strerror_r (EACCES, buf, sizeof buf);
    strerror_r (-5, buf, sizeof buf);
    ASSERT (STREQ (msg4, str4));

    free (str1);
    free (str2);
    free (str3);
    free (str4);
  }
#endif

  return 0;
}
