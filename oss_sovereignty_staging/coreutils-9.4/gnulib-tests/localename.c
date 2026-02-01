 
 
 

 
#define _GL_ARG_NONNULL(params)

#include <config.h>

 
#include "localename.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include "flexmember.h"
#include "setlocale_null.h"
#include "thread-optim.h"

#if HAVE_GOOD_USELOCALE
 
# if defined __APPLE__ && defined __MACH__
#  include <xlocale.h>
# endif
# if (__GLIBC__ >= 2 && !defined __UCLIBC__) || (defined __linux__ && HAVE_LANGINFO_H) || defined __CYGWIN__
#  include <langinfo.h>
# endif
# include "glthread/lock.h"
# if defined __sun
#  if HAVE_GETLOCALENAME_L
 
extern char * getlocalename_l(int, locale_t);
#  elif HAVE_SOLARIS114_LOCALES
#   include <sys/localedef.h>
#  endif
# endif
# if HAVE_NAMELESS_LOCALES
#  include "localename-table.h"
# endif
# if defined __HAIKU__
#  include <dlfcn.h>
# endif
#endif

#if HAVE_CFPREFERENCESCOPYAPPVALUE
# include <CoreFoundation/CFString.h>
# include <CoreFoundation/CFPreferences.h>
#endif

#if defined _WIN32 && !defined __CYGWIN__
# define WINDOWS_NATIVE
# include "glthread/lock.h"
#endif

