 

 

 

 

 

 
#define YYBISON 30802

 
#define YYBISON_VERSION "3.8.2"

 
#define YYSKELETON_NAME "yacc.c"

 
#define YYPURE 1

 
#define YYPUSH 0

 
#define YYPULL 1




 
#line 1 "parse-datetime.y"

 

 

#include <config.h>

#include "parse-datetime.h"

#include "idx.h"
#include "intprops.h"
#include "timespec.h"
#include "strftime.h"

 
#define YYSTACK_USE_ALLOCA 0

 
#define YYMAXDEPTH 20
#define YYINITDEPTH YYMAXDEPTH

#include <inttypes.h>
#include <c-ctype.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gettext.h"

#define _(str) gettext (str)

 
 
#ifdef _STDLIB_H_
# undef _STDLIB_H
# define _STDLIB_H 1
#endif

 
#define SHR(a, b)       \
  (-1 >> 1 == -1        \
   ? (a) >> (b)         \
   : (a) / (1 << (b)) - ((a) % (1 << (b)) < 0))

#define HOUR(x) (60 * 60 * (x))

#define STREQ(a, b) (strcmp (a, b) == 0)

 
static_assert (TYPE_IS_INTEGER (time_t));
static_assert (!TYPE_SIGNED (time_t) || INTMAX_MIN <= TYPE_MINIMUM (time_t));
static_assert (TYPE_MAXIMUM (time_t) <= INTMAX_MAX);

 
static bool
time_overflow (intmax_t n)
{
  return ! ((TYPE_SIGNED (time_t) ? TYPE_MINIMUM (time_t) <= n : 0 <= n)
            && n <= TYPE_MAXIMUM (time_t));
}

 
static unsigned char to_uchar (char ch) { return ch; }

static void _GL_ATTRIBUTE_FORMAT ((__printf__, 1, 2))
dbg_printf (char const *msg, ...)
{
  va_list args;
   
  fputs ("date: ", stderr);

  va_start (args, msg);
  vfprintf (stderr, msg, args);
  va_end (args);
}


 
typedef struct
{
  bool negative;
  intmax_t value;
  idx_t digits;
} textint;

 
typedef struct
{
  char const *name;
  int type;
  int value;
} table;

 
enum { MERam, MERpm, MER24 };

 
enum { DBGBUFSIZE = 100 };

enum { BILLION = 1000000000, LOG10_BILLION = 9 };

 
typedef struct
{
   
  intmax_t year;
  intmax_t month;
  intmax_t day;
  intmax_t hour;
  intmax_t minutes;
  intmax_t seconds;
  int ns;
} relative_time;

#if HAVE_COMPOUND_LITERALS
# define RELATIVE_TIME_0 ((relative_time) { 0, 0, 0, 0, 0, 0, 0 })
#else
static relative_time const RELATIVE_TIME_0;
#endif

 
typedef struct
{
   
  const char *input;

   
  intmax_t day_ordinal;

   
  int day_number;

   
  int local_isdst;

   
  int time_zone;

   
  int meridian;

   
  textint year;
  intmax_t month;
  intmax_t day;
  intmax_t hour;
  intmax_t minutes;
  struct timespec seconds;  

   
  relative_time rel;

   
  bool timespec_seen;
  bool rels_seen;
  idx_t dates_seen;
  idx_t days_seen;
  idx_t J_zones_seen;
  idx_t local_zones_seen;
  idx_t dsts_seen;
  idx_t times_seen;
  idx_t zones_seen;
  bool year_seen;

#ifdef GNULIB_PARSE_DATETIME2
   
  bool parse_datetime_debug;
#endif

   
  bool debug_dates_seen;
  bool debug_days_seen;
  bool debug_local_zones_seen;
  bool debug_times_seen;
  bool debug_zones_seen;
  bool debug_year_seen;

   
  bool debug_ordinal_day_seen;

   
  table local_time_zone_table[3];
} parser_control;

static bool
debugging (parser_control const *pc)
{
#ifdef GNULIB_PARSE_DATETIME2
  return pc->parse_datetime_debug;
#else
  return false;
#endif
}

union YYSTYPE;
static int yylex (union YYSTYPE *, parser_control *);
static int yyerror (parser_control const *, char const *);
static bool time_zone_hhmm (parser_control *, textint, intmax_t);

 
static void
digits_to_date_time (parser_control *pc, textint text_int)
{
  if (pc->dates_seen && ! pc->year.digits
      && ! pc->rels_seen && (pc->times_seen || 2 < text_int.digits))
    {
      pc->year_seen = true;
      pc->year = text_int;
    }
  else
    {
      if (4 < text_int.digits)
        {
          pc->dates_seen++;
          pc->day = text_int.value % 100;
          pc->month = (text_int.value / 100) % 100;
          pc->year.value = text_int.value / 10000;
          pc->year.digits = text_int.digits - 4;
        }
      else
        {
          pc->times_seen++;
          if (text_int.digits <= 2)
            {
              pc->hour = text_int.value;
              pc->minutes = 0;
            }
          else
            {
              pc->hour = text_int.value / 100;
              pc->minutes = text_int.value % 100;
            }
          pc->seconds = (struct timespec) {0};
          pc->meridian = MER24;
        }
    }
}

 
static bool
apply_relative_time (parser_control *pc, relative_time rel, int factor)
{
  if (factor < 0
      ? (ckd_sub (&pc->rel.ns, pc->rel.ns, rel.ns)
         | ckd_sub (&pc->rel.seconds, pc->rel.seconds, rel.seconds)
         | ckd_sub (&pc->rel.minutes, pc->rel.minutes, rel.minutes)
         | ckd_sub (&pc->rel.hour, pc->rel.hour, rel.hour)
         | ckd_sub (&pc->rel.day, pc->rel.day, rel.day)
         | ckd_sub (&pc->rel.month, pc->rel.month, rel.month)
         | ckd_sub (&pc->rel.year, pc->rel.year, rel.year))
      : (ckd_add (&pc->rel.ns, pc->rel.ns, rel.ns)
         | ckd_add (&pc->rel.seconds, pc->rel.seconds, rel.seconds)
         | ckd_add (&pc->rel.minutes, pc->rel.minutes, rel.minutes)
         | ckd_add (&pc->rel.hour, pc->rel.hour, rel.hour)
         | ckd_add (&pc->rel.day, pc->rel.day, rel.day)
         | ckd_add (&pc->rel.month, pc->rel.month, rel.month)
         | ckd_add (&pc->rel.year, pc->rel.year, rel.year)))
    return false;
  pc->rels_seen = true;
  return true;
}

 
static void
set_hhmmss (parser_control *pc, intmax_t hour, intmax_t minutes,
            time_t sec, int nsec)
{
  pc->hour = hour;
  pc->minutes = minutes;
  pc->seconds = (struct timespec) { .tv_sec = sec, .tv_nsec = nsec };
}

 
static const char *
str_days (parser_control *pc, char *buffer, int n)
{
   
  static char const ordinal_values[][11] = {
     "last",
     "this",
     "next/first",
     "(SECOND)",  
     "third",
     "fourth",
     "fifth",
     "sixth",
     "seventh",
     "eight",
     "ninth",
     "tenth",
     "eleventh",
     "twelfth"
  };

  static char const days_values[][4] = {
     "Sun",
     "Mon",
     "Tue",
     "Wed",
     "Thu",
     "Fri",
     "Sat"
  };

  int len;

   
  if (pc->debug_ordinal_day_seen)
    {
       
      len = (-1 <= pc->day_ordinal && pc->day_ordinal <= 12
             ? snprintf (buffer, n, "%s", ordinal_values[pc->day_ordinal + 1])
             : snprintf (buffer, n, "%"PRIdMAX, pc->day_ordinal));
    }
  else
    {
      buffer[0] = '\0';
      len = 0;
    }

   
  if (0 <= pc->day_number && pc->day_number <= 6 && 0 <= len && len < n)
    snprintf (buffer + len, n - len, &" %s"[len == 0],
              days_values[pc->day_number]);
  else
    {
       
    }
  return buffer;
}

 

enum { TIME_ZONE_BUFSIZE = INT_STRLEN_BOUND (intmax_t) + sizeof ":MM:SS" } ;

static char const *
time_zone_str (int time_zone, char time_zone_buf[TIME_ZONE_BUFSIZE])
{
  char *p = time_zone_buf;
  char sign = time_zone < 0 ? '-' : '+';
  int hour = abs (time_zone / (60 * 60));
  p += sprintf (time_zone_buf, "%c%02d", sign, hour);
  int offset_from_hour = abs (time_zone % (60 * 60));
  if (offset_from_hour != 0)
    {
      int mm = offset_from_hour / 60;
      int ss = offset_from_hour % 60;
      *p++ = ':';
      *p++ = '0' + mm / 10;
      *p++ = '0' + mm % 10;
      if (ss)
        {
          *p++ = ':';
          *p++ = '0' + ss / 10;
          *p++ = '0' + ss % 10;
        }
      *p = '\0';
    }
  return time_zone_buf;
}

 
static void
debug_print_current_time (char const *item, parser_control *pc)
{
  bool space = false;

  if (!debugging (pc))
    return;

   
  dbg_printf (_("parsed %s part: "), item);

  if (pc->dates_seen && !pc->debug_dates_seen)
    {
       
      fprintf (stderr, "(Y-M-D) %04"PRIdMAX"-%02"PRIdMAX"-%02"PRIdMAX,
              pc->year.value, pc->month, pc->day);
      pc->debug_dates_seen = true;
      space = true;
    }

  if (pc->year_seen != pc->debug_year_seen)
    {
      if (space)
        fputc (' ', stderr);
      fprintf (stderr, _("year: %04"PRIdMAX), pc->year.value);

      pc->debug_year_seen = pc->year_seen;
      space = true;
    }

  if (pc->times_seen && !pc->debug_times_seen)
    {
      intmax_t sec = pc->seconds.tv_sec;
      fprintf (stderr, &" %02"PRIdMAX":%02"PRIdMAX":%02"PRIdMAX[!space],
               pc->hour, pc->minutes, sec);
      if (pc->seconds.tv_nsec != 0)
        {
          int nsec = pc->seconds.tv_nsec;
          fprintf (stderr, ".%09d", nsec);
        }
      if (pc->meridian == MERpm)
        fputs ("pm", stderr);

      pc->debug_times_seen = true;
      space = true;
    }

  if (pc->days_seen && !pc->debug_days_seen)
    {
      if (space)
        fputc (' ', stderr);
      char tmp[DBGBUFSIZE];
      fprintf (stderr, _("%s (day ordinal=%"PRIdMAX" number=%d)"),
               str_days (pc, tmp, sizeof tmp),
               pc->day_ordinal, pc->day_number);
      pc->debug_days_seen = true;
      space = true;
    }

   
  if (pc->local_zones_seen && !pc->debug_local_zones_seen)
    {
      fprintf (stderr, &" isdst=%d%s"[!space],
               pc->local_isdst, pc->dsts_seen ? " DST" : "");
      pc->debug_local_zones_seen = true;
      space = true;
    }

  if (pc->zones_seen && !pc->debug_zones_seen)
    {
      char time_zone_buf[TIME_ZONE_BUFSIZE];
      fprintf (stderr, &" UTC%s"[!space],
               time_zone_str (pc->time_zone, time_zone_buf));
      pc->debug_zones_seen = true;
      space = true;
    }

  if (pc->timespec_seen)
    {
      intmax_t sec = pc->seconds.tv_sec;
      if (space)
        fputc (' ', stderr);
      fprintf (stderr, _("number of seconds: %"PRIdMAX), sec);
    }

  fputc ('\n', stderr);
}

 

