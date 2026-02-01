 

#include <config.h>

#include "localename.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

#if HAVE_WORKING_NEWLOCALE && HAVE_WORKING_USELOCALE && !HAVE_FAKE_LOCALES
# define HAVE_GOOD_USELOCALE 1
#endif

#ifdef __HAIKU__
 
#if __GNUC__ >= 12
# pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#endif

#if HAVE_GOOD_USELOCALE

static struct { int cat; int mask; const char *string; } const categories[] =
  {
      { LC_CTYPE,          LC_CTYPE_MASK,          "LC_CTYPE" },
      { LC_NUMERIC,        LC_NUMERIC_MASK,        "LC_NUMERIC" },
      { LC_TIME,           LC_TIME_MASK,           "LC_TIME" },
      { LC_COLLATE,        LC_COLLATE_MASK,        "LC_COLLATE" },
      { LC_MONETARY,       LC_MONETARY_MASK,       "LC_MONETARY" },
      { LC_MESSAGES,       LC_MESSAGES_MASK,       "LC_MESSAGES" }
# ifdef LC_PAPER
    , { LC_PAPER,          LC_PAPER_MASK,          "LC_PAPER" }
# endif
# ifdef LC_NAME
    , { LC_NAME,           LC_NAME_MASK,           "LC_NAME" }
# endif
# ifdef LC_ADDRESS
    , { LC_ADDRESS,        LC_ADDRESS_MASK,        "LC_ADDRESS" }
# endif
# ifdef LC_TELEPHONE
    , { LC_TELEPHONE,      LC_TELEPHONE_MASK,      "LC_TELEPHONE" }
# endif
# ifdef LC_MEASUREMENT
    , { LC_MEASUREMENT,    LC_MEASUREMENT_MASK,    "LC_MEASUREMENT" }
# endif
# ifdef LC_IDENTIFICATION
    , { LC_IDENTIFICATION, LC_IDENTIFICATION_MASK, "LC_IDENTIFICATION" }
# endif
  };

#endif

 
static void
test_locale_name (void)
{
  const char *ret;
  const char *name;

   
  ASSERT (gl_locale_name (LC_MESSAGES, "LC_MESSAGES") != NULL);

   
  setlocale (LC_ALL, "en_US.UTF-8");
#if HAVE_GOOD_USELOCALE
  uselocale (LC_GLOBAL_LOCALE);
#endif

   
  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LC_NUMERIC");
  unsetenv ("LANG");
   
  unsetenv ("LC_COLLATE");
  unsetenv ("LC_MONETARY");
  unsetenv ("LC_TIME");
  unsetenv ("LC_ADDRESS");
  unsetenv ("LC_IDENTIFICATION");
  unsetenv ("LC_MEASUREMENT");
  unsetenv ("LC_NAME");
  unsetenv ("LC_PAPER");
  unsetenv ("LC_TELEPHONE");
  ret = setlocale (LC_ALL, "");
  ASSERT (ret != NULL);
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"),
                  gl_locale_name_default ()) == 0);
  ASSERT (strcmp (gl_locale_name (LC_NUMERIC, "LC_NUMERIC"),
                  gl_locale_name_default ()) == 0);

   

  setenv ("LC_ALL", "", 1);
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"),
                  gl_locale_name_default ()) == 0);

  unsetenv ("LC_ALL");
  setenv ("LC_CTYPE", "", 1);
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"),
                  gl_locale_name_default ()) == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "", 1);
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"),
                  gl_locale_name_default ()) == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "", 1);
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"),
                  gl_locale_name_default ()) == 0);

   

  setenv ("LC_ALL", "C", 1);
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"), "C") == 0);

  unsetenv ("LC_ALL");
  setenv ("LC_CTYPE", "C", 1);
  setenv ("LC_MESSAGES", "C", 1);
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"), "C") == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "C", 1);
  setlocale (LC_ALL, "");
  ASSERT (strcmp (gl_locale_name (LC_MESSAGES, "LC_MESSAGES"), "C") == 0);

   

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "fr_FR.UTF-8", 1);
  setenv ("LANG", "de_DE.UTF-8", 1);
  if (setlocale (LC_ALL, "") != NULL)
    {
      name = gl_locale_name (LC_CTYPE, "LC_CTYPE");
#if defined _WIN32 && !defined __CYGWIN__
       
      ASSERT (strcmp (name, "de_DE") == 0 || strcmp (name, "de_DE.UTF-8") == 0);
#else
      ASSERT (strcmp (name, "de_DE.UTF-8") == 0);
#endif
      name = gl_locale_name (LC_MESSAGES, "LC_MESSAGES");
      ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
    }

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "fr_FR.UTF-8", 1);
  unsetenv ("LANG");
  if (setlocale (LC_ALL, "") != NULL)
    {
      name = gl_locale_name (LC_CTYPE, "LC_CTYPE");
      ASSERT (strcmp (name, gl_locale_name_default ()) == 0);
      name = gl_locale_name (LC_MESSAGES, "LC_MESSAGES");
      ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
    }

