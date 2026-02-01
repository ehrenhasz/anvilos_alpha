
 


#include <kunit/test-bug.h>

DEFINE_STATIC_KEY_FALSE(kunit_running);
EXPORT_SYMBOL(kunit_running);

 
struct kunit_hooks_table kunit_hooks;
EXPORT_SYMBOL(kunit_hooks);

