 
  MBA_UNIBYTE_FALLBACK = 0x0001,

   
  MBA_UNIBYTE_ONLY = 0x0002,

   
  MBA_NO_LEFT_PAD = 0x0004,

   
  MBA_NO_RIGHT_PAD = 0x0008

#if 0  
   
  MBA_IGNORE_INVALID

   
  MBA_USE_FIGURE_SPACE

   
  MBA_NO_TRUNCATE

   
  MBA_LSTRIP

   
  MBA_RSTRIP
#endif
};

size_t
mbsalign (char const *src, char *dest, size_t dest_size,
          size_t *width, mbs_align_t align, int flags)
  _GL_ATTRIBUTE_NONNULL ();

char *
ambsalign (char const *src, size_t *width, mbs_align_t align, int flags)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_NONNULL ();