#if defined WINDOWS_NATIVE || defined __CYGWIN__  
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winnls.h>
 
 
# ifndef LANG_AFRIKAANS
# define LANG_AFRIKAANS 0x36
# endif
# ifndef LANG_ALBANIAN
# define LANG_ALBANIAN 0x1c
# endif
# ifndef LANG_ALSATIAN
# define LANG_ALSATIAN 0x84
# endif
# ifndef LANG_AMHARIC
# define LANG_AMHARIC 0x5e
# endif
# ifndef LANG_ARABIC
# define LANG_ARABIC 0x01
# endif
# ifndef LANG_ARMENIAN
# define LANG_ARMENIAN 0x2b
# endif
# ifndef LANG_ASSAMESE
# define LANG_ASSAMESE 0x4d
# endif
# ifndef LANG_AZERI
# define LANG_AZERI 0x2c
# endif
# ifndef LANG_BASHKIR
# define LANG_BASHKIR 0x6d
# endif
# ifndef LANG_BASQUE
# define LANG_BASQUE 0x2d
# endif
# ifndef LANG_BELARUSIAN
# define LANG_BELARUSIAN 0x23
# endif
# ifndef LANG_BENGALI
# define LANG_BENGALI 0x45
# endif
# ifndef LANG_BRETON
# define LANG_BRETON 0x7e
# endif
# ifndef LANG_BURMESE
# define LANG_BURMESE 0x55
# endif
# ifndef LANG_CAMBODIAN
# define LANG_CAMBODIAN 0x53
# endif
# ifndef LANG_CATALAN
# define LANG_CATALAN 0x03
# endif
# ifndef LANG_CHEROKEE
# define LANG_CHEROKEE 0x5c
# endif
# ifndef LANG_CORSICAN
# define LANG_CORSICAN 0x83
# endif
# ifndef LANG_DARI
# define LANG_DARI 0x8c
# endif
# ifndef LANG_DIVEHI
# define LANG_DIVEHI 0x65
# endif
# ifndef LANG_EDO
# define LANG_EDO 0x66
# endif
# ifndef LANG_ESTONIAN
# define LANG_ESTONIAN 0x25
# endif
# ifndef LANG_FAEROESE
# define LANG_FAEROESE 0x38
# endif
# ifndef LANG_FARSI
# define LANG_FARSI 0x29
# endif
# ifndef LANG_FRISIAN
# define LANG_FRISIAN 0x62
# endif
# ifndef LANG_FULFULDE
# define LANG_FULFULDE 0x67
# endif
# ifndef LANG_GAELIC
# define LANG_GAELIC 0x3c
# endif
# ifndef LANG_GALICIAN
# define LANG_GALICIAN 0x56
# endif
# ifndef LANG_GEORGIAN
# define LANG_GEORGIAN 0x37
# endif
# ifndef LANG_GREENLANDIC
# define LANG_GREENLANDIC 0x6f
# endif
# ifndef LANG_GUARANI
# define LANG_GUARANI 0x74
# endif
# ifndef LANG_GUJARATI
# define LANG_GUJARATI 0x47
# endif
# ifndef LANG_HAUSA
# define LANG_HAUSA 0x68
# endif
# ifndef LANG_HAWAIIAN
# define LANG_HAWAIIAN 0x75
# endif
# ifndef LANG_HEBREW
# define LANG_HEBREW 0x0d
# endif
# ifndef LANG_HINDI
# define LANG_HINDI 0x39
# endif
# ifndef LANG_IBIBIO
# define LANG_IBIBIO 0x69
# endif
# ifndef LANG_IGBO
# define LANG_IGBO 0x70
# endif
# ifndef LANG_INDONESIAN
# define LANG_INDONESIAN 0x21
# endif
# ifndef LANG_INUKTITUT
# define LANG_INUKTITUT 0x5d
# endif
# ifndef LANG_KANNADA
# define LANG_KANNADA 0x4b
# endif
# ifndef LANG_KANURI
# define LANG_KANURI 0x71
# endif
# ifndef LANG_KASHMIRI
# define LANG_KASHMIRI 0x60
# endif
# ifndef LANG_KAZAK
# define LANG_KAZAK 0x3f
# endif
# ifndef LANG_KICHE
# define LANG_KICHE 0x86
# endif
# ifndef LANG_KINYARWANDA
# define LANG_KINYARWANDA 0x87
# endif
# ifndef LANG_KONKANI
# define LANG_KONKANI 0x57
# endif
# ifndef LANG_KYRGYZ
# define LANG_KYRGYZ 0x40
# endif
# ifndef LANG_LAO
# define LANG_LAO 0x54
# endif
# ifndef LANG_LATIN
# define LANG_LATIN 0x76
# endif
# ifndef LANG_LATVIAN
# define LANG_LATVIAN 0x26
# endif
# ifndef LANG_LITHUANIAN
# define LANG_LITHUANIAN 0x27
# endif
# ifndef LANG_LUXEMBOURGISH
# define LANG_LUXEMBOURGISH 0x6e
# endif
# ifndef LANG_MACEDONIAN
# define LANG_MACEDONIAN 0x2f
# endif
# ifndef LANG_MALAY
# define LANG_MALAY 0x3e
# endif
# ifndef LANG_MALAYALAM
# define LANG_MALAYALAM 0x4c
# endif
# ifndef LANG_MALTESE
# define LANG_MALTESE 0x3a
# endif
# ifndef LANG_MANIPURI
# define LANG_MANIPURI 0x58
# endif
# ifndef LANG_MAORI
# define LANG_MAORI 0x81
# endif
# ifndef LANG_MAPUDUNGUN
# define LANG_MAPUDUNGUN 0x7a
# endif
# ifndef LANG_MARATHI
# define LANG_MARATHI 0x4e
# endif
# ifndef LANG_MOHAWK
# define LANG_MOHAWK 0x7c
# endif
# ifndef LANG_MONGOLIAN
# define LANG_MONGOLIAN 0x50
# endif
# ifndef LANG_NEPALI
# define LANG_NEPALI 0x61
# endif
# ifndef LANG_OCCITAN
# define LANG_OCCITAN 0x82
# endif
# ifndef LANG_ORIYA
# define LANG_ORIYA 0x48
# endif
# ifndef LANG_OROMO
# define LANG_OROMO 0x72
# endif
# ifndef LANG_PAPIAMENTU
# define LANG_PAPIAMENTU 0x79
# endif
# ifndef LANG_PASHTO
# define LANG_PASHTO 0x63
# endif
# ifndef LANG_PUNJABI
# define LANG_PUNJABI 0x46
# endif
# ifndef LANG_QUECHUA
# define LANG_QUECHUA 0x6b
# endif
# ifndef LANG_ROMANSH
# define LANG_ROMANSH 0x17
# endif
# ifndef LANG_SAMI
# define LANG_SAMI 0x3b
# endif
# ifndef LANG_SANSKRIT
# define LANG_SANSKRIT 0x4f
# endif
# ifndef LANG_SCOTTISH_GAELIC
# define LANG_SCOTTISH_GAELIC 0x91
# endif
# ifndef LANG_SERBIAN
# define LANG_SERBIAN 0x1a
# endif
# ifndef LANG_SINDHI
# define LANG_SINDHI 0x59
# endif
# ifndef LANG_SINHALESE
# define LANG_SINHALESE 0x5b
# endif
# ifndef LANG_SLOVAK
# define LANG_SLOVAK 0x1b
# endif
# ifndef LANG_SOMALI
# define LANG_SOMALI 0x77
# endif
# ifndef LANG_SORBIAN
# define LANG_SORBIAN 0x2e
# endif
# ifndef LANG_SOTHO
# define LANG_SOTHO 0x6c
# endif
# ifndef LANG_SUTU
# define LANG_SUTU 0x30
# endif
# ifndef LANG_SWAHILI
# define LANG_SWAHILI 0x41
# endif
# ifndef LANG_SYRIAC
# define LANG_SYRIAC 0x5a
# endif
# ifndef LANG_TAGALOG
# define LANG_TAGALOG 0x64
# endif
# ifndef LANG_TAJIK
# define LANG_TAJIK 0x28
# endif
# ifndef LANG_TAMAZIGHT
# define LANG_TAMAZIGHT 0x5f
# endif
# ifndef LANG_TAMIL
# define LANG_TAMIL 0x49
# endif
# ifndef LANG_TATAR
# define LANG_TATAR 0x44
# endif
# ifndef LANG_TELUGU
# define LANG_TELUGU 0x4a
# endif
# ifndef LANG_THAI
# define LANG_THAI 0x1e
# endif
# ifndef LANG_TIBETAN
# define LANG_TIBETAN 0x51
# endif
# ifndef LANG_TIGRINYA
# define LANG_TIGRINYA 0x73
# endif
# ifndef LANG_TSONGA
# define LANG_TSONGA 0x31
# endif
# ifndef LANG_TSWANA
# define LANG_TSWANA 0x32
# endif
# ifndef LANG_TURKMEN
# define LANG_TURKMEN 0x42
# endif
# ifndef LANG_UIGHUR
# define LANG_UIGHUR 0x80
# endif
# ifndef LANG_UKRAINIAN
# define LANG_UKRAINIAN 0x22
# endif
# ifndef LANG_URDU
# define LANG_URDU 0x20
# endif
# ifndef LANG_UZBEK
# define LANG_UZBEK 0x43
# endif
# ifndef LANG_VENDA
# define LANG_VENDA 0x33
# endif
# ifndef LANG_VIETNAMESE
# define LANG_VIETNAMESE 0x2a
# endif
# ifndef LANG_WELSH
# define LANG_WELSH 0x52
# endif
# ifndef LANG_WOLOF
# define LANG_WOLOF 0x88
# endif
# ifndef LANG_XHOSA
# define LANG_XHOSA 0x34
# endif
# ifndef LANG_YAKUT
# define LANG_YAKUT 0x85
# endif
# ifndef LANG_YI
# define LANG_YI 0x78
# endif
# ifndef LANG_YIDDISH
# define LANG_YIDDISH 0x3d
# endif
# ifndef LANG_YORUBA
# define LANG_YORUBA 0x6a
# endif
# ifndef LANG_ZULU
# define LANG_ZULU 0x35
# endif
# ifndef SUBLANG_AFRIKAANS_SOUTH_AFRICA
# define SUBLANG_AFRIKAANS_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_ALBANIAN_ALBANIA
# define SUBLANG_ALBANIAN_ALBANIA 0x01
# endif
# ifndef SUBLANG_ALSATIAN_FRANCE
# define SUBLANG_ALSATIAN_FRANCE 0x01
# endif
# ifndef SUBLANG_AMHARIC_ETHIOPIA
# define SUBLANG_AMHARIC_ETHIOPIA 0x01
# endif
# ifndef SUBLANG_ARABIC_SAUDI_ARABIA
# define SUBLANG_ARABIC_SAUDI_ARABIA 0x01
# endif
# ifndef SUBLANG_ARABIC_IRAQ
# define SUBLANG_ARABIC_IRAQ 0x02
# endif
# ifndef SUBLANG_ARABIC_EGYPT
# define SUBLANG_ARABIC_EGYPT 0x03
# endif
# ifndef SUBLANG_ARABIC_LIBYA
# define SUBLANG_ARABIC_LIBYA 0x04
# endif
# ifndef SUBLANG_ARABIC_ALGERIA
# define SUBLANG_ARABIC_ALGERIA 0x05
# endif
# ifndef SUBLANG_ARABIC_MOROCCO
# define SUBLANG_ARABIC_MOROCCO 0x06
# endif
# ifndef SUBLANG_ARABIC_TUNISIA
# define SUBLANG_ARABIC_TUNISIA 0x07
# endif
# ifndef SUBLANG_ARABIC_OMAN
# define SUBLANG_ARABIC_OMAN 0x08
# endif
# ifndef SUBLANG_ARABIC_YEMEN
# define SUBLANG_ARABIC_YEMEN 0x09
# endif
# ifndef SUBLANG_ARABIC_SYRIA
# define SUBLANG_ARABIC_SYRIA 0x0a
# endif
# ifndef SUBLANG_ARABIC_JORDAN
# define SUBLANG_ARABIC_JORDAN 0x0b
# endif
# ifndef SUBLANG_ARABIC_LEBANON
# define SUBLANG_ARABIC_LEBANON 0x0c
# endif
# ifndef SUBLANG_ARABIC_KUWAIT
# define SUBLANG_ARABIC_KUWAIT 0x0d
# endif
# ifndef SUBLANG_ARABIC_UAE
# define SUBLANG_ARABIC_UAE 0x0e
# endif
# ifndef SUBLANG_ARABIC_BAHRAIN
# define SUBLANG_ARABIC_BAHRAIN 0x0f
# endif
# ifndef SUBLANG_ARABIC_QATAR
# define SUBLANG_ARABIC_QATAR 0x10
# endif
# ifndef SUBLANG_ARMENIAN_ARMENIA
# define SUBLANG_ARMENIAN_ARMENIA 0x01
# endif
# ifndef SUBLANG_ASSAMESE_INDIA
# define SUBLANG_ASSAMESE_INDIA 0x01
# endif
# ifndef SUBLANG_AZERI_LATIN
# define SUBLANG_AZERI_LATIN 0x01
# endif
# ifndef SUBLANG_AZERI_CYRILLIC
# define SUBLANG_AZERI_CYRILLIC 0x02
# endif
# ifndef SUBLANG_BASHKIR_RUSSIA
# define SUBLANG_BASHKIR_RUSSIA 0x01
# endif
# ifndef SUBLANG_BASQUE_BASQUE
# define SUBLANG_BASQUE_BASQUE 0x01
# endif
# ifndef SUBLANG_BELARUSIAN_BELARUS
# define SUBLANG_BELARUSIAN_BELARUS 0x01
# endif
# ifndef SUBLANG_BENGALI_INDIA
# define SUBLANG_BENGALI_INDIA 0x01
# endif
# ifndef SUBLANG_BENGALI_BANGLADESH
# define SUBLANG_BENGALI_BANGLADESH 0x02
# endif
# ifndef SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN
# define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN 0x05
# endif
# ifndef SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC
# define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC 0x08
# endif
# ifndef SUBLANG_BRETON_FRANCE
# define SUBLANG_BRETON_FRANCE 0x01
# endif
# ifndef SUBLANG_BULGARIAN_BULGARIA
# define SUBLANG_BULGARIAN_BULGARIA 0x01
# endif
# ifndef SUBLANG_CAMBODIAN_CAMBODIA
# define SUBLANG_CAMBODIAN_CAMBODIA 0x01
# endif
# ifndef SUBLANG_CATALAN_SPAIN
# define SUBLANG_CATALAN_SPAIN 0x01
# endif
# ifndef SUBLANG_CORSICAN_FRANCE
# define SUBLANG_CORSICAN_FRANCE 0x01
# endif
# ifndef SUBLANG_CROATIAN_CROATIA
# define SUBLANG_CROATIAN_CROATIA 0x01
# endif
# ifndef SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN
# define SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN 0x04
# endif
# ifndef SUBLANG_CHINESE_MACAU
# define SUBLANG_CHINESE_MACAU 0x05
# endif
# ifndef SUBLANG_CZECH_CZECH_REPUBLIC
# define SUBLANG_CZECH_CZECH_REPUBLIC 0x01
# endif
# ifndef SUBLANG_DANISH_DENMARK
# define SUBLANG_DANISH_DENMARK 0x01
# endif
# ifndef SUBLANG_DARI_AFGHANISTAN
# define SUBLANG_DARI_AFGHANISTAN 0x01
# endif
# ifndef SUBLANG_DIVEHI_MALDIVES
# define SUBLANG_DIVEHI_MALDIVES 0x01
# endif
# ifndef SUBLANG_DUTCH_SURINAM
# define SUBLANG_DUTCH_SURINAM 0x03
# endif
# ifndef SUBLANG_ENGLISH_SOUTH_AFRICA
# define SUBLANG_ENGLISH_SOUTH_AFRICA 0x07
# endif
# ifndef SUBLANG_ENGLISH_JAMAICA
# define SUBLANG_ENGLISH_JAMAICA 0x08
# endif
# ifndef SUBLANG_ENGLISH_CARIBBEAN
# define SUBLANG_ENGLISH_CARIBBEAN 0x09
# endif
# ifndef SUBLANG_ENGLISH_BELIZE
# define SUBLANG_ENGLISH_BELIZE 0x0a
# endif
# ifndef SUBLANG_ENGLISH_TRINIDAD
# define SUBLANG_ENGLISH_TRINIDAD 0x0b
# endif
# ifndef SUBLANG_ENGLISH_ZIMBABWE
# define SUBLANG_ENGLISH_ZIMBABWE 0x0c
# endif
# ifndef SUBLANG_ENGLISH_PHILIPPINES
# define SUBLANG_ENGLISH_PHILIPPINES 0x0d
# endif
# ifndef SUBLANG_ENGLISH_INDONESIA
# define SUBLANG_ENGLISH_INDONESIA 0x0e
# endif
# ifndef SUBLANG_ENGLISH_HONGKONG
# define SUBLANG_ENGLISH_HONGKONG 0x0f
# endif
# ifndef SUBLANG_ENGLISH_INDIA
# define SUBLANG_ENGLISH_INDIA 0x10
# endif
# ifndef SUBLANG_ENGLISH_MALAYSIA
# define SUBLANG_ENGLISH_MALAYSIA 0x11
# endif
# ifndef SUBLANG_ENGLISH_SINGAPORE
# define SUBLANG_ENGLISH_SINGAPORE 0x12
# endif
# ifndef SUBLANG_ESTONIAN_ESTONIA
# define SUBLANG_ESTONIAN_ESTONIA 0x01
# endif
# ifndef SUBLANG_FAEROESE_FAROE_ISLANDS
# define SUBLANG_FAEROESE_FAROE_ISLANDS 0x01
# endif
# ifndef SUBLANG_FARSI_IRAN
# define SUBLANG_FARSI_IRAN 0x01
# endif
# ifndef SUBLANG_FINNISH_FINLAND
# define SUBLANG_FINNISH_FINLAND 0x01
# endif
# ifndef SUBLANG_FRENCH_LUXEMBOURG
# define SUBLANG_FRENCH_LUXEMBOURG 0x05
# endif
# ifndef SUBLANG_FRENCH_MONACO
# define SUBLANG_FRENCH_MONACO 0x06
# endif
# ifndef SUBLANG_FRENCH_WESTINDIES
# define SUBLANG_FRENCH_WESTINDIES 0x07
# endif
# ifndef SUBLANG_FRENCH_REUNION
# define SUBLANG_FRENCH_REUNION 0x08
# endif
# ifndef SUBLANG_FRENCH_CONGO
# define SUBLANG_FRENCH_CONGO 0x09
# endif
# ifndef SUBLANG_FRENCH_SENEGAL
# define SUBLANG_FRENCH_SENEGAL 0x0a
# endif
# ifndef SUBLANG_FRENCH_CAMEROON
# define SUBLANG_FRENCH_CAMEROON 0x0b
# endif
# ifndef SUBLANG_FRENCH_COTEDIVOIRE
# define SUBLANG_FRENCH_COTEDIVOIRE 0x0c
# endif
# ifndef SUBLANG_FRENCH_MALI
# define SUBLANG_FRENCH_MALI 0x0d
# endif
# ifndef SUBLANG_FRENCH_MOROCCO
# define SUBLANG_FRENCH_MOROCCO 0x0e
# endif
# ifndef SUBLANG_FRENCH_HAITI
# define SUBLANG_FRENCH_HAITI 0x0f
# endif
# ifndef SUBLANG_FRISIAN_NETHERLANDS
# define SUBLANG_FRISIAN_NETHERLANDS 0x01
# endif
# ifndef SUBLANG_GALICIAN_SPAIN
# define SUBLANG_GALICIAN_SPAIN 0x01
# endif
# ifndef SUBLANG_GEORGIAN_GEORGIA
# define SUBLANG_GEORGIAN_GEORGIA 0x01
# endif
# ifndef SUBLANG_GERMAN_LUXEMBOURG
# define SUBLANG_GERMAN_LUXEMBOURG 0x04
# endif
# ifndef SUBLANG_GERMAN_LIECHTENSTEIN
# define SUBLANG_GERMAN_LIECHTENSTEIN 0x05
# endif
# ifndef SUBLANG_GREEK_GREECE
# define SUBLANG_GREEK_GREECE 0x01
# endif
# ifndef SUBLANG_GREENLANDIC_GREENLAND
# define SUBLANG_GREENLANDIC_GREENLAND 0x01
# endif
# ifndef SUBLANG_GUJARATI_INDIA
# define SUBLANG_GUJARATI_INDIA 0x01
# endif
# ifndef SUBLANG_HAUSA_NIGERIA_LATIN
# define SUBLANG_HAUSA_NIGERIA_LATIN 0x01
# endif
# ifndef SUBLANG_HEBREW_ISRAEL
# define SUBLANG_HEBREW_ISRAEL 0x01
# endif
# ifndef SUBLANG_HINDI_INDIA
# define SUBLANG_HINDI_INDIA 0x01
# endif
# ifndef SUBLANG_HUNGARIAN_HUNGARY
# define SUBLANG_HUNGARIAN_HUNGARY 0x01
# endif
# ifndef SUBLANG_ICELANDIC_ICELAND
# define SUBLANG_ICELANDIC_ICELAND 0x01
# endif
# ifndef SUBLANG_IGBO_NIGERIA
# define SUBLANG_IGBO_NIGERIA 0x01
# endif
# ifndef SUBLANG_INDONESIAN_INDONESIA
# define SUBLANG_INDONESIAN_INDONESIA 0x01
# endif
# ifndef SUBLANG_INUKTITUT_CANADA
# define SUBLANG_INUKTITUT_CANADA 0x01
# endif
# undef SUBLANG_INUKTITUT_CANADA_LATIN
# define SUBLANG_INUKTITUT_CANADA_LATIN 0x02
# undef SUBLANG_IRISH_IRELAND
# define SUBLANG_IRISH_IRELAND 0x02
# ifndef SUBLANG_JAPANESE_JAPAN
# define SUBLANG_JAPANESE_JAPAN 0x01
# endif
# ifndef SUBLANG_KANNADA_INDIA
# define SUBLANG_KANNADA_INDIA 0x01
# endif
# ifndef SUBLANG_KASHMIRI_INDIA
# define SUBLANG_KASHMIRI_INDIA 0x02
# endif
# ifndef SUBLANG_KAZAK_KAZAKHSTAN
# define SUBLANG_KAZAK_KAZAKHSTAN 0x01
# endif
# ifndef SUBLANG_KICHE_GUATEMALA
# define SUBLANG_KICHE_GUATEMALA 0x01
# endif
# ifndef SUBLANG_KINYARWANDA_RWANDA
# define SUBLANG_KINYARWANDA_RWANDA 0x01
# endif
# ifndef SUBLANG_KONKANI_INDIA
# define SUBLANG_KONKANI_INDIA 0x01
# endif
# ifndef SUBLANG_KYRGYZ_KYRGYZSTAN
# define SUBLANG_KYRGYZ_KYRGYZSTAN 0x01
# endif
# ifndef SUBLANG_LAO_LAOS
# define SUBLANG_LAO_LAOS 0x01
# endif
# ifndef SUBLANG_LATVIAN_LATVIA
# define SUBLANG_LATVIAN_LATVIA 0x01
# endif
# ifndef SUBLANG_LITHUANIAN_LITHUANIA
# define SUBLANG_LITHUANIAN_LITHUANIA 0x01
# endif
# undef SUBLANG_LOWER_SORBIAN_GERMANY
# define SUBLANG_LOWER_SORBIAN_GERMANY 0x02
# ifndef SUBLANG_LUXEMBOURGISH_LUXEMBOURG
# define SUBLANG_LUXEMBOURGISH_LUXEMBOURG 0x01
# endif
# ifndef SUBLANG_MACEDONIAN_MACEDONIA
# define SUBLANG_MACEDONIAN_MACEDONIA 0x01
# endif
# ifndef SUBLANG_MALAY_MALAYSIA
# define SUBLANG_MALAY_MALAYSIA 0x01
# endif
# ifndef SUBLANG_MALAY_BRUNEI_DARUSSALAM
# define SUBLANG_MALAY_BRUNEI_DARUSSALAM 0x02
# endif
# ifndef SUBLANG_MALAYALAM_INDIA
# define SUBLANG_MALAYALAM_INDIA 0x01
# endif
# ifndef SUBLANG_MALTESE_MALTA
# define SUBLANG_MALTESE_MALTA 0x01
# endif
# ifndef SUBLANG_MAORI_NEW_ZEALAND
# define SUBLANG_MAORI_NEW_ZEALAND 0x01
# endif
# ifndef SUBLANG_MAPUDUNGUN_CHILE
# define SUBLANG_MAPUDUNGUN_CHILE 0x01
# endif
# ifndef SUBLANG_MARATHI_INDIA
# define SUBLANG_MARATHI_INDIA 0x01
# endif
# ifndef SUBLANG_MOHAWK_CANADA
# define SUBLANG_MOHAWK_CANADA 0x01
# endif
# ifndef SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA
# define SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA 0x01
# endif
# ifndef SUBLANG_MONGOLIAN_PRC
# define SUBLANG_MONGOLIAN_PRC 0x02
# endif
# ifndef SUBLANG_NEPALI_NEPAL
# define SUBLANG_NEPALI_NEPAL 0x01
# endif
# ifndef SUBLANG_NEPALI_INDIA
# define SUBLANG_NEPALI_INDIA 0x02
# endif
# ifndef SUBLANG_OCCITAN_FRANCE
# define SUBLANG_OCCITAN_FRANCE 0x01
# endif
# ifndef SUBLANG_ORIYA_INDIA
# define SUBLANG_ORIYA_INDIA 0x01
# endif
# ifndef SUBLANG_PASHTO_AFGHANISTAN
# define SUBLANG_PASHTO_AFGHANISTAN 0x01
# endif
# ifndef SUBLANG_POLISH_POLAND
# define SUBLANG_POLISH_POLAND 0x01
# endif
# ifndef SUBLANG_PUNJABI_INDIA
# define SUBLANG_PUNJABI_INDIA 0x01
# endif
# ifndef SUBLANG_PUNJABI_PAKISTAN
# define SUBLANG_PUNJABI_PAKISTAN 0x02
# endif
# ifndef SUBLANG_QUECHUA_BOLIVIA
# define SUBLANG_QUECHUA_BOLIVIA 0x01
# endif
# ifndef SUBLANG_QUECHUA_ECUADOR
# define SUBLANG_QUECHUA_ECUADOR 0x02
# endif
# ifndef SUBLANG_QUECHUA_PERU
# define SUBLANG_QUECHUA_PERU 0x03
# endif
# ifndef SUBLANG_ROMANIAN_ROMANIA
# define SUBLANG_ROMANIAN_ROMANIA 0x01
# endif
# ifndef SUBLANG_ROMANIAN_MOLDOVA
# define SUBLANG_ROMANIAN_MOLDOVA 0x02
# endif
# ifndef SUBLANG_ROMANSH_SWITZERLAND
# define SUBLANG_ROMANSH_SWITZERLAND 0x01
# endif
# ifndef SUBLANG_RUSSIAN_RUSSIA
# define SUBLANG_RUSSIAN_RUSSIA 0x01
# endif
# ifndef SUBLANG_RUSSIAN_MOLDAVIA
# define SUBLANG_RUSSIAN_MOLDAVIA 0x02
# endif
# ifndef SUBLANG_SAMI_NORTHERN_NORWAY
# define SUBLANG_SAMI_NORTHERN_NORWAY 0x01
# endif
# ifndef SUBLANG_SAMI_NORTHERN_SWEDEN
# define SUBLANG_SAMI_NORTHERN_SWEDEN 0x02
# endif
# ifndef SUBLANG_SAMI_NORTHERN_FINLAND
# define SUBLANG_SAMI_NORTHERN_FINLAND 0x03
# endif
# ifndef SUBLANG_SAMI_LULE_NORWAY
# define SUBLANG_SAMI_LULE_NORWAY 0x04
# endif
# ifndef SUBLANG_SAMI_LULE_SWEDEN
# define SUBLANG_SAMI_LULE_SWEDEN 0x05
# endif
# ifndef SUBLANG_SAMI_SOUTHERN_NORWAY
# define SUBLANG_SAMI_SOUTHERN_NORWAY 0x06
# endif
# ifndef SUBLANG_SAMI_SOUTHERN_SWEDEN
# define SUBLANG_SAMI_SOUTHERN_SWEDEN 0x07
# endif
# undef SUBLANG_SAMI_SKOLT_FINLAND
# define SUBLANG_SAMI_SKOLT_FINLAND 0x08
# undef SUBLANG_SAMI_INARI_FINLAND
# define SUBLANG_SAMI_INARI_FINLAND 0x09
# ifndef SUBLANG_SANSKRIT_INDIA
# define SUBLANG_SANSKRIT_INDIA 0x01
# endif
# ifndef SUBLANG_SERBIAN_LATIN
# define SUBLANG_SERBIAN_LATIN 0x02
# endif
# ifndef SUBLANG_SERBIAN_CYRILLIC
# define SUBLANG_SERBIAN_CYRILLIC 0x03
# endif
# ifndef SUBLANG_SINDHI_INDIA
# define SUBLANG_SINDHI_INDIA 0x01
# endif
# undef SUBLANG_SINDHI_PAKISTAN
# define SUBLANG_SINDHI_PAKISTAN 0x02
# ifndef SUBLANG_SINDHI_AFGHANISTAN
# define SUBLANG_SINDHI_AFGHANISTAN 0x02
# endif
# ifndef SUBLANG_SINHALESE_SRI_LANKA
# define SUBLANG_SINHALESE_SRI_LANKA 0x01
# endif
# ifndef SUBLANG_SLOVAK_SLOVAKIA
# define SUBLANG_SLOVAK_SLOVAKIA 0x01
# endif
# ifndef SUBLANG_SLOVENIAN_SLOVENIA
# define SUBLANG_SLOVENIAN_SLOVENIA 0x01
# endif
# ifndef SUBLANG_SOTHO_SOUTH_AFRICA
# define SUBLANG_SOTHO_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_SPANISH_GUATEMALA
# define SUBLANG_SPANISH_GUATEMALA 0x04
# endif
# ifndef SUBLANG_SPANISH_COSTA_RICA
# define SUBLANG_SPANISH_COSTA_RICA 0x05
# endif
# ifndef SUBLANG_SPANISH_PANAMA
# define SUBLANG_SPANISH_PANAMA 0x06
# endif
# ifndef SUBLANG_SPANISH_DOMINICAN_REPUBLIC
# define SUBLANG_SPANISH_DOMINICAN_REPUBLIC 0x07
# endif
# ifndef SUBLANG_SPANISH_VENEZUELA
# define SUBLANG_SPANISH_VENEZUELA 0x08
# endif
# ifndef SUBLANG_SPANISH_COLOMBIA
# define SUBLANG_SPANISH_COLOMBIA 0x09
# endif
# ifndef SUBLANG_SPANISH_PERU
# define SUBLANG_SPANISH_PERU 0x0a
# endif
# ifndef SUBLANG_SPANISH_ARGENTINA
# define SUBLANG_SPANISH_ARGENTINA 0x0b
# endif
# ifndef SUBLANG_SPANISH_ECUADOR
# define SUBLANG_SPANISH_ECUADOR 0x0c
# endif
# ifndef SUBLANG_SPANISH_CHILE
# define SUBLANG_SPANISH_CHILE 0x0d
# endif
# ifndef SUBLANG_SPANISH_URUGUAY
# define SUBLANG_SPANISH_URUGUAY 0x0e
# endif
# ifndef SUBLANG_SPANISH_PARAGUAY
# define SUBLANG_SPANISH_PARAGUAY 0x0f
# endif
# ifndef SUBLANG_SPANISH_BOLIVIA
# define SUBLANG_SPANISH_BOLIVIA 0x10
# endif
# ifndef SUBLANG_SPANISH_EL_SALVADOR
# define SUBLANG_SPANISH_EL_SALVADOR 0x11
# endif
# ifndef SUBLANG_SPANISH_HONDURAS
# define SUBLANG_SPANISH_HONDURAS 0x12
# endif
# ifndef SUBLANG_SPANISH_NICARAGUA
# define SUBLANG_SPANISH_NICARAGUA 0x13
# endif
# ifndef SUBLANG_SPANISH_PUERTO_RICO
# define SUBLANG_SPANISH_PUERTO_RICO 0x14
# endif
# ifndef SUBLANG_SPANISH_US
# define SUBLANG_SPANISH_US 0x15
# endif
# ifndef SUBLANG_SWAHILI_KENYA
# define SUBLANG_SWAHILI_KENYA 0x01
# endif
# ifndef SUBLANG_SWEDISH_SWEDEN
# define SUBLANG_SWEDISH_SWEDEN 0x01
# endif
# ifndef SUBLANG_SWEDISH_FINLAND
# define SUBLANG_SWEDISH_FINLAND 0x02
# endif
# ifndef SUBLANG_SYRIAC_SYRIA
# define SUBLANG_SYRIAC_SYRIA 0x01
# endif
# ifndef SUBLANG_TAGALOG_PHILIPPINES
# define SUBLANG_TAGALOG_PHILIPPINES 0x01
# endif
# ifndef SUBLANG_TAJIK_TAJIKISTAN
# define SUBLANG_TAJIK_TAJIKISTAN 0x01
# endif
# ifndef SUBLANG_TAMAZIGHT_ARABIC
# define SUBLANG_TAMAZIGHT_ARABIC 0x01
# endif
# ifndef SUBLANG_TAMAZIGHT_ALGERIA_LATIN
# define SUBLANG_TAMAZIGHT_ALGERIA_LATIN 0x02
# endif
# ifndef SUBLANG_TAMIL_INDIA
# define SUBLANG_TAMIL_INDIA 0x01
# endif
# ifndef SUBLANG_TATAR_RUSSIA
# define SUBLANG_TATAR_RUSSIA 0x01
# endif
# ifndef SUBLANG_TELUGU_INDIA
# define SUBLANG_TELUGU_INDIA 0x01
# endif
# ifndef SUBLANG_THAI_THAILAND
# define SUBLANG_THAI_THAILAND 0x01
# endif
# ifndef SUBLANG_TIBETAN_PRC
# define SUBLANG_TIBETAN_PRC 0x01
# endif
# undef SUBLANG_TIBETAN_BHUTAN
# define SUBLANG_TIBETAN_BHUTAN 0x02
# ifndef SUBLANG_TIGRINYA_ETHIOPIA
# define SUBLANG_TIGRINYA_ETHIOPIA 0x01
# endif
# ifndef SUBLANG_TIGRINYA_ERITREA
# define SUBLANG_TIGRINYA_ERITREA 0x02
# endif
# ifndef SUBLANG_TSWANA_SOUTH_AFRICA
# define SUBLANG_TSWANA_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_TURKISH_TURKEY
# define SUBLANG_TURKISH_TURKEY 0x01
# endif
# ifndef SUBLANG_TURKMEN_TURKMENISTAN
# define SUBLANG_TURKMEN_TURKMENISTAN 0x01
# endif
# ifndef SUBLANG_UIGHUR_PRC
# define SUBLANG_UIGHUR_PRC 0x01
# endif
# ifndef SUBLANG_UKRAINIAN_UKRAINE
# define SUBLANG_UKRAINIAN_UKRAINE 0x01
# endif
# ifndef SUBLANG_UPPER_SORBIAN_GERMANY
# define SUBLANG_UPPER_SORBIAN_GERMANY 0x01
# endif
# ifndef SUBLANG_URDU_PAKISTAN
# define SUBLANG_URDU_PAKISTAN 0x01
# endif
# ifndef SUBLANG_URDU_INDIA
# define SUBLANG_URDU_INDIA 0x02
# endif
# ifndef SUBLANG_UZBEK_LATIN
# define SUBLANG_UZBEK_LATIN 0x01
# endif
# ifndef SUBLANG_UZBEK_CYRILLIC
# define SUBLANG_UZBEK_CYRILLIC 0x02
# endif
# ifndef SUBLANG_VIETNAMESE_VIETNAM
# define SUBLANG_VIETNAMESE_VIETNAM 0x01
# endif
# ifndef SUBLANG_WELSH_UNITED_KINGDOM
# define SUBLANG_WELSH_UNITED_KINGDOM 0x01
# endif
# ifndef SUBLANG_WOLOF_SENEGAL
# define SUBLANG_WOLOF_SENEGAL 0x01
# endif
# ifndef SUBLANG_XHOSA_SOUTH_AFRICA
# define SUBLANG_XHOSA_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_YAKUT_RUSSIA
# define SUBLANG_YAKUT_RUSSIA 0x01
# endif
# ifndef SUBLANG_YI_PRC
# define SUBLANG_YI_PRC 0x01
# endif
# ifndef SUBLANG_YORUBA_NIGERIA
# define SUBLANG_YORUBA_NIGERIA 0x01
# endif
# ifndef SUBLANG_ZULU_SOUTH_AFRICA
# define SUBLANG_ZULU_SOUTH_AFRICA 0x01
# endif
 
