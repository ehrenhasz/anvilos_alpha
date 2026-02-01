
 

#include <linux/module.h>

#define EXPORT_SYMBOL_RUST_GPL(sym) extern int sym; EXPORT_SYMBOL_GPL(sym)

#include "exports_core_generated.h"
#include "exports_alloc_generated.h"
#include "exports_bindings_generated.h"
#include "exports_kernel_generated.h"


#ifdef CONFIG_RUST_BUILD_ASSERT_ALLOW
EXPORT_SYMBOL_RUST_GPL(rust_build_error);
#endif
