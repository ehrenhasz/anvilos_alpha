 

 
struct tm_zone
{
   
  struct tm_zone *next;

#if HAVE_TZNAME && !HAVE_STRUCT_TM_TM_ZONE
   
  char *tzname_copy[2];
#endif

   
  char tz_is_set;

   
  char abbrs[FLEXIBLE_ARRAY_MEMBER];
};
