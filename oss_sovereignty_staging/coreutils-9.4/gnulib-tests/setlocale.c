 

#include <config.h>

 

 

 
#include <locale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "localename.h"

#if HAVE_CFLOCALECOPYPREFERREDLANGUAGES || HAVE_CFPREFERENCESCOPYAPPVALUE
# if HAVE_CFLOCALECOPYPREFERREDLANGUAGES
#  include <CoreFoundation/CFLocale.h>
# elif HAVE_CFPREFERENCESCOPYAPPVALUE
#  include <CoreFoundation/CFPreferences.h>
# endif
# include <CoreFoundation/CFPropertyList.h>
# include <CoreFoundation/CFArray.h>
# include <CoreFoundation/CFString.h>
extern void gl_locale_name_canonicalize (char *name);
#endif

#if 1

# undef setlocale

 
# if NEED_SETLOCALE_IMPROVED
#  define setlocale_improved rpl_setlocale
# elif NEED_SETLOCALE_MTSAFE
#  define setlocale_mtsafe rpl_setlocale
# else
#  error "This file should only be compiled if NEED_SETLOCALE_IMPROVED || NEED_SETLOCALE_MTSAFE."
# endif

 
# if !SETLOCALE_NULL_ALL_MTSAFE || !SETLOCALE_NULL_ONE_MTSAFE  

#  if NEED_SETLOCALE_IMPROVED
static
#  endif
char *
setlocale_mtsafe (int category, const char *locale)
{
  if (locale == NULL)
    return (char *) setlocale_null (category);
  else
    return setlocale (category, locale);
}
# else  

#  define setlocale_mtsafe setlocale

# endif  

# if NEED_SETLOCALE_IMPROVED

 
static const char *
category_to_name (int category)
{
  const char *retval;

  switch (category)
  {
  case LC_COLLATE:
    retval = "LC_COLLATE";
    break;
  case LC_CTYPE:
    retval = "LC_CTYPE";
    break;
  case LC_MONETARY:
    retval = "LC_MONETARY";
    break;
  case LC_NUMERIC:
    retval = "LC_NUMERIC";
    break;
  case LC_TIME:
    retval = "LC_TIME";
    break;
  case LC_MESSAGES:
    retval = "LC_MESSAGES";
    break;
  default:
     
    retval = "LC_XXX";
  }

  return retval;
}