static bool
print_rel_part (bool space, intmax_t val, char const *name)
{
  if (val == 0)
    return space;
  fprintf (stderr, &" %+"PRIdMAX" %s"[!space], val, name);
  return true;
}

static void
debug_print_relative_time (char const *item, parser_control const *pc)
{
  bool space = false;

  if (!debugging (pc))
    return;

   
  dbg_printf (_("parsed %s part: "), item);

  if (pc->rel.year == 0 && pc->rel.month == 0 && pc->rel.day == 0
      && pc->rel.hour == 0 && pc->rel.minutes == 0 && pc->rel.seconds == 0
      && pc->rel.ns == 0)
    {
       
      fputs (_("today/this/now\n"), stderr);
      return;
    }

  space = print_rel_part (space, pc->rel.year, "year(s)");
  space = print_rel_part (space, pc->rel.month, "month(s)");
  space = print_rel_part (space, pc->rel.day, "day(s)");
  space = print_rel_part (space, pc->rel.hour, "hour(s)");
  space = print_rel_part (space, pc->rel.minutes, "minutes");
  space = print_rel_part (space, pc->rel.seconds, "seconds");
  print_rel_part (space, pc->rel.ns, "nanoseconds");

  fputc ('\n', stderr);
}




#line 625 "parse-datetime.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parse-datetime-gen.h"
 
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                       
  YYSYMBOL_YYerror = 1,                     
  YYSYMBOL_YYUNDEF = 2,                     
  YYSYMBOL_tAGO = 3,                        
  YYSYMBOL_tDST = 4,                        
  YYSYMBOL_tYEAR_UNIT = 5,                  
  YYSYMBOL_tMONTH_UNIT = 6,                 
  YYSYMBOL_tHOUR_UNIT = 7,                  
  YYSYMBOL_tMINUTE_UNIT = 8,                
  YYSYMBOL_tSEC_UNIT = 9,                   
  YYSYMBOL_tDAY_UNIT = 10,                  
  YYSYMBOL_tDAY_SHIFT = 11,                 
  YYSYMBOL_tDAY = 12,                       
  YYSYMBOL_tDAYZONE = 13,                   
  YYSYMBOL_tLOCAL_ZONE = 14,                
  YYSYMBOL_tMERIDIAN = 15,                  
  YYSYMBOL_tMONTH = 16,                     
  YYSYMBOL_tORDINAL = 17,                   
  YYSYMBOL_tZONE = 18,                      
  YYSYMBOL_tSNUMBER = 19,                   
  YYSYMBOL_tUNUMBER = 20,                   
  YYSYMBOL_tSDECIMAL_NUMBER = 21,           
  YYSYMBOL_tUDECIMAL_NUMBER = 22,           
  YYSYMBOL_23_ = 23,                        
  YYSYMBOL_24_J_ = 24,                      
  YYSYMBOL_25_T_ = 25,                      
  YYSYMBOL_26_ = 26,                        
  YYSYMBOL_27_ = 27,                        
  YYSYMBOL_28_ = 28,                        
  YYSYMBOL_YYACCEPT = 29,                   
  YYSYMBOL_spec = 30,                       
  YYSYMBOL_timespec = 31,                   
  YYSYMBOL_items = 32,                      
  YYSYMBOL_item = 33,                       
  YYSYMBOL_datetime = 34,                   
  YYSYMBOL_iso_8601_datetime = 35,          
  YYSYMBOL_time = 36,                       
  YYSYMBOL_iso_8601_time = 37,              
  YYSYMBOL_o_zone_offset = 38,              
  YYSYMBOL_zone_offset = 39,                
  YYSYMBOL_local_zone = 40,                 
  YYSYMBOL_zone = 41,                       
  YYSYMBOL_day = 42,                        
  YYSYMBOL_date = 43,                       
  YYSYMBOL_iso_8601_date = 44,              
  YYSYMBOL_rel = 45,                        
  YYSYMBOL_relunit = 46,                    
  YYSYMBOL_relunit_snumber = 47,            
  YYSYMBOL_dayshift = 48,                   
  YYSYMBOL_seconds = 49,                    
  YYSYMBOL_signed_seconds = 50,             
  YYSYMBOL_unsigned_seconds = 51,           
  YYSYMBOL_number = 52,                     
  YYSYMBOL_hybrid = 53,                     
  YYSYMBOL_o_colon_minutes = 54             
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

 

#ifndef __PTRDIFF_MAX__
# include <limits.h>  
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h>  
#  define YY_STDINT_H
# endif
#endif

 

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

 
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h>  
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


 
typedef yytype_int8 yy_state_t;

 
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h>  
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

 
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E)  
#endif

 
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value)  
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

 

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h>  
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h>  
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h>  
       
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
    
#  define YYSTACK_FREE(Ptr) do {  ; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
     
#   define YYSTACK_ALLOC_MAXIMUM 4032  
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h>  
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T);  
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *);  
#   endif
#  endif
# endif
#endif  

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

 
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

 
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

 
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

 
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
 
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif  

 
#define YYFINAL  12
 
#define YYLAST   114

 
#define YYNTOKENS  29
 
#define YYNNTS  26
 
#define YYNRULES  92
 
#define YYNSTATES  115

 
#define YYMAXUTOK   277


 
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

 
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    27,     2,     2,    28,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    26,     2,
       2,     2,     2,     2,    23,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    24,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    25,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22
};

#if YYDEBUG
 
static const yytype_int16 yyrline[] =
{
       0,   592,   592,   593,   597,   605,   607,   611,   616,   621,
     626,   631,   636,   641,   646,   650,   654,   661,   665,   669,
     674,   679,   684,   688,   693,   698,   705,   707,   711,   736,
     738,   748,   750,   752,   757,   762,   765,   767,   772,   777,
     782,   788,   797,   802,   835,   843,   851,   856,   862,   867,
     873,   877,   887,   889,   891,   896,   898,   900,   902,   904,
     906,   908,   911,   914,   916,   918,   920,   922,   924,   926,
     928,   930,   932,   934,   936,   938,   942,   944,   946,   949,
     951,   953,   958,   962,   962,   965,   966,   972,   973,   979,
     984,   995,   996
};
#endif

 
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
 
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

 
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "tAGO", "tDST",
  "tYEAR_UNIT", "tMONTH_UNIT", "tHOUR_UNIT", "tMINUTE_UNIT", "tSEC_UNIT",
  "tDAY_UNIT", "tDAY_SHIFT", "tDAY", "tDAYZONE", "tLOCAL_ZONE",
  "tMERIDIAN", "tMONTH", "tORDINAL", "tZONE", "tSNUMBER", "tUNUMBER",
  "tSDECIMAL_NUMBER", "tUDECIMAL_NUMBER", "'@'", "'J'", "'T'", "':'",
  "','", "'/'", "$accept", "spec", "timespec", "items", "item", "datetime",
  "iso_8601_datetime", "time", "iso_8601_time", "o_zone_offset",
  "zone_offset", "local_zone", "zone", "day", "date", "iso_8601_date",
  "rel", "relunit", "relunit_snumber", "dayshift", "seconds",
  "signed_seconds", "unsigned_seconds", "number", "hybrid",
  "o_colon_minutes", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-91)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

 
static const yytype_int8 yypact[] =
{
     -14,     7,    39,   -91,    37,   -91,   -91,   -91,   -91,   -91,
     -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,
      14,   -91,    64,    47,    67,     6,    82,    -4,    74,    75,
     -91,    76,   -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,
     -91,    69,   -91,    93,   -91,   -91,   -91,   -91,   -91,   -91,
      79,    72,   -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,
      26,   -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,
     -91,   -91,   -91,   -91,   -91,   -91,    62,    11,    80,    81,
     -91,   -91,   -91,   -91,   -91,    83,   -91,   -91,    84,    85,
     -91,   -91,   -91,   -91,   -91,    45,    86,   -12,   -91,   -91,
     -91,   -91,    87,    18,   -91,   -91,    88,    89,    78,   -91,
      59,   -91,   -91,    18,    91
};

 
static const yytype_int8 yydefact[] =
{
       5,     0,     0,     2,     3,    86,    88,    85,    87,     4,
      83,    84,     1,    57,    60,    66,    69,    74,    63,    82,
      38,    36,    29,     0,     0,    31,     0,    89,     0,     0,
      10,    32,     6,     7,    17,     8,    22,     9,    11,    13,
      12,    50,    14,    53,    75,    54,    15,    16,    39,    30,
       0,    46,    55,    58,    64,    67,    70,    61,    40,    37,
      91,    33,    76,    77,    79,    80,    81,    78,    56,    59,
      65,    68,    71,    62,    41,    19,    48,    91,     0,     0,
      23,    90,    72,    73,    34,     0,    52,    45,     0,     0,
      35,    44,    49,    51,    28,    26,    42,     0,    18,    47,
      92,    20,    91,     0,    24,    27,     0,     0,    26,    43,
      26,    21,    25,     0,    26
};

 
static const yytype_int8 yypgoto[] =
{
     -91,   -91,   -91,   -91,   -91,   -91,   -91,   -91,    17,   -28,
     -27,   -91,   -91,   -91,   -91,   -91,   -91,   -91,    38,   -91,
     -91,   -91,   -90,   -91,   -91,    46
};

 
static const yytype_int8 yydefgoto[] =
{
       0,     2,     3,     4,    32,    33,    34,    35,    36,   104,
     105,    37,    38,    39,    40,    41,    42,    43,    44,    45,
       9,    10,    11,    46,    47,    94
};

 
static const yytype_int8 yytable[] =
{
      80,    68,    69,    70,    71,    72,    73,   102,    74,     1,
      59,    75,    76,   108,   107,    77,    62,    63,    64,    65,
      66,    67,    78,   114,    79,    60,     5,     6,     7,     8,
      93,    62,    63,    64,    65,    66,    67,    89,     6,    12,
       8,    48,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    89,    23,    24,    25,    26,    27,    28,    29,
     101,    30,    31,    61,   102,    81,    50,    51,    49,    84,
      80,   103,    52,    53,    54,    55,    56,    57,   102,    58,
     112,    91,    92,    82,    83,   113,   112,    62,    63,    64,
      65,    66,    67,   111,    85,    26,    86,   102,    87,    88,
      95,    96,    98,    97,    99,   100,    90,     0,   109,   110,
     102,     0,     0,    89,   106
};

