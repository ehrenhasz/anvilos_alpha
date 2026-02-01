 

static int
test_sys_wait_macros (void)
{
   
  int i;
  for (i = 0; i < 0x8000; i = (i ? i << 1 : 1))
    {
       
      if (i == 0x80)
        continue;
      if (!!WIFSIGNALED (i) + !!WIFEXITED (i) + !!WIFSTOPPED (i) != 1)
        return 1;
    }
  i = WEXITSTATUS (i) + WSTOPSIG (i) + WTERMSIG (i);

#if 0
  switch (i)
    {
   
    case WNOHANG:
    case WUNTRACED:
      break;
    }
#endif
  return 0;
}
