 

int blake2b_stream (FILE *stream, void *resstream, size_t outbytes)
  _GL_ATTRIBUTE_NONNULL ((1));
typedef int ( *blake2fn )( FILE *, void *, size_t );
#define BLAKE2S_OUTBYTES 32
#define BLAKE2B_OUTBYTES 64