#if HAVE_GOOD_USELOCALE
   
  {
    locale_t locale = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
    if (locale != NULL)
      {
        uselocale (locale);
        name = gl_locale_name (LC_CTYPE, "LC_CTYPE");
        ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
        name = gl_locale_name (LC_MESSAGES, "LC_MESSAGES");
        ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
        uselocale (LC_GLOBAL_LOCALE);
        freelocale (locale);
      }
  }

   
  {
    unsigned int i;

    for (i = 0; i < SIZEOF (categories); i++)
      {
        int category_mask = categories[i].mask;
        locale_t loc = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
        if (loc != NULL)
          {
            locale_t locale = newlocale (category_mask, "de_DE.UTF-8", loc);
            if (locale == NULL)
              freelocale (loc);
            else
              {
                unsigned int j;

                uselocale (locale);
                for (j = 0; j < SIZEOF (categories); j++)
                  {
                    const char *name_j =
                      gl_locale_name (categories[j].cat, categories[j].string);
                    if (j == i)
                      ASSERT (strcmp (name_j, "de_DE.UTF-8") == 0);
                    else
                      ASSERT (strcmp (name_j, "fr_FR.UTF-8") == 0);
                  }
                uselocale (LC_GLOBAL_LOCALE);
                freelocale (locale);
              }
          }
      }
  }
