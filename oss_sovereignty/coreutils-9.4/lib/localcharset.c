 

 

#include <config.h>

 
#include "localcharset.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined __APPLE__ && defined __MACH__ && HAVE_LANGINFO_CODESET
# define DARWIN7  
#endif

#if defined _WIN32 && !defined __CYGWIN__
# define WINDOWS_NATIVE
# include <locale.h>
#endif

#if defined __EMX__
 
# ifndef OS2
#  define OS2
# endif
#endif

#if !defined WINDOWS_NATIVE
# if HAVE_LANGINFO_CODESET
#  include <langinfo.h>
# else
#  if 0  
#   include <locale.h>
#  endif
# endif
# ifdef __CYGWIN__
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
# endif
#elif defined WINDOWS_NATIVE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
   
# undef setlocale
#endif
#if defined OS2
# define INCL_DOS
# include <os2.h>
#endif

 
#if defined DARWIN7
# include <xlocale.h>
#endif


#if HAVE_LANGINFO_CODESET || defined WINDOWS_NATIVE || defined OS2

 

 
# if !((defined __GNU_LIBRARY__ && __GLIBC__ >= 2) || defined __UCLIBC__)

struct table_entry
{
  const char alias[11+1];
  const char canonical[11+1];
};

 
static const struct table_entry alias_table[] =
  {
#  if defined __FreeBSD__                                    
   
    { "Big5",       "BIG5" },
    { "C",          "ASCII" },
   
   
   
   
   
   
   
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-13", "ISO-8859-13" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-7",  "ISO-8859-7" },
    { "ISO8859-9",  "ISO-8859-9" },
   
   
    { "SJIS",       "SHIFT_JIS" },
    { "US-ASCII",   "ASCII" },
    { "eucCN",      "GB2312" },
    { "eucJP",      "EUC-JP" },
    { "eucKR",      "EUC-KR" }
#   define alias_table_defined
#  endif
#  if defined __NetBSD__                                     
    { "646",        "ASCII" },
   
   
    { "Big5-HKSCS", "BIG5-HKSCS" },
   
   
   
   
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-13", "ISO-8859-13" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-4",  "ISO-8859-4" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-7",  "ISO-8859-7" },
   
   
   
    { "SJIS",       "SHIFT_JIS" },
    { "eucCN",      "GB2312" },
    { "eucJP",      "EUC-JP" },
    { "eucKR",      "EUC-KR" },
    { "eucTW",      "EUC-TW" }
#   define alias_table_defined
#  endif
#  if defined __OpenBSD__                                    
    { "646",        "ASCII" },
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-13", "ISO-8859-13" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-4",  "ISO-8859-4" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-7",  "ISO-8859-7" },
    { "US-ASCII",   "ASCII" }
#   define alias_table_defined
#  endif
#  if defined __APPLE__ && defined __MACH__                  
     
    { "ARMSCII-8",  "ARMSCII-8" },
    { "Big5",       "BIG5" },
    { "Big5HKSCS",  "BIG5-HKSCS" },
    { "CP1131",     "CP1131" },
    { "CP1251",     "CP1251" },
    { "CP866",      "CP866" },
    { "CP949",      "CP949" },
    { "GB18030",    "GB18030" },
    { "GB2312",     "GB2312" },
    { "GBK",        "GBK" },
   
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-13", "ISO-8859-13" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-4",  "ISO-8859-4" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-7",  "ISO-8859-7" },
    { "ISO8859-9",  "ISO-8859-9" },
    { "KOI8-R",     "KOI8-R" },
    { "KOI8-U",     "KOI8-U" },
    { "PT154",      "PT154" },
    { "SJIS",       "SHIFT_JIS" },
    { "eucCN",      "GB2312" },
    { "eucJP",      "EUC-JP" },
    { "eucKR",      "EUC-KR" }
