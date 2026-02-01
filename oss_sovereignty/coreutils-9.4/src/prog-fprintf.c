 
void
prog_fprintf (FILE *fp, char const *fmt, ...)
{
  va_list ap;
  fputs (program_name, fp);
  fputs (": ", fp);
  va_start (ap, fmt);
  vfprintf (fp, fmt, ap);
  va_end (ap);
  fputc ('\n', fp);
}
