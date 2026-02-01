 

 

#include "stdc.h"
#include "hashlib.h"

#define FILENAME_HASH_BUCKETS 256	 

extern HASH_TABLE *hashed_filenames;

typedef struct _pathdata {
  char *path;		 
  int flags;
} PATH_DATA;

#define HASH_RELPATH	0x01	 
#define HASH_CHKDOT	0x02	 

#define pathdata(x) ((PATH_DATA *)(x)->data)

extern void phash_create PARAMS((void));
extern void phash_flush PARAMS((void));

extern void phash_insert PARAMS((char *, char *, int, int));
extern int phash_remove PARAMS((const char *));
extern char *phash_search PARAMS((const char *));