#  if defined _WIN32 && ! defined __CYGWIN__

 

 
struct table_entry
{
  const char *code;
  const char *english;
};
static const struct table_entry language_table[] =
  {
    { "af", "Afrikaans" },
    { "am", "Amharic" },
    { "ar", "Arabic" },
    { "arn", "Mapudungun" },
    { "as", "Assamese" },
    { "az@cyrillic", "Azeri (Cyrillic)" },
    { "az@latin", "Azeri (Latin)" },
    { "ba", "Bashkir" },
    { "be", "Belarusian" },
    { "ber", "Tamazight" },
    { "ber@arabic", "Tamazight (Arabic)" },
    { "ber@latin", "Tamazight (Latin)" },
    { "bg", "Bulgarian" },
    { "bin", "Edo" },
    { "bn", "Bengali" },
    { "bn_BD", "Bengali (Bangladesh)" },
    { "bn_IN", "Bengali (India)" },
    { "bnt", "Sutu" },
    { "bo", "Tibetan" },
    { "br", "Breton" },
    { "bs", "BSB" },  
    { "bs@cyrillic", "BSC" },  
    { "ca", "Catalan" },
    { "chr", "Cherokee" },
    { "co", "Corsican" },
    { "cpe", "Hawaiian" },
    { "cs", "Czech" },
    { "cy", "Welsh" },
    { "da", "Danish" },
    { "de", "German" },
    { "dsb", "Lower Sorbian" },
    { "dv", "Divehi" },
    { "el", "Greek" },
    { "en", "English" },
    { "es", "Spanish" },
    { "et", "Estonian" },
    { "eu", "Basque" },
    { "fa", "Farsi" },
    { "ff", "Fulfulde" },
    { "fi", "Finnish" },
    { "fo", "Faroese" },  
    { "fr", "French" },
    { "fy", "Frisian" },
    { "ga", "IRE" },  
    { "gd", "Gaelic (Scotland)" },
    { "gd", "Scottish Gaelic" },
    { "gl", "Galician" },
    { "gn", "Guarani" },
    { "gsw", "Alsatian" },
    { "gu", "Gujarati" },
    { "ha", "Hausa" },
    { "he", "Hebrew" },
    { "hi", "Hindi" },
    { "hr", "Croatian" },
    { "hsb", "Upper Sorbian" },
    { "hu", "Hungarian" },
    { "hy", "Armenian" },
    { "id", "Indonesian" },
    { "ig", "Igbo" },
    { "ii", "Yi" },
    { "is", "Icelandic" },
    { "it", "Italian" },
    { "iu", "IUK" },  
    { "ja", "Japanese" },
    { "ka", "Georgian" },
    { "kk", "Kazakh" },
    { "kl", "Greenlandic" },
    { "km", "Cambodian" },
    { "km", "Khmer" },
    { "kn", "Kannada" },
    { "ko", "Korean" },
    { "kok", "Konkani" },
    { "kr", "Kanuri" },
    { "ks", "Kashmiri" },
    { "ks_IN", "Kashmiri_India" },
    { "ks_PK", "Kashmiri (Arabic)_Pakistan" },
    { "ky", "Kyrgyz" },
    { "la", "Latin" },
    { "lb", "Luxembourgish" },
    { "lo", "Lao" },
    { "lt", "Lithuanian" },
    { "lv", "Latvian" },
    { "mi", "Maori" },
    { "mk", "FYRO Macedonian" },
    { "mk", "Macedonian" },
    { "ml", "Malayalam" },
    { "mn", "Mongolian" },
    { "mni", "Manipuri" },
    { "moh", "Mohawk" },
    { "mr", "Marathi" },
    { "ms", "Malay" },
    { "mt", "Maltese" },
    { "my", "Burmese" },
    { "nb", "NOR" },  
    { "ne", "Nepali" },
    { "nic", "Ibibio" },
    { "nl", "Dutch" },
    { "nn", "NON" },  
    { "no", "Norwegian" },
    { "nso", "Northern Sotho" },
    { "nso", "Sepedi" },
    { "oc", "Occitan" },
    { "om", "Oromo" },
    { "or", "Oriya" },
    { "pa", "Punjabi" },
    { "pap", "Papiamentu" },
    { "pl", "Polish" },
    { "prs", "Dari" },
    { "ps", "Pashto" },
    { "pt", "Portuguese" },
    { "qu", "Quechua" },
    { "qut", "K'iche'" },
    { "rm", "Romansh" },
    { "ro", "Romanian" },
    { "ru", "Russian" },
    { "rw", "Kinyarwanda" },
    { "sa", "Sanskrit" },
    { "sah", "Yakut" },
    { "sd", "Sindhi" },
    { "se", "Sami (Northern)" },
    { "se", "Northern Sami" },
    { "si", "Sinhalese" },
    { "sk", "Slovak" },
    { "sl", "Slovenian" },
    { "sma", "Sami (Southern)" },
    { "sma", "Southern Sami" },
    { "smj", "Sami (Lule)" },
    { "smj", "Lule Sami" },
    { "smn", "Sami (Inari)" },
    { "smn", "Inari Sami" },
    { "sms", "Sami (Skolt)" },
    { "sms", "Skolt Sami" },
    { "so", "Somali" },
    { "sq", "Albanian" },
    { "sr", "Serbian (Latin)" },
    { "sr@cyrillic", "SRB" },  
    { "sv", "Swedish" },
    { "sw", "Swahili" },
    { "syr", "Syriac" },
    { "ta", "Tamil" },
    { "te", "Telugu" },
    { "tg", "Tajik" },
    { "th", "Thai" },
    { "ti", "Tigrinya" },
    { "tk", "Turkmen" },
    { "tl", "Filipino" },
    { "tn", "Tswana" },
    { "tr", "Turkish" },
    { "ts", "Tsonga" },
    { "tt", "Tatar" },
    { "ug", "Uighur" },
    { "uk", "Ukrainian" },
    { "ur", "Urdu" },
    { "uz", "Uzbek" },
    { "uz", "Uzbek (Latin)" },
    { "uz@cyrillic", "Uzbek (Cyrillic)" },
    { "ve", "Venda" },
    { "vi", "Vietnamese" },
    { "wen", "Sorbian" },
    { "wo", "Wolof" },
    { "xh", "Xhosa" },
    { "yi", "Yiddish" },
    { "yo", "Yoruba" },
    { "zh", "Chinese" },
    { "zu", "Zulu" }
  };

 
