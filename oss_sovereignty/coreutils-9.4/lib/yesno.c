 

bool
yesno (void)
{
  bool yes;

#if ENABLE_NLS
  char *response = NULL;
  size_t response_size = 0;
  ssize_t response_len = getline (&response, &response_size, stdin);

  if (response_len <= 0)
    yes = false;
  else
    {
       
      if (response[response_len - 1] == '\n')
        response[response_len - 1] = '\0';
      yes = (0 < rpmatch (response));
    }

  free (response);
#else
   
  int c = getchar ();
  yes = (c == 'y' || c == 'Y');
  while (c != '\n' && c != EOF)
    c = getchar ();
#endif

  return yes;
}
