

#ifndef	ZED_STRINGS_H
#define	ZED_STRINGS_H

typedef struct zed_strings zed_strings_t;

zed_strings_t *zed_strings_create(void);
void zed_strings_destroy(zed_strings_t *zsp);
int zed_strings_add(zed_strings_t *zsp, const char *key, const char *s);
const char *zed_strings_first(zed_strings_t *zsp);
const char *zed_strings_next(zed_strings_t *zsp);
int zed_strings_count(zed_strings_t *zsp);

#endif	
