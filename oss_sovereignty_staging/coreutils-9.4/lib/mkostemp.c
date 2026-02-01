 
int
mkostemp (char *xtemplate, int flags)
{
  return __gen_tempname (xtemplate, 0, flags, __GT_FILE);
}
