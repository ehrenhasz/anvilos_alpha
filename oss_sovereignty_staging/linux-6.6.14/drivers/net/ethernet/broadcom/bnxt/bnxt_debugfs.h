 

#include "bnxt_hsi.h"
#include "bnxt.h"

#ifdef CONFIG_DEBUG_FS
void bnxt_debug_init(void);
void bnxt_debug_exit(void);
void bnxt_debug_dev_init(struct bnxt *bp);
void bnxt_debug_dev_exit(struct bnxt *bp);
#else
static inline void bnxt_debug_init(void) {}
static inline void bnxt_debug_exit(void) {}
static inline void bnxt_debug_dev_init(struct bnxt *bp) {}
static inline void bnxt_debug_dev_exit(struct bnxt *bp) {}
#endif
