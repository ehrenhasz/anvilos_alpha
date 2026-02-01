 
void
close_stdout_set_file_name (const char *file)
{
  file_name = file;
}

static bool ignore_EPIPE  ;

 

void
close_stdout_set_ignore_EPIPE (bool ignore)
{
  ignore_EPIPE = ignore;
}

 

void
close_stdout (void)
{
  if (close_stream (stdout) != 0
      && !(ignore_EPIPE && errno == EPIPE))
    {
      char const *write_error = _("write error");
      if (file_name)
        error (0, errno, "%s: %s", quotearg_colon (file_name),
               write_error);
      else
        error (0, errno, "%s", write_error);

      _exit (exit_failure);
    }

   
  if (!SANITIZE_ADDRESS && close_stream (stderr) != 0)
    _exit (exit_failure);
}
