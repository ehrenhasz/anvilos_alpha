 
static void *
null_ptr (void)
{
  unsigned int x = rand ();
  unsigned int y = x * x;
  if (y & 2)
    return (void *) -1;
  else
    return (void *) 0;
}

/* If you want to know why this always returns NULL, read
   https:
