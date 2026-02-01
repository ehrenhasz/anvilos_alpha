 

 

#ifndef _MSTATS_H
#define _MSTATS_H

#include "imalloc.h"

#ifdef MALLOC_STATS

 
#ifndef NBUCKETS
#  define NBUCKETS 28
#endif

 
struct _malstats {
  int nmalloc[NBUCKETS];
  int tmalloc[NBUCKETS];
  int nmorecore[NBUCKETS];
  int nlesscore[NBUCKETS];
  int nmal;
  int nfre;
  int nrealloc;
  int nrcopy;
  int nrecurse;
  int nsbrk;
  bits32_t tsbrk;
  bits32_t bytesused;
  bits32_t bytesfree;
  u_bits32_t bytesreq;
  int tbsplit;
  int nsplit[NBUCKETS];
  int tbcoalesce;
  int ncoalesce[NBUCKETS];
  int nmmap;
  bits32_t tmmap;
};

 
struct bucket_stats {
  u_bits32_t blocksize;
  int nfree;
  int nused;
  int nmal;
  int nmorecore;
  int nlesscore;
  int nsplit;
  int ncoalesce;
  int nmmap;		 
};

extern struct bucket_stats malloc_bucket_stats PARAMS((int));
extern struct _malstats malloc_stats PARAMS((void));
extern void print_malloc_stats PARAMS((char *));
extern void trace_malloc_stats PARAMS((char *, char *));

#endif  

#endif  