#   define alias_table_defined
#  endif
#  if defined _AIX                                           
   
    { "IBM-1046",   "CP1046" },
    { "IBM-1124",   "CP1124" },
    { "IBM-1129",   "CP1129" },
    { "IBM-1252",   "CP1252" },
    { "IBM-850",    "CP850" },
    { "IBM-856",    "CP856" },
    { "IBM-921",    "ISO-8859-13" },
    { "IBM-922",    "CP922" },
    { "IBM-932",    "CP932" },
    { "IBM-943",    "CP943" },
    { "IBM-eucCN",  "GB2312" },
    { "IBM-eucJP",  "EUC-JP" },
    { "IBM-eucKR",  "EUC-KR" },
    { "IBM-eucTW",  "EUC-TW" },
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-6",  "ISO-8859-6" },
    { "ISO8859-7",  "ISO-8859-7" },
    { "ISO8859-8",  "ISO-8859-8" },
    { "ISO8859-9",  "ISO-8859-9" },
    { "TIS-620",    "TIS-620" },
   
    { "big5",       "BIG5" }
#   define alias_table_defined
#  endif
#  if defined __hpux                                         
    { "SJIS",      "SHIFT_JIS" },
    { "arabic8",   "HP-ARABIC8" },
    { "big5",      "BIG5" },
    { "cp1251",    "CP1251" },
    { "eucJP",     "EUC-JP" },
    { "eucKR",     "EUC-KR" },
    { "eucTW",     "EUC-TW" },
    { "gb18030",   "GB18030" },
    { "greek8",    "HP-GREEK8" },
    { "hebrew8",   "HP-HEBREW8" },
    { "hkbig5",    "BIG5-HKSCS" },
    { "hp15CN",    "GB2312" },
    { "iso88591",  "ISO-8859-1" },
    { "iso885913", "ISO-8859-13" },
    { "iso885915", "ISO-8859-15" },
    { "iso88592",  "ISO-8859-2" },
    { "iso88594",  "ISO-8859-4" },
    { "iso88595",  "ISO-8859-5" },
    { "iso88596",  "ISO-8859-6" },
    { "iso88597",  "ISO-8859-7" },
    { "iso88598",  "ISO-8859-8" },
    { "iso88599",  "ISO-8859-9" },
    { "kana8",     "HP-KANA8" },
    { "koi8r",     "KOI8-R" },
    { "roman8",    "HP-ROMAN8" },
    { "tis620",    "TIS-620" },
    { "turkish8",  "HP-TURKISH8" },
    { "utf8",      "UTF-8" }
#   define alias_table_defined
#  endif
#  if defined __sgi                                          
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-7",  "ISO-8859-7" },
    { "ISO8859-9",  "ISO-8859-9" },
    { "eucCN",      "GB2312" },
    { "eucJP",      "EUC-JP" },
    { "eucKR",      "EUC-KR" },
    { "eucTW",      "EUC-TW" }
#   define alias_table_defined
#  endif
#  if defined __osf__                                        
   
    { "ISO8859-1",  "ISO-8859-1" },
    { "ISO8859-15", "ISO-8859-15" },
    { "ISO8859-2",  "ISO-8859-2" },
    { "ISO8859-4",  "ISO-8859-4" },
    { "ISO8859-5",  "ISO-8859-5" },
    { "ISO8859-7",  "ISO-8859-7" },
    { "ISO8859-8",  "ISO-8859-8" },
    { "ISO8859-9",  "ISO-8859-9" },
    { "KSC5601",    "CP949" },
    { "SJIS",       "SHIFT_JIS" },
    { "TACTIS",     "TIS-620" },
   
    { "big5",       "BIG5" },
    { "cp850",      "CP850" },
    { "dechanyu",   "DEC-HANYU" },
    { "dechanzi",   "GB2312" },
    { "deckanji",   "DEC-KANJI" },
    { "deckorean",  "EUC-KR" },
    { "eucJP",      "EUC-JP" },
    { "eucKR",      "EUC-KR" },
    { "eucTW",      "EUC-TW" },
    { "sdeckanji",  "EUC-JP" }