# ifndef LOCALE_SNAME
# define LOCALE_SNAME 0x5c
# endif
# ifndef LOCALE_NAME_MAX_LENGTH
# define LOCALE_NAME_MAX_LENGTH 85
# endif
 
# undef GetLocaleInfo
# define GetLocaleInfo GetLocaleInfoA
# undef EnumSystemLocales
# define EnumSystemLocales EnumSystemLocalesA
#endif

 
#undef setlocale


#if HAVE_CFPREFERENCESCOPYAPPVALUE
 

 
# if !defined IN_LIBINTL
static
# endif
void
gl_locale_name_canonicalize (char *name)
{
   
  typedef struct { const char legacy[21+1]; const char unixy[5+1]; }
          legacy_entry;
  static const legacy_entry legacy_table[] = {
    { "Afrikaans",             "af" },
    { "Albanian",              "sq" },
    { "Amharic",               "am" },
    { "Arabic",                "ar" },
    { "Armenian",              "hy" },
    { "Assamese",              "as" },
    { "Aymara",                "ay" },
    { "Azerbaijani",           "az" },
    { "Basque",                "eu" },
    { "Belarusian",            "be" },
    { "Belorussian",           "be" },
    { "Bengali",               "bn" },
    { "Brazilian Portugese",   "pt_BR" },
    { "Brazilian Portuguese",  "pt_BR" },
    { "Breton",                "br" },
    { "Bulgarian",             "bg" },
    { "Burmese",               "my" },
    { "Byelorussian",          "be" },
    { "Catalan",               "ca" },
    { "Chewa",                 "ny" },
    { "Chichewa",              "ny" },
    { "Chinese",               "zh" },
    { "Chinese, Simplified",   "zh_CN" },
    { "Chinese, Traditional",  "zh_TW" },
    { "Chinese, Tradtional",   "zh_TW" },
    { "Croatian",              "hr" },
    { "Czech",                 "cs" },
    { "Danish",                "da" },
    { "Dutch",                 "nl" },
    { "Dzongkha",              "dz" },
    { "English",               "en" },
    { "Esperanto",             "eo" },
    { "Estonian",              "et" },
    { "Faroese",               "fo" },
    { "Farsi",                 "fa" },
    { "Finnish",               "fi" },
    { "Flemish",               "nl_BE" },
    { "French",                "fr" },
    { "Galician",              "gl" },
    { "Gallegan",              "gl" },
    { "Georgian",              "ka" },
    { "German",                "de" },
    { "Greek",                 "el" },
    { "Greenlandic",           "kl" },
    { "Guarani",               "gn" },
    { "Gujarati",              "gu" },
    { "Hawaiian",              "haw" },  
    { "Hebrew",                "he" },
    { "Hindi",                 "hi" },
    { "Hungarian",             "hu" },
    { "Icelandic",             "is" },
    { "Indonesian",            "id" },
    { "Inuktitut",             "iu" },
    { "Irish",                 "ga" },
    { "Italian",               "it" },
    { "Japanese",              "ja" },
    { "Javanese",              "jv" },
    { "Kalaallisut",           "kl" },
    { "Kannada",               "kn" },
    { "Kashmiri",              "ks" },
    { "Kazakh",                "kk" },
    { "Khmer",                 "km" },
    { "Kinyarwanda",           "rw" },
    { "Kirghiz",               "ky" },
    { "Korean",                "ko" },
    { "Kurdish",               "ku" },
    { "Latin",                 "la" },
    { "Latvian",               "lv" },
    { "Lithuanian",            "lt" },
    { "Macedonian",            "mk" },
    { "Malagasy",              "mg" },
    { "Malay",                 "ms" },
    { "Malayalam",             "ml" },
    { "Maltese",               "mt" },
    { "Manx",                  "gv" },
    { "Marathi",               "mr" },
    { "Moldavian",             "mo" },
    { "Mongolian",             "mn" },
    { "Nepali",                "ne" },
    { "Norwegian",             "nb" },  
    { "Nyanja",                "ny" },
    { "Nynorsk",               "nn" },
    { "Oriya",                 "or" },
    { "Oromo",                 "om" },
    { "Panjabi",               "pa" },
    { "Pashto",                "ps" },
    { "Persian",               "fa" },
    { "Polish",                "pl" },
    { "Portuguese",            "pt" },
    { "Portuguese, Brazilian", "pt_BR" },
    { "Punjabi",               "pa" },
    { "Pushto",                "ps" },
    { "Quechua",               "qu" },
    { "Romanian",              "ro" },
    { "Ruanda",                "rw" },
    { "Rundi",                 "rn" },
    { "Russian",               "ru" },
    { "Sami",                  "se_NO" },  
    { "Sanskrit",              "sa" },
    { "Scottish",              "gd" },
    { "Serbian",               "sr" },
    { "Simplified Chinese",    "zh_CN" },
    { "Sindhi",                "sd" },
    { "Sinhalese",             "si" },
    { "Slovak",                "sk" },
    { "Slovenian",             "sl" },
    { "Somali",                "so" },
    { "Spanish",               "es" },
    { "Sundanese",             "su" },
    { "Swahili",               "sw" },
    { "Swedish",               "sv" },
    { "Tagalog",               "tl" },
    { "Tajik",                 "tg" },
    { "Tajiki",                "tg" },
    { "Tamil",                 "ta" },
    { "Tatar",                 "tt" },
    { "Telugu",                "te" },
    { "Thai",                  "th" },
    { "Tibetan",               "bo" },
    { "Tigrinya",              "ti" },
    { "Tongan",                "to" },
    { "Traditional Chinese",   "zh_TW" },
    { "Turkish",               "tr" },
    { "Turkmen",               "tk" },
    { "Uighur",                "ug" },
    { "Ukrainian",             "uk" },
    { "Urdu",                  "ur" },
    { "Uzbek",                 "uz" },
    { "Vietnamese",            "vi" },
    { "Welsh",                 "cy" },
    { "Yiddish",               "yi" }
  };

   
  typedef struct { const char langtag[7+1]; const char unixy[12+1]; }
          langtag_entry;
  static const langtag_entry langtag_table[] = {
     
    { "az-Latn", "az" },
     
    { "bs-Latn", "bs" },
     
    { "ga-dots", "ga" },
     
    { "kk-Cyrl", "kk" },
     
    { "mn-Cyrl", "mn" },
     
    { "ms-Latn", "ms" },
     
    { "pa-Arab", "pa_PK" },
    { "pa-Guru", "pa_IN" },
     
     
    { "sr-Cyrl", "sr" },
     
    { "tg-Cyrl", "tg" },
     
    { "tk-Cyrl", "tk" },
     
    { "tt-Cyrl", "tt" },
     
    { "uz-Latn", "uz" },
     
     
    { "yue-Hans", "yue" },
     
    { "zh-Hans", "zh_CN" },
    { "zh-Hant", "zh_TW" }
  };

   
  if (name[0] >= 'A' && name[0] <= 'Z')
    {
      unsigned int i1, i2;
      i1 = 0;
      i2 = sizeof (legacy_table) / sizeof (legacy_entry);
      while (i2 - i1 > 1)
        {
           
          unsigned int i = (i1 + i2) >> 1;
          const legacy_entry *p = &legacy_table[i];
          if (strcmp (name, p->legacy) < 0)
            i2 = i;
          else
            i1 = i;
        }
      if (strcmp (name, legacy_table[i1].legacy) == 0)
        {
          strcpy (name, legacy_table[i1].unixy);
          return;
        }
    }

   
  if (strlen (name) == 7 && name[2] == '-')
    {
      unsigned int i1, i2;
      i1 = 0;
      i2 = sizeof (langtag_table) / sizeof (langtag_entry);
      while (i2 - i1 > 1)
        {
           
          unsigned int i = (i1 + i2) >> 1;
          const langtag_entry *p = &langtag_table[i];
          if (strcmp (name, p->langtag) < 0)
            i2 = i;
          else
            i1 = i;
        }
      if (strcmp (name, langtag_table[i1].langtag) == 0)
        {
          strcpy (name, langtag_table[i1].unixy);
          return;
        }

      i1 = 0;
      i2 = sizeof (script_table) / sizeof (script_entry);
      while (i2 - i1 > 1)
        {
           
          unsigned int i = (i1 + i2) >> 1;
          const script_entry *p = &script_table[i];
          if (strcmp (name + 3, p->script) < 0)
            i2 = i;
          else
            i1 = i;
        }
      if (strcmp (name + 3, script_table[i1].script) == 0)
        {
          name[2] = '@';
          strcpy (name + 3, script_table[i1].unixy);
          return;
        }
    }

   
  {
    char *p;
    for (p = name; *p != '\0'; p++)
      if (*p == '-')
        *p = '_';
  }
}

