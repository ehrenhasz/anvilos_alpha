 

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

int
main (int argc, char *argv[])
{
  if (argc == 4)
    {
      char mode = argv[1][0];

      if (mode == 'r' || mode == 'w')
        {
          int fd = atoi (argv[2]);

          if (fd >= 0)
            {
              const char *result_file_name = argv[3];
              FILE *result_file = fopen (result_file_name, "wb");

              if (result_file != NULL)
                {
                  fd_set fds;
                  struct timeval timeout;
                  int ret;

                  FD_ZERO (&fds);
                  FD_SET (fd, &fds);
                  timeout.tv_sec = 0;
                  timeout.tv_usec = 10000;
                  ret = (mode == 'r'
                         ? select (fd + 1, &fds, NULL, NULL, &timeout)
                         : select (fd + 1, NULL, &fds, NULL, &timeout));
                  if (ret < 0)
                    {
                      perror ("select failed");
                      exit (1);
                    }
                  if ((ret == 0) != ! FD_ISSET (fd, &fds))
                    {
                      fprintf (stderr, "incorrect return value\n");
                      exit (1);
                    }
                  fprintf (result_file, "%d\n", ret);
                  exit (0);
                }
            }
        }
    }
  fprintf (stderr, "Usage: test-select-fd mode fd result-file-name\n");
  exit (1);
}
