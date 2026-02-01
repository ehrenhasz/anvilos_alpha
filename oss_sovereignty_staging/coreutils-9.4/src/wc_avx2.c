 
#define BUFSIZE (16320)

extern bool
wc_lines_avx2 (char const *file, int fd, uintmax_t *lines_out,
               uintmax_t *bytes_out);

extern bool
wc_lines_avx2 (char const *file, int fd, uintmax_t *lines_out,
               uintmax_t *bytes_out)
{
  __m256i accumulator;
  __m256i accumulator2;
  __m256i zeroes;
  __m256i endlines;
  __m256i avx_buf[BUFSIZE / sizeof (__m256i)];
  __m256i *datap;
  uintmax_t lines = 0;
  uintmax_t bytes = 0;
  size_t bytes_read = 0;


  if (!lines_out || !bytes_out)
    return false;

   
  accumulator = _mm256_setzero_si256 ();
  accumulator2 = _mm256_setzero_si256 ();
  zeroes = _mm256_setzero_si256 ();
  endlines = _mm256_set1_epi8 ('\n');

  while ((bytes_read = safe_read (fd, avx_buf, sizeof (avx_buf))) > 0)
    {
      __m256i to_match;
      __m256i to_match2;
      __m256i matches;
      __m256i matches2;

      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, "%s", quotef (file));
          return false;
        }

      bytes += bytes_read;

      datap = avx_buf;
      char *end = ((char *)avx_buf) + bytes_read;

      while (bytes_read >= 64)
        {
          to_match = _mm256_load_si256 (datap);
          to_match2 = _mm256_load_si256 (datap + 1);

          matches = _mm256_cmpeq_epi8 (to_match, endlines);
          matches2 = _mm256_cmpeq_epi8 (to_match2, endlines);
           
          accumulator = _mm256_sub_epi8 (accumulator, matches);
          accumulator2 = _mm256_sub_epi8 (accumulator2, matches2);

          datap += 2;
          bytes_read -= 64;
        }

       
      accumulator = _mm256_sad_epu8 (accumulator, zeroes);
      lines +=   _mm256_extract_epi16 (accumulator, 0)
               + _mm256_extract_epi16 (accumulator, 4)
               + _mm256_extract_epi16 (accumulator, 8)
               + _mm256_extract_epi16 (accumulator, 12);
      accumulator = _mm256_setzero_si256 ();

      accumulator2 = _mm256_sad_epu8 (accumulator2, zeroes);
      lines +=   _mm256_extract_epi16 (accumulator2, 0)
               + _mm256_extract_epi16 (accumulator2, 4)
               + _mm256_extract_epi16 (accumulator2, 8)
               + _mm256_extract_epi16 (accumulator2, 12);
      accumulator2 = _mm256_setzero_si256 ();

       
      char *p = (char *)datap;
      while (p != end)
        lines += *p++ == '\n';
    }

  *lines_out = lines;
  *bytes_out = bytes;

  return true;
}