static const struct table_entry country_table[] =
  {
    { "AE", "U.A.E." },
    { "AF", "Afghanistan" },
    { "AL", "Albania" },
    { "AM", "Armenia" },
    { "AN", "Netherlands Antilles" },
    { "AR", "Argentina" },
    { "AT", "Austria" },
    { "AU", "Australia" },
    { "AZ", "Azerbaijan" },
    { "BA", "Bosnia and Herzegovina" },
    { "BD", "Bangladesh" },
    { "BE", "Belgium" },
    { "BG", "Bulgaria" },
    { "BH", "Bahrain" },
    { "BN", "Brunei Darussalam" },
    { "BO", "Bolivia" },
    { "BR", "Brazil" },
    { "BT", "Bhutan" },
    { "BY", "Belarus" },
    { "BZ", "Belize" },
    { "CA", "Canada" },
    { "CG", "Congo" },
    { "CH", "Switzerland" },
    { "CI", "Cote d'Ivoire" },
    { "CL", "Chile" },
    { "CM", "Cameroon" },
    { "CN", "People's Republic of China" },
    { "CO", "Colombia" },
    { "CR", "Costa Rica" },
    { "CS", "Serbia and Montenegro" },
    { "CZ", "Czech Republic" },
    { "DE", "Germany" },
    { "DK", "Denmark" },
    { "DO", "Dominican Republic" },
    { "DZ", "Algeria" },
    { "EC", "Ecuador" },
    { "EE", "Estonia" },
    { "EG", "Egypt" },
    { "ER", "Eritrea" },
    { "ES", "Spain" },
    { "ET", "Ethiopia" },
    { "FI", "Finland" },
    { "FO", "Faroe Islands" },
    { "FR", "France" },
    { "GB", "United Kingdom" },
    { "GD", "Caribbean" },
    { "GE", "Georgia" },
    { "GL", "Greenland" },
    { "GR", "Greece" },
    { "GT", "Guatemala" },
    { "HK", "Hong Kong" },
    { "HK", "Hong Kong S.A.R." },
    { "HN", "Honduras" },
    { "HR", "Croatia" },
    { "HT", "Haiti" },
    { "HU", "Hungary" },
    { "ID", "Indonesia" },
    { "IE", "Ireland" },
    { "IL", "Israel" },
    { "IN", "India" },
    { "IQ", "Iraq" },
    { "IR", "Iran" },
    { "IS", "Iceland" },
    { "IT", "Italy" },
    { "JM", "Jamaica" },
    { "JO", "Jordan" },
    { "JP", "Japan" },
    { "KE", "Kenya" },
    { "KG", "Kyrgyzstan" },
    { "KH", "Cambodia" },
    { "KR", "South Korea" },
    { "KW", "Kuwait" },
    { "KZ", "Kazakhstan" },
    { "LA", "Laos" },
    { "LB", "Lebanon" },
    { "LI", "Liechtenstein" },
    { "LK", "Sri Lanka" },
    { "LT", "Lithuania" },
    { "LU", "Luxembourg" },
    { "LV", "Latvia" },
    { "LY", "Libya" },
    { "MA", "Morocco" },
    { "MC", "Principality of Monaco" },
    { "MD", "Moldava" },
    { "MD", "Moldova" },
    { "ME", "Montenegro" },
    { "MK", "Former Yugoslav Republic of Macedonia" },
    { "ML", "Mali" },
    { "MM", "Myanmar" },
    { "MN", "Mongolia" },
    { "MO", "Macau S.A.R." },
    { "MT", "Malta" },
    { "MV", "Maldives" },
    { "MX", "Mexico" },
    { "MY", "Malaysia" },
    { "NG", "Nigeria" },
    { "NI", "Nicaragua" },
    { "NL", "Netherlands" },
    { "NO", "Norway" },
    { "NP", "Nepal" },
    { "NZ", "New Zealand" },
    { "OM", "Oman" },
    { "PA", "Panama" },
    { "PE", "Peru" },
    { "PH", "Philippines" },
    { "PK", "Islamic Republic of Pakistan" },
    { "PL", "Poland" },
    { "PR", "Puerto Rico" },
    { "PT", "Portugal" },
    { "PY", "Paraguay" },
    { "QA", "Qatar" },
    { "RE", "Reunion" },
    { "RO", "Romania" },
    { "RS", "Serbia" },
    { "RU", "Russia" },
    { "RW", "Rwanda" },
    { "SA", "Saudi Arabia" },
    { "SE", "Sweden" },
    { "SG", "Singapore" },
    { "SI", "Slovenia" },
    { "SK", "Slovak" },
    { "SN", "Senegal" },
    { "SO", "Somalia" },
    { "SR", "Suriname" },
    { "SV", "El Salvador" },
    { "SY", "Syria" },
    { "TH", "Thailand" },
    { "TJ", "Tajikistan" },
    { "TM", "Turkmenistan" },
    { "TN", "Tunisia" },
    { "TR", "Turkey" },
    { "TT", "Trinidad and Tobago" },
    { "TW", "Taiwan" },
    { "TZ", "Tanzania" },
    { "UA", "Ukraine" },
    { "US", "United States" },
    { "UY", "Uruguay" },
    { "VA", "Vatican" },
    { "VE", "Venezuela" },
    { "VN", "Viet Nam" },
    { "YE", "Yemen" },
    { "ZA", "South Africa" },
    { "ZW", "Zimbabwe" }
  };

 
