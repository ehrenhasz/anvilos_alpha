 
static _Noreturn void
handler (int sig)
{
  _exit (0);
}

int
main (void)
{
   
  ASSERT (raise (-1) != 0);

   
  ASSERT (signal (SIGINT, handler) != SIG_ERR);

  raise (SIGINT);

   
  exit (1);
}