#endif
}

 
static void
test_locale_name_thread (void)
{
   
  setlocale (LC_ALL, "en_US.UTF-8");

#if HAVE_GOOD_USELOCALE
   
  uselocale (LC_GLOBAL_LOCALE);
  ASSERT (gl_locale_name_thread (LC_CTYPE, "LC_CTYPE") == NULL);
  ASSERT (gl_locale_name_thread (LC_MESSAGES, "LC_MESSAGES") == NULL);

   
  {
    locale_t locale = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
    if (locale != NULL)
      {
        const char *name;

        uselocale (locale);
        name = gl_locale_name_thread (LC_CTYPE, "LC_CTYPE");
        ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
        name = gl_locale_name_thread (LC_MESSAGES, "LC_MESSAGES");
        ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
        uselocale (LC_GLOBAL_LOCALE);
        freelocale (locale);
      }
  }

   
  {
    unsigned int i;

    for (i = 0; i < SIZEOF (categories); i++)
      {
        int category_mask = categories[i].mask;
        locale_t loc = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
        if (loc != NULL)
          {
            locale_t locale = newlocale (category_mask, "de_DE.UTF-8", loc);
            if (locale == NULL)
              freelocale (loc);
            else
              {
                unsigned int j;

                uselocale (locale);
                for (j = 0; j < SIZEOF (categories); j++)
                  {
                    const char *name_j =
                      gl_locale_name_thread (categories[j].cat,
                                             categories[j].string);
                    if (j == i)
                      ASSERT (strcmp (name_j, "de_DE.UTF-8") == 0);
                    else
                      ASSERT (strcmp (name_j, "fr_FR.UTF-8") == 0);
                  }
                uselocale (LC_GLOBAL_LOCALE);
                freelocale (locale);
              }
          }
      }
  }

   
  {
     
    static const char * const choices[] =
      {
        "C",
        "POSIX",
        "af_ZA",
        "af_ZA.UTF-8",
        "am_ET",
        "am_ET.UTF-8",
        "be_BY",
        "be_BY.UTF-8",
        "bg_BG",
        "bg_BG.UTF-8",
        "ca_ES",
        "ca_ES.UTF-8",
        "cs_CZ",
        "cs_CZ.UTF-8",
        "da_DK",
        "da_DK.UTF-8",
        "de_AT",
        "de_AT.UTF-8",
        "de_CH",
        "de_CH.UTF-8",
        "de_DE",
        "de_DE.UTF-8",
        "el_GR",
        "el_GR.UTF-8",
        "en_AU",
        "en_AU.UTF-8",
        "en_CA",
        "en_CA.UTF-8",
        "en_GB",
        "en_GB.UTF-8",
        "en_IE",
        "en_IE.UTF-8",
        "en_NZ",
        "en_NZ.UTF-8",
        "en_US",
        "en_US.UTF-8",
        "es_ES",
        "es_ES.UTF-8",
        "et_EE",
        "et_EE.UTF-8",
        "eu_ES",
        "eu_ES.UTF-8",
        "fi_FI",
        "fi_FI.UTF-8",
        "fr_BE",
        "fr_BE.UTF-8",
        "fr_CA",
        "fr_CA.UTF-8",
        "fr_CH",
        "fr_CH.UTF-8",
        "fr_FR",
        "fr_FR.UTF-8",
        "he_IL",
        "he_IL.UTF-8",
        "hr_HR",
        "hr_HR.UTF-8",
        "hu_HU",
        "hu_HU.UTF-8",
        "hy_AM",
        "is_IS",
        "is_IS.UTF-8",
        "it_CH",
        "it_CH.UTF-8",
        "it_IT",
        "it_IT.UTF-8",
        "ja_JP.UTF-8",
        "kk_KZ",
        "kk_KZ.UTF-8",
        "ko_KR.UTF-8",
        "lt_LT",
        "lt_LT.UTF-8",
        "nl_BE",
        "nl_BE.UTF-8",
        "nl_NL",
        "nl_NL.UTF-8",
        "no_NO",
        "no_NO.UTF-8",
        "pl_PL",
        "pl_PL.UTF-8",
        "pt_BR",
        "pt_BR.UTF-8",
        "pt_PT",
        "pt_PT.UTF-8",
        "ro_RO",
        "ro_RO.UTF-8",
        "ru_RU",
        "ru_RU.UTF-8",
        "sk_SK",
        "sk_SK.UTF-8",
        "sl_SI",
        "sl_SI.UTF-8",
        "sv_SE",
        "sv_SE.UTF-8",
        "tr_TR",
        "tr_TR.UTF-8",
        "uk_UA",
        "uk_UA.UTF-8",
        "zh_CN",
        "zh_CN.UTF-8",
        "zh_HK",
        "zh_HK.UTF-8",
        "zh_TW",
        "zh_TW.UTF-8"
      };
     
    unsigned char   available[SIZEOF (choices)];
     
    const char *unsaved_names[SIZEOF (choices)][SIZEOF (categories)];
     
    char *saved_names[SIZEOF (choices)][SIZEOF (categories)];
    unsigned int j;

    for (j = 0; j < SIZEOF (choices); j++)
      {
        locale_t locale = newlocale (LC_ALL_MASK, choices[j], NULL);
        available[j] = (locale != NULL);
        if (locale != NULL)
          {
            unsigned int i;

            uselocale (locale);
            for (i = 0; i < SIZEOF (categories); i++)
              {
                unsaved_names[j][i] = gl_locale_name_thread (categories[i].cat, categories[i].string);
                saved_names[j][i] = strdup (unsaved_names[j][i]);
              }
            uselocale (LC_GLOBAL_LOCALE);
            freelocale (locale);
          }
      }
     
    for (j = 0; j < SIZEOF (choices); j++)
      if (available[j])
        {
          unsigned int i;

          for (i = 0; i < SIZEOF (categories); i++)
            ASSERT (strcmp (unsaved_names[j][i], saved_names[j][i]) == 0);
        }
     
    for (j = SIZEOF (choices); j > 0; )
      {
        j--;
        if (available[j])
          {
            locale_t locale = newlocale (LC_ALL_MASK, choices[j], NULL);
            unsigned int i;

            ASSERT (locale != NULL);
            uselocale (locale);
            for (i = 0; i < SIZEOF (categories); i++)
              {
                const char *name = gl_locale_name_thread (categories[i].cat, categories[i].string);
                ASSERT (strcmp (unsaved_names[j][i], name) == 0);
              }
            uselocale (LC_GLOBAL_LOCALE);
            freelocale (locale);
          }
      }
     
    for (j = 0; j < SIZEOF (choices); j++)
      if (available[j])
        {
          unsigned int i;

          for (i = 0; i < SIZEOF (categories); i++)
            {
              ASSERT (strcmp (unsaved_names[j][i], saved_names[j][i]) == 0);
              free (saved_names[j][i]);
            }
        }
  }
#else
   
  ASSERT (gl_locale_name_thread (LC_CTYPE, "LC_CTYPE") == NULL);
  ASSERT (gl_locale_name_thread (LC_MESSAGES, "LC_MESSAGES") == NULL);
#endif
}

 
static void
test_locale_name_posix (void)
{
  const char *ret;
  const char *name;

   
  setlocale (LC_ALL, "en_US.UTF-8");
#if HAVE_GOOD_USELOCALE
  uselocale (LC_GLOBAL_LOCALE);
#endif

   
  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LC_NUMERIC");
  unsetenv ("LANG");
   
  unsetenv ("LC_COLLATE");
  unsetenv ("LC_MONETARY");
  unsetenv ("LC_TIME");
  unsetenv ("LC_ADDRESS");
  unsetenv ("LC_IDENTIFICATION");
  unsetenv ("LC_MEASUREMENT");
  unsetenv ("LC_NAME");
  unsetenv ("LC_PAPER");
  unsetenv ("LC_TELEPHONE");
  ret = setlocale (LC_ALL, "");
  ASSERT (ret != NULL);
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);
  name = gl_locale_name_posix (LC_NUMERIC, "LC_NUMERIC");
  ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);

   

  setenv ("LC_ALL", "", 1);
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);

  unsetenv ("LC_ALL");
  setenv ("LC_CTYPE", "", 1);
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "", 1);
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "", 1);
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);

   

  setenv ("LC_ALL", "C", 1);
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "C") == 0);

  unsetenv ("LC_ALL");
  setenv ("LC_CTYPE", "C", 1);
  setenv ("LC_MESSAGES", "C", 1);
  unsetenv ("LANG");
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "C") == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "C", 1);
  setlocale (LC_ALL, "");
  name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "C") == 0);

   

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "fr_FR.UTF-8", 1);
  setenv ("LANG", "de_DE.UTF-8", 1);
  if (setlocale (LC_ALL, "") != NULL)
    {
      name = gl_locale_name_posix (LC_CTYPE, "LC_CTYPE");
#if defined _WIN32 && !defined __CYGWIN__
      ASSERT (strcmp (name, "de_DE") == 0 || strcmp (name, "de_DE.UTF-8") == 0);
#else
      ASSERT (strcmp (name, "de_DE.UTF-8") == 0);
#endif
      name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
      ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
    }

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "fr_FR.UTF-8", 1);
  unsetenv ("LANG");
  if (setlocale (LC_ALL, "") != NULL)
    {
      name = gl_locale_name_posix (LC_CTYPE, "LC_CTYPE");
      ASSERT (name == NULL || strcmp (name, gl_locale_name_default ()) == 0);
      name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
      ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);
    }