typedef struct { size_t lo; size_t hi; } range_t;
static void
search (const struct table_entry *table, size_t table_size, const char *string,
        range_t *result)
{
   
  size_t hi = table_size;
  size_t lo = 0;
  while (lo < hi)
    {
       
      size_t mid = (hi + lo) >> 1;  
      int cmp = strcmp (table[mid].code, string);
      if (cmp < 0)
        lo = mid + 1;
      else if (cmp > 0)
        hi = mid;
      else
        {
           
          {
            size_t i;

            for (i = mid; i > lo; )
              {
                i--;
                if (strcmp (table[i].code, string) < 0)
                  {
                    lo = i + 1;
                    break;
                  }
              }
          }
          {
            size_t i;

            for (i = mid + 1; i < hi; i++)
              {
                if (strcmp (table[i].code, string) > 0)
                  {
                    hi = i;
                    break;
                  }
              }
          }
           
          break;
        }
    }
  result->lo = lo;
  result->hi = hi;
}

 
static char *
setlocale_unixlike (int category, const char *locale)
{
  char *result;
  char llCC_buf[64];
  char ll_buf[64];
  char CC_buf[64];

   
  if (locale != NULL && strcmp (locale, "POSIX") == 0)
    locale = "C";

   
  result = setlocale_mtsafe (category, locale);
  if (result != NULL)
    return result;

   
  if (strlen (locale) < sizeof (llCC_buf))
    {
       
      {
        const char *p = locale;
        char *q = llCC_buf;

         
        for (; *p != '\0' && *p != '.'; p++, q++)
          *q = *p;
        if (*p == '.')
           
          for (; *p != '\0' && *p != '@'; p++)
            ;
         
        for (; *p != '\0'; p++, q++)
          *q = *p;
        *q = '\0';
      }
       
      if (strcmp (llCC_buf, locale) != 0)
        {
          result = setlocale (category, llCC_buf);
          if (result != NULL)
            return result;
        }
       
      {
        range_t range;
        size_t i;

        search (language_table,
                sizeof (language_table) / sizeof (language_table[0]),
                llCC_buf,
                &range);

        for (i = range.lo; i < range.hi; i++)
          {
             
            result = setlocale (category, language_table[i].english);
            if (result != NULL)
              return result;
          }
      }
       
      {
        const char *underscore = strchr (llCC_buf, '_');
        if (underscore != NULL)
          {
            const char *territory_start = underscore + 1;
            const char *territory_end = strchr (territory_start, '@');
            if (territory_end == NULL)
              territory_end = territory_start + strlen (territory_start);

            memcpy (ll_buf, llCC_buf, underscore - llCC_buf);
            strcpy (ll_buf + (underscore - llCC_buf), territory_end);

            memcpy (CC_buf, territory_start, territory_end - territory_start);
            CC_buf[territory_end - territory_start] = '\0';

            {
               
              range_t language_range;

              search (language_table,
                      sizeof (language_table) / sizeof (language_table[0]),
                      ll_buf,
                      &language_range);
              if (language_range.lo < language_range.hi)
                {
                  range_t country_range;

                  search (country_table,
                          sizeof (country_table) / sizeof (country_table[0]),
                          CC_buf,
                          &country_range);
                  if (country_range.lo < country_range.hi)
                    {
                      size_t i;
                      size_t j;

                      for (i = language_range.lo; i < language_range.hi; i++)
                        for (j = country_range.lo; j < country_range.hi; j++)
                          {
                             
                            const char *part1 = language_table[i].english;
                            size_t part1_len = strlen (part1);
                            const char *part2 = country_table[j].english;
                            size_t part2_len = strlen (part2) + 1;
                            char buf[64+64];

                            if (!(part1_len + 1 + part2_len <= sizeof (buf)))
                              abort ();
                            memcpy (buf, part1, part1_len);
                            buf[part1_len] = '_';
                            memcpy (buf + part1_len + 1, part2, part2_len);

                             
                            result = setlocale (category, buf);
                            if (result != NULL)
                              return result;
                          }
                    }

                   
                  {
                    size_t i;

                    for (i = language_range.lo; i < language_range.hi; i++)
                      {
                         
                        result =
                          setlocale (category, language_table[i].english);
                        if (result != NULL)
                          return result;
                      }
                  }
                }
            }
          }
      }
    }

   
  return NULL;
}

#  elif defined __ANDROID__

 
static char *
setlocale_unixlike (int category, const char *locale)
{
  char *result = setlocale_mtsafe (category, locale);
  if (result == NULL)
    switch (category)
      {
      case LC_CTYPE:
      case LC_NUMERIC:
      case LC_TIME:
      case LC_COLLATE:
      case LC_MONETARY:
      case LC_MESSAGES:
      case LC_ALL:
      case LC_PAPER:
      case LC_NAME:
      case LC_ADDRESS:
      case LC_TELEPHONE:
      case LC_MEASUREMENT:
        if (locale == NULL
            || strcmp (locale, "C") == 0 || strcmp (locale, "POSIX") == 0)
          result = (char *) "C";
        break;
      default:
        break;
      }
  return result;
}
#   define setlocale setlocale_unixlike

