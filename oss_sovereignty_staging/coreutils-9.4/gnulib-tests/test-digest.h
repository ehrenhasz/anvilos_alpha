 
            break;
          case 2:
             
            fputs ("ABCD", fp);
            FALLTHROUGH;
          case 1:
             
            fputs ("The quick brown fox jumps over the lazy dog.\n", fp);
            break;
          case 4:
             
            fputs ("ABCD", fp);
            FALLTHROUGH;
          case 3:
             
            {
              unsigned int i;
              for (i = 0; i < 0x400000; i++)
                {
                  unsigned char c[2];
                  unsigned int j = i * (i-1) * (i-5);
                  c[0] = (unsigned char)(j >> 6);
                  c[1] = (i % 499) + (i % 101);
                  fwrite (c, 1, 2, fp);
                }
            }
            break;
          }
        if (ferror (fp))
          {
            fprintf (stderr, "Could not write data to file %s.\n", TESTFILE);
            exit (1);
          }
        fclose (fp);
      }
      {
         
        char *digest = (char *) malloc (digest_size + 1) + 1;
        const void *expected;
        FILE *fp;
        int ret;

        switch (pass)
          {
          case 0:         expected = expected_for_empty_file; break;
          case 1: case 2: expected = expected_for_small_file; break;
          case 3: case 4: expected = expected_for_large_file; break;
          default: abort ();
          }

        fp = fopen (TESTFILE, "rb");
        if (fp == NULL)
          {
            fprintf (stderr, "Could not open file %s.\n", TESTFILE);
            exit (1);
          }
        switch (pass)
          {
          case 2:
          case 4:
            {
              char header[4];
              if (fread (header, 1, sizeof (header), fp) != sizeof (header))
                {
                  fprintf (stderr, "Could not read the header of %s.\n",
                           TESTFILE);
                  exit (1);
                }
            }
            break;
          }
        ret = streamfunc (fp, digest);
        if (ret)
          {
            fprintf (stderr, "%s failed with error %d\n", streamfunc_name, -ret);
            exit (1);
          }
        if (memcmp (digest, expected, digest_size) != 0)
          {
            size_t i;
            fprintf (stderr, "%s produced wrong result.\n", streamfunc_name);
            fprintf (stderr, "Expected: ");
            for (i = 0; i < digest_size; i++)
              fprintf (stderr, "\\x%02x", ((const unsigned char *) expected)[i]);
            fprintf (stderr, "\n");
            fprintf (stderr, "Got:      ");
            for (i = 0; i < digest_size; i++)
              fprintf (stderr, "\\x%02x", ((const unsigned char *) digest)[i]);
            fprintf (stderr, "\n");
            exit (1);
          }
         
        if (getc (fp) != EOF)
          {
            fprintf (stderr, "%s left the stream not at EOF\n", streamfunc_name);
            exit (1);
          }
        fclose (fp);
        free (digest - 1);
      }
    }

  unlink (TESTFILE);
}
