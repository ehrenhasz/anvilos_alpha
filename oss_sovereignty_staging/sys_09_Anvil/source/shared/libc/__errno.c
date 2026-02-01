


static int embed_errno;

#if defined(__linux__)
int *__errno_location(void)
#else
int *__errno(void)
#endif
{
    return &embed_errno;
}
