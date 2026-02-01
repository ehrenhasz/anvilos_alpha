 

#include <config.h>

#include "c-strcasestr.h"

#include <stdlib.h>
#include <string.h>

#include "macros.h"

int
main ()
{
  {
    const char input[] = "foo";
    const char *result = c_strcasestr (input, "");
    ASSERT (result == input);
  }

  {
    const char input[] = "foo";
    const char *result = c_strcasestr (input, "O");
    ASSERT (result == input + 1);
  }

  {
    const char input[] = "ABC ABCDAB ABCDABCDABDE";
    const char *result = c_strcasestr (input, "ABCDaBD");
    ASSERT (result == input + 15);
  }

  {
    const char input[] = "ABC ABCDAB ABCDABCDABDE";
    const char *result = c_strcasestr (input, "ABCDaBE");
    ASSERT (result == NULL);
  }

  {
    const char input[] = "ABC ABCDAB ABCDABCDABDE";
    const char *result = c_strcasestr (input, "ABCDaBCD");
    ASSERT (result == input + 11);
  }

   
  {
    const char input[] = "F_BD_CE_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD"
                         "_C3_88_20_EF_BF_BD_EF_BF_BD_EF_BF_BD"
                         "_C3_A7_20_EF_BF_BD";
    const char need[] = "_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD";
    const char *result = c_strcasestr (input, need);
    ASSERT (result == NULL);
  }
  {
    const char input[] = "F_BD_CE_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD"
                         "_C3_88_20_EF_BF_BD_EF_BF_BD_EF_BF_BD"
                         "_C3_A7_20_EF_BF_BD_DA_B5_C2_A6_20"
                         "_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD";
    const char need[] = "_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD_EF_BF_BD";
    const char *result = c_strcasestr (input, need);
    ASSERT (result == input + 115);
  }

   
  {
    size_t repeat = 10000;
    size_t m = 1000000;
    const char *needle =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAaaaaaaAAAAaaaaaaa"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    char *haystack = (char *) malloc (m + 1);
    if (haystack != NULL)
      {
        memset (haystack, 'A', m);
        haystack[0] = 'B';
        haystack[m] = '\0';

        for (; repeat > 0; repeat--)
          {
            ASSERT (c_strcasestr (haystack, needle) == haystack + 1);
          }

        free (haystack);
      }
  }

   
  {
    size_t repeat = 10000;
    size_t m = 1000000;
    const char *haystack =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "ABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAB";
    char *needle = (char *) malloc (m + 1);
    if (needle != NULL)
      {
        memset (needle, 'A', m);
        needle[m] = '\0';

        for (; repeat > 0; repeat--)
          {
            ASSERT (c_strcasestr (haystack, needle) == NULL);
          }

        free (needle);
      }
  }

   
  {
    size_t m = 1000000;
    char *haystack = (char *) malloc (2 * m + 2);
    char *needle = (char *) malloc (m + 2);
    if (haystack != NULL && needle != NULL)
      {
        const char *result;

        memset (haystack, 'A', 2 * m);
        haystack[2 * m] = 'B';
        haystack[2 * m + 1] = '\0';

        memset (needle, 'a', m);
        needle[m] = 'B';
        needle[m + 1] = '\0';

        result = c_strcasestr (haystack, needle);
        ASSERT (result == haystack + m);
      }
    free (needle);
    free (haystack);
  }

  {
     
    const char *haystack =
      "\n"
      "with_build_libsubdir\n"
      "with_local_prefix\n"
      "with_gxx_include_dir\n"
      "with_cpp_install_dir\n"
      "enable_generated_files_in_srcdir\n"
      "with_gnu_ld\n"
      "with_ld\n"
      "with_demangler_in_ld\n"
      "with_gnu_as\n"
      "with_as\n"
      "enable_largefile\n"
      "enable_werror_always\n"
      "enable_checking\n"
      "enable_coverage\n"
      "enable_gather_detailed_mem_stats\n"
      "enable_build_with_cxx\n"
      "with_stabs\n"
      "enable_multilib\n"
      "enable___cxa_atexit\n"
      "enable_decimal_float\n"
      "enable_fixed_point\n"
      "enable_threads\n"
      "enable_tls\n"
      "enable_objc_gc\n"
      "with_dwarf2\n"
      "enable_shared\n"
      "with_build_sysroot\n"
      "with_sysroot\n"
      "with_specs\n"
      "with_pkgversion\n"
      "with_bugurl\n"
      "enable_languages\n"
      "with_multilib_list\n";
    const char *needle = "\n"
      "with_GNU_ld\n";
    const char* p = c_strcasestr (haystack, needle);
    ASSERT (p - haystack == 114);
  }

  {
     
    const char *haystack = "..wi.D.";
    const char *needle = ".d.";
    const char* p = c_strcasestr (haystack, needle);
    ASSERT (p - haystack == 4);
  }

  {
     
    const char *needle = "\nwith_gnu_ld-extend-to-len-32-b\n";
    const char *h =
      "\n"
      "with_build_libsubdir\n"
      "with_local_prefix\n"
      "with_gxx_include_dir\n"
      "with_cpp_install_dir\n"
      "with_e_\n"
      "..............................\n"
      "with_FGHIJKLMNOPQRSTUVWXYZ\n"
      "with_567890123456789\n"
      "with_multilib_list\n";
    size_t h_len = strlen (h);
    char *haystack = malloc (h_len + 1);
    size_t i;
    ASSERT (haystack);
    for (i = 0; i < h_len - strlen (needle); i++)
      {
        const char *p;
        memcpy (haystack, h, h_len + 1);
        memcpy (haystack + i, needle, strlen (needle) + 1);
        p = c_strcasestr (haystack, needle);
        ASSERT (p);
        ASSERT (p - haystack == i);
      }
    free (haystack);
  }

   
  {
    size_t m = 1024;
    char *haystack = (char *) malloc (2 * m + 1);
    char *needle = (char *) malloc (m + 1);
    if (haystack != NULL && needle != NULL)
      {
        const char *p;
        haystack[0] = 'x';
        memset (haystack + 1, ' ', m - 1);
        memset (haystack + m, 'x', m);
        haystack[2 * m] = '\0';
        memset (needle, 'X', m);
        needle[m] = '\0';
        p = c_strcasestr (haystack, needle);
        ASSERT (p);
        ASSERT (p - haystack == m);
      }
    free (needle);
    free (haystack);
  }

  return 0;
}