#   define alias_table_defined
#  endif
#  if defined __sun                                          
    { "5601",        "EUC-KR" },
    { "646",         "ASCII" },
   
    { "Big5-HKSCS",  "BIG5-HKSCS" },
    { "GB18030",     "GB18030" },
   
    { "ISO8859-1",   "ISO-8859-1" },
    { "ISO8859-11",  "TIS-620" },
    { "ISO8859-13",  "ISO-8859-13" },
    { "ISO8859-15",  "ISO-8859-15" },
    { "ISO8859-2",   "ISO-8859-2" },
    { "ISO8859-3",   "ISO-8859-3" },
    { "ISO8859-4",   "ISO-8859-4" },
    { "ISO8859-5",   "ISO-8859-5" },
    { "ISO8859-6",   "ISO-8859-6" },
    { "ISO8859-7",   "ISO-8859-7" },
    { "ISO8859-8",   "ISO-8859-8" },
    { "ISO8859-9",   "ISO-8859-9" },
    { "PCK",         "SHIFT_JIS" },
    { "TIS620.2533", "TIS-620" },
   
    { "ansi-1251",   "CP1251" },
    { "cns11643",    "EUC-TW" },
    { "eucJP",       "EUC-JP" },
    { "gb2312",      "GB2312" },
    { "koi8-r",      "KOI8-R" }
#   define alias_table_defined
#  endif
#  if defined __minix                                        
    { "646", "ASCII" }
#   define alias_table_defined
#  endif
#  if defined WINDOWS_NATIVE || defined __CYGWIN__           
    { "CP1361",  "JOHAB" },
    { "CP20127", "ASCII" },
    { "CP20866", "KOI8-R" },
    { "CP20936", "GB2312" },
    { "CP21866", "KOI8-RU" },
    { "CP28591", "ISO-8859-1" },
    { "CP28592", "ISO-8859-2" },
    { "CP28593", "ISO-8859-3" },
    { "CP28594", "ISO-8859-4" },
    { "CP28595", "ISO-8859-5" },
    { "CP28596", "ISO-8859-6" },
    { "CP28597", "ISO-8859-7" },
    { "CP28598", "ISO-8859-8" },
    { "CP28599", "ISO-8859-9" },
    { "CP28605", "ISO-8859-15" },
    { "CP38598", "ISO-8859-8" },
    { "CP51932", "EUC-JP" },
    { "CP51936", "GB2312" },
    { "CP51949", "EUC-KR" },
    { "CP51950", "EUC-TW" },
    { "CP54936", "GB18030" },
    { "CP65001", "UTF-8" },
    { "CP936",   "GBK" }