#if HAVE_GOOD_USELOCALE
   
  {
    locale_t locale = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
    if (locale != NULL)
      {
        unsetenv ("LC_ALL");
        unsetenv ("LC_CTYPE");
        unsetenv ("LC_MESSAGES");
        setenv ("LANG", "C", 1);
        setlocale (LC_ALL, "");
        uselocale (locale);
        name = gl_locale_name_posix (LC_MESSAGES, "LC_MESSAGES");
        ASSERT (strcmp (name, "C") == 0);
        uselocale (LC_GLOBAL_LOCALE);
        freelocale (locale);
      }
  }
#endif
}

 
static void
test_locale_name_environ (void)
{
  const char *name;

   
  setlocale (LC_ALL, "en_US.UTF-8");
#if HAVE_GOOD_USELOCALE
  uselocale (LC_GLOBAL_LOCALE);
#endif

   
  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LC_NUMERIC");
  unsetenv ("LANG");
  ASSERT (gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES") == NULL);
  ASSERT (gl_locale_name_environ (LC_NUMERIC, "LC_NUMERIC") == NULL);

   

  setenv ("LC_ALL", "", 1);
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  ASSERT (gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES") == NULL);

  unsetenv ("LC_ALL");
  setenv ("LC_CTYPE", "", 1);
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  ASSERT (gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES") == NULL);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "", 1);
  unsetenv ("LANG");
  ASSERT (gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES") == NULL);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "", 1);
  ASSERT (gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES") == NULL);

   

  setenv ("LC_ALL", "C", 1);
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  unsetenv ("LANG");
  name = gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "C") == 0);

  unsetenv ("LC_ALL");
  setenv ("LC_CTYPE", "C", 1);
  setenv ("LC_MESSAGES", "C", 1);
  unsetenv ("LANG");
  name = gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "C") == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "C", 1);
  name = gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "C") == 0);

   

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "fr_FR.UTF-8", 1);
  setenv ("LANG", "de_DE.UTF-8", 1);
  name = gl_locale_name_environ (LC_CTYPE, "LC_CTYPE");
  ASSERT (strcmp (name, "de_DE.UTF-8") == 0);
  name = gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);

  unsetenv ("LC_ALL");
  unsetenv ("LC_CTYPE");
  setenv ("LC_MESSAGES", "fr_FR.UTF-8", 1);
  unsetenv ("LANG");
  name = gl_locale_name_environ (LC_CTYPE, "LC_CTYPE");
  ASSERT (name == NULL);
  name = gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES");
  ASSERT (strcmp (name, "fr_FR.UTF-8") == 0);

