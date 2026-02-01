 
  #if defined __ANDROID__  
    #define COUNT 1
  #else
    #define COUNT 1000
  #endif

  int i;
  for (i = 0; i < COUNT; i++)
    {
      errno = 0;
      if (! fdopen (STDOUT_FILENO, "w"))
        {
          ASSERT (errno != 0);
          break;
        }
    }

  return 0;
}
