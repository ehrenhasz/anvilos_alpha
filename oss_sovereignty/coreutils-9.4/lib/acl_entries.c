 

#include <config.h>

#include "acl-internal.h"

 

 

int
acl_entries (acl_t acl)
{
  int count = 0;

  if (acl != NULL)
    {
#if HAVE_ACL_FIRST_ENTRY  
# if HAVE_ACL_TYPE_EXTENDED  
       
      acl_entry_t ace;
      int got_one;

      for (got_one = acl_get_entry (acl, ACL_FIRST_ENTRY, &ace);
           got_one >= 0;
           got_one = acl_get_entry (acl, ACL_NEXT_ENTRY, &ace))
        count++;
# else  
       
      acl_entry_t ace;
      int got_one;

      for (got_one = acl_get_entry (acl, ACL_FIRST_ENTRY, &ace);
           got_one > 0;
           got_one = acl_get_entry (acl, ACL_NEXT_ENTRY, &ace))
        count++;
      if (got_one < 0)
        return -1;
# endif
#else  
# if HAVE_ACL_TO_SHORT_TEXT  
       
      count = acl->acl_cnt;
# endif
# if HAVE_ACL_FREE_TEXT  
       
      count = acl->acl_num;
# endif
#endif
    }

  return count;
}
