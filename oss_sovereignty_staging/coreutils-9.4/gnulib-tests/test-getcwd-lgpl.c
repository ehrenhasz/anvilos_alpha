 
  if (1 < argc)
    {
      if (chdir (argv[1]) == 0)
        printf ("changed to directory %s\n", argv[1]);
    }

  pwd1 = getcwd (NULL, 0);
  ASSERT (pwd1 && *pwd1);
  if (1 < argc)
    printf ("cwd=%s\n", pwd1);

   
  ASSERT (chdir (pwd1) == 0);
  ASSERT (chdir (".//./.") == 0);

   
  pwd2 = getcwd (NULL, 0);
  ASSERT (pwd2);
  ASSERT (strcmp (pwd1, pwd2) == 0);
  free (pwd2);
  {
    size_t len = strlen (pwd1);
    ssize_t i = len - 10;
    if (i < 1)
      i = 1;
    pwd2 = getcwd (NULL, len + 1);
    ASSERT (pwd2);
    free (pwd2);
    pwd2 = malloc (len + 2);
    for ( ; i <= len; i++)
      {
        char *tmp;
        errno = 0;
        ASSERT (getcwd (pwd2, i) == NULL);
        ASSERT (errno == ERANGE);
         
        errno = 0;
        tmp = getcwd (NULL, i);
        if (tmp)
          {
            ASSERT (strcmp (pwd1, tmp) == 0);
            free (tmp);
          }
        else
          {
            ASSERT (errno == ERANGE);
          }
      }
    ASSERT (getcwd (pwd2, len + 1) == pwd2);
    pwd2[len] = '/';
    pwd2[len + 1] = '\0';
  }
  ASSERT (strstr (pwd2, "/./") == NULL);
  ASSERT (strstr (pwd2, "/../") == NULL);
  ASSERT (strstr (pwd2 + 1 + (pwd2[1] == '/'), "//") == NULL);

   
  errno = 0;
  ASSERT (getcwd(pwd2, 0) == NULL);
  ASSERT (errno == EINVAL);

  free (pwd1);
  free (pwd2);

  return 0;
}
