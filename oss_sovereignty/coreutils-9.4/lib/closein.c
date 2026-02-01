 
void
close_stdin_set_file_name (const char *file)
{
  file_name = file;
}

 

void
close_stdin (void)
{
  bool fail = false;

   
  if (freadahead (stdin) > 0)
    {
       
      if (fseeko (stdin, 0, SEEK_CUR) == 0 && fflush (stdin) != 0)
        fail = true;
    }
  if (close_stream (stdin) != 0)
    fail = true;
  if (fail)
    {
       
      char const *close_error = _("error closing file");
      if (file_name)
        error (0, errno, "%s: %s", quotearg_colon (file_name),
               close_error);
      else
        error (0, errno, "%s", close_error);
    }

  close_stdout ();

  if (fail)
    _exit (exit_failure);
}
