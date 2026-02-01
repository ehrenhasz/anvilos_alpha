 
  set_binary_mode (0, O_BINARY);

  if (argc > 1)
    switch (argv[1][0])
      {
      case '1':
         

        c = fgetc (stdin);
        ASSERT (c == '#');

        c = fgetc (stdin);
        ASSERT (c == '!');

         

        c = ungetc ('!', stdin);
        ASSERT (c == '!');

        fflush (stdin);

         

        c = fgetc (stdin);
        ASSERT (c == '!');

        c = fgetc (stdin);
        ASSERT (c == '/');

        return 0;

      case '2':
         
         

        c = fgetc (stdin);
        ASSERT (c == '#');

        c = fgetc (stdin);
        ASSERT (c == '!');

         

        c = ungetc ('@', stdin);
        ASSERT (c == '@');

        fflush (stdin);

         

        c = fgetc (stdin);
        ASSERT (c == '!');

        c = fgetc (stdin);
        ASSERT (c == '/');

        return 0;
      }

  return 1;
}