static const yytype_int8 yycheck[] =
{
      27,     5,     6,     7,     8,     9,    10,    19,    12,    23,
       4,    15,    16,   103,    26,    19,     5,     6,     7,     8,
       9,    10,    26,   113,    28,    19,    19,    20,    21,    22,
      19,     5,     6,     7,     8,     9,    10,    26,    20,     0,
      22,    27,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    26,    16,    17,    18,    19,    20,    21,    22,
      15,    24,    25,    25,    19,    27,    19,    20,     4,    31,
      97,    26,     5,     6,     7,     8,     9,    10,    19,    12,
     108,    19,    20,     9,     9,    26,   114,     5,     6,     7,
       8,     9,    10,    15,    25,    19,     3,    19,    19,    27,
      20,    20,    85,    20,    20,    20,    60,    -1,    20,    20,
      19,    -1,    -1,    26,    28
};

 
static const yytype_int8 yystos[] =
{
       0,    23,    30,    31,    32,    19,    20,    21,    22,    49,
      50,    51,     0,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    16,    17,    18,    19,    20,    21,    22,
      24,    25,    33,    34,    35,    36,    37,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    52,    53,    27,     4,
      19,    20,     5,     6,     7,     8,     9,    10,    12,     4,
      19,    47,     5,     6,     7,     8,     9,    10,     5,     6,
       7,     8,     9,    10,    12,    15,    16,    19,    26,    28,
      39,    47,     9,     9,    47,    25,     3,    19,    27,    26,
      54,    19,    20,    19,    54,    20,    20,    20,    37,    20,
      20,    15,    19,    26,    38,    39,    28,    26,    51,    20,
      20,    15,    38,    26,    51
};

 
static const yytype_int8 yyr1[] =
{
       0,    29,    30,    30,    31,    32,    32,    33,    33,    33,
      33,    33,    33,    33,    33,    33,    33,    34,    35,    36,
      36,    36,    36,    37,    37,    37,    38,    38,    39,    40,
      40,    41,    41,    41,    41,    41,    41,    41,    42,    42,
      42,    42,    43,    43,    43,    43,    43,    43,    43,    43,
      43,    44,    45,    45,    45,    46,    46,    46,    46,    46,
      46,    46,    46,    46,    46,    46,    46,    46,    46,    46,
      46,    46,    46,    46,    46,    46,    47,    47,    47,    47,
      47,    47,    48,    49,    49,    50,    50,    51,    51,    52,
      53,    54,    54
};

 
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     2,     0,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     3,     2,
       4,     6,     1,     2,     4,     6,     0,     1,     2,     1,
       2,     1,     1,     2,     2,     3,     1,     2,     1,     2,
       2,     2,     3,     5,     3,     3,     2,     4,     2,     3,
       1,     3,     2,     1,     1,     2,     2,     1,     2,     2,
       1,     2,     2,     1,     2,     2,     1,     2,     2,     1,
       2,     2,     2,     2,     1,     1,     2,     2,     2,     2,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     0,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (pc, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

 
#define YYERRCODE YYUNDEF


 
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h>  
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, pc); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


 

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, parser_control *pc)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (pc);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


 

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, parser_control *pc)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, pc);
  YYFPRINTF (yyo, ")");
}

 

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


 

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, parser_control *pc)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
   
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], pc);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, pc); \
} while (0)

 
int yydebug;
#else  
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif  


 
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

 

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






 

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, parser_control *pc)
{
  YY_USE (yyvaluep);
  YY_USE (pc);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






 

int
yyparse (parser_control *pc)
{
 
int yychar;


 
 
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

     
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
     
    int yyerrstatus = 0;

     

     
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

     
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

     
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
   
  int yyresult;
   
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
   
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

   
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY;  

  goto yysetstate;


 
yynewstate:
   
  yyssp++;


 
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
       
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
         
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

         
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else  
       
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif  


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


 
yybackup:
   

   
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

   

   
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, pc);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
       
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

   
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

   
  if (yyerrstatus)
    yyerrstatus--;

   
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

   
  yychar = YYEMPTY;
  goto yynewstate;


 
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


 
yyreduce:
   
  yylen = yyr2[yyn];

   
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 4:  
#line 598 "parse-datetime.y"
      {
        pc->seconds = (yyvsp[0].timespec);
        pc->timespec_seen = true;
        debug_print_current_time (_("number of seconds"), pc);
      }
#line 1761 "parse-datetime.c"
    break;

  case 7:  
#line 612 "parse-datetime.y"
      {
        pc->times_seen++; pc->dates_seen++;
        debug_print_current_time (_("datetime"), pc);
      }
#line 1770 "parse-datetime.c"
    break;

  case 8:  
#line 617 "parse-datetime.y"
      {
        pc->times_seen++;
        debug_print_current_time (_("time"), pc);
      }
#line 1779 "parse-datetime.c"
    break;

  case 9:  
#line 622 "parse-datetime.y"
      {
        pc->local_zones_seen++;
        debug_print_current_time (_("local_zone"), pc);
      }
#line 1788 "parse-datetime.c"
    break;

  case 10:  
#line 627 "parse-datetime.y"
      {
        pc->J_zones_seen++;
        debug_print_current_time ("J", pc);
      }
#line 1797 "parse-datetime.c"
    break;

  case 11:  
#line 632 "parse-datetime.y"
      {
        pc->zones_seen++;
        debug_print_current_time (_("zone"), pc);
      }
#line 1806 "parse-datetime.c"
    break;

  case 12:  
#line 637 "parse-datetime.y"
      {
        pc->dates_seen++;
        debug_print_current_time (_("date"), pc);
      }
#line 1815 "parse-datetime.c"
    break;

  case 13:  
#line 642 "parse-datetime.y"
      {
        pc->days_seen++;
        debug_print_current_time (_("day"), pc);
      }
#line 1824 "parse-datetime.c"
    break;

  case 14:  
#line 647 "parse-datetime.y"
      {
        debug_print_relative_time (_("relative"), pc);
      }
#line 1832 "parse-datetime.c"
    break;

  case 15:  
#line 651 "parse-datetime.y"
      {
        debug_print_current_time (_("number"), pc);
      }
#line 1840 "parse-datetime.c"
    break;

  case 16:  
#line 655 "parse-datetime.y"
      {
        debug_print_relative_time (_("hybrid"), pc);
      }
#line 1848 "parse-datetime.c"
    break;

  case 19:  
#line 670 "parse-datetime.y"
      {
        set_hhmmss (pc, (yyvsp[-1].textintval).value, 0, 0, 0);
        pc->meridian = (yyvsp[0].intval);
      }
#line 1857 "parse-datetime.c"
    break;

  case 20:  
#line 675 "parse-datetime.y"
      {
        set_hhmmss (pc, (yyvsp[-3].textintval).value, (yyvsp[-1].textintval).value, 0, 0);
        pc->meridian = (yyvsp[0].intval);
      }
#line 1866 "parse-datetime.c"
    break;

  case 21:  
#line 680 "parse-datetime.y"
      {
        set_hhmmss (pc, (yyvsp[-5].textintval).value, (yyvsp[-3].textintval).value, (yyvsp[-1].timespec).tv_sec, (yyvsp[-1].timespec).tv_nsec);
        pc->meridian = (yyvsp[0].intval);
      }
#line 1875 "parse-datetime.c"
    break;

  case 23:  
#line 689 "parse-datetime.y"
      {
        set_hhmmss (pc, (yyvsp[-1].textintval).value, 0, 0, 0);
        pc->meridian = MER24;
      }
#line 1884 "parse-datetime.c"
    break;

  case 24:  
#line 694 "parse-datetime.y"
      {
        set_hhmmss (pc, (yyvsp[-3].textintval).value, (yyvsp[-1].textintval).value, 0, 0);
        pc->meridian = MER24;
      }
#line 1893 "parse-datetime.c"
    break;

  case 25:  
#line 699 "parse-datetime.y"
      {
        set_hhmmss (pc, (yyvsp[-5].textintval).value, (yyvsp[-3].textintval).value, (yyvsp[-1].timespec).tv_sec, (yyvsp[-1].timespec).tv_nsec);
        pc->meridian = MER24;
      }
#line 1902 "parse-datetime.c"
    break;

  case 28:  
#line 712 "parse-datetime.y"
      {
        pc->zones_seen++;
        if (! time_zone_hhmm (pc, (yyvsp[-1].textintval), (yyvsp[0].intval))) YYABORT;
      }
#line 1911 "parse-datetime.c"
    break;

  case 29:  
#line 737 "parse-datetime.y"
      { pc->local_isdst = (yyvsp[0].intval); }
#line 1917 "parse-datetime.c"
    break;

  case 30:  
#line 739 "parse-datetime.y"
      {
        pc->local_isdst = 1;
        pc->dsts_seen++;
      }
#line 1926 "parse-datetime.c"
    break;

  case 31:  
#line 749 "parse-datetime.y"
      { pc->time_zone = (yyvsp[0].intval); }
#line 1932 "parse-datetime.c"
    break;

  case 32:  
#line 751 "parse-datetime.y"
      { pc->time_zone = -HOUR (7); }
#line 1938 "parse-datetime.c"
    break;

  case 33:  
#line 753 "parse-datetime.y"
      { pc->time_zone = (yyvsp[-1].intval);
        if (! apply_relative_time (pc, (yyvsp[0].rel), 1)) YYABORT;
        debug_print_relative_time (_("relative"), pc);
      }
#line 1947 "parse-datetime.c"
    break;

  case 34:  
#line 758 "parse-datetime.y"
      { pc->time_zone = -HOUR (7);
        if (! apply_relative_time (pc, (yyvsp[0].rel), 1)) YYABORT;
        debug_print_relative_time (_("relative"), pc);
      }
#line 1956 "parse-datetime.c"
    break;

  case 35:  
#line 763 "parse-datetime.y"
      { if (! time_zone_hhmm (pc, (yyvsp[-1].textintval), (yyvsp[0].intval))) YYABORT;
        if (ckd_add (&pc->time_zone, pc->time_zone, (yyvsp[-2].intval))) YYABORT; }
#line 1963 "parse-datetime.c"
    break;

  case 36:  
#line 766 "parse-datetime.y"
      { pc->time_zone = (yyvsp[0].intval) + 60 * 60; }
#line 1969 "parse-datetime.c"
    break;

  case 37:  
#line 768 "parse-datetime.y"
      { pc->time_zone = (yyvsp[-1].intval) + 60 * 60; }
#line 1975 "parse-datetime.c"
    break;

  case 38:  
#line 773 "parse-datetime.y"
      {
        pc->day_ordinal = 0;
        pc->day_number = (yyvsp[0].intval);
      }
#line 1984 "parse-datetime.c"
    break;

  case 39:  
#line 778 "parse-datetime.y"
      {
        pc->day_ordinal = 0;
        pc->day_number = (yyvsp[-1].intval);
      }
#line 1993 "parse-datetime.c"
    break;

  case 40:  
#line 783 "parse-datetime.y"
      {
        pc->day_ordinal = (yyvsp[-1].intval);
        pc->day_number = (yyvsp[0].intval);
        pc->debug_ordinal_day_seen = true;
      }
#line 2003 "parse-datetime.c"
    break;

  case 41:  
#line 789 "parse-datetime.y"
      {
        pc->day_ordinal = (yyvsp[-1].textintval).value;
        pc->day_number = (yyvsp[0].intval);
        pc->debug_ordinal_day_seen = true;
      }
#line 2013 "parse-datetime.c"
    break;

  case 42:  
#line 798 "parse-datetime.y"
      {
        pc->month = (yyvsp[-2].textintval).value;
        pc->day = (yyvsp[0].textintval).value;
      }
#line 2022 "parse-datetime.c"
    break;

  case 43:  
#line 803 "parse-datetime.y"
      {
         
        if (4 <= (yyvsp[-4].textintval).digits)
          {
            if (debugging (pc))
              {
                intmax_t digits = (yyvsp[-4].textintval).digits;
                dbg_printf (_("warning: value %"PRIdMAX" has %"PRIdMAX" digits. "
                              "Assuming YYYY/MM/DD\n"),
                            (yyvsp[-4].textintval).value, digits);
              }

            pc->year = (yyvsp[-4].textintval);
            pc->month = (yyvsp[-2].textintval).value;
            pc->day = (yyvsp[0].textintval).value;
          }
        else
          {
            if (debugging (pc))
              dbg_printf (_("warning: value %"PRIdMAX" has less than 4 digits. "
                            "Assuming MM/DD/YY[YY]\n"),
                          (yyvsp[-4].textintval).value);

            pc->month = (yyvsp[-4].textintval).value;
            pc->day = (yyvsp[-2].textintval).value;
            pc->year = (yyvsp[0].textintval);
          }
      }
#line 2059 "parse-datetime.c"
    break;

  case 44:  
#line 836 "parse-datetime.y"
      {
         
        pc->day = (yyvsp[-2].textintval).value;
        pc->month = (yyvsp[-1].intval);
        if (ckd_sub (&pc->year.value, 0, (yyvsp[0].textintval).value)) YYABORT;
        pc->year.digits = (yyvsp[0].textintval).digits;
      }
#line 2071 "parse-datetime.c"
    break;

  case 45:  
#line 844 "parse-datetime.y"
      {
         
        pc->month = (yyvsp[-2].intval);
        if (ckd_sub (&pc->day, 0, (yyvsp[-1].textintval).value)) YYABORT;
        if (ckd_sub (&pc->year.value, 0, (yyvsp[0].textintval).value)) YYABORT;
        pc->year.digits = (yyvsp[0].textintval).digits;
      }
#line 2083 "parse-datetime.c"
    break;

  case 46:  
#line 852 "parse-datetime.y"
      {
        pc->month = (yyvsp[-1].intval);
        pc->day = (yyvsp[0].textintval).value;
      }
#line 2092 "parse-datetime.c"
    break;

  case 47:  
#line 857 "parse-datetime.y"
      {
        pc->month = (yyvsp[-3].intval);
        pc->day = (yyvsp[-2].textintval).value;
        pc->year = (yyvsp[0].textintval);
      }
#line 2102 "parse-datetime.c"
    break;

  case 48:  
#line 863 "parse-datetime.y"
      {
        pc->day = (yyvsp[-1].textintval).value;
        pc->month = (yyvsp[0].intval);
      }
#line 2111 "parse-datetime.c"
    break;

  case 49:  
#line 868 "parse-datetime.y"
      {
        pc->day = (yyvsp[-2].textintval).value;
        pc->month = (yyvsp[-1].intval);
        pc->year = (yyvsp[0].textintval);
      }
#line 2121 "parse-datetime.c"
    break;

  case 51:  
#line 878 "parse-datetime.y"
      {
         
        pc->year = (yyvsp[-2].textintval);
        if (ckd_sub (&pc->month, 0, (yyvsp[-1].textintval).value)) YYABORT;
        if (ckd_sub (&pc->day, 0, (yyvsp[0].textintval).value)) YYABORT;
      }
#line 2132 "parse-datetime.c"
    break;

  case 52:  
#line 888 "parse-datetime.y"
      { if (! apply_relative_time (pc, (yyvsp[-1].rel), (yyvsp[0].intval))) YYABORT; }
#line 2138 "parse-datetime.c"
    break;

  case 53:  
#line 890 "parse-datetime.y"
      { if (! apply_relative_time (pc, (yyvsp[0].rel), 1)) YYABORT; }
#line 2144 "parse-datetime.c"
    break;

  case 54:  
#line 892 "parse-datetime.y"
      { if (! apply_relative_time (pc, (yyvsp[0].rel), 1)) YYABORT; }
#line 2150 "parse-datetime.c"
    break;

  case 55:  
#line 897 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).year = (yyvsp[-1].intval); }
#line 2156 "parse-datetime.c"
    break;

  case 56:  
#line 899 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).year = (yyvsp[-1].textintval).value; }
#line 2162 "parse-datetime.c"
    break;

  case 57:  
#line 901 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).year = 1; }
#line 2168 "parse-datetime.c"
    break;

  case 58:  
#line 903 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).month = (yyvsp[-1].intval); }
#line 2174 "parse-datetime.c"
    break;

  case 59:  
#line 905 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).month = (yyvsp[-1].textintval).value; }
#line 2180 "parse-datetime.c"
    break;

  case 60:  
#line 907 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).month = 1; }
#line 2186 "parse-datetime.c"
    break;

  case 61:  
#line 909 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0;
        if (ckd_mul (&(yyval.rel).day, (yyvsp[-1].intval), (yyvsp[0].intval))) YYABORT; }
