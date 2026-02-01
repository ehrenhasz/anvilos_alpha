 
  struct timeval real_start;
  struct timeval user_start;
  struct timeval sys_start;
   
  long real_usec;
  long user_usec;
  long sys_usec;
};

static void
timing_start (struct timings_state *ts)
{
  struct rusage usage;

  getrusage (RUSAGE_SELF, &usage);
  ts->user_start = usage.ru_utime;
  ts->sys_start = usage.ru_stime;

  gettimeofday (&ts->real_start, NULL);
}

static void
timing_end (struct timings_state *ts)
{
  struct timeval real_end;
  struct rusage usage;

  gettimeofday (&real_end, NULL);

  getrusage (RUSAGE_SELF, &usage);

  ts->real_usec = (real_end.tv_sec - ts->real_start.tv_sec) * 1000000
                  + real_end.tv_usec - ts->real_start.tv_usec;
  ts->user_usec = (usage.ru_utime.tv_sec - ts->user_start.tv_sec) * 1000000
                  + usage.ru_utime.tv_usec - ts->user_start.tv_usec;
  ts->sys_usec = (usage.ru_stime.tv_sec - ts->sys_start.tv_sec) * 1000000
                 + usage.ru_stime.tv_usec - ts->sys_start.tv_usec;
}

static void
timing_output (const struct timings_state *ts)
{
  printf ("real %10.6f\n", (double)ts->real_usec / 1000000.0);
  printf ("user %7.3f\n", (double)ts->user_usec / 1000000.0);
  printf ("sys  %7.3f\n", (double)ts->sys_usec / 1000000.0);
}