#  else
#   define setlocale_unixlike setlocale_mtsafe
#  endif

#  if LC_MESSAGES == 1729

 
static char lc_messages_name[64] = "C";

 
static char *
setlocale_single (int category, const char *locale)
{
  if (category == LC_MESSAGES)
    {
      if (locale != NULL)
        {
          lc_messages_name[sizeof (lc_messages_name) - 1] = '\0';
          strncpy (lc_messages_name, locale, sizeof (lc_messages_name) - 1);
        }
      return lc_messages_name;
    }
  else
    return setlocale_unixlike (category, locale);
}

#  else
#   define setlocale_single setlocale_unixlike
#  endif

#  if defined __APPLE__ && defined __MACH__

 
static char const locales_with_principal_territory[][6 + 1] =
  {
                 
    "ace_ID",    
    "af_ZA",     
    "ak_GH",     
    "am_ET",     
    "an_ES",     
    "ang_GB",    
    "arn_CL",    
    "as_IN",     
    "ast_ES",    
    "av_RU",     
    "awa_IN",    
    "az_AZ",     
    "ban_ID",    
    "be_BY",     
    "bej_SD",    
    "bem_ZM",    
    "bg_BG",     
    "bho_IN",    
    "bi_VU",     
    "bik_PH",    
    "bin_NG",    
    "bm_ML",     
    "bn_IN",     
    "bo_CN",     
    "br_FR",     
    "bs_BA",     
    "bug_ID",    
    "ca_ES",     
    "ce_RU",     
    "ceb_PH",    
    "co_FR",     
    "cr_CA",     
     
    "cs_CZ",     
    "csb_PL",    
    "cy_GB",     
    "da_DK",     
    "de_DE",     
    "din_SD",    
    "doi_IN",    
    "dsb_DE",    
    "dv_MV",     
    "dz_BT",     
    "ee_GH",     
    "el_GR",     
     
    "es_ES",     
    "et_EE",     
    "fa_IR",     
    "fi_FI",     
    "fil_PH",    
    "fj_FJ",     
    "fo_FO",     
    "fon_BJ",    
    "fr_FR",     
    "fur_IT",    
    "fy_NL",     
    "ga_IE",     
    "gd_GB",     
    "gon_IN",    
    "gsw_CH",    
    "gu_IN",     
    "he_IL",     
    "hi_IN",     
    "hil_PH",    
    "hr_HR",     
    "hsb_DE",    
    "ht_HT",     
    "hu_HU",     
    "hy_AM",     
    "id_ID",     
    "ig_NG",     
    "ii_CN",     
    "ilo_PH",    
    "is_IS",     
    "it_IT",     
    "ja_JP",     
    "jab_NG",    
    "jv_ID",     
    "ka_GE",     
    "kab_DZ",    
    "kaj_NG",    
    "kam_KE",    
    "kmb_AO",    
    "kcg_NG",    
    "kdm_NG",    
    "kg_CD",     
    "kk_KZ",     
    "kl_GL",     
    "km_KH",     
    "kn_IN",     
    "ko_KR",     
    "kok_IN",    
    "kr_NG",     
    "kru_IN",    
    "ky_KG",     
    "lg_UG",     
    "li_BE",     
    "lo_LA",     
    "lt_LT",     
    "lu_CD",     
    "lua_CD",    
    "luo_KE",    
    "lv_LV",     
    "mad_ID",    
    "mag_IN",    
    "mai_IN",    
    "mak_ID",    
    "man_ML",    
    "men_SL",    
    "mfe_MU",    
    "mg_MG",     
    "mi_NZ",     
    "min_ID",    
    "mk_MK",     
    "ml_IN",     
    "mn_MN",     
    "mni_IN",    
    "mos_BF",    
    "mr_IN",     
    "ms_MY",     
    "mt_MT",     
    "mwr_IN",    
    "my_MM",     
    "na_NR",     
    "nah_MX",    
    "nap_IT",    
    "nb_NO",     
    "nds_DE",    
    "ne_NP",     
    "nl_NL",     
    "nn_NO",     
    "no_NO",     
    "nr_ZA",     
    "nso_ZA",    
    "ny_MW",     
    "nym_TZ",    
    "nyn_UG",    
    "oc_FR",     
    "oj_CA",     
    "or_IN",     
    "pa_IN",     
    "pag_PH",    
    "pam_PH",    
    "pap_AN",    
    "pbb_CO",    
    "pl_PL",     
    "ps_AF",     
    "pt_PT",     
    "raj_IN",    
    "rm_CH",     
    "rn_BI",     
    "ro_RO",     
    "ru_RU",     
    "rw_RW",     
    "sa_IN",     
    "sah_RU",    
    "sas_ID",    
    "sat_IN",    
    "sc_IT",     
    "scn_IT",    
    "sg_CF",     
    "shn_MM",    
    "si_LK",     
    "sid_ET",    
    "sk_SK",     
    "sl_SI",     
    "sm_WS",     
    "smn_FI",    
    "sms_FI",    
    "so_SO",     
    "sq_AL",     
    "sr_RS",     
    "srr_SN",    
    "suk_TZ",    
    "sus_GN",    
    "sv_SE",     
    "te_IN",     
    "tem_SL",    
    "tet_ID",    
    "tg_TJ",     
    "th_TH",     
    "ti_ER",     
    "tiv_NG",    
    "tk_TM",     
    "tl_PH",     
    "to_TO",     
    "tpi_PG",    
    "tr_TR",     
    "tum_MW",    
    "ug_CN",     
    "uk_UA",     
    "umb_AO",    
    "ur_PK",     
    "uz_UZ",     
    "ve_ZA",     
    "vi_VN",     
    "wa_BE",     
    "wal_ET",    
    "war_PH",    
    "wen_DE",    
    "yao_MW",    
    "zap_MX"     
  };

 