#line 2193 "parse-datetime.c"
    break;

  case 62:  
#line 912 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0;
        if (ckd_mul (&(yyval.rel).day, (yyvsp[-1].textintval).value, (yyvsp[0].intval))) YYABORT; }
#line 2200 "parse-datetime.c"
    break;

  case 63:  
#line 915 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).day = (yyvsp[0].intval); }
#line 2206 "parse-datetime.c"
    break;

  case 64:  
#line 917 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).hour = (yyvsp[-1].intval); }
#line 2212 "parse-datetime.c"
    break;

  case 65:  
#line 919 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).hour = (yyvsp[-1].textintval).value; }
#line 2218 "parse-datetime.c"
    break;

  case 66:  
#line 921 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).hour = 1; }
#line 2224 "parse-datetime.c"
    break;

  case 67:  
#line 923 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).minutes = (yyvsp[-1].intval); }
#line 2230 "parse-datetime.c"
    break;

  case 68:  
#line 925 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).minutes = (yyvsp[-1].textintval).value; }
#line 2236 "parse-datetime.c"
    break;

  case 69:  
#line 927 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).minutes = 1; }
#line 2242 "parse-datetime.c"
    break;

  case 70:  
#line 929 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).seconds = (yyvsp[-1].intval); }
#line 2248 "parse-datetime.c"
    break;

  case 71:  
#line 931 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).seconds = (yyvsp[-1].textintval).value; }
#line 2254 "parse-datetime.c"
    break;

  case 72:  
#line 933 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).seconds = (yyvsp[-1].timespec).tv_sec; (yyval.rel).ns = (yyvsp[-1].timespec).tv_nsec; }
#line 2260 "parse-datetime.c"
    break;

  case 73:  
#line 935 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).seconds = (yyvsp[-1].timespec).tv_sec; (yyval.rel).ns = (yyvsp[-1].timespec).tv_nsec; }
#line 2266 "parse-datetime.c"
    break;

  case 74:  
#line 937 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).seconds = 1; }
#line 2272 "parse-datetime.c"
    break;

  case 76:  
#line 943 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).year = (yyvsp[-1].textintval).value; }
#line 2278 "parse-datetime.c"
    break;

  case 77:  
#line 945 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).month = (yyvsp[-1].textintval).value; }
#line 2284 "parse-datetime.c"
    break;

  case 78:  
#line 947 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0;
        if (ckd_mul (&(yyval.rel).day, (yyvsp[-1].textintval).value, (yyvsp[0].intval))) YYABORT; }
#line 2291 "parse-datetime.c"
    break;

  case 79:  
#line 950 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).hour = (yyvsp[-1].textintval).value; }
#line 2297 "parse-datetime.c"
    break;

  case 80:  
#line 952 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).minutes = (yyvsp[-1].textintval).value; }
#line 2303 "parse-datetime.c"
    break;

  case 81:  
#line 954 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).seconds = (yyvsp[-1].textintval).value; }
#line 2309 "parse-datetime.c"
    break;

  case 82:  
#line 959 "parse-datetime.y"
      { (yyval.rel) = RELATIVE_TIME_0; (yyval.rel).day = (yyvsp[0].intval); }
#line 2315 "parse-datetime.c"
    break;

  case 86:  
#line 967 "parse-datetime.y"
      { if (time_overflow ((yyvsp[0].textintval).value)) YYABORT;
        (yyval.timespec) = (struct timespec) { .tv_sec = (yyvsp[0].textintval).value }; }
#line 2322 "parse-datetime.c"
    break;

  case 88:  
#line 974 "parse-datetime.y"
      { if (time_overflow ((yyvsp[0].textintval).value)) YYABORT;
        (yyval.timespec) = (struct timespec) { .tv_sec = (yyvsp[0].textintval).value }; }
#line 2329 "parse-datetime.c"
    break;

  case 89:  
#line 980 "parse-datetime.y"
      { digits_to_date_time (pc, (yyvsp[0].textintval)); }
#line 2335 "parse-datetime.c"
    break;

  case 90:  
#line 985 "parse-datetime.y"
      {
         
        digits_to_date_time (pc, (yyvsp[-1].textintval));
        if (! apply_relative_time (pc, (yyvsp[0].rel), 1)) YYABORT;
      }
#line 2346 "parse-datetime.c"
    break;

  case 91:  
#line 995 "parse-datetime.y"
      { (yyval.intval) = -1; }
#line 2352 "parse-datetime.c"
    break;

  case 92:  
#line 997 "parse-datetime.y"
      { (yyval.intval) = (yyvsp[0].textintval).value; }
#line 2358 "parse-datetime.c"
    break;


#line 2362 "parse-datetime.c"

      default: break;
    }
   
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

   
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


 
yyerrlab:
   
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
   
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (pc, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
       

      if (yychar <= YYEOF)
        {
           
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, pc);
          yychar = YYEMPTY;
        }
    }

   
  goto yyerrlab1;


 
yyerrorlab:
   
  if (0)
    YYERROR;
  ++yynerrs;

   
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


 
yyerrlab1:
  yyerrstatus = 3;       

   
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

       
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, pc);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


   
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


 
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


 
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


 
yyexhaustedlab:
  yyerror (pc, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


 
yyreturnlab:
  if (yychar != YYEMPTY)
    {
       
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, pc);
    }
   
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, pc);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1000 "parse-datetime.y"