#   define alias_table_defined
#  endif
#  if defined OS2                                            
     
   
    { "CP1089",        "ISO-8859-6" },
   
   
    { "CP1208",        "UTF-8" },
   
    { "CP1381",        "GB2312" },
    { "CP1383",        "GB2312" },
    { "CP1386",        "GBK" },
   
    { "CP3372",        "EUC-JP" },
    { "CP4946",        "CP850" },
   
   
   
    { "CP813",         "ISO-8859-7" },
    { "CP819",         "ISO-8859-1" },
    { "CP878",         "KOI8-R" },
   
    { "CP912",         "ISO-8859-2" },
    { "CP913",         "ISO-8859-3" },
    { "CP914",         "ISO-8859-4" },
    { "CP915",         "ISO-8859-5" },
    { "CP916",         "ISO-8859-8" },
    { "CP920",         "ISO-8859-9" },
    { "CP921",         "ISO-8859-13" },
    { "CP923",         "ISO-8859-15" },
   
   
   
   
   
    { "CP954",         "EUC-JP" },
    { "CP964",         "EUC-TW" },
    { "CP970",         "EUC-KR" },
   
    { "IBM-1004",      "CP1252" },
   
   
   
   
   
    { "IBM-1089",      "ISO-8859-6" },
   
   
   
   
   
   
   
    { "IBM-1124",      "CP1124" },
    { "IBM-1125",      "CP1125" },
    { "IBM-1131",      "CP1131" },
    { "IBM-1208",      "UTF-8" },
    { "IBM-1250",      "CP1250" },
    { "IBM-1251",      "CP1251" },
    { "IBM-1252",      "CP1252" },
    { "IBM-1253",      "CP1253" },
    { "IBM-1254",      "CP1254" },
    { "IBM-1255",      "CP1255" },
    { "IBM-1256",      "CP1256" },
    { "IBM-1257",      "CP1257" },
   
   
   
   
   
   
   
   
    { "IBM-1381",      "GB2312" },
    { "IBM-1383",      "GB2312" },
    { "IBM-1386",      "GBK" },
   
    { "IBM-3372",      "EUC-JP" },
    { "IBM-367",       "ASCII" },
    { "IBM-437",       "CP437" },
    { "IBM-4946",      "CP850" },
   
   
   
    { "IBM-813",       "ISO-8859-7" },
    { "IBM-819",       "ISO-8859-1" },
    { "IBM-850",       "CP850" },
   
    { "IBM-852",       "CP852" },
    { "IBM-855",       "CP855" },
    { "IBM-856",       "CP856" },
    { "IBM-857",       "CP857" },
   
    { "IBM-860",       "CP860" },
    { "IBM-861",       "CP861" },
    { "IBM-862",       "CP862" },
    { "IBM-863",       "CP863" },
    { "IBM-864",       "CP864" },
    { "IBM-865",       "CP865" },
    { "IBM-866",       "CP866" },
   
    { "IBM-869",       "CP869" },
    { "IBM-874",       "CP874" },
    { "IBM-878",       "KOI8-R" },
   
   
   
   
    { "IBM-912",       "ISO-8859-2" },
    { "IBM-913",       "ISO-8859-3" },
    { "IBM-914",       "ISO-8859-4" },
    { "IBM-915",       "ISO-8859-5" },
    { "IBM-916",       "ISO-8859-8" },
    { "IBM-920",       "ISO-8859-9" },
    { "IBM-921",       "ISO-8859-13" },
    { "IBM-922",       "CP922" },
    { "IBM-923",       "ISO-8859-15" },
    { "IBM-932",       "CP932" },
   
   
    { "IBM-943",       "CP943" },
   
    { "IBM-949",       "CP949" },
    { "IBM-950",       "CP950" },
   
   
   
    { "IBM-954",       "EUC-JP" },
   
    { "IBM-964",       "EUC-TW" },
    { "IBM-970",       "EUC-KR" },
   
    { "IBM-eucCN",     "GB2312" },
    { "IBM-eucJP",     "EUC-JP" },
    { "IBM-eucKR",     "EUC-KR" },
    { "IBM-eucTW",     "EUC-TW" },
    { "IBM33722",      "EUC-JP" },
    { "ISO8859-1",     "ISO-8859-1" },
    { "ISO8859-2",     "ISO-8859-2" },
    { "ISO8859-3",     "ISO-8859-3" },
    { "ISO8859-4",     "ISO-8859-4" },
    { "ISO8859-5",     "ISO-8859-5" },
    { "ISO8859-6",     "ISO-8859-6" },
    { "ISO8859-7",     "ISO-8859-7" },
    { "ISO8859-8",     "ISO-8859-8" },
    { "ISO8859-9",     "ISO-8859-9" },
   
   
   
   
   
   
    { "SJIS-1",        "CP943" },
    { "SJIS-2",        "CP943" },
    { "eucJP",         "EUC-JP" },
    { "eucKR",         "EUC-KR" },
    { "eucTW-1993",    "EUC-TW" }
#   define alias_table_defined
#  endif
#  if defined VMS                                            
     
    { "DECHANYU",  "DEC-HANYU" },
    { "DECHANZI",  "GB2312" },
    { "DECKANJI",  "DEC-KANJI" },
    { "DECKOREAN", "EUC-KR" },
    { "ISO8859-1", "ISO-8859-1" },
    { "ISO8859-2", "ISO-8859-2" },
    { "ISO8859-5", "ISO-8859-5" },
    { "ISO8859-7", "ISO-8859-7" },
    { "ISO8859-8", "ISO-8859-8" },
    { "ISO8859-9", "ISO-8859-9" },
    { "SDECKANJI", "EUC-JP" },
    { "SJIS",      "SHIFT_JIS" },
    { "eucJP",     "EUC-JP" },
    { "eucTW",     "EUC-TW" }
#   define alias_table_defined
#  endif
#  ifndef alias_table_defined
     
    { "", "" }
#  endif
  };

# endif

#else

 

