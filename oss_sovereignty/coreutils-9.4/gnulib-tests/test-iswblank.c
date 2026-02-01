 
wint_t e = WEOF;

int
main (void)
{
   
  (void) iswblank (0);
   
  ASSERT (!iswblank (e));

   
  ASSERT (iswblank (L' '));
  ASSERT (iswblank (L'\t'));
  ASSERT (!iswblank (L'\n'));

  return 0;
}