static table const meridian_table[] =
{
  { "AM",   tMERIDIAN, MERam },
  { "A.M.", tMERIDIAN, MERam },
  { "PM",   tMERIDIAN, MERpm },
  { "P.M.", tMERIDIAN, MERpm },
  { NULL, 0, 0 }
};

static table const dst_table[] =
{
  { "DST", tDST, 0 }
};

static table const month_and_day_table[] =
{
  { "JANUARY",  tMONTH,  1 },
  { "FEBRUARY", tMONTH,  2 },
  { "MARCH",    tMONTH,  3 },
  { "APRIL",    tMONTH,  4 },
  { "MAY",      tMONTH,  5 },
  { "JUNE",     tMONTH,  6 },
  { "JULY",     tMONTH,  7 },
  { "AUGUST",   tMONTH,  8 },
  { "SEPTEMBER",tMONTH,  9 },
  { "SEPT",     tMONTH,  9 },
  { "OCTOBER",  tMONTH, 10 },
  { "NOVEMBER", tMONTH, 11 },
  { "DECEMBER", tMONTH, 12 },
  { "SUNDAY",   tDAY,    0 },
  { "MONDAY",   tDAY,    1 },
  { "TUESDAY",  tDAY,    2 },
  { "TUES",     tDAY,    2 },
  { "WEDNESDAY",tDAY,    3 },
  { "WEDNES",   tDAY,    3 },
  { "THURSDAY", tDAY,    4 },
  { "THUR",     tDAY,    4 },
  { "THURS",    tDAY,    4 },
  { "FRIDAY",   tDAY,    5 },
  { "SATURDAY", tDAY,    6 },
  { NULL, 0, 0 }
};

static table const time_units_table[] =
{
  { "YEAR",     tYEAR_UNIT,      1 },
  { "MONTH",    tMONTH_UNIT,     1 },
  { "FORTNIGHT",tDAY_UNIT,      14 },
  { "WEEK",     tDAY_UNIT,       7 },
  { "DAY",      tDAY_UNIT,       1 },
  { "HOUR",     tHOUR_UNIT,      1 },
  { "MINUTE",   tMINUTE_UNIT,    1 },
  { "MIN",      tMINUTE_UNIT,    1 },
  { "SECOND",   tSEC_UNIT,       1 },
  { "SEC",      tSEC_UNIT,       1 },
  { NULL, 0, 0 }
};

 
static table const relative_time_table[] =
{
  { "TOMORROW", tDAY_SHIFT,      1 },
  { "YESTERDAY",tDAY_SHIFT,     -1 },
  { "TODAY",    tDAY_SHIFT,      0 },
  { "NOW",      tDAY_SHIFT,      0 },
  { "LAST",     tORDINAL,       -1 },
  { "THIS",     tORDINAL,        0 },
  { "NEXT",     tORDINAL,        1 },
  { "FIRST",    tORDINAL,        1 },
 
  { "THIRD",    tORDINAL,        3 },
  { "FOURTH",   tORDINAL,        4 },
  { "FIFTH",    tORDINAL,        5 },
  { "SIXTH",    tORDINAL,        6 },
  { "SEVENTH",  tORDINAL,        7 },
  { "EIGHTH",   tORDINAL,        8 },
  { "NINTH",    tORDINAL,        9 },
  { "TENTH",    tORDINAL,       10 },
  { "ELEVENTH", tORDINAL,       11 },
  { "TWELFTH",  tORDINAL,       12 },
  { "AGO",      tAGO,           -1 },
  { "HENCE",    tAGO,            1 },
  { NULL, 0, 0 }
};

 
static table const universal_time_zone_table[] =
{
  { "GMT",      tZONE,     HOUR ( 0) },  
  { "UT",       tZONE,     HOUR ( 0) },  
  { "UTC",      tZONE,     HOUR ( 0) },
  { NULL, 0, 0 }
};

 
static table const time_zone_table[] =
{
  { "WET",      tZONE,     HOUR ( 0) },  
  { "WEST",     tDAYZONE,  HOUR ( 0) },  
  { "BST",      tDAYZONE,  HOUR ( 0) },  
  { "ART",      tZONE,    -HOUR ( 3) },  
  { "BRT",      tZONE,    -HOUR ( 3) },  
  { "BRST",     tDAYZONE, -HOUR ( 3) },  
  { "NST",      tZONE,   -(HOUR ( 3) + 30 * 60) },  
  { "NDT",      tDAYZONE,-(HOUR ( 3) + 30 * 60) },  
  { "AST",      tZONE,    -HOUR ( 4) },  
  { "ADT",      tDAYZONE, -HOUR ( 4) },  
  { "CLT",      tZONE,    -HOUR ( 4) },  
  { "CLST",     tDAYZONE, -HOUR ( 4) },  
  { "EST",      tZONE,    -HOUR ( 5) },  
  { "EDT",      tDAYZONE, -HOUR ( 5) },  
  { "CST",      tZONE,    -HOUR ( 6) },  
  { "CDT",      tDAYZONE, -HOUR ( 6) },  
  { "MST",      tZONE,    -HOUR ( 7) },  
  { "MDT",      tDAYZONE, -HOUR ( 7) },  
  { "PST",      tZONE,    -HOUR ( 8) },  
  { "PDT",      tDAYZONE, -HOUR ( 8) },  
  { "AKST",     tZONE,    -HOUR ( 9) },  
  { "AKDT",     tDAYZONE, -HOUR ( 9) },  
  { "HST",      tZONE,    -HOUR (10) },  
  { "HAST",     tZONE,    -HOUR (10) },  
  { "HADT",     tDAYZONE, -HOUR (10) },  
  { "SST",      tZONE,    -HOUR (12) },  
  { "WAT",      tZONE,     HOUR ( 1) },  
  { "CET",      tZONE,     HOUR ( 1) },  
  { "CEST",     tDAYZONE,  HOUR ( 1) },  
  { "MET",      tZONE,     HOUR ( 1) },  
  { "MEZ",      tZONE,     HOUR ( 1) },  
  { "MEST",     tDAYZONE,  HOUR ( 1) },  
  { "MESZ",     tDAYZONE,  HOUR ( 1) },  
  { "EET",      tZONE,     HOUR ( 2) },  
  { "EEST",     tDAYZONE,  HOUR ( 2) },  
  { "CAT",      tZONE,     HOUR ( 2) },  
  { "SAST",     tZONE,     HOUR ( 2) },  
  { "EAT",      tZONE,     HOUR ( 3) },  
  { "MSK",      tZONE,     HOUR ( 3) },  
  { "MSD",      tDAYZONE,  HOUR ( 3) },  
  { "IST",      tZONE,    (HOUR ( 5) + 30 * 60) },  
  { "SGT",      tZONE,     HOUR ( 8) },  
  { "KST",      tZONE,     HOUR ( 9) },  
  { "JST",      tZONE,     HOUR ( 9) },  
  { "GST",      tZONE,     HOUR (10) },  
  { "NZST",     tZONE,     HOUR (12) },  
  { "NZDT",     tDAYZONE,  HOUR (12) },  
  { NULL, 0, 0 }
};

 
static table const military_table[] =
{
  { "A", tZONE,  HOUR ( 1) },
  { "B", tZONE,  HOUR ( 2) },
  { "C", tZONE,  HOUR ( 3) },
  { "D", tZONE,  HOUR ( 4) },
  { "E", tZONE,  HOUR ( 5) },
  { "F", tZONE,  HOUR ( 6) },
  { "G", tZONE,  HOUR ( 7) },
  { "H", tZONE,  HOUR ( 8) },
  { "I", tZONE,  HOUR ( 9) },
  { "J", 'J',    0 },
  { "K", tZONE,  HOUR (10) },
  { "L", tZONE,  HOUR (11) },
  { "M", tZONE,  HOUR (12) },
  { "N", tZONE, -HOUR ( 1) },
  { "O", tZONE, -HOUR ( 2) },
  { "P", tZONE, -HOUR ( 3) },
  { "Q", tZONE, -HOUR ( 4) },
  { "R", tZONE, -HOUR ( 5) },
  { "S", tZONE, -HOUR ( 6) },
  { "T", 'T',    0 },
  { "U", tZONE, -HOUR ( 8) },
  { "V", tZONE, -HOUR ( 9) },
  { "W", tZONE, -HOUR (10) },
  { "X", tZONE, -HOUR (11) },
  { "Y", tZONE, -HOUR (12) },
  { "Z", tZONE,  HOUR ( 0) },
  { NULL, 0, 0 }
};



 

static bool
time_zone_hhmm (parser_control *pc, textint s, intmax_t mm)
{
  intmax_t n_minutes;
  bool overflow = false;

   
  if (s.digits <= 2 && mm < 0)
    s.value *= 100;

  if (mm < 0)
    n_minutes = (s.value / 100) * 60 + s.value % 100;
  else
    {
      overflow |= ckd_mul (&n_minutes, s.value, 60);
      overflow |= (s.negative
                   ? ckd_sub (&n_minutes, n_minutes, mm)
                   : ckd_add (&n_minutes, n_minutes, mm));
    }

  if (overflow || ! (-24 * 60 <= n_minutes && n_minutes <= 24 * 60))
    return false;
  pc->time_zone = n_minutes * 60;
  return true;
}

static int
to_hour (intmax_t hours, int meridian)
{
  switch (meridian)
    {
    default:  
    case MER24:
      return 0 <= hours && hours < 24 ? hours : -1;
    case MERam:
      return 0 < hours && hours < 12 ? hours : hours == 12 ? 0 : -1;
    case MERpm:
      return 0 < hours && hours < 12 ? hours + 12 : hours == 12 ? 12 : -1;
    }
}

enum { TM_YEAR_BASE = 1900 };
enum { TM_YEAR_BUFSIZE = INT_BUFSIZE_BOUND (int) + 1 };

 

static char const *
tm_year_str (int tm_year, char buf[TM_YEAR_BUFSIZE])
{
  static_assert (TM_YEAR_BASE % 100 == 0);
  sprintf (buf, &"-%02d%02d"[-TM_YEAR_BASE <= tm_year],
           abs (tm_year / 100 + TM_YEAR_BASE / 100),
           abs (tm_year % 100));
  return buf;
}

 

static bool
to_tm_year (textint textyear, bool debug, int *tm_year)
{
  intmax_t year = textyear.value;

   
  if (0 <= year && textyear.digits == 2)
    {
      year += year < 69 ? 2000 : 1900;
      if (debug)
        dbg_printf (_("warning: adjusting year value %"PRIdMAX
                      " to %"PRIdMAX"\n"),
                    textyear.value, year);
    }

  if (year < 0
      ? ckd_sub (tm_year, -TM_YEAR_BASE, year)
      : ckd_sub (tm_year, year, TM_YEAR_BASE))
    {
      if (debug)
        dbg_printf (_("error: out-of-range year %"PRIdMAX"\n"), year);
      return false;
    }

  return true;
}

