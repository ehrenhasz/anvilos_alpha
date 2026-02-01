 
  if (setlocale (LC_ALL, "") != NULL)
     
    if (strcmp (setlocale (LC_CTYPE, NULL), "C") == 0)
      {
        fprintf (stderr, "setlocale did not fail for implicit %s\n",
                 getenv ("LC_ALL"));
        return 1;
      }

   
  if (setlocale (LC_ALL, "C") == NULL)
    return 1;

   
  if (setlocale (LC_ALL, getenv ("LC_ALL")) != NULL)
     
    if (strcmp (setlocale (LC_CTYPE, NULL), "C") == 0)
      {
        fprintf (stderr, "setlocale did not fail for explicit %s\n",
                 getenv ("LC_ALL"));
        return 1;
      }

  return 0;
}