#endif


#if defined WINDOWS_NATIVE || defined __CYGWIN__  

 
# if !defined IN_LIBINTL
static
# endif
void
gl_locale_name_canonicalize (char *name)
{
   
  char *p;

  for (p = name; *p != '\0'; p++)
    if (*p == '-')
      {
        *p = '_';
        p++;
        for (; *p != '\0'; p++)
          {
            if (*p >= 'a' && *p <= 'z')
              *p += 'A' - 'a';
            if (*p == '-')
              {
                *p = '\0';
                return;
              }
          }
        return;
      }
}

# if !defined IN_LIBINTL
static
# endif
const char *
gl_locale_name_from_win32_LANGID (LANGID langid)
{
   
  if (getenv ("GETTEXT_MUI") != NULL)
    {
      static char namebuf[256];

       
      if (GetLocaleInfoA (MAKELCID (langid, SORT_DEFAULT), LOCALE_SNAME,
                          namebuf, sizeof (namebuf) - 1))
        {
           
          gl_locale_name_canonicalize (namebuf);
          return namebuf;
        }
    }
   
   
  {
    int primary, sub;

     
    primary = PRIMARYLANGID (langid);
    sub = SUBLANGID (langid);

     
          case 0x1e: return "az@latin";
          case SUBLANG_AZERI_LATIN: return "az_AZ@latin";
          case 0x1d: return "az@cyrillic";
          case SUBLANG_AZERI_CYRILLIC: return "az_AZ@cyrillic";
          }
        return "az";
      case LANG_BASHKIR:
        switch (sub)
          {
          case SUBLANG_BASHKIR_RUSSIA: return "ba_RU";
          }
        return "ba";
      case LANG_BASQUE:
        switch (sub)
          {
          case SUBLANG_BASQUE_BASQUE: return "eu_ES";
          }
        return "eu";  
      case LANG_BELARUSIAN:
        switch (sub)
          {
          case SUBLANG_BELARUSIAN_BELARUS: return "be_BY";
          }
        return "be";
      case LANG_BENGALI:
        switch (sub)
          {
          case SUBLANG_BENGALI_INDIA: return "bn_IN";
          case SUBLANG_BENGALI_BANGLADESH: return "bn_BD";
          }
        return "bn";
      case LANG_BRETON:
        switch (sub)
          {
          case SUBLANG_BRETON_FRANCE: return "br_FR";
          }
        return "br";
      case LANG_BULGARIAN:
        switch (sub)
          {
          case SUBLANG_BULGARIAN_BULGARIA: return "bg_BG";
          }
        return "bg";
      case LANG_BURMESE:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "my_MM";
          }
        return "my";
      case LANG_CAMBODIAN:
        switch (sub)
          {
          case SUBLANG_CAMBODIAN_CAMBODIA: return "km_KH";
          }
        return "km";
      case LANG_CATALAN:
        switch (sub)
          {
          case SUBLANG_CATALAN_SPAIN: return "ca_ES";
          }
        return "ca";
      case LANG_CHEROKEE:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "chr_US";
          }
        return "chr";
      case LANG_CHINESE:
        switch (sub)
          {
          case SUBLANG_CHINESE_TRADITIONAL: case 0x1f: return "zh_TW";
          case SUBLANG_CHINESE_SIMPLIFIED: case 0x00: return "zh_CN";
          case SUBLANG_CHINESE_HONGKONG: return "zh_HK";  
          case SUBLANG_CHINESE_SINGAPORE: return "zh_SG";  
          case SUBLANG_CHINESE_MACAU: return "zh_MO";  
          }
        return "zh";
      case LANG_CORSICAN:
        switch (sub)
          {
          case SUBLANG_CORSICAN_FRANCE: return "co_FR";
          }
        return "co";
      case LANG_CROATIAN:       
        switch (sub)
          {
           
          case 0x00: return "hr";
          case SUBLANG_CROATIAN_CROATIA: return "hr_HR";
          case SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN: return "hr_BA";
           
          case 0x1f: return "sr";
          case 0x1c: return "sr";  
          case SUBLANG_SERBIAN_LATIN: return "sr_CS";  
          case 0x09: return "sr_RS";  
          case 0x0b: return "sr_ME";  
          case 0x06: return "sr_BA";  
          case 0x1b: return "sr@cyrillic";
          case SUBLANG_SERBIAN_CYRILLIC: return "sr_CS@cyrillic";
          case 0x0a: return "sr_RS@cyrillic";
          case 0x0c: return "sr_ME@cyrillic";
          case 0x07: return "sr_BA@cyrillic";
           
          case 0x1e: return "bs";
          case 0x1a: return "bs";  
          case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN: return "bs_BA";  
          case 0x19: return "bs@cyrillic";
          case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC: return "bs_BA@cyrillic";
          }
        return "hr";
      case LANG_CZECH:
        switch (sub)
          {
          case SUBLANG_CZECH_CZECH_REPUBLIC: return "cs_CZ";
          }
        return "cs";
      case LANG_DANISH:
        switch (sub)
          {
          case SUBLANG_DANISH_DENMARK: return "da_DK";
          }
        return "da";
      case LANG_DARI:
         
        switch (sub)
          {
          case SUBLANG_DARI_AFGHANISTAN: return "prs_AF";
          }
        return "prs";
      case LANG_DIVEHI:
        switch (sub)
          {
          case SUBLANG_DIVEHI_MALDIVES: return "dv_MV";
          }
        return "dv";
      case LANG_DUTCH:
        switch (sub)
          {
          case SUBLANG_DUTCH: return "nl_NL";
          case SUBLANG_DUTCH_BELGIAN:   return "nl_BE";
          case SUBLANG_DUTCH_SURINAM: return "nl_SR";
          }
        return "nl";
      case LANG_EDO:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "bin_NG";
          }
        return "bin";
      case LANG_ENGLISH:
        switch (sub)
          {
           
          case SUBLANG_ENGLISH_US: return "en_US";
          case SUBLANG_ENGLISH_UK: return "en_GB";
          case SUBLANG_ENGLISH_AUS: return "en_AU";
          case SUBLANG_ENGLISH_CAN: return "en_CA";
          case SUBLANG_ENGLISH_NZ: return "en_NZ";
          case SUBLANG_ENGLISH_EIRE: return "en_IE";
          case SUBLANG_ENGLISH_SOUTH_AFRICA: return "en_ZA";
          case SUBLANG_ENGLISH_JAMAICA: return "en_JM";
          case SUBLANG_ENGLISH_CARIBBEAN: return "en_GD";  
          case SUBLANG_ENGLISH_BELIZE: return "en_BZ";
          case SUBLANG_ENGLISH_TRINIDAD: return "en_TT";
          case SUBLANG_ENGLISH_ZIMBABWE: return "en_ZW";
          case SUBLANG_ENGLISH_PHILIPPINES: return "en_PH";
          case SUBLANG_ENGLISH_INDONESIA: return "en_ID";
          case SUBLANG_ENGLISH_HONGKONG: return "en_HK";
          case SUBLANG_ENGLISH_INDIA: return "en_IN";
          case SUBLANG_ENGLISH_MALAYSIA: return "en_MY";
          case SUBLANG_ENGLISH_SINGAPORE: return "en_SG";
          }
        return "en";
      case LANG_ESTONIAN:
        switch (sub)
          {
          case SUBLANG_ESTONIAN_ESTONIA: return "et_EE";
          }
        return "et";
      case LANG_FAEROESE:
        switch (sub)
          {
          case SUBLANG_FAEROESE_FAROE_ISLANDS: return "fo_FO";
          }
        return "fo";
      case LANG_FARSI:
        switch (sub)
          {
          case SUBLANG_FARSI_IRAN: return "fa_IR";
          }
        return "fa";
      case LANG_FINNISH:
        switch (sub)
          {
          case SUBLANG_FINNISH_FINLAND: return "fi_FI";
          }
        return "fi";
      case LANG_FRENCH:
        switch (sub)
          {
          case SUBLANG_FRENCH: return "fr_FR";
          case SUBLANG_FRENCH_BELGIAN:   return "fr_BE";
          case SUBLANG_FRENCH_CANADIAN: return "fr_CA";
          case SUBLANG_FRENCH_SWISS: return "fr_CH";
          case SUBLANG_FRENCH_LUXEMBOURG: return "fr_LU";
          case SUBLANG_FRENCH_MONACO: return "fr_MC";
          case SUBLANG_FRENCH_WESTINDIES: return "fr";  
          case SUBLANG_FRENCH_REUNION: return "fr_RE";
          case SUBLANG_FRENCH_CONGO: return "fr_CG";
          case SUBLANG_FRENCH_SENEGAL: return "fr_SN";
          case SUBLANG_FRENCH_CAMEROON: return "fr_CM";
          case SUBLANG_FRENCH_COTEDIVOIRE: return "fr_CI";
          case SUBLANG_FRENCH_MALI: return "fr_ML";
          case SUBLANG_FRENCH_MOROCCO: return "fr_MA";
          case SUBLANG_FRENCH_HAITI: return "fr_HT";
          }
        return "fr";
      case LANG_FRISIAN:
        switch (sub)
          {
          case SUBLANG_FRISIAN_NETHERLANDS: return "fy_NL";
          }
        return "fy";
      case LANG_FULFULDE:
         
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ff_NG";
          }
        return "ff";
      case LANG_GAELIC:
        switch (sub)
          {
          case 0x01:  
             
            return "gd_GB";
          case SUBLANG_IRISH_IRELAND: return "ga_IE";
          }
        return "ga";
      case LANG_GALICIAN:
        switch (sub)
          {
          case SUBLANG_GALICIAN_SPAIN: return "gl_ES";
          }
        return "gl";
      case LANG_GEORGIAN:
        switch (sub)
          {
          case SUBLANG_GEORGIAN_GEORGIA: return "ka_GE";
          }
        return "ka";
      case LANG_GERMAN:
        switch (sub)
          {
          case SUBLANG_GERMAN: return "de_DE";
          case SUBLANG_GERMAN_SWISS: return "de_CH";
          case SUBLANG_GERMAN_AUSTRIAN: return "de_AT";
          case SUBLANG_GERMAN_LUXEMBOURG: return "de_LU";
          case SUBLANG_GERMAN_LIECHTENSTEIN: return "de_LI";
          }
        return "de";
      case LANG_GREEK:
        switch (sub)
          {
          case SUBLANG_GREEK_GREECE: return "el_GR";
          }
        return "el";
      case LANG_GREENLANDIC:
        switch (sub)
          {
          case SUBLANG_GREENLANDIC_GREENLAND: return "kl_GL";
          }
        return "kl";
      case LANG_GUARANI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "gn_PY";
          }
        return "gn";
      case LANG_GUJARATI:
        switch (sub)
          {
          case SUBLANG_GUJARATI_INDIA: return "gu_IN";
          }
        return "gu";
      case LANG_HAUSA:
        switch (sub)
          {
          case 0x1f: return "ha";
          case SUBLANG_HAUSA_NIGERIA_LATIN: return "ha_NG";
          }
        return "ha";
      case LANG_HAWAIIAN:
         
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "cpe_US";
          }
        return "cpe";
      case LANG_HEBREW:
        switch (sub)
          {
          case SUBLANG_HEBREW_ISRAEL: return "he_IL";
          }
        return "he";
      case LANG_HINDI:
        switch (sub)
          {
          case SUBLANG_HINDI_INDIA: return "hi_IN";
          }
        return "hi";
      case LANG_HUNGARIAN:
        switch (sub)
          {
          case SUBLANG_HUNGARIAN_HUNGARY: return "hu_HU";
          }
        return "hu";
      case LANG_IBIBIO:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "nic_NG";
          }
        return "nic";
      case LANG_ICELANDIC:
        switch (sub)
          {
          case SUBLANG_ICELANDIC_ICELAND: return "is_IS";
          }
        return "is";
      case LANG_IGBO:
        switch (sub)
          {
          case SUBLANG_IGBO_NIGERIA: return "ig_NG";
          }
        return "ig";
      case LANG_INDONESIAN:
        switch (sub)
          {
          case SUBLANG_INDONESIAN_INDONESIA: return "id_ID";
          }
        return "id";
      case LANG_INUKTITUT:
        switch (sub)
          {
          case 0x1e: return "iu";  
          case SUBLANG_INUKTITUT_CANADA: return "iu_CA";  
          case 0x1f: return "iu@latin";
          case SUBLANG_INUKTITUT_CANADA_LATIN: return "iu_CA@latin";
          }
        return "iu";
      case LANG_ITALIAN:
        switch (sub)
          {
          case SUBLANG_ITALIAN: return "it_IT";
          case SUBLANG_ITALIAN_SWISS: return "it_CH";
          }
        return "it";
      case LANG_JAPANESE:
        switch (sub)
          {
          case SUBLANG_JAPANESE_JAPAN: return "ja_JP";
          }
        return "ja";
      case LANG_KANNADA:
        switch (sub)
          {
          case SUBLANG_KANNADA_INDIA: return "kn_IN";
          }
        return "kn";
      case LANG_KANURI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "kr_NG";
          }
        return "kr";
      case LANG_KASHMIRI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ks_PK";
          case SUBLANG_KASHMIRI_INDIA: return "ks_IN";
          }
        return "ks";
      case LANG_KAZAK:
        switch (sub)
          {
          case SUBLANG_KAZAK_KAZAKHSTAN: return "kk_KZ";
          }
        return "kk";
      case LANG_KICHE:
         
        switch (sub)
          {
          case SUBLANG_KICHE_GUATEMALA: return "qut_GT";
          }
        return "qut";
      case LANG_KINYARWANDA:
        switch (sub)
          {
          case SUBLANG_KINYARWANDA_RWANDA: return "rw_RW";
          }
        return "rw";
      case LANG_KONKANI:
         
        switch (sub)
          {
          case SUBLANG_KONKANI_INDIA: return "kok_IN";
          }
        return "kok";
      case LANG_KOREAN:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ko_KR";
          }
        return "ko";
      case LANG_KYRGYZ:
        switch (sub)
          {
          case SUBLANG_KYRGYZ_KYRGYZSTAN: return "ky_KG";
          }
        return "ky";
      case LANG_LAO:
        switch (sub)
          {
          case SUBLANG_LAO_LAOS: return "lo_LA";
          }
        return "lo";
      case LANG_LATIN:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "la_VA";
          }
        return "la";
      case LANG_LATVIAN:
        switch (sub)
          {
          case SUBLANG_LATVIAN_LATVIA: return "lv_LV";
          }
        return "lv";
      case LANG_LITHUANIAN:
        switch (sub)
          {
          case SUBLANG_LITHUANIAN_LITHUANIA: return "lt_LT";
          }
        return "lt";
      case LANG_LUXEMBOURGISH:
        switch (sub)
          {
          case SUBLANG_LUXEMBOURGISH_LUXEMBOURG: return "lb_LU";
          }
        return "lb";
      case LANG_MACEDONIAN:
        switch (sub)
          {
          case SUBLANG_MACEDONIAN_MACEDONIA: return "mk_MK";
          }
        return "mk";
      case LANG_MALAY:
        switch (sub)
          {
          case SUBLANG_MALAY_MALAYSIA: return "ms_MY";
          case SUBLANG_MALAY_BRUNEI_DARUSSALAM: return "ms_BN";
          }
        return "ms";
      case LANG_MALAYALAM:
        switch (sub)
          {
          case SUBLANG_MALAYALAM_INDIA: return "ml_IN";
          }
        return "ml";
      case LANG_MALTESE:
        switch (sub)
          {
          case SUBLANG_MALTESE_MALTA: return "mt_MT";
          }
        return "mt";
      case LANG_MANIPURI:
         
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "mni_IN";
          }
        return "mni";
      case LANG_MAORI:
        switch (sub)
          {
          case SUBLANG_MAORI_NEW_ZEALAND: return "mi_NZ";
          }
        return "mi";
      case LANG_MAPUDUNGUN:
        switch (sub)
          {
          case SUBLANG_MAPUDUNGUN_CHILE: return "arn_CL";
          }
        return "arn";
      case LANG_MARATHI:
        switch (sub)
          {
          case SUBLANG_MARATHI_INDIA: return "mr_IN";
          }
        return "mr";
      case LANG_MOHAWK:
        switch (sub)
          {
          case SUBLANG_MOHAWK_CANADA: return "moh_CA";
          }
        return "moh";
      case LANG_MONGOLIAN:
        switch (sub)
          {
          case SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA: case 0x1e: return "mn_MN";
          case SUBLANG_MONGOLIAN_PRC: case 0x1f: return "mn_CN";
          }
        return "mn";  
      case LANG_NEPALI:
        switch (sub)
          {
          case SUBLANG_NEPALI_NEPAL: return "ne_NP";
          case SUBLANG_NEPALI_INDIA: return "ne_IN";
          }
        return "ne";
      case LANG_NORWEGIAN:
        switch (sub)
          {
          case 0x1f: return "nb";
          case SUBLANG_NORWEGIAN_BOKMAL: return "nb_NO";
          case 0x1e: return "nn";
          case SUBLANG_NORWEGIAN_NYNORSK: return "nn_NO";
          }
        return "no";
      case LANG_OCCITAN:
        switch (sub)
          {
          case SUBLANG_OCCITAN_FRANCE: return "oc_FR";
          }
        return "oc";
      case LANG_ORIYA:
        switch (sub)
          {
          case SUBLANG_ORIYA_INDIA: return "or_IN";
          }
        return "or";
      case LANG_OROMO:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "om_ET";
          }
        return "om";
      case LANG_PAPIAMENTU:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "pap_AN";
          }
        return "pap";
      case LANG_PASHTO:
        switch (sub)
          {
          case SUBLANG_PASHTO_AFGHANISTAN: return "ps_AF";
          }
        return "ps";  
      case LANG_POLISH:
        switch (sub)
          {
          case SUBLANG_POLISH_POLAND: return "pl_PL";
          }
        return "pl";
      case LANG_PORTUGUESE:
        switch (sub)
          {
           
          case SUBLANG_PORTUGUESE_BRAZILIAN: return "pt_BR";
          case SUBLANG_PORTUGUESE: return "pt_PT";
          }
        return "pt";
      case LANG_PUNJABI:
        switch (sub)
          {
          case SUBLANG_PUNJABI_INDIA: return "pa_IN";  
          case SUBLANG_PUNJABI_PAKISTAN: return "pa_PK";  
          }
        return "pa";
      case LANG_QUECHUA:
         
        switch (sub)
          {
          case SUBLANG_QUECHUA_BOLIVIA: return "qu_BO";
          case SUBLANG_QUECHUA_ECUADOR: return "qu_EC";
          case SUBLANG_QUECHUA_PERU: return "qu_PE";
          }
        return "qu";
      case LANG_ROMANIAN:
        switch (sub)
          {
          case SUBLANG_ROMANIAN_ROMANIA: return "ro_RO";
          case SUBLANG_ROMANIAN_MOLDOVA: return "ro_MD";
          }
        return "ro";
      case LANG_ROMANSH:
        switch (sub)
          {
          case SUBLANG_ROMANSH_SWITZERLAND: return "rm_CH";
          }
        return "rm";
      case LANG_RUSSIAN:
        switch (sub)
          {
          case SUBLANG_RUSSIAN_RUSSIA: return "ru_RU";
          case SUBLANG_RUSSIAN_MOLDAVIA: return "ru_MD";
          }
        return "ru";  
      case LANG_SAMI:
        switch (sub)
          {
           
          case 0x00: return "se";
          case SUBLANG_SAMI_NORTHERN_NORWAY: return "se_NO";
          case SUBLANG_SAMI_NORTHERN_SWEDEN: return "se_SE";
          case SUBLANG_SAMI_NORTHERN_FINLAND: return "se_FI";
           
          case 0x1f: return "smj";
          case SUBLANG_SAMI_LULE_NORWAY: return "smj_NO";
          case SUBLANG_SAMI_LULE_SWEDEN: return "smj_SE";
           
          case 0x1e: return "sma";
          case SUBLANG_SAMI_SOUTHERN_NORWAY: return "sma_NO";
          case SUBLANG_SAMI_SOUTHERN_SWEDEN: return "sma_SE";
           
          case 0x1d: return "sms";
          case SUBLANG_SAMI_SKOLT_FINLAND: return "sms_FI";
           
          case 0x1c: return "smn";
          case SUBLANG_SAMI_INARI_FINLAND: return "smn_FI";
          }
        return "se";  
      case LANG_SANSKRIT:
        switch (sub)
          {
          case SUBLANG_SANSKRIT_INDIA: return "sa_IN";
          }
        return "sa";
      case LANG_SCOTTISH_GAELIC:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "gd_GB";
          }
        return "gd";
      case LANG_SINDHI:
        switch (sub)
          {
          case SUBLANG_SINDHI_INDIA: return "sd_IN";
          case SUBLANG_SINDHI_PAKISTAN: return "sd_PK";
           
          }
        return "sd";
      case LANG_SINHALESE:
        switch (sub)
          {
          case SUBLANG_SINHALESE_SRI_LANKA: return "si_LK";
          }
        return "si";
      case LANG_SLOVAK:
        switch (sub)
          {
          case SUBLANG_SLOVAK_SLOVAKIA: return "sk_SK";
          }
        return "sk";
      case LANG_SLOVENIAN:
        switch (sub)
          {
          case SUBLANG_SLOVENIAN_SLOVENIA: return "sl_SI";
          }
        return "sl";
      case LANG_SOMALI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "so_SO";
          }
        return "so";
      case LANG_SORBIAN:
         
        switch (sub)
          {
           
          case 0x00: return "hsb";
          case SUBLANG_UPPER_SORBIAN_GERMANY: return "hsb_DE";
           
          case 0x1f: return "dsb";
          case SUBLANG_LOWER_SORBIAN_GERMANY: return "dsb_DE";
          }
        return "wen";
      case LANG_SOTHO:
         
        switch (sub)
          {
          case SUBLANG_SOTHO_SOUTH_AFRICA: return "nso_ZA";
          }
        return "nso";
      case LANG_SPANISH:
        switch (sub)
          {
          case SUBLANG_SPANISH: return "es_ES";
          case SUBLANG_SPANISH_MEXICAN: return "es_MX";
          case SUBLANG_SPANISH_MODERN:
            return "es_ES@modern";       
          case SUBLANG_SPANISH_GUATEMALA: return "es_GT";
          case SUBLANG_SPANISH_COSTA_RICA: return "es_CR";
          case SUBLANG_SPANISH_PANAMA: return "es_PA";
          case SUBLANG_SPANISH_DOMINICAN_REPUBLIC: return "es_DO";
          case SUBLANG_SPANISH_VENEZUELA: return "es_VE";
          case SUBLANG_SPANISH_COLOMBIA: return "es_CO";
          case SUBLANG_SPANISH_PERU: return "es_PE";
          case SUBLANG_SPANISH_ARGENTINA: return "es_AR";
          case SUBLANG_SPANISH_ECUADOR: return "es_EC";
          case SUBLANG_SPANISH_CHILE: return "es_CL";
          case SUBLANG_SPANISH_URUGUAY: return "es_UY";
          case SUBLANG_SPANISH_PARAGUAY: return "es_PY";
          case SUBLANG_SPANISH_BOLIVIA: return "es_BO";
          case SUBLANG_SPANISH_EL_SALVADOR: return "es_SV";
          case SUBLANG_SPANISH_HONDURAS: return "es_HN";
          case SUBLANG_SPANISH_NICARAGUA: return "es_NI";
          case SUBLANG_SPANISH_PUERTO_RICO: return "es_PR";
          case SUBLANG_SPANISH_US: return "es_US";
          }
        return "es";
      case LANG_SUTU:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "bnt_TZ";  
          }
        return "bnt";
      case LANG_SWAHILI:
        switch (sub)
          {
          case SUBLANG_SWAHILI_KENYA: return "sw_KE";
          }
        return "sw";
      case LANG_SWEDISH:
        switch (sub)
          {
          case SUBLANG_SWEDISH_SWEDEN: return "sv_SE";
          case SUBLANG_SWEDISH_FINLAND: return "sv_FI";
          }
        return "sv";
      case LANG_SYRIAC:
        switch (sub)
          {
          case SUBLANG_SYRIAC_SYRIA: return "syr_SY";  
          }
        return "syr";
      case LANG_TAGALOG:
        switch (sub)
          {
          case SUBLANG_TAGALOG_PHILIPPINES: return "tl_PH";  
          }
        return "tl";  
      case LANG_TAJIK:
        switch (sub)
          {
          case 0x1f: return "tg";
          case SUBLANG_TAJIK_TAJIKISTAN: return "tg_TJ";
          }
        return "tg";
      case LANG_TAMAZIGHT:
         
        switch (sub)
          {
           
          case SUBLANG_TAMAZIGHT_ARABIC: return "ber_MA@arabic";
          case 0x1f: return "ber@latin";
          case SUBLANG_TAMAZIGHT_ALGERIA_LATIN: return "ber_DZ@latin";
          }
        return "ber";
      case LANG_TAMIL:
        switch (sub)
          {
          case SUBLANG_TAMIL_INDIA: return "ta_IN";
          }
        return "ta";  
      case LANG_TATAR:
        switch (sub)
          {
          case SUBLANG_TATAR_RUSSIA: return "tt_RU";
          }
        return "tt";
      case LANG_TELUGU:
        switch (sub)
          {
          case SUBLANG_TELUGU_INDIA: return "te_IN";
          }
        return "te";
      case LANG_THAI:
        switch (sub)
          {
          case SUBLANG_THAI_THAILAND: return "th_TH";
          }
        return "th";
      case LANG_TIBETAN:
        switch (sub)
          {
          case SUBLANG_TIBETAN_PRC:
             
            return "bo";
          case SUBLANG_TIBETAN_BHUTAN: return "bo_BT";
          }
        return "bo";
      case LANG_TIGRINYA:
        switch (sub)
          {
          case SUBLANG_TIGRINYA_ETHIOPIA: return "ti_ET";
          case SUBLANG_TIGRINYA_ERITREA: return "ti_ER";
          }
        return "ti";
      case LANG_TSONGA:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ts_ZA";
          }
        return "ts";
      case LANG_TSWANA:
         
        switch (sub)
          {
          case SUBLANG_TSWANA_SOUTH_AFRICA: return "tn_ZA";
          }
        return "tn";
      case LANG_TURKISH:
        switch (sub)
          {
          case SUBLANG_TURKISH_TURKEY: return "tr_TR";
          }
        return "tr";
      case LANG_TURKMEN:
        switch (sub)
          {
          case SUBLANG_TURKMEN_TURKMENISTAN: return "tk_TM";
          }
        return "tk";
      case LANG_UIGHUR:
        switch (sub)
          {
          case SUBLANG_UIGHUR_PRC: return "ug_CN";
          }
        return "ug";
      case LANG_UKRAINIAN:
        switch (sub)
          {
          case SUBLANG_UKRAINIAN_UKRAINE: return "uk_UA";
          }
        return "uk";
      case LANG_URDU:
        switch (sub)
          {
          case SUBLANG_URDU_PAKISTAN: return "ur_PK";
          case SUBLANG_URDU_INDIA: return "ur_IN";
          }
        return "ur";
      case LANG_UZBEK:
        switch (sub)
          {
          case 0x1f: return "uz";
          case SUBLANG_UZBEK_LATIN: return "uz_UZ";
          case 0x1e: return "uz@cyrillic";
          case SUBLANG_UZBEK_CYRILLIC: return "uz_UZ@cyrillic";
          }
        return "uz";
      case LANG_VENDA:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ve_ZA";
          }
        return "ve";
      case LANG_VIETNAMESE:
        switch (sub)
          {
          case SUBLANG_VIETNAMESE_VIETNAM: return "vi_VN";
          }
        return "vi";
      case LANG_WELSH:
        switch (sub)
          {
          case SUBLANG_WELSH_UNITED_KINGDOM: return "cy_GB";
          }
        return "cy";
      case LANG_WOLOF:
        switch (sub)
          {
          case SUBLANG_WOLOF_SENEGAL: return "wo_SN";
          }
        return "wo";
      case LANG_XHOSA:
        switch (sub)
          {
          case SUBLANG_XHOSA_SOUTH_AFRICA: return "xh_ZA";
          }
        return "xh";
      case LANG_YAKUT:
        switch (sub)
          {
          case SUBLANG_YAKUT_RUSSIA: return "sah_RU";
          }
        return "sah";
      case LANG_YI:
        switch (sub)
          {
          case SUBLANG_YI_PRC: return "ii_CN";
          }
        return "ii";
      case LANG_YIDDISH:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "yi_IL";
          }
        return "yi";
      case LANG_YORUBA:
        switch (sub)
          {
          case SUBLANG_YORUBA_NIGERIA: return "yo_NG";
          }
        return "yo";
      case LANG_ZULU:
        switch (sub)
          {
          case SUBLANG_ZULU_SOUTH_AFRICA: return "zu_ZA";
          }
        return "zu";
      default: return "C";
      }
  }
}

