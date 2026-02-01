 
#include <stddef.h>

 
#include "localcharset.h"

#ifdef __cplusplus
extern "C" {
#endif


 

 

 
extern int
       uc_width (ucs4_t uc, const char *encoding)
       _UC_ATTRIBUTE_PURE;

 
extern int
       u8_width (const uint8_t *s, size_t n, const char *encoding)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_width (const uint16_t *s, size_t n, const char *encoding)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_width (const uint32_t *s, size_t n, const char *encoding)
       _UC_ATTRIBUTE_PURE;

 
extern int
       u8_strwidth (const uint8_t *s, const char *encoding)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_strwidth (const uint16_t *s, const char *encoding)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strwidth (const uint32_t *s, const char *encoding)
       _UC_ATTRIBUTE_PURE;


#ifdef __cplusplus
}
#endif

#endif  
