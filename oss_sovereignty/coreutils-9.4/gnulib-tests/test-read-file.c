 
  if (stat (FILE1, &statbuf) >= 0)
    {
      size_t len;
      char *out = read_file (FILE1, flags, &len);

      if (!out)
        {
          perror ("Could not read file");
          err = 1;
        }
      else
        {
          if (out[len] != '\0')
            {
              perror ("BAD: out[len] not zero");
              err = 1;
            }

          if (S_ISREG (statbuf.st_mode))
            {
               
              if (len != statbuf.st_size)
                {
                  fprintf (stderr, "Read %lu from %s...\n",
                           (unsigned long) len, FILE1);
                  err = 1;
                }
            }
          else
            {
               
              if (len == 0)
                {
                  fprintf (stderr, "Read nothing from %s\n", FILE1);
                  err = 1;
                }
            }
          free (out);
        }
    }

   
  if (stat (FILE2, &statbuf) >= 0)
    {
      size_t len;
      char *out = read_file (FILE2, flags, &len);

      if (!out)
        {
          perror ("Could not read file");
          err = 1;
        }
      else
        {
          if (out[len] != '\0')
            {
              perror ("BAD: out[len] not zero");
              err = 1;
            }

           
          if (len != 0)
            {
              fprintf (stderr, "Read %lu from %s...\n",
                       (unsigned long) len, FILE2);
              err = 1;
            }
          free (out);
        }
    }

  return err;
}

int
main (void)
{
  ASSERT (!test_read_file (0));
  ASSERT (!test_read_file (RF_BINARY));
  ASSERT (!test_read_file (RF_SENSITIVE));
  ASSERT (!test_read_file (RF_BINARY | RF_SENSITIVE));

  return 0;
}
