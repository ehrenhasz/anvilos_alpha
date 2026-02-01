 

 

char *
memset (char *str, int c, unsigned int len)
{
  register char *st = str;

  while (len-- > 0)
    *st++ = c;
  return str;
}