# if !defined IN_LIBINTL
static
# endif
const char *
gl_locale_name_from_win32_LCID (LCID lcid)
{
  LANGID langid;

   
  langid = LANGIDFROMLCID (lcid);

  return gl_locale_name_from_win32_LANGID (langid);
}

# ifdef WINDOWS_NATIVE

 
static LCID found_lcid;
static char lname[LC_MAX * (LOCALE_NAME_MAX_LENGTH + 1) + 1];

 
static BOOL CALLBACK
enum_locales_fn (LPSTR locale_num_str)
{
  char *endp;
  char locval[2 * LOCALE_NAME_MAX_LENGTH + 1 + 1];
  LCID try_lcid = strtoul (locale_num_str, &endp, 16);

  if (GetLocaleInfo (try_lcid, LOCALE_SENGLANGUAGE,
                    locval, LOCALE_NAME_MAX_LENGTH))
    {
      strcat (locval, "_");
      if (GetLocaleInfo (try_lcid, LOCALE_SENGCOUNTRY,
                        locval + strlen (locval), LOCALE_NAME_MAX_LENGTH))
       {
         size_t locval_len = strlen (locval);

         if (strncmp (locval, lname, locval_len) == 0
             && (lname[locval_len] == '.'
                 || lname[locval_len] == '\0'))
           {
             found_lcid = try_lcid;
             return FALSE;
           }
       }
    }
  return TRUE;
}

 
gl_lock_define_initialized(static, get_lcid_lock)

 
static LCID
get_lcid (const char *locale_name)
{
   
  static LCID last_lcid;
  static char last_locale[1000];

   
  gl_lock_lock (get_lcid_lock);
  if (last_lcid > 0 && strcmp (locale_name, last_locale) == 0)
    {
      gl_lock_unlock (get_lcid_lock);
      return last_lcid;
    }
  strncpy (lname, locale_name, sizeof (lname) - 1);
  lname[sizeof (lname) - 1] = '\0';
  found_lcid = 0;
  EnumSystemLocales (enum_locales_fn, LCID_SUPPORTED);
  if (found_lcid > 0)
    {
      last_lcid = found_lcid;
      strcpy (last_locale, locale_name);
    }
  gl_lock_unlock (get_lcid_lock);
  return found_lcid;
}

# endif
#endif


#if HAVE_GOOD_USELOCALE  

 

# define SIZE_BITS (sizeof (size_t) * CHAR_BIT)

 
static size_t _GL_ATTRIBUTE_PURE
string_hash (const void *x)
{
  const char *s = (const char *) x;
  size_t h = 0;

  for (; *s; s++)
    h = *s + ((h << 9) | (h >> (SIZE_BITS - 9)));

  return h;
}

 

 
struct struniq_hash_node
  {
    struct struniq_hash_node * volatile next;
    char contents[FLEXIBLE_ARRAY_MEMBER];
  };

# define STRUNIQ_HASH_TABLE_SIZE 257
static struct struniq_hash_node * volatile struniq_hash_table[STRUNIQ_HASH_TABLE_SIZE]
   ;

 
gl_lock_define_initialized(static, struniq_lock)

 
static const char *
struniq (const char *string)
{
  size_t hashcode = string_hash (string);
  size_t slot = hashcode % STRUNIQ_HASH_TABLE_SIZE;
  size_t size;
  struct struniq_hash_node *new_node;
  struct struniq_hash_node *p;
  for (p = struniq_hash_table[slot]; p != NULL; p = p->next)
    if (strcmp (p->contents, string) == 0)
      return p->contents;
  size = strlen (string) + 1;
  new_node =
    (struct struniq_hash_node *)
    malloc (FLEXSIZEOF (struct struniq_hash_node, contents, size));
  if (new_node == NULL)
     
    return "C";
  memcpy (new_node->contents, string, size);
  {
    bool mt = gl_multithreaded ();
     
    if (mt) gl_lock_lock (struniq_lock);
     
    for (p = struniq_hash_table[slot]; p != NULL; p = p->next)
      if (strcmp (p->contents, string) == 0)
        {
          free (new_node);
          new_node = p;
          goto done;
        }
     
    new_node->next = struniq_hash_table[slot];
    struniq_hash_table[slot] = new_node;
   done:
     
    if (mt) gl_lock_unlock (struniq_lock);
  }
  return new_node->contents;
}

