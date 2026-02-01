 
void
explicit_bzero (void *s, size_t len)
{
  memset_explicit (s, 0, len);
}
