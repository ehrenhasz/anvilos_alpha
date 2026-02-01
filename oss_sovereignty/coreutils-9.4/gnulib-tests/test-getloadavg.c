 
      if (avg < 0.01)
        printf ("suspiciously low %d-minute average: %f\n", minutes, avg);
      if (avg > 1000000)
        printf ("suspiciously high %d-minute average: %f\n", minutes, avg);
    }
  if (avg < 0 || avg != avg)
    exit (minutes);
}

 
int
main (int argc, char **argv)
{
  int naptime = 0;

  if (argc > 1)
    naptime = atoi (argv[1]);

  while (1)
    {
      double avg[3];
      int loads = getloadavg (avg, 3);
      if (loads == -1)
        {
          if (! (errno == ENOSYS || errno == ENOTSUP || errno == ENOENT))
            return 1;
          perror ("Skipping test; load average not supported");
          return 77;
        }
      if (loads > 0)
        check_avg (1, avg[0], argc > 1);
      if (loads > 1)
        check_avg (5, avg[1], argc > 1);
      if (loads > 2)
        check_avg (15, avg[2], argc > 1);
      if (loads > 0 && argc > 1)
        putchar ('\n');

      if (naptime == 0)
        break;
      sleep (naptime);
    }

  return 0;
}