static int
langcmp (const char *locale1, const char *locale2)
{
  size_t locale1_len;
  size_t locale2_len;
  int cmp;

  {
    const char *locale1_end = strchr (locale1, '_');
    if (locale1_end != NULL)
      locale1_len = locale1_end - locale1;
    else
      locale1_len = strlen (locale1);
  }
  {
    const char *locale2_end = strchr (locale2, '_');
    if (locale2_end != NULL)
      locale2_len = locale2_end - locale2;
    else
      locale2_len = strlen (locale2);
  }

  if (locale1_len < locale2_len)
    {
      cmp = memcmp (locale1, locale2, locale1_len);
      if (cmp == 0)
        cmp = -1;
    }
  else
    {
      cmp = memcmp (locale1, locale2, locale2_len);
      if (locale1_len > locale2_len && cmp == 0)
        cmp = 1;
    }

  return cmp;
}

 
static const char *
get_main_locale_with_same_language (const char *locale)
{
#   define table locales_with_principal_territory
   
  size_t hi = sizeof (table) / sizeof (table[0]);
  size_t lo = 0;
  while (lo < hi)
    {
       
      size_t mid = (hi + lo) >> 1;  
      int cmp = langcmp (table[mid], locale);
      if (cmp < 0)
        lo = mid + 1;
      else if (cmp > 0)
        hi = mid;
      else
        {
           
          if (mid > lo && langcmp (table[mid - 1], locale) >= 0)
            abort ();
          if (mid + 1 < hi && langcmp (table[mid + 1], locale) <= 0)
            abort ();
          return table[mid];
        }
    }
#   undef table
  return NULL;
}

 
static char const locales_with_principal_language[][6 + 1] =
  {
     
     
               
    "ca_AD",     
    "ar_AE",     
    "ps_AF",     
    "en_AG",     
    "sq_AL",     
    "hy_AM",     
    "pap_AN",    
    "pt_AO",     
    "es_AR",     
    "de_AT",     
    "en_AU",     
     
    "az_AZ",     
    "bs_BA",     
    "bn_BD",     
    "nl_BE",     
    "fr_BF",     
    "bg_BG",     
    "ar_BH",     
    "rn_BI",     
    "fr_BJ",     
    "es_BO",     
    "pt_BR",     
    "dz_BT",     
    "en_BW",     
    "be_BY",     
    "en_CA",     
    "fr_CD",     
    "sg_CF",     
    "de_CH",     
    "es_CL",     
    "zh_CN",     
    "es_CO",     
    "es_CR",     
    "es_CU",     
     
    "el_CY",     
    "cs_CZ",     
    "de_DE",     
     
    "da_DK",     
    "es_DO",     
    "ar_DZ",     
    "es_EC",     
    "et_EE",     
    "ar_EG",     
    "ti_ER",     
    "es_ES",     
    "am_ET",     
    "fi_FI",     
     
    "fo_FO",     
    "fr_FR",     
    "en_GB",     
    "ka_GE",     
    "en_GH",     
    "kl_GL",     
    "fr_GN",     
    "el_GR",     
    "es_GT",     
    "zh_HK",     
    "es_HN",     
    "hr_HR",     
    "ht_HT",     
    "hu_HU",     
    "id_ID",     
    "en_IE",     
    "he_IL",     
    "hi_IN",     
    "ar_IQ",     
    "fa_IR",     
    "is_IS",     
    "it_IT",     
    "ar_JO",     
    "ja_JP",     
    "sw_KE",     
    "ky_KG",     
    "km_KH",     
    "ko_KR",     
    "ar_KW",     
    "kk_KZ",     
    "lo_LA",     
    "ar_LB",     
    "de_LI",     
    "si_LK",     
    "lt_LT",     
     
    "lv_LV",     
    "ar_LY",     
    "ar_MA",     
    "sr_ME",     
    "mg_MG",     
    "mk_MK",     
    "fr_ML",     
    "my_MM",     
    "mn_MN",     
    "mt_MT",     
    "mfe_MU",    
    "dv_MV",     
    "ny_MW",     
    "es_MX",     
    "ms_MY",     
    "en_NG",     
    "es_NI",     
    "nl_NL",     
    "no_NO",     
    "ne_NP",     
    "na_NR",     
    "niu_NU",    
    "en_NZ",     
    "ar_OM",     
    "es_PA",     
    "es_PE",     
    "tpi_PG",    
    "fil_PH",    
    "pa_PK",     
    "pl_PL",     
    "es_PR",     
    "pt_PT",     
    "es_PY",     
    "ar_QA",     
    "ro_RO",     
    "sr_RS",     
    "ru_RU",     
    "rw_RW",     
    "ar_SA",     
    "en_SC",     
    "ar_SD",     
    "sv_SE",     
    "en_SG",     
    "sl_SI",     
    "sk_SK",     
    "en_SL",     
    "fr_SN",     
    "so_SO",     
    "ar_SS",     
    "es_SV",     
    "ar_SY",     
    "th_TH",     
    "tg_TJ",     
    "tk_TM",     
    "ar_TN",     
    "to_TO",     
    "tr_TR",     
    "zh_TW",     
    "sw_TZ",     
    "uk_UA",     
    "lg_UG",     
    "en_US",     
    "es_UY",     
    "uz_UZ",     
    "es_VE",     
    "vi_VN",     
    "bi_VU",     
    "sm_WS",     
    "ar_YE",     
    "en_ZA",     
    "en_ZM",     
    "en_ZW"      
  };

 
