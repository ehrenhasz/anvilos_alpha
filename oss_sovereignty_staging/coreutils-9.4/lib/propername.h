 

#ifndef _PROPERNAME_H
#define _PROPERNAME_H


#ifdef __cplusplus
extern "C" {
#endif

 
extern const char * proper_name (const char *name)  ;

 
extern const char * proper_name_utf8 (const char *name_ascii,
                                      const char *name_utf8);

 
extern const char *proper_name_lite (const char *name_ascii,
                                     const char *name_utf8);

#ifdef __cplusplus
}
#endif


#endif  
