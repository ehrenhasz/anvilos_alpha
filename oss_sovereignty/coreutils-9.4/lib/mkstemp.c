 
int
mkstemp (char *xtemplate)
{
  return __gen_tempname (xtemplate, 0, 0, __GT_FILE);
}