#endif


#if LOCALENAME_ENHANCE_LOCALE_FUNCS

 

 
static const char *
get_locale_t_name (int category, locale_t locale)
{
  if (locale == LC_GLOBAL_LOCALE)
    {
       
      const char *name = setlocale_null (category);
      if (name != NULL)
        return struniq (name);
      else
         
        return "";
    }
  else
    {
       
      size_t hashcode = locale_hash_function (locale);
      size_t slot = hashcode % LOCALE_HASH_TABLE_SIZE;
       
      const char *name = "";
      struct locale_hash_node *p;
       
      gl_rwlock_rdlock (locale_lock);
      for (p = locale_hash_table[slot]; p != NULL; p = p->next)
        if (p->locale == locale)
          {
            name = p->names.category_name[category];
            break;
          }
      gl_rwlock_unlock (locale_lock);
      return name;
    }
}

# if !(defined newlocale && defined duplocale && defined freelocale)
#  error "newlocale, duplocale, freelocale not being replaced as expected!"
# endif

 
locale_t
newlocale (int category_mask, const char *name, locale_t base)
#undef newlocale
{
  struct locale_categories_names names;
  struct locale_hash_node *node;
  locale_t result;

   
  if (((LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK
        | LC_MONETARY_MASK | LC_MESSAGES_MASK)
       & category_mask) != 0)
    name = struniq (name);

   
  if (((LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK
        | LC_MONETARY_MASK | LC_MESSAGES_MASK)
       & ~category_mask) == 0)
    {
       
      int category;

      name = struniq (name);
      for (category = 0; category < 6; category++)
        names.category_name[category] = name;
    }
  else
    {
       
      if (base == NULL)
        {
          int category;

          for (category = 0; category < 6; category++)
            {
              int mask;

              switch (category)
                {
                case LC_CTYPE:
                  mask = LC_CTYPE_MASK;
                  break;
                case LC_NUMERIC:
                  mask = LC_NUMERIC_MASK;
                  break;
                case LC_TIME:
                  mask = LC_TIME_MASK;
                  break;
                case LC_COLLATE:
                  mask = LC_COLLATE_MASK;
                  break;
                case LC_MONETARY:
                  mask = LC_MONETARY_MASK;
                  break;
                case LC_MESSAGES:
                  mask = LC_MESSAGES_MASK;
                  break;
                default:
                  abort ();
                }
              names.category_name[category] =
                ((mask & category_mask) != 0 ? name : "C");
            }
        }
      else if (base == LC_GLOBAL_LOCALE)
        {
          int category;

          for (category = 0; category < 6; category++)
            {
              int mask;

              switch (category)
                {
                case LC_CTYPE:
                  mask = LC_CTYPE_MASK;
                  break;
                case LC_NUMERIC:
                  mask = LC_NUMERIC_MASK;
                  break;
                case LC_TIME:
                  mask = LC_TIME_MASK;
                  break;
                case LC_COLLATE:
                  mask = LC_COLLATE_MASK;
                  break;
                case LC_MONETARY:
                  mask = LC_MONETARY_MASK;
                  break;
                case LC_MESSAGES:
                  mask = LC_MESSAGES_MASK;
                  break;
                default:
                  abort ();
                }
              names.category_name[category] =
                ((mask & category_mask) != 0
                 ? name
                 : get_locale_t_name (category, LC_GLOBAL_LOCALE));
            }
        }
      else
        {
           
          struct locale_hash_node *p;
          int category;

           
          gl_rwlock_rdlock (locale_lock);
          for (p = locale_hash_table[locale_hash_function (base) % LOCALE_HASH_TABLE_SIZE];
               p != NULL;
               p = p->next)
            if (p->locale == base)
              break;

          for (category = 0; category < 6; category++)
            {
              int mask;

              switch (category)
                {
                case LC_CTYPE:
                  mask = LC_CTYPE_MASK;
                  break;
                case LC_NUMERIC:
                  mask = LC_NUMERIC_MASK;
                  break;
                case LC_TIME:
                  mask = LC_TIME_MASK;
                  break;
                case LC_COLLATE:
                  mask = LC_COLLATE_MASK;
                  break;
                case LC_MONETARY:
                  mask = LC_MONETARY_MASK;
                  break;
                case LC_MESSAGES:
                  mask = LC_MESSAGES_MASK;
                  break;
                default:
                  abort ();
                }
              names.category_name[category] =
                ((mask & category_mask) != 0
                 ? name
                 : (p != NULL ? p->names.category_name[category] : ""));
            }

          gl_rwlock_unlock (locale_lock);
        }
    }

  node = (struct locale_hash_node *) malloc (sizeof (struct locale_hash_node));
  if (node == NULL)
     
    return NULL;

  result = newlocale (category_mask, name, base);
  if (result == NULL)
    {
      free (node);
      return NULL;
    }

   
  node->locale = result;
  node->names = names;

   
  {
    size_t hashcode = locale_hash_function (result);
    size_t slot = hashcode % LOCALE_HASH_TABLE_SIZE;
    struct locale_hash_node *p;

     
    gl_rwlock_wrlock (locale_lock);
    for (p = locale_hash_table[slot]; p != NULL; p = p->next)
      if (p->locale == result)
        {
           
          p->names = node->names;
          break;
        }
    if (p == NULL)
      {
        node->next = locale_hash_table[slot];
        locale_hash_table[slot] = node;
      }

    gl_rwlock_unlock (locale_lock);

    if (p != NULL)
      free (node);
  }

  return result;
}

 
locale_t
duplocale (locale_t locale)
#undef duplocale
{
  struct locale_hash_node *node;
  locale_t result;

  if (locale == NULL)
     
    abort ();

  node = (struct locale_hash_node *) malloc (sizeof (struct locale_hash_node));
  if (node == NULL)
     
    return NULL;

  result = duplocale (locale);
  if (result == NULL)
    {
      free (node);
      return NULL;
    }

   
  node->locale = result;
  if (locale == LC_GLOBAL_LOCALE)
    {
      int category;

      for (category = 0; category < 6; category++)
        node->names.category_name[category] =
          get_locale_t_name (category, LC_GLOBAL_LOCALE);

       
      gl_rwlock_wrlock (locale_lock);
    }
  else
    {
      struct locale_hash_node *p;

       
      gl_rwlock_wrlock (locale_lock);

      for (p = locale_hash_table[locale_hash_function (locale) % LOCALE_HASH_TABLE_SIZE];
           p != NULL;
           p = p->next)
        if (p->locale == locale)
          break;
      if (p != NULL)
        node->names = p->names;
      else
        {
           
          int category;

          for (category = 0; category < 6; category++)
            node->names.category_name[category] = "";
        }
    }

   
  {
    size_t hashcode = locale_hash_function (result);
    size_t slot = hashcode % LOCALE_HASH_TABLE_SIZE;
    struct locale_hash_node *p;

    for (p = locale_hash_table[slot]; p != NULL; p = p->next)
      if (p->locale == result)
        {
           
          p->names = node->names;
          break;
        }
    if (p == NULL)
      {
        node->next = locale_hash_table[slot];
        locale_hash_table[slot] = node;
      }

    gl_rwlock_unlock (locale_lock);

    if (p != NULL)
      free (node);
  }

  return result;
}

 
void
freelocale (locale_t locale)
#undef freelocale
{
  if (locale == NULL || locale == LC_GLOBAL_LOCALE)
     
    abort ();

  {
    size_t hashcode = locale_hash_function (locale);
    size_t slot = hashcode % LOCALE_HASH_TABLE_SIZE;
    struct locale_hash_node *found;
    struct locale_hash_node **p;

    found = NULL;
     
    gl_rwlock_wrlock (locale_lock);
    for (p = &locale_hash_table[slot]; *p != NULL; p = &(*p)->next)
      if ((*p)->locale == locale)
        {
          found = *p;
          *p = (*p)->next;
          break;
        }
    gl_rwlock_unlock (locale_lock);
    free (found);
  }

  freelocale (locale);
}