struct table_entry
{
  const char locale[17+1];
  const char canonical[11+1];
};

 
static const struct table_entry locale_table[] =
  {
# if defined __FreeBSD__                                     
    { "cs_CZ.ISO_8859-2",  "ISO-8859-2" },
    { "da_DK.DIS_8859-15", "ISO-8859-15" },
    { "da_DK.ISO_8859-1",  "ISO-8859-1" },
    { "de_AT.DIS_8859-15", "ISO-8859-15" },
    { "de_AT.ISO_8859-1",  "ISO-8859-1" },
    { "de_CH.DIS_8859-15", "ISO-8859-15" },
    { "de_CH.ISO_8859-1",  "ISO-8859-1" },
    { "de_DE.DIS_8859-15", "ISO-8859-15" },
    { "de_DE.ISO_8859-1",  "ISO-8859-1" },
    { "en_AU.DIS_8859-15", "ISO-8859-15" },
    { "en_AU.ISO_8859-1",  "ISO-8859-1" },
    { "en_CA.DIS_8859-15", "ISO-8859-15" },
    { "en_CA.ISO_8859-1",  "ISO-8859-1" },
    { "en_GB.DIS_8859-15", "ISO-8859-15" },
    { "en_GB.ISO_8859-1",  "ISO-8859-1" },
    { "en_US.DIS_8859-15", "ISO-8859-15" },
    { "en_US.ISO_8859-1",  "ISO-8859-1" },
    { "es_ES.DIS_8859-15", "ISO-8859-15" },
    { "es_ES.ISO_8859-1",  "ISO-8859-1" },
    { "fi_FI.DIS_8859-15", "ISO-8859-15" },
    { "fi_FI.ISO_8859-1",  "ISO-8859-1" },
    { "fr_BE.DIS_8859-15", "ISO-8859-15" },
    { "fr_BE.ISO_8859-1",  "ISO-8859-1" },
    { "fr_CA.DIS_8859-15", "ISO-8859-15" },
    { "fr_CA.ISO_8859-1",  "ISO-8859-1" },
    { "fr_CH.DIS_8859-15", "ISO-8859-15" },
    { "fr_CH.ISO_8859-1",  "ISO-8859-1" },
    { "fr_FR.DIS_8859-15", "ISO-8859-15" },
    { "fr_FR.ISO_8859-1",  "ISO-8859-1" },
    { "hr_HR.ISO_8859-2",  "ISO-8859-2" },
    { "hu_HU.ISO_8859-2",  "ISO-8859-2" },
    { "is_IS.DIS_8859-15", "ISO-8859-15" },
    { "is_IS.ISO_8859-1",  "ISO-8859-1" },
    { "it_CH.DIS_8859-15", "ISO-8859-15" },
    { "it_CH.ISO_8859-1",  "ISO-8859-1" },
    { "it_IT.DIS_8859-15", "ISO-8859-15" },
    { "it_IT.ISO_8859-1",  "ISO-8859-1" },
    { "ja_JP.EUC",         "EUC-JP" },
    { "ja_JP.SJIS",        "SHIFT_JIS" },
    { "ja_JP.Shift_JIS",   "SHIFT_JIS" },
    { "ko_KR.EUC",         "EUC-KR" },
    { "la_LN.ASCII",       "ASCII" },
    { "la_LN.DIS_8859-15", "ISO-8859-15" },
    { "la_LN.ISO_8859-1",  "ISO-8859-1" },
    { "la_LN.ISO_8859-2",  "ISO-8859-2" },
    { "la_LN.ISO_8859-4",  "ISO-8859-4" },
    { "lt_LN.ASCII",       "ASCII" },
    { "lt_LN.DIS_8859-15", "ISO-8859-15" },
    { "lt_LN.ISO_8859-1",  "ISO-8859-1" },
    { "lt_LN.ISO_8859-2",  "ISO-8859-2" },
    { "lt_LT.ISO_8859-4",  "ISO-8859-4" },
    { "nl_BE.DIS_8859-15", "ISO-8859-15" },
    { "nl_BE.ISO_8859-1",  "ISO-8859-1" },
    { "nl_NL.DIS_8859-15", "ISO-8859-15" },
    { "nl_NL.ISO_8859-1",  "ISO-8859-1" },
    { "no_NO.DIS_8859-15", "ISO-8859-15" },
    { "no_NO.ISO_8859-1",  "ISO-8859-1" },
    { "pl_PL.ISO_8859-2",  "ISO-8859-2" },
    { "pt_PT.DIS_8859-15", "ISO-8859-15" },
    { "pt_PT.ISO_8859-1",  "ISO-8859-1" },
    { "ru_RU.CP866",       "CP866" },
    { "ru_RU.ISO_8859-5",  "ISO-8859-5" },
    { "ru_RU.KOI8-R",      "KOI8-R" },
    { "ru_SU.CP866",       "CP866" },
    { "ru_SU.ISO_8859-5",  "ISO-8859-5" },
    { "ru_SU.KOI8-R",      "KOI8-R" },
    { "sl_SI.ISO_8859-2",  "ISO-8859-2" },
    { "sv_SE.DIS_8859-15", "ISO-8859-15" },
    { "sv_SE.ISO_8859-1",  "ISO-8859-1" },
    { "uk_UA.KOI8-U",      "KOI8-U" },
    { "zh_CN.EUC",         "GB2312" },
    { "zh_TW.BIG5",        "BIG5" },
    { "zh_TW.Big5",        "BIG5" }
#  define locale_table_defined
# endif
# if defined __DJGPP__                                       
     
    { "C",     "ASCII" },
    { "ar",    "CP864" },
    { "ar_AE", "CP864" },
    { "ar_DZ", "CP864" },
    { "ar_EG", "CP864" },
    { "ar_IQ", "CP864" },
    { "ar_IR", "CP864" },
    { "ar_JO", "CP864" },
    { "ar_KW", "CP864" },
    { "ar_MA", "CP864" },
    { "ar_OM", "CP864" },
    { "ar_QA", "CP864" },
    { "ar_SA", "CP864" },
    { "ar_SY", "CP864" },
    { "be",    "CP866" },
    { "be_BE", "CP866" },
    { "bg",    "CP866" },  
    { "bg_BG", "CP866" },  
    { "ca",    "CP850" },
    { "ca_ES", "CP850" },
    { "cs",    "CP852" },
    { "cs_CZ", "CP852" },
    { "da",    "CP865" },  
    { "da_DK", "CP865" },  
    { "de",    "CP850" },
    { "de_AT", "CP850" },
    { "de_CH", "CP850" },
    { "de_DE", "CP850" },
    { "el",    "CP869" },
    { "el_GR", "CP869" },
    { "en",    "CP850" },
    { "en_AU", "CP850" },  
    { "en_CA", "CP850" },
    { "en_GB", "CP850" },
    { "en_NZ", "CP437" },
    { "en_US", "CP437" },
    { "en_ZA", "CP850" },  
    { "eo",    "CP850" },
    { "eo_EO", "CP850" },
    { "es",    "CP850" },
    { "es_AR", "CP850" },
    { "es_BO", "CP850" },
    { "es_CL", "CP850" },
    { "es_CO", "CP850" },
    { "es_CR", "CP850" },
    { "es_CU", "CP850" },
    { "es_DO", "CP850" },
    { "es_EC", "CP850" },
    { "es_ES", "CP850" },
    { "es_GT", "CP850" },
    { "es_HN", "CP850" },
    { "es_MX", "CP850" },
    { "es_NI", "CP850" },
    { "es_PA", "CP850" },
    { "es_PE", "CP850" },
    { "es_PY", "CP850" },
    { "es_SV", "CP850" },
    { "es_UY", "CP850" },
    { "es_VE", "CP850" },
    { "et",    "CP850" },
    { "et_EE", "CP850" },
    { "eu",    "CP850" },
    { "eu_ES", "CP850" },
    { "fi",    "CP850" },
    { "fi_FI", "CP850" },
    { "fr",    "CP850" },
    { "fr_BE", "CP850" },
    { "fr_CA", "CP850" },
    { "fr_CH", "CP850" },
    { "fr_FR", "CP850" },
    { "ga",    "CP850" },
    { "ga_IE", "CP850" },
    { "gd",    "CP850" },
    { "gd_GB", "CP850" },
    { "gl",    "CP850" },
    { "gl_ES", "CP850" },
    { "he",    "CP862" },
    { "he_IL", "CP862" },
    { "hr",    "CP852" },
    { "hr_HR", "CP852" },
    { "hu",    "CP852" },
    { "hu_HU", "CP852" },
    { "id",    "CP850" },  
    { "id_ID", "CP850" },  
    { "is",    "CP861" },  
    { "is_IS", "CP861" },  
    { "it",    "CP850" },
    { "it_CH", "CP850" },
    { "it_IT", "CP850" },
    { "ja",    "CP932" },
    { "ja_JP", "CP932" },
    { "kr",    "CP949" },  
    { "kr_KR", "CP949" },  
    { "lt",    "CP775" },
    { "lt_LT", "CP775" },
    { "lv",    "CP775" },
    { "lv_LV", "CP775" },
    { "mk",    "CP866" },  
    { "mk_MK", "CP866" },  
    { "mt",    "CP850" },
    { "mt_MT", "CP850" },
    { "nb",    "CP865" },  
    { "nb_NO", "CP865" },  
    { "nl",    "CP850" },
    { "nl_BE", "CP850" },
    { "nl_NL", "CP850" },
    { "nn",    "CP865" },  
    { "nn_NO", "CP865" },  
    { "no",    "CP865" },  
    { "no_NO", "CP865" },  
    { "pl",    "CP852" },
    { "pl_PL", "CP852" },
    { "pt",    "CP850" },
    { "pt_BR", "CP850" },
    { "pt_PT", "CP850" },
    { "ro",    "CP852" },
    { "ro_RO", "CP852" },
    { "ru",    "CP866" },
    { "ru_RU", "CP866" },
    { "sk",    "CP852" },
    { "sk_SK", "CP852" },
    { "sl",    "CP852" },
    { "sl_SI", "CP852" },
    { "sq",    "CP852" },
    { "sq_AL", "CP852" },
    { "sr",    "CP852" },  
    { "sr_CS", "CP852" },  
    { "sr_YU", "CP852" },  
    { "sv",    "CP850" },
    { "sv_SE", "CP850" },
    { "th",    "CP874" },
    { "th_TH", "CP874" },
    { "tr",    "CP857" },
    { "tr_TR", "CP857" },
    { "uk",    "CP1125" },
    { "uk_UA", "CP1125" },
    { "zh_CN", "GBK" },
    { "zh_TW", "CP950" }  
#  define locale_table_defined
# endif
# ifndef locale_table_defined
     
    { "", "" }
# endif
  };