static table const * _GL_ATTRIBUTE_PURE
lookup_zone (parser_control const *pc, char const *name)
{
  table const *tp;

  for (tp = universal_time_zone_table; tp->name; tp++)
    if (strcmp (name, tp->name) == 0)
      return tp;

   
  for (tp = pc->local_time_zone_table; tp->name; tp++)
    if (strcmp (name, tp->name) == 0)
      return tp;

  for (tp = time_zone_table; tp->name; tp++)
    if (strcmp (name, tp->name) == 0)
      return tp;

  return NULL;
}

#if ! HAVE_TM_GMTOFF
 
static int
tm_diff (const struct tm *a, const struct tm *b)
{
   
  int a4 = SHR (a->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (a->tm_year & 3);
  int b4 = SHR (b->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (b->tm_year & 3);
  int a100 = a4 / 25 - (a4 % 25 < 0);
  int b100 = b4 / 25 - (b4 % 25 < 0);
  int a400 = SHR (a100, 2);
  int b400 = SHR (b100, 2);
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
  int years = a->tm_year - b->tm_year;
  int days = (365 * years + intervening_leap_days
              + (a->tm_yday - b->tm_yday));
  return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
                + (a->tm_min - b->tm_min))
          + (a->tm_sec - b->tm_sec));
}
#endif  

static table const *
lookup_word (parser_control const *pc, char *word)
{
  char *p;
  char *q;
  idx_t wordlen;
  table const *tp;
  bool period_found;
  bool abbrev;

   
  for (p = word; *p; p++)
    *p = c_toupper (to_uchar (*p));

  for (tp = meridian_table; tp->name; tp++)
    if (strcmp (word, tp->name) == 0)
      return tp;

   
  wordlen = strlen (word);
  abbrev = wordlen == 3 || (wordlen == 4 && word[3] == '.');

  for (tp = month_and_day_table; tp->name; tp++)
    if ((abbrev ? strncmp (word, tp->name, 3) : strcmp (word, tp->name)) == 0)
      return tp;

  if ((tp = lookup_zone (pc, word)))
    return tp;

  if (strcmp (word, dst_table[0].name) == 0)
    return dst_table;

  for (tp = time_units_table; tp->name; tp++)
    if (strcmp (word, tp->name) == 0)
      return tp;

   
  if (word[wordlen - 1] == 'S')
    {
      word[wordlen - 1] = '\0';
      for (tp = time_units_table; tp->name; tp++)
        if (strcmp (word, tp->name) == 0)
          return tp;
      word[wordlen - 1] = 'S';   
    }

  for (tp = relative_time_table; tp->name; tp++)
    if (strcmp (word, tp->name) == 0)
      return tp;

   
  if (wordlen == 1)
    for (tp = military_table; tp->name; tp++)
      if (word[0] == tp->name[0])
        return tp;

   
  for (period_found = false, p = q = word; (*p = *q); q++)
    if (*q == '.')
      period_found = true;
    else
      p++;
  if (period_found && (tp = lookup_zone (pc, word)))
    return tp;

  return NULL;
}

static int
yylex (union YYSTYPE *lvalp, parser_control *pc)
{
  unsigned char c;

  for (;;)
    {
      while (c = *pc->input, c_isspace (c))
        pc->input++;

      if (c_isdigit (c) || c == '-' || c == '+')
        {
          char const *p = pc->input;
          int sign;
          if (c == '-' || c == '+')
            {
              sign = c == '-' ? -1 : 1;
              while (c = *(pc->input = ++p), c_isspace (c))
                continue;
              if (! c_isdigit (c))
                 
                continue;
            }
          else
            sign = 0;

          time_t value = 0;
          do
            {
              if (ckd_mul (&value, value, 10))
                return '?';
              if (ckd_add (&value, value, sign < 0 ? '0' - c : c - '0'))
                return '?';
              c = *++p;
            }
          while (c_isdigit (c));

          if ((c == '.' || c == ',') && c_isdigit (p[1]))
            {
              time_t s = value;
              int digits;

               
              p++;
              int ns = *p++ - '0';
              for (digits = 2; digits <= LOG10_BILLION; digits++)
                {
                  ns *= 10;
                  if (c_isdigit (*p))
                    ns += *p++ - '0';
                }

               
              if (sign < 0)
                for (; c_isdigit (*p); p++)
                  if (*p != '0')
                    {
                      ns++;
                      break;
                    }
              while (c_isdigit (*p))
                p++;

               
              if (sign < 0 && ns)
                {
                  if (ckd_sub (&s, s, 1))
                    return '?';
                  ns = BILLION - ns;
                }

              lvalp->timespec = (struct timespec) { .tv_sec = s,
                                                    .tv_nsec = ns };
              pc->input = p;
              return sign ? tSDECIMAL_NUMBER : tUDECIMAL_NUMBER;
            }
          else
            {
              lvalp->textintval.negative = sign < 0;
              lvalp->textintval.value = value;
              lvalp->textintval.digits = p - pc->input;
              pc->input = p;
              return sign ? tSNUMBER : tUNUMBER;
            }
        }

      if (c_isalpha (c))
        {
          char buff[20];
          char *p = buff;
          table const *tp;

          do
            {
              if (p < buff + sizeof buff - 1)
                *p++ = c;
              c = *++pc->input;
            }
          while (c_isalpha (c) || c == '.');

          *p = '\0';
          tp = lookup_word (pc, buff);
          if (! tp)
            {
              if (debugging (pc))
                dbg_printf (_("error: unknown word '%s'\n"), buff);
              return '?';
            }
          lvalp->intval = tp->value;
          return tp->type;
        }

      if (c != '(')
        return to_uchar (*pc->input++);

      idx_t count = 0;
      do
        {
          c = *pc->input++;
          if (c == '\0')
            return c;
          if (c == '(')
            count++;
          else if (c == ')')
            count--;
        }
      while (count != 0);
    }
}

 
static int
yyerror (_GL_UNUSED parser_control const *pc,
         _GL_UNUSED char const *s)
{
  return 0;
}

 

static bool
mktime_ok (struct tm const *tm0, struct tm const *tm1)
{
  if (tm1->tm_wday < 0)
    return false;

  return ! ((tm0->tm_sec ^ tm1->tm_sec)
            | (tm0->tm_min ^ tm1->tm_min)
            | (tm0->tm_hour ^ tm1->tm_hour)
            | (tm0->tm_mday ^ tm1->tm_mday)
            | (tm0->tm_mon ^ tm1->tm_mon)
            | (tm0->tm_year ^ tm1->tm_year));
}

 
static char const *
debug_strfdatetime (struct tm const *tm, parser_control const *pc,
                    char *buf, int n)
{
   
  int m = nstrftime (buf, n, "(Y-M-D) %Y-%m-%d %H:%M:%S", tm, 0, 0);

   
  if (pc && m < n && pc->zones_seen)
    {
      int tz = pc->time_zone;

       
      if (pc->local_zones_seen && !pc->zones_seen && 0 < pc->local_isdst)
        tz += 60 * 60;

      char time_zone_buf[TIME_ZONE_BUFSIZE];
      snprintf (&buf[m], n - m, " TZ=%s", time_zone_str (tz, time_zone_buf));
    }
  return buf;
}

static char const *
debug_strfdate (struct tm const *tm, char *buf, int n)
{
  char tm_year_buf[TM_YEAR_BUFSIZE];
  snprintf (buf, n, "(Y-M-D) %s-%02d-%02d",
            tm_year_str (tm->tm_year, tm_year_buf),
            tm->tm_mon + 1, tm->tm_mday);
  return buf;
}