#if HAVE_GOOD_USELOCALE
   
  {
    locale_t locale = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
    if (locale != NULL)
      {
        unsetenv ("LC_ALL");
        unsetenv ("LC_CTYPE");
        unsetenv ("LC_MESSAGES");
        setenv ("LANG", "C", 1);
        setlocale (LC_ALL, "");
        uselocale (locale);
        name = gl_locale_name_environ (LC_MESSAGES, "LC_MESSAGES");
        ASSERT (strcmp (name, "C") == 0);
        uselocale (LC_GLOBAL_LOCALE);
        freelocale (locale);
      }
  }
#endif
}

 
static void
test_locale_name_default (void)
{
  const char *name = gl_locale_name_default ();

  ASSERT (name != NULL);

   
#if !((defined __APPLE__ && defined __MACH__) || (defined _WIN32 || defined __CYGWIN__))
  ASSERT (strcmp (name, "C") == 0);
#endif

#if HAVE_GOOD_USELOCALE
   
  {
    locale_t locale = newlocale (LC_ALL_MASK, "fr_FR.UTF-8", NULL);
    if (locale != NULL)
      {
        uselocale (locale);
        ASSERT (strcmp (gl_locale_name_default (), name) == 0);
        uselocale (LC_GLOBAL_LOCALE);
        freelocale (locale);
      }
  }
#endif
}

int
main ()
{
  test_locale_name ();
  test_locale_name_thread ();
  test_locale_name_posix ();
  test_locale_name_environ ();
  test_locale_name_default ();

  return 0;
}
