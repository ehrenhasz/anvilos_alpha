 

 
int
xprintf (char const *restrict format, ...)
{
  va_list args;
  int retval;
  va_start (args, format);
  retval = xvprintf (format, args);
  va_end (args);

  return retval;
}

 
int
xvprintf (char const *restrict format, va_list args)
{
  int retval = vprintf (format, args);
  if (retval < 0 && ! ferror (stdout))
    error (exit_failure, errno, gettext ("cannot perform formatted output"));

  return retval;
}

 
int
xfprintf (FILE *restrict stream, char const *restrict format, ...)
{
  va_list args;
  int retval;
  va_start (args, format);
  retval = xvfprintf (stream, format, args);
  va_end (args);

  return retval;
}

 
int
xvfprintf (FILE *restrict stream, char const *restrict format, va_list args)
{
  int retval = vfprintf (stream, format, args);
  if (retval < 0 && ! ferror (stream))
    error (exit_failure, errno, gettext ("cannot perform formatted output"));

  return retval;
}
