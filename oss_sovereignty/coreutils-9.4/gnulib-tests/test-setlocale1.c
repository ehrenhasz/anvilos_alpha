 
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  name1 = strdup (setlocale (LC_ALL, NULL));

   
  if (setlocale (LC_ALL, "C") == NULL)
    return 1;

   
  if (setlocale (LC_ALL, getenv ("LC_ALL")) == NULL)
    return 1;

  name2 = strdup (setlocale (LC_ALL, NULL));

  ASSERT (name1);
  ASSERT (name2);

   
  ASSERT (strcmp (name1, name2) == 0);
  free (name1);
  free (name2);

  return 0;
}
