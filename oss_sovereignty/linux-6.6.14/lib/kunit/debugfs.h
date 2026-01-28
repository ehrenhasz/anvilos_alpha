#ifndef _KUNIT_DEBUGFS_H
#define _KUNIT_DEBUGFS_H
#include <kunit/test.h>
#ifdef CONFIG_KUNIT_DEBUGFS
void kunit_debugfs_create_suite(struct kunit_suite *suite);
void kunit_debugfs_destroy_suite(struct kunit_suite *suite);
void kunit_debugfs_init(void);
void kunit_debugfs_cleanup(void);
#else
static inline void kunit_debugfs_create_suite(struct kunit_suite *suite) { }
static inline void kunit_debugfs_destroy_suite(struct kunit_suite *suite) { }
static inline void kunit_debugfs_init(void) { }
static inline void kunit_debugfs_cleanup(void) { }
#endif  
#endif  
