 
#define BUFLEN (1 << 16)

extern uint_fast32_t const crctab[8][256];

extern bool
cksum_pclmul (FILE *fp, uint_fast32_t *crc_out, uintmax_t *length_out);

 

bool
cksum_pclmul (FILE *fp, uint_fast32_t *crc_out, uintmax_t *length_out)
{
  __m128i buf[BUFLEN / sizeof (__m128i)];
  uint_fast32_t crc = 0;
  uintmax_t length = 0;
  size_t bytes_read;
  __m128i single_mult_constant;
  __m128i four_mult_constant;
  __m128i shuffle_constant;

  if (!fp || !crc_out || !length_out)
    return false;

   
  single_mult_constant = _mm_set_epi64x (0xC5B9CD4C, 0xE8A45605);
  four_mult_constant = _mm_set_epi64x (0x8833794C, 0xE6228B11);

   
  shuffle_constant = _mm_set_epi8 (0, 1, 2, 3, 4, 5, 6, 7, 8,
                                   9, 10, 11, 12, 13, 14, 15);

  while ((bytes_read = fread (buf, 1, BUFLEN, fp)) > 0)
    {
      __m128i *datap;
      __m128i data;
      __m128i data2;
      __m128i data3;
      __m128i data4;
      __m128i data5;
      __m128i data6;
      __m128i data7;
      __m128i data8;
      __m128i fold_data;
      __m128i xor_crc;

      if (length + bytes_read < length)
        {
          errno = EOVERFLOW;
          return false;
        }
      length += bytes_read;

      datap = (__m128i *)buf;

       
      if (bytes_read >= 16 * 8)
        {
          data = _mm_loadu_si128 (datap);
          data = _mm_shuffle_epi8 (data, shuffle_constant);
           
          xor_crc = _mm_set_epi32 (crc, 0, 0, 0);
          crc = 0;
          data = _mm_xor_si128 (data, xor_crc);
          data3 = _mm_loadu_si128 (datap + 1);
          data3 = _mm_shuffle_epi8 (data3, shuffle_constant);
          data5 = _mm_loadu_si128 (datap + 2);
          data5 = _mm_shuffle_epi8 (data5, shuffle_constant);
          data7 = _mm_loadu_si128 (datap + 3);
          data7 = _mm_shuffle_epi8 (data7, shuffle_constant);


          while (bytes_read >= 16 * 8)
            {
              datap += 4;

               
              data2 = _mm_clmulepi64_si128 (data, four_mult_constant, 0x00);
              data = _mm_clmulepi64_si128 (data, four_mult_constant, 0x11);
              data4 = _mm_clmulepi64_si128 (data3, four_mult_constant, 0x00);
              data3 = _mm_clmulepi64_si128 (data3, four_mult_constant, 0x11);
              data6 = _mm_clmulepi64_si128 (data5, four_mult_constant, 0x00);
              data5 = _mm_clmulepi64_si128 (data5, four_mult_constant, 0x11);
              data8 = _mm_clmulepi64_si128 (data7, four_mult_constant, 0x00);
              data7 = _mm_clmulepi64_si128 (data7, four_mult_constant, 0x11);

               
              data = _mm_xor_si128 (data, data2);
              data2 = _mm_loadu_si128 (datap);
              data2 = _mm_shuffle_epi8 (data2, shuffle_constant);
              data = _mm_xor_si128 (data, data2);

              data3 = _mm_xor_si128 (data3, data4);
              data4 = _mm_loadu_si128 (datap + 1);
              data4 = _mm_shuffle_epi8 (data4, shuffle_constant);
              data3 = _mm_xor_si128 (data3, data4);

              data5 = _mm_xor_si128 (data5, data6);
              data6 = _mm_loadu_si128 (datap + 2);
              data6 = _mm_shuffle_epi8 (data6, shuffle_constant);
              data5 = _mm_xor_si128 (data5, data6);

              data7 = _mm_xor_si128 (data7, data8);
              data8 = _mm_loadu_si128 (datap + 3);
              data8 = _mm_shuffle_epi8 (data8, shuffle_constant);
              data7 = _mm_xor_si128 (data7, data8);

              bytes_read -= (16 * 4);
            }
           
          data = _mm_shuffle_epi8 (data, shuffle_constant);
          _mm_storeu_si128 (datap, data);
          data3 = _mm_shuffle_epi8 (data3, shuffle_constant);
          _mm_storeu_si128 (datap + 1, data3);
          data5 = _mm_shuffle_epi8 (data5, shuffle_constant);
          _mm_storeu_si128 (datap + 2, data5);
          data7 = _mm_shuffle_epi8 (data7, shuffle_constant);
          _mm_storeu_si128 (datap + 3, data7);
        }

       
      if (bytes_read >= 32)
        {
          data = _mm_loadu_si128 (datap);
          data = _mm_shuffle_epi8 (data, shuffle_constant);
          xor_crc = _mm_set_epi32 (crc, 0, 0, 0);
          crc = 0;
          data = _mm_xor_si128 (data, xor_crc);
          while (bytes_read >= 32)
            {
              datap++;

              data2 = _mm_clmulepi64_si128 (data, single_mult_constant, 0x00);
              data = _mm_clmulepi64_si128 (data, single_mult_constant, 0x11);
              fold_data = _mm_loadu_si128 (datap);
              fold_data = _mm_shuffle_epi8 (fold_data, shuffle_constant);
              data = _mm_xor_si128 (data, data2);
              data = _mm_xor_si128 (data, fold_data);
              bytes_read -= 16;
            }
          data = _mm_shuffle_epi8 (data, shuffle_constant);
          _mm_storeu_si128 (datap, data);
        }

       
      unsigned char *cp = (unsigned char *)datap;
      while (bytes_read--)
        crc = (crc << 8) ^ crctab[0][((crc >> 24) ^ *cp++) & 0xFF];
      if (feof (fp))
        break;
    }

  *crc_out = crc;
  *length_out = length;

  return !ferror (fp);
}