#endif


 

#ifdef STATIC
STATIC
#endif
const char *
locale_charset (void)
{
  const char *codeset;

   

#if HAVE_LANGINFO_CODESET || defined WINDOWS_NATIVE || defined OS2

# if HAVE_LANGINFO_CODESET

   
  codeset = nl_langinfo (CODESET);

#  ifdef __CYGWIN__
   
  if (codeset != NULL && strcmp (codeset, "US-ASCII") == 0)
    {
      const char *locale;
      static char resultbuf[2 + 10 + 1];

      locale = getenv ("LC_ALL");
      if (locale == NULL || locale[0] == '\0')
        {
          locale = getenv ("LC_CTYPE");
          if (locale == NULL || locale[0] == '\0')
            locale = getenv ("LANG");
        }
      if (locale != NULL && locale[0] != '\0')
        {
           
          const char *dot = strchr (locale, '.');

          if (dot != NULL)
            {
              const char *modifier;

              dot++;
               
              modifier = strchr (dot, '@');
              if (modifier == NULL)
                return dot;
              if (modifier - dot < sizeof (resultbuf))
                {
                   
                  memcpy (resultbuf, dot, modifier - dot);
                  resultbuf [modifier - dot] = '\0';
                  return resultbuf;
                }
            }
        }

       
      {
        char buf[2 + 10 + 1];

        sprintf (buf, "CP%u", GetACP ());
        strcpy (resultbuf, buf);
        codeset = resultbuf;
      }
    }
#  endif

  if (codeset == NULL)
     
    codeset = "";

# elif defined WINDOWS_NATIVE

  char buf[2 + 10 + 1];
  static char resultbuf[2 + 10 + 1];

   
  char *current_locale = setlocale (LC_CTYPE, NULL);
  char *pdot = strrchr (current_locale, '.');

  if (pdot && 2 + strlen (pdot + 1) + 1 <= sizeof (buf))
    sprintf (buf, "CP%s", pdot + 1);
  else
    {
       
      sprintf (buf, "CP%u", GetACP ());
    }
   
  if (strcmp (buf + 2, "65001") == 0 || strcmp (buf + 2, "utf8") == 0)
    codeset = "UTF-8";
  else
    {
      strcpy (resultbuf, buf);
      codeset = resultbuf;
    }

# elif defined OS2

  const char *locale;
  static char resultbuf[2 + 10 + 1];
  ULONG cp[3];
  ULONG cplen;

  codeset = NULL;

   
  locale = getenv ("LC_ALL");
  if (locale == NULL || locale[0] == '\0')
    {
      locale = getenv ("LC_CTYPE");
      if (locale == NULL || locale[0] == '\0')
        locale = getenv ("LANG");
    }
  if (locale != NULL && locale[0] != '\0')
    {
       
      const char *dot = strchr (locale, '.');

      if (dot != NULL)
        {
          const char *modifier;

          dot++;
           
          modifier = strchr (dot, '@');
          if (modifier == NULL)
            return dot;
          if (modifier - dot < sizeof (resultbuf))
            {
               
              memcpy (resultbuf, dot, modifier - dot);
              resultbuf [modifier - dot] = '\0';
              return resultbuf;
            }
        }

       
      if (strcmp (locale, "C") == 0 || strcmp (locale, "POSIX") == 0)
        codeset = "";
    }

  if (codeset == NULL)
    {
       
      if (DosQueryCp (sizeof (cp), cp, &cplen))
        codeset = "";
      else
        {
          char buf[2 + 10 + 1];

          sprintf (buf, "CP%u", cp[0]);
          strcpy (resultbuf, buf);
          codeset = resultbuf;
        }
    }

# else

#  error "Add code for other platforms here."

# endif

   
  {
# ifdef alias_table_defined
     
#  if defined __OpenBSD__ || (defined __APPLE__ && defined __MACH__) || defined __sun || defined __CYGWIN__
    if (strcmp (codeset, "UTF-8") == 0)
      goto done_table_lookup;
    else
#  endif
      {
        const struct table_entry * const table = alias_table;
        size_t const table_size =
          sizeof (alias_table) / sizeof (struct table_entry);
         
        size_t hi = table_size;
        size_t lo = 0;
        while (lo < hi)
          {
             
            size_t mid = (hi + lo) >> 1;  
            int cmp = strcmp (table[mid].alias, codeset);
            if (cmp < 0)
              lo = mid + 1;
            else if (cmp > 0)
              hi = mid;
            else
              {
                 
                codeset = table[mid].canonical;
                goto done_table_lookup;
              }
          }
      }
    if (0)
      done_table_lookup: ;
    else
# endif
      {
         
         
# if (defined __APPLE__ && defined __MACH__) || defined __BEOS__ || defined __HAIKU__
        codeset = "UTF-8";
# else
         
        if (codeset[0] == '\0')
          codeset = "ASCII";
# endif
      }
  }

#else

   
  const char *locale = NULL;

   
# if 0
  locale = setlocale (LC_CTYPE, NULL);
# endif
  if (locale == NULL || locale[0] == '\0')
    {
      locale = getenv ("LC_ALL");
      if (locale == NULL || locale[0] == '\0')
        {
          locale = getenv ("LC_CTYPE");
          if (locale == NULL || locale[0] == '\0')
            locale = getenv ("LANG");
            if (locale == NULL)
              locale = "";
        }
    }

   
  {
# ifdef locale_table_defined
    const struct table_entry * const table = locale_table;
    size_t const table_size =
      sizeof (locale_table) / sizeof (struct table_entry);
     
    size_t hi = table_size;
    size_t lo = 0;
    while (lo < hi)
      {
         
        size_t mid = (hi + lo) >> 1;  
        int cmp = strcmp (table[mid].locale, locale);
        if (cmp < 0)
          lo = mid + 1;
        else if (cmp > 0)
          hi = mid;
        else
          {
             
            codeset = table[mid].canonical;
            goto done_table_lookup;
          }
      }
    if (0)
      done_table_lookup: ;
    else
# endif
      {
         
         
# if (defined __APPLE__ && defined __MACH__) || defined __BEOS__ || defined __HAIKU__
        codeset = "UTF-8";
# else
         
         
        codeset = "ASCII";
# endif
      }
  }

#endif

#ifdef DARWIN7
   
  if (strcmp (codeset, "UTF-8") == 0 && MB_CUR_MAX_L (uselocale (NULL)) <= 1)
    codeset = "ASCII";
#endif

  return codeset;
}
