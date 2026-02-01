 
extern int target_directory_operand (char const *file, struct stat *st);

 
TARGETDIR_INLINE _GL_ATTRIBUTE_PURE bool
target_dirfd_valid (int fd)
{
  return fd != -1 - (AT_FDCWD == -1);
}

_GL_INLINE_HEADER_END