#endif


#if defined IN_LIBINTL || HAVE_GOOD_USELOCALE

 
# if !defined IN_LIBINTL
static
# endif
const char *
gl_locale_name_thread_unsafe (int category, _GL_UNUSED const char *categoryname)
{
# if HAVE_GOOD_USELOCALE
  {
    locale_t thread_locale = uselocale (NULL);
    if (thread_locale != LC_GLOBAL_LOCALE)
      {
#  if __GLIBC__ >= 2 && !defined __UCLIBC__
         
        const char *name =
          nl_langinfo (_NL_ITEM ((category), _NL_ITEM_INDEX (-1)));
        if (name[0] == '\0')
           
          name = thread_locale->__names[category];
        return name;
#  elif defined __linux__ && HAVE_LANGINFO_H && defined NL_LOCALE_NAME
         
        return nl_langinfo_l (NL_LOCALE_NAME (category), thread_locale);
#  elif (defined __FreeBSD__ || defined __DragonFly__) || (defined __APPLE__ && defined __MACH__)
         
        int mask;

        switch (category)
          {
          case LC_CTYPE:
            mask = LC_CTYPE_MASK;
            break;
          case LC_NUMERIC:
            mask = LC_NUMERIC_MASK;
            break;
          case LC_TIME:
            mask = LC_TIME_MASK;
            break;
          case LC_COLLATE:
            mask = LC_COLLATE_MASK;
            break;
          case LC_MONETARY:
            mask = LC_MONETARY_MASK;
            break;
          case LC_MESSAGES:
            mask = LC_MESSAGES_MASK;
            break;
          default:  
            return "";
          }
        return querylocale (mask, thread_locale);
#  elif defined __sun
#   if HAVE_GETLOCALENAME_L
         
        return getlocalename_l (category, thread_locale);
#   elif HAVE_SOLARIS114_LOCALES
         
        void *lcp = (*thread_locale)->core.data->lcp;
        if (lcp != NULL)
          switch (category)
            {
            case LC_CTYPE:
            case LC_NUMERIC:
            case LC_TIME:
            case LC_COLLATE:
            case LC_MONETARY:
            case LC_MESSAGES:
              return ((const char * const *) lcp)[category];
            default:  
              return "";
            }
#   elif HAVE_NAMELESS_LOCALES
        return get_locale_t_name (category, thread_locale);
#   else
         
        switch (category)
          {
          case LC_CTYPE:
          case LC_NUMERIC:
          case LC_TIME:
          case LC_COLLATE:
          case LC_MONETARY:
          case LC_MESSAGES:
            return ((const char * const *) thread_locale)[category];
          default:  
            return "";
          }
#   endif
#  elif defined _AIX && HAVE_NAMELESS_LOCALES
        return get_locale_t_name (category, thread_locale);
#  elif defined __CYGWIN__
         
#   ifdef NL_LOCALE_NAME
        return nl_langinfo_l (NL_LOCALE_NAME (category), thread_locale);
#   else
         
        struct __locale_t {
          char categories[7][32];
        };
        return ((struct __locale_t *) thread_locale)->categories[category];
#   endif
#  elif defined __HAIKU__
         
        struct LocaleBackendData {
          int magic;
          void   *backend;
          void   *databridge;
        };
        void *thread_locale_backend =
          ((struct LocaleBackendData *) thread_locale)->backend;
        if (thread_locale_backend != NULL)
          {
             
            static void * volatile querylocale_method  ;
            static int volatile querylocale_found  ;
             
            if (querylocale_found == 0)
              {
                void *handle =
                  dlopen ("/boot/system/lib/libroot-addon-icu.so", 0);
                if (handle != NULL)
                  {
                    void *sym =
                      dlsym (handle, "_ZN8BPrivate7Libroot16ICULocaleBackend12_QueryLocaleEi");
                    if (sym != NULL)
                      {
                        querylocale_method = sym;
                        querylocale_found = 1;
                      }
                    else
                       
                      querylocale_found = -1;
                  }
                else
                   
                  querylocale_found = -1;
              }
            if (querylocale_found > 0)
              {
                 
                const char * (*querylocale_func) (void *, int) =
                  (const char * (*) (void *, int)) querylocale_method;
                return querylocale_func (thread_locale_backend, category);
              }
          }
        else
           
          return "C";
#  elif defined __ANDROID__
        return MB_CUR_MAX == 4 ? "C.UTF-8" : "C";
#  endif
      }
  }
# endif
  return NULL;
}

#endif

const char *
gl_locale_name_thread (int category, _GL_UNUSED const char *categoryname)
{
#if HAVE_GOOD_USELOCALE
  const char *name = gl_locale_name_thread_unsafe (category, categoryname);
  if (name != NULL)
    return struniq (name);
#endif
   
  return NULL;
}

 
#if defined _LIBC || ((defined __GLIBC__ && __GLIBC__ >= 2) && !defined __UCLIBC__)
# define HAVE_LOCALE_NULL
#endif

const char *
gl_locale_name_posix (int category, _GL_UNUSED const char *categoryname)
{
#if defined WINDOWS_NATIVE
  if (LC_MIN <= category && category <= LC_MAX)
    {
      const char *locname =
         
        setlocale (category, NULL);

       
      LCID lcid = get_lcid (locname);

      if (lcid > 0)
        return gl_locale_name_from_win32_LCID (lcid);
    }
#endif
  {
    const char *locname;

     
#if defined HAVE_LC_MESSAGES && defined HAVE_LOCALE_NULL
    locname = setlocale_null (category);
#else
     
     
    locname = gl_locale_name_environ (category, categoryname);
#endif
     
#if defined WINDOWS_NATIVE
    if (locname != NULL)
      {
         
        LCID lcid = get_lcid (locname);

        if (lcid > 0)
          return gl_locale_name_from_win32_LCID (lcid);
      }
#endif
    return locname;
  }
}

const char *
gl_locale_name_environ (_GL_UNUSED int category, const char *categoryname)
{
  const char *retval;

   
  retval = getenv ("LC_ALL");
  if (retval != NULL && retval[0] != '\0')
    return retval;
   
  retval = getenv (categoryname);
  if (retval != NULL && retval[0] != '\0')
    return retval;
   
  retval = getenv ("LANG");
  if (retval != NULL && retval[0] != '\0')
    {
#if HAVE_CFPREFERENCESCOPYAPPVALUE
       
      if (strcmp (retval, "UTF-8") != 0)
#endif
#if defined __CYGWIN__
       
      if (strcmp (retval, "C.UTF-8") != 0)
#endif
        return retval;
    }

  return NULL;
}

const char *
gl_locale_name_default (void)
{
   

#if !(HAVE_CFPREFERENCESCOPYAPPVALUE || defined WINDOWS_NATIVE || defined __CYGWIN__)

   
  return "C";

#else

   

# if HAVE_CFPREFERENCESCOPYAPPVALUE
   
   
  {
     
    static const char *cached_localename;

    if (cached_localename == NULL)
      {
        char namebuf[256];
        CFTypeRef value =
          CFPreferencesCopyAppValue (CFSTR ("AppleLocale"),
                                     kCFPreferencesCurrentApplication);
        if (value != NULL && CFGetTypeID (value) == CFStringGetTypeID ())
          {
            CFStringRef name = (CFStringRef)value;

            if (CFStringGetCString (name, namebuf, sizeof (namebuf),
                                    kCFStringEncodingASCII))
              {
                gl_locale_name_canonicalize (namebuf);
                cached_localename = strdup (namebuf);
              }
          }
        if (cached_localename == NULL)
          cached_localename = "C";
      }
    return cached_localename;
  }

# endif

# if defined WINDOWS_NATIVE || defined __CYGWIN__  
  {
    LCID lcid;

     
    lcid = GetThreadLocale ();

    return gl_locale_name_from_win32_LCID (lcid);
  }
# endif
#endif
}

 

const char *
gl_locale_name (int category, const char *categoryname)
{
  const char *retval;

  retval = gl_locale_name_thread (category, categoryname);
  if (retval != NULL)
    return retval;

  retval = gl_locale_name_posix (category, categoryname);
  if (retval != NULL)
    return retval;

  return gl_locale_name_default ();
}