static char const *
debug_strftime (struct tm const *tm, char *buf, int n)
{
  snprintf (buf, n, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
  return buf;
}

 
static void
debug_mktime_not_ok (struct tm const *tm0, struct tm const *tm1,
                     parser_control const *pc, bool time_zone_seen)
{
   
  char tmp[DBGBUFSIZE];
  int i;
  const bool eq_sec   = (tm0->tm_sec  == tm1->tm_sec);
  const bool eq_min   = (tm0->tm_min  == tm1->tm_min);
  const bool eq_hour  = (tm0->tm_hour == tm1->tm_hour);
  const bool eq_mday  = (tm0->tm_mday == tm1->tm_mday);
  const bool eq_month = (tm0->tm_mon  == tm1->tm_mon);
  const bool eq_year  = (tm0->tm_year == tm1->tm_year);

  const bool dst_shift = eq_sec && eq_min && !eq_hour
                         && eq_mday && eq_month && eq_year;

  if (!debugging (pc))
    return;

  dbg_printf (_("error: invalid date/time value:\n"));
  dbg_printf (_("    user provided time: '%s'\n"),
              debug_strfdatetime (tm0, pc, tmp, sizeof tmp));
  dbg_printf (_("       normalized time: '%s'\n"),
              debug_strfdatetime (tm1, pc, tmp, sizeof tmp));
   
  i = snprintf (tmp, sizeof tmp,
                "                                 %4s %2s %2s %2s %2s %2s",
                eq_year ? "" : "----",
                eq_month ? "" : "--",
                eq_mday ? "" : "--",
                eq_hour ? "" : "--",
                eq_min ? "" : "--",
                eq_sec ? "" : "--");
   
  if (0 <= i)
    {
      if (sizeof tmp - 1 < i)
        i = sizeof tmp - 1;
      while (0 < i && tmp[i - 1] == ' ')
        --i;
      tmp[i] = '\0';
    }
  dbg_printf ("%s\n", tmp);

  dbg_printf (_("     possible reasons:\n"));
  if (dst_shift)
    dbg_printf (_("       nonexistent due to daylight-saving time;\n"));
  if (!eq_mday && !eq_month)
    dbg_printf (_("       invalid day/month combination;\n"));
  dbg_printf (_("       numeric values overflow;\n"));
  dbg_printf ("       %s\n", (time_zone_seen ? _("incorrect timezone")
                              : _("missing timezone")));
}

 
static bool
parse_datetime_body (struct timespec *result, char const *p,
                     struct timespec const *now, unsigned int flags,
                     timezone_t tzdefault, char const *tzstring)
{
  struct tm tm;
  struct tm tm0;
  char time_zone_buf[TIME_ZONE_BUFSIZE];
  char dbg_tm[DBGBUFSIZE];
  bool ok = false;
  char const *input_sentinel = p + strlen (p);
  char *tz1alloc = NULL;

   
  enum { TZBUFSIZE = 100 };
  char tz1buf[TZBUFSIZE];

  struct timespec gettime_buffer;
  if (! now)
    {
      gettime (&gettime_buffer);
      now = &gettime_buffer;
    }

  time_t Start = now->tv_sec;
  int Start_ns = now->tv_nsec;

  unsigned char c;
  while (c = *p, c_isspace (c))
    p++;

  timezone_t tz = tzdefault;

   
  const relative_time rel_time_0 = RELATIVE_TIME_0;

  if (strncmp (p, "TZ=\"", 4) == 0)
    {
      char const *tzbase = p + 4;
      idx_t tzsize = 1;
      char const *s;

      for (s = tzbase; *s; s++, tzsize++)
        if (*s == '\\')
          {
            s++;
            if (! (*s == '\\' || *s == '"'))
              break;
          }
        else if (*s == '"')
          {
            timezone_t tz1;
            char *tz1string = tz1buf;
            char *z;
            if (TZBUFSIZE < tzsize)
              {
                tz1alloc = malloc (tzsize);
                if (!tz1alloc)
                  goto fail;
                tz1string = tz1alloc;
              }
            z = tz1string;
            for (s = tzbase; *s != '"'; s++)
              *z++ = *(s += *s == '\\');
            *z = '\0';
            tz1 = tzalloc (tz1string);
            if (!tz1)
              goto fail;
            tz = tz1;
            tzstring = tz1string;

            p = s + 1;
            while (c = *p, c_isspace (c))
              p++;

            break;
          }
    }

  struct tm tmp;
  if (! localtime_rz (tz, &now->tv_sec, &tmp))
    goto fail;

   
  if (*p == '\0')
    p = "0";

  parser_control pc;
  pc.input = p;
#ifdef GNULIB_PARSE_DATETIME2
  pc.parse_datetime_debug = (flags & PARSE_DATETIME_DEBUG) != 0;
#endif
  if (ckd_add (&pc.year.value, tmp.tm_year, TM_YEAR_BASE))
    {
      if (debugging (&pc))
        dbg_printf (_("error: initial year out of range\n"));
      goto fail;
    }
  pc.year.digits = 0;
  pc.month = tmp.tm_mon + 1;
  pc.day = tmp.tm_mday;
  pc.hour = tmp.tm_hour;
  pc.minutes = tmp.tm_min;
  pc.seconds = (struct timespec) { .tv_sec = tmp.tm_sec, .tv_nsec = Start_ns };
  tm.tm_isdst = tmp.tm_isdst;

  pc.meridian = MER24;
  pc.rel = rel_time_0;
  pc.timespec_seen = false;
  pc.rels_seen = false;
  pc.dates_seen = 0;
  pc.days_seen = 0;
  pc.times_seen = 0;
  pc.J_zones_seen = 0;
  pc.local_zones_seen = 0;
  pc.dsts_seen = 0;
  pc.zones_seen = 0;
  pc.year_seen = false;
  pc.debug_dates_seen = false;
  pc.debug_days_seen = false;
  pc.debug_times_seen = false;
  pc.debug_local_zones_seen = false;
  pc.debug_zones_seen = false;
  pc.debug_year_seen = false;
  pc.debug_ordinal_day_seen = false;

#if HAVE_STRUCT_TM_TM_ZONE
  pc.local_time_zone_table[0].name = tmp.tm_zone;
  pc.local_time_zone_table[0].type = tLOCAL_ZONE;
  pc.local_time_zone_table[0].value = tmp.tm_isdst;
  pc.local_time_zone_table[1].name = NULL;

   
  {
    int quarter;
    for (quarter = 1; quarter <= 3; quarter++)
      {
        time_t probe;
        if (ckd_add (&probe, Start, quarter * (90 * 24 * 60 * 60)))
          break;
        struct tm probe_tm;
        if (localtime_rz (tz, &probe, &probe_tm) && probe_tm.tm_zone
            && probe_tm.tm_isdst != pc.local_time_zone_table[0].value)
          {
              {
                pc.local_time_zone_table[1].name = probe_tm.tm_zone;
                pc.local_time_zone_table[1].type = tLOCAL_ZONE;
                pc.local_time_zone_table[1].value = probe_tm.tm_isdst;
                pc.local_time_zone_table[2].name = NULL;
              }
            break;
          }
      }
  }
#else
#if HAVE_TZNAME
  {
# if !HAVE_DECL_TZNAME
    extern char *tzname[];
# endif
    int i;
    for (i = 0; i < 2; i++)
      {
        pc.local_time_zone_table[i].name = tzname[i];
        pc.local_time_zone_table[i].type = tLOCAL_ZONE;
        pc.local_time_zone_table[i].value = i;
      }
    pc.local_time_zone_table[i].name = NULL;
  }
#else
  pc.local_time_zone_table[0].name = NULL;
#endif
#endif

  if (pc.local_time_zone_table[0].name && pc.local_time_zone_table[1].name
      && ! strcmp (pc.local_time_zone_table[0].name,
                   pc.local_time_zone_table[1].name))
    {
       
      pc.local_time_zone_table[0].value = -1;
      pc.local_time_zone_table[1].name = NULL;
    }

  if (yyparse (&pc) != 0)
    {
      if (debugging (&pc))
        dbg_printf ((input_sentinel <= pc.input
                     ? _("error: parsing failed\n")
                     : _("error: parsing failed, stopped at '%s'\n")),
                    pc.input);
      goto fail;
    }


   

  if (debugging (&pc))
    {
      dbg_printf (_("input timezone: "));

      if (pc.timespec_seen)
        fprintf (stderr, _("'@timespec' - always UTC"));
      else if (pc.zones_seen)
        fprintf (stderr, _("parsed date/time string"));
      else if (tzstring)
        {
          if (tz != tzdefault)
            fprintf (stderr, _("TZ=\"%s\" in date string"), tzstring);
          else if (STREQ (tzstring, "UTC0"))
            {
               
              fprintf (stderr, _("TZ=\"UTC0\" environment value or -u"));
            }
          else
            fprintf (stderr, _("TZ=\"%s\" environment value"), tzstring);
        }
      else
        fprintf (stderr, _("system default"));

       
      if (pc.local_zones_seen && !pc.zones_seen && 0 < pc.local_isdst)
        fprintf (stderr, ", dst");

      if (pc.zones_seen)
        fprintf (stderr, " (%s)", time_zone_str (pc.time_zone, time_zone_buf));

      fputc ('\n', stderr);
    }

  if (pc.timespec_seen)
    *result = pc.seconds;
  else
    {
      if (1 < (pc.times_seen | pc.dates_seen | pc.days_seen | pc.dsts_seen
               | (pc.J_zones_seen + pc.local_zones_seen + pc.zones_seen)))
        {
          if (debugging (&pc))
            {
              if (pc.times_seen > 1)
                dbg_printf ("error: seen multiple time parts\n");
              if (pc.dates_seen > 1)
                dbg_printf ("error: seen multiple date parts\n");
              if (pc.days_seen > 1)
                dbg_printf ("error: seen multiple days parts\n");
              if (pc.dsts_seen > 1)
                dbg_printf ("error: seen multiple daylight-saving parts\n");
              if ((pc.J_zones_seen + pc.local_zones_seen + pc.zones_seen) > 1)
                dbg_printf ("error: seen multiple time-zone parts\n");
            }
          goto fail;
        }

      if (! to_tm_year (pc.year, debugging (&pc), &tm.tm_year)
          || ckd_add (&tm.tm_mon, pc.month, -1)
          || ckd_add (&tm.tm_mday, pc.day, 0))
        {
          if (debugging (&pc))
            dbg_printf (_("error: year, month, or day overflow\n"));
          goto fail;
        }
      if (pc.times_seen || (pc.rels_seen && ! pc.dates_seen && ! pc.days_seen))
        {
          tm.tm_hour = to_hour (pc.hour, pc.meridian);
          if (tm.tm_hour < 0)
            {
              char const *mrd = (pc.meridian == MERam ? "am"
                                 : pc.meridian == MERpm ?"pm" : "");
              if (debugging (&pc))
                dbg_printf (_("error: invalid hour %"PRIdMAX"%s\n"),
                            pc.hour, mrd);
              goto fail;
            }
          tm.tm_min = pc.minutes;
          tm.tm_sec = pc.seconds.tv_sec;
          if (debugging (&pc))
            dbg_printf ((pc.times_seen
                         ? _("using specified time as starting value: '%s'\n")
                         : _("using current time as starting value: '%s'\n")),
                        debug_strftime (&tm, dbg_tm, sizeof dbg_tm));
        }
      else
        {
          tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
          pc.seconds.tv_nsec = 0;
          if (debugging (&pc))
            dbg_printf ("warning: using midnight as starting time: 00:00:00\n");
        }

       
      if (pc.dates_seen | pc.days_seen | pc.times_seen)
        tm.tm_isdst = -1;

       
      if (pc.local_zones_seen)
        tm.tm_isdst = pc.local_isdst;

      tm0.tm_sec = tm.tm_sec;
      tm0.tm_min = tm.tm_min;
      tm0.tm_hour = tm.tm_hour;
      tm0.tm_mday = tm.tm_mday;
      tm0.tm_mon = tm.tm_mon;
      tm0.tm_year = tm.tm_year;
      tm0.tm_isdst = tm.tm_isdst;
      tm.tm_wday = -1;

      Start = mktime_z (tz, &tm);

      if (! mktime_ok (&tm0, &tm))
        {
          bool repaired = false;
          bool time_zone_seen = pc.zones_seen != 0;
          if (time_zone_seen)
            {
               

              char tz2buf[sizeof "XXX" - 1 + TIME_ZONE_BUFSIZE];
              tz2buf[0] = tz2buf[1] = tz2buf[2] = 'X';
              time_zone_str (pc.time_zone, &tz2buf[3]);
              timezone_t tz2 = tzalloc (tz2buf);
              if (!tz2)
                {
                  if (debugging (&pc))
                    dbg_printf (_("error: tzalloc (\"%s\") failed\n"), tz2buf);
                  goto fail;
                }
              tm.tm_sec = tm0.tm_sec;
              tm.tm_min = tm0.tm_min;
              tm.tm_hour = tm0.tm_hour;
              tm.tm_mday = tm0.tm_mday;
              tm.tm_mon = tm0.tm_mon;
              tm.tm_year = tm0.tm_year;
              tm.tm_isdst = tm0.tm_isdst;
              tm.tm_wday = -1;
              Start = mktime_z (tz2, &tm);
              repaired = mktime_ok (&tm0, &tm);
              tzfree (tz2);
            }

          if (! repaired)
            {
              debug_mktime_not_ok (&tm0, &tm, &pc, time_zone_seen);
              goto fail;
            }
        }

      char dbg_ord[DBGBUFSIZE];

      if (pc.days_seen && ! pc.dates_seen)
        {
          intmax_t dayincr;
          tm.tm_yday = -1;
          intmax_t day_ordinal = (pc.day_ordinal
                                  - (0 < pc.day_ordinal
                                     && tm.tm_wday != pc.day_number));
          if (! (ckd_mul (&dayincr, day_ordinal, 7)
                 || ckd_add (&dayincr, (pc.day_number - tm.tm_wday + 7) % 7,
                             dayincr)
                 || ckd_add (&tm.tm_mday, dayincr, tm.tm_mday)))
            {
              tm.tm_isdst = -1;
              Start = mktime_z (tz, &tm);
            }

          if (tm.tm_yday < 0)
            {
              if (debugging (&pc))
                dbg_printf (_("error: day '%s' "
                              "(day ordinal=%"PRIdMAX" number=%d) "
                              "resulted in an invalid date: '%s'\n"),
                            str_days (&pc, dbg_ord, sizeof dbg_ord),
                            pc.day_ordinal, pc.day_number,
                            debug_strfdatetime (&tm, &pc, dbg_tm,
                                                sizeof dbg_tm));
              goto fail;
            }

          if (debugging (&pc))
            dbg_printf (_("new start date: '%s' is '%s'\n"),
                        str_days (&pc, dbg_ord, sizeof dbg_ord),
                        debug_strfdatetime (&tm, &pc, dbg_tm, sizeof dbg_tm));

        }

      if (debugging (&pc))
        {
          if (!pc.dates_seen && !pc.days_seen)
            dbg_printf (_("using current date as starting value: '%s'\n"),
                        debug_strfdate (&tm, dbg_tm, sizeof dbg_tm));

          if (pc.days_seen && pc.dates_seen)
            dbg_printf (_("warning: day (%s) ignored when explicit dates "
                          "are given\n"),
                        str_days (&pc, dbg_ord, sizeof dbg_ord));

          dbg_printf (_("starting date/time: '%s'\n"),
                      debug_strfdatetime (&tm, &pc, dbg_tm, sizeof dbg_tm));
        }

       
      if (pc.rel.year | pc.rel.month | pc.rel.day)
        {
          if (debugging (&pc))
            {
              if ((pc.rel.year != 0 || pc.rel.month != 0) && tm.tm_mday != 15)
                dbg_printf (_("warning: when adding relative months/years, "
                              "it is recommended to specify the 15th of the "
                              "months\n"));

              if (pc.rel.day != 0 && tm.tm_hour != 12)
                dbg_printf (_("warning: when adding relative days, "
                              "it is recommended to specify noon\n"));
            }

          int year, month, day;
          if (ckd_add (&year, tm.tm_year, pc.rel.year)
              || ckd_add (&month, tm.tm_mon, pc.rel.month)
              || ckd_add (&day, tm.tm_mday, pc.rel.day))
            {
              if (debugging (&pc))
                dbg_printf (_("error: %s:%d\n"), __FILE__, __LINE__);
              goto fail;
            }
          tm.tm_year = year;
          tm.tm_mon = month;
          tm.tm_mday = day;
          tm.tm_hour = tm0.tm_hour;
          tm.tm_min = tm0.tm_min;
          tm.tm_sec = tm0.tm_sec;
          tm.tm_isdst = tm0.tm_isdst;
          tm.tm_wday = -1;
          Start = mktime_z (tz, &tm);
          if (tm.tm_wday < 0)
            {
              if (debugging (&pc))
                dbg_printf (_("error: adding relative date resulted "
                              "in an invalid date: '%s'\n"),
                            debug_strfdatetime (&tm, &pc, dbg_tm,
                                                sizeof dbg_tm));
              goto fail;
            }

          if (debugging (&pc))
            {
              dbg_printf (_("after date adjustment "
                            "(%+"PRIdMAX" years, %+"PRIdMAX" months, "
                            "%+"PRIdMAX" days),\n"),
                          pc.rel.year, pc.rel.month, pc.rel.day);
              dbg_printf (_("    new date/time = '%s'\n"),
                          debug_strfdatetime (&tm, &pc, dbg_tm,
                                              sizeof dbg_tm));

               
              if (tm0.tm_isdst != -1 && tm.tm_isdst != tm0.tm_isdst)
                dbg_printf (_("warning: daylight saving time changed after "
                              "date adjustment\n"));

               
              if (pc.rel.day == 0
                  && (tm.tm_mday != day
                      || (pc.rel.month == 0 && tm.tm_mon != month)))
                {
                  dbg_printf (_("warning: month/year adjustment resulted in "
                                "shifted dates:\n"));
                  char tm_year_buf[TM_YEAR_BUFSIZE];
                  dbg_printf (_("     adjusted Y M D: %s %02d %02d\n"),
                              tm_year_str (year, tm_year_buf), month + 1, day);
                  dbg_printf (_("   normalized Y M D: %s %02d %02d\n"),
                              tm_year_str (tm.tm_year, tm_year_buf),
                              tm.tm_mon + 1, tm.tm_mday);
                }
            }

        }

       
      if (pc.zones_seen)
        {
          bool overflow = false;
#ifdef HAVE_TM_GMTOFF
          long int utcoff = tm.tm_gmtoff;
#else
          time_t t = Start;
          struct tm gmt;
          int utcoff = (gmtime_r (&t, &gmt)
                        ? tm_diff (&tm, &gmt)
                        : (overflow = true, 0));
#endif
          intmax_t delta;
          overflow |= ckd_sub (&delta, pc.time_zone, utcoff);
          time_t t1;
          overflow |= ckd_sub (&t1, Start, delta);
          if (overflow)
            {
              if (debugging (&pc))
                dbg_printf (_("error: timezone %d caused time_t overflow\n"),
                            pc.time_zone);
              goto fail;
            }
          Start = t1;
        }

      if (debugging (&pc))
        {
          intmax_t Starti = Start;
          dbg_printf (_("'%s' = %"PRIdMAX" epoch-seconds\n"),
                      debug_strfdatetime (&tm, &pc, dbg_tm, sizeof dbg_tm),
                      Starti);
        }


       
      {
        intmax_t orig_ns = pc.seconds.tv_nsec;
        intmax_t sum_ns = orig_ns + pc.rel.ns;
        int normalized_ns = (sum_ns % BILLION + BILLION) % BILLION;
        int d4 = (sum_ns - normalized_ns) / BILLION;
        intmax_t d1, t1, d2, t2, t3;
        time_t t4;
        if (ckd_mul (&d1, pc.rel.hour, 60 * 60)
            || ckd_add (&t1, Start, d1)
            || ckd_mul (&d2, pc.rel.minutes, 60)
            || ckd_add (&t2, t1, d2)
            || ckd_add (&t3, t2, pc.rel.seconds)
            || ckd_add (&t4, t3, d4))
          {
            if (debugging (&pc))
              dbg_printf (_("error: adding relative time caused an "
                            "overflow\n"));
            goto fail;
          }

        result->tv_sec = t4;
        result->tv_nsec = normalized_ns;

        if (debugging (&pc)
            && (pc.rel.hour | pc.rel.minutes | pc.rel.seconds | pc.rel.ns))
          {
            dbg_printf (_("after time adjustment (%+"PRIdMAX" hours, "
                          "%+"PRIdMAX" minutes, "
                          "%+"PRIdMAX" seconds, %+d ns),\n"),
                        pc.rel.hour, pc.rel.minutes, pc.rel.seconds,
                        pc.rel.ns);
            intmax_t t4i = t4;
            dbg_printf (_("    new time = %"PRIdMAX" epoch-seconds\n"), t4i);

             
            struct tm lmt;
            if (tm.tm_isdst != -1 && localtime_rz (tz, &result->tv_sec, &lmt)
                && tm.tm_isdst != lmt.tm_isdst)
              dbg_printf (_("warning: daylight saving time changed after "
                            "time adjustment\n"));
          }
      }
    }

  if (debugging (&pc))
    {
       
      if (! tzstring)
        dbg_printf (_("timezone: system default\n"));
      else if (STREQ (tzstring, "UTC0"))
        dbg_printf (_("timezone: Universal Time\n"));
      else
        dbg_printf (_("timezone: TZ=\"%s\" environment value\n"), tzstring);

      intmax_t sec = result->tv_sec;
      int nsec = result->tv_nsec;
      dbg_printf (_("final: %"PRIdMAX".%09d (epoch-seconds)\n"),
                  sec, nsec);

      struct tm gmt, lmt;
      bool got_utc = !!gmtime_r (&result->tv_sec, &gmt);
      if (got_utc)
        dbg_printf (_("final: %s (UTC)\n"),
                    debug_strfdatetime (&gmt, NULL,
                                        dbg_tm, sizeof dbg_tm));
      if (localtime_rz (tz, &result->tv_sec, &lmt))
        {
#ifdef HAVE_TM_GMTOFF
          bool got_utcoff = true;
          long int utcoff = lmt.tm_gmtoff;
#else
          bool got_utcoff = got_utc;
          int utcoff;
          if (got_utcoff)
            utcoff = tm_diff (&lmt, &gmt);
#endif
          if (got_utcoff)
            dbg_printf (_("final: %s (UTC%s)\n"),
                        debug_strfdatetime (&lmt, NULL, dbg_tm, sizeof dbg_tm),
                        time_zone_str (utcoff, time_zone_buf));
          else
            dbg_printf (_("final: %s (unknown time zone offset)\n"),
                        debug_strfdatetime (&lmt, NULL, dbg_tm, sizeof dbg_tm));
        }
    }

  ok = true;

 fail:
  if (tz != tzdefault)
    tzfree (tz);
  free (tz1alloc);
  return ok;
}

#ifdef GNULIB_PARSE_DATETIME2
 
bool
parse_datetime2 (struct timespec *result, char const *p,
                 struct timespec const *now, unsigned int flags,
                 timezone_t tzdefault, char const *tzstring)
{
  return parse_datetime_body (result, p, now, flags, tzdefault, tzstring);
}
#endif


 
bool
parse_datetime (struct timespec *result, char const *p,
                struct timespec const *now)
{
  char const *tzstring = getenv ("TZ");
  timezone_t tz = tzalloc (tzstring);
  if (!tz)
    return false;
  bool ok = parse_datetime_body (result, p, now, 0, tz, tzstring);
  tzfree (tz);
  return ok;
}

#if TEST

int
main (int ac, char **av)
{
  char buff[BUFSIZ];

  printf ("Enter date, or blank line to exit.\n\t> ");
  fflush (stdout);

  buff[BUFSIZ - 1] = '\0';
  while (fgets (buff, BUFSIZ - 1, stdin) && buff[0])
    {
      struct timespec d;
      struct tm const *tm;
      if (! parse_datetime (&d, buff, NULL))
        printf ("Bad format - couldn't convert.\n");
      else if (! (tm = localtime (&d.tv_sec)))
        {
          intmax_t sec = d.tv_sec;
          printf ("localtime (%"PRIdMAX") failed\n", sec);
        }
      else
        {
          int ns = d.tv_nsec;
          char tm_year_buf[TM_YEAR_BUFSIZE];
          printf ("%s-%02d-%02d %02d:%02d:%02d.%09d\n",
                  tm_year_str (tm->tm_year, tm_year_buf),
                  tm->tm_mon + 1, tm->tm_mday,
                  tm->tm_hour, tm->tm_min, tm->tm_sec, ns);
        }
      printf ("\t> ");
      fflush (stdout);
    }
  return 0;
}
#endif  