static int
terrcmp (const char *locale1, const char *locale2)
{
  const char *territory1 = strrchr (locale1, '_') + 1;
  const char *territory2 = strrchr (locale2, '_') + 1;

  return strcmp (territory1, territory2);
}

 
static const char *
get_main_locale_with_same_territory (const char *locale)
{
  if (strrchr (locale, '_') != NULL)
    {
#   define table locales_with_principal_language
       
      size_t hi = sizeof (table) / sizeof (table[0]);
      size_t lo = 0;
      while (lo < hi)
        {
           
          size_t mid = (hi + lo) >> 1;  
          int cmp = terrcmp (table[mid], locale);
          if (cmp < 0)
            lo = mid + 1;
          else if (cmp > 0)
            hi = mid;
          else
            {
               
              if (mid > lo && terrcmp (table[mid - 1], locale) >= 0)
                abort ();
              if (mid + 1 < hi && terrcmp (table[mid + 1], locale) <= 0)
                abort ();
              return table[mid];
            }
        }
#   undef table
    }
  return NULL;
}

#  endif

char *
setlocale_improved (int category, const char *locale)
{
  if (locale != NULL && locale[0] == '\0')
    {
       
      if (category == LC_ALL)
        {
           
          static int const categories[] =
            {
              LC_CTYPE,
              LC_NUMERIC,
              LC_TIME,
              LC_COLLATE,
              LC_MONETARY,
              LC_MESSAGES
            };
          char *saved_locale;
          const char *base_name;
          unsigned int i;

           
          saved_locale = setlocale (LC_ALL, NULL);
          if (saved_locale == NULL)
            return NULL;
          saved_locale = strdup (saved_locale);
          if (saved_locale == NULL)
            return NULL;

           
          base_name =
            gl_locale_name_environ (LC_CTYPE, category_to_name (LC_CTYPE));
          if (base_name == NULL)
            base_name = gl_locale_name_default ();

          if (setlocale_unixlike (LC_ALL, base_name) != NULL)
            {
               
              i = 1;
            }
          else
            {
               
              base_name = "C";
              if (setlocale_unixlike (LC_ALL, base_name) == NULL)
                goto fail;
              i = 0;
            }
#  if defined _WIN32 && ! defined __CYGWIN__
           
          if (strchr (base_name, '.') != NULL
              && strcmp (setlocale (LC_CTYPE, NULL), "C") == 0)
            goto fail;
#  endif

          for (; i < sizeof (categories) / sizeof (categories[0]); i++)
            {
              int cat = categories[i];
              const char *name;

              name = gl_locale_name_environ (cat, category_to_name (cat));
              if (name == NULL)
                name = gl_locale_name_default ();

               
              if (strcmp (name, base_name) != 0
#  if LC_MESSAGES == 1729
                  || cat == LC_MESSAGES
#  endif
                 )
                if (setlocale_single (cat, name) == NULL)
#  if defined __APPLE__ && defined __MACH__
                  {
                     
                    int warn = 0;

                    if (cat == LC_CTYPE)
                      warn = (setlocale_single (cat, "UTF-8") == NULL);
                    else if (cat == LC_MESSAGES)
                      {
#   if HAVE_CFLOCALECOPYPREFERREDLANGUAGES || HAVE_CFPREFERENCESCOPYAPPVALUE  
                         
#    if HAVE_CFLOCALECOPYPREFERREDLANGUAGES  
                        CFArrayRef prefArray = CFLocaleCopyPreferredLanguages ();
#    elif HAVE_CFPREFERENCESCOPYAPPVALUE  
                        CFTypeRef preferences =
                          CFPreferencesCopyAppValue (CFSTR ("AppleLanguages"),
                                                     kCFPreferencesCurrentApplication);
                        if (preferences != NULL
                            && CFGetTypeID (preferences) == CFArrayGetTypeID ())
                          {
                            CFArrayRef prefArray = (CFArrayRef)preferences;
#    endif
                            int n = CFArrayGetCount (prefArray);
                            if (n > 0)
                              {
                                char buf[256];
                                CFTypeRef element = CFArrayGetValueAtIndex (prefArray, 0);
                                if (element != NULL
                                    && CFGetTypeID (element) == CFStringGetTypeID ()
                                    && CFStringGetCString ((CFStringRef)element,
                                                           buf, sizeof (buf),
                                                           kCFStringEncodingASCII))
                                  {
                                     
                                    char *last_minus = strrchr (buf, '-');
                                    if (last_minus != NULL)
                                      *last_minus = '\0';

                                     
                                    gl_locale_name_canonicalize (buf);

                                     
                                    if (setlocale_single (cat, buf) == NULL)
                                      {
                                        const char *last_try =
                                          get_main_locale_with_same_language (buf);

                                        if (last_try == NULL
                                            || setlocale_single (cat, last_try) == NULL)
                                          warn = 1;
                                      }
                                  }
                              }
#    if HAVE_CFLOCALECOPYPREFERREDLANGUAGES  
                        CFRelease (prefArray);
#    elif HAVE_CFPREFERENCESCOPYAPPVALUE  
                          }
#    endif
#   else
                        const char *last_try =
                          get_main_locale_with_same_language (name);

                        if (last_try == NULL
                            || setlocale_single (cat, last_try) == NULL)
                          warn = 1;
#   endif
                      }
                    else
                      {
                         
                        const char *last_try =
                          get_main_locale_with_same_territory (name);

                        if (last_try == NULL
                            || setlocale_single (cat, last_try) == NULL)
                          warn = 1;
                      }

                    if (warn)
                      {
                         
                        const char *verbose = getenv ("SETLOCALE_VERBOSE");
                        if (verbose != NULL && verbose[0] != '\0')
                          fprintf (stderr,
                                   "Warning: Failed to set locale category %s to %s.\n",
                                   category_to_name (cat), name);
                      }
                  }
#  else
                  goto fail;
#  endif
            }

           
          free (saved_locale);
          return setlocale (LC_ALL, NULL);

        fail:
          if (saved_locale[0] != '\0')  
            setlocale (LC_ALL, saved_locale);
          free (saved_locale);
          return NULL;
        }
      else
        {
          const char *name =
            gl_locale_name_environ (category, category_to_name (category));
          if (name == NULL)
            name = gl_locale_name_default ();

          return setlocale_single (category, name);
        }
    }
  else
    {
#  if defined _WIN32 && ! defined __CYGWIN__
      if (category == LC_ALL && locale != NULL && strchr (locale, '.') != NULL)
        {
          char *saved_locale;

           
          saved_locale = setlocale (LC_ALL, NULL);
          if (saved_locale == NULL)
            return NULL;
          saved_locale = strdup (saved_locale);
          if (saved_locale == NULL)
            return NULL;

          if (setlocale_unixlike (LC_ALL, locale) == NULL)
            {
              free (saved_locale);
              return NULL;
            }

           
          if (strcmp (setlocale (LC_CTYPE, NULL), "C") == 0)
            {
              if (saved_locale[0] != '\0')  
                setlocale (LC_ALL, saved_locale);
              free (saved_locale);
              return NULL;
            }

           
          free (saved_locale);
          return setlocale (LC_ALL, NULL);
        }
      else
#  endif
        return setlocale_single (category, locale);
    }
}

# endif  

#endif
