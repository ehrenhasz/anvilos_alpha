 

 

 

 

 

 

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif  

#if defined (SHELL)
#  include "bashtypes.h"
#  include "stdc.h"
#else
#  include <sys/types.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

 
#include <signal.h>

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <errno.h>
#include <stdio.h>

#if !defined (botch)
#include <stdlib.h>
#endif

#if defined (HAVE_MMAP)
#include <sys/mman.h>
#endif

 
#ifndef HAVE_GETPAGESIZE
#  include "getpagesize.h"
#endif

#include "imalloc.h"
#ifdef MALLOC_STATS
#  include "mstats.h"
#endif
#ifdef MALLOC_REGISTER
#  include "table.h"
#endif
#ifdef MALLOC_WATCH
#  include "watch.h"
#endif

#ifdef powerof2
#  undef powerof2
#endif
 
#define powerof2(x)	((((x) - 1) & (x)) == 0)

 
#ifdef HPUX
#  define NO_VALLOC
#endif

#define MALLOC_PAGESIZE_MIN	4096
#define MALLOC_INCR_PAGES	8192

#define ISALLOC ((char) 0xf7)	 
#define ISFREE ((char) 0x54)	 
				 
#define ISMEMALIGN ((char) 0xd6)   


 
union mhead {
  bits64_t mh_align[2];						 
  struct {
    char mi_alloc; 		 		 
    char mi_index;		 		 
     
    u_bits16_t mi_magic2;	 	 
    u_bits32_t mi_nbytes;	 	 
    char mi_magic8[8];		 	 
  } minfo;
};
#define mh_alloc	minfo.mi_alloc
#define mh_index	minfo.mi_index
#define mh_nbytes	minfo.mi_nbytes
#define mh_magic2	minfo.mi_magic2
#define mh_magic8	minfo.mi_magic8

#define MAGIC8_NUMBYTES	8
#define MALLOC_SIZE_T		u_bits32_t

#define MOVERHEAD	sizeof(union mhead)

#define MALIGN_MASK	15		 

 
typedef union _malloc_guard {
  char s[4];
  u_bits32_t i;
} mguard_t;

 
 

 
#define CHAIN(a) \
  (*(union mhead **) (sizeof (char *) + (char *) (a)))

 

 
#define MAGIC1 0x55
#define MAGIC2 0x5555

#define MSLOP  4		 

 
#define ALLOCATED_BYTES(n) \
	(((n) + MOVERHEAD + MSLOP + MALIGN_MASK) & ~MALIGN_MASK)

#define ASSERT(p) \
  do \
    { \
      if (!(p)) xbotch((PTR_T)0, ERR_ASSERT_FAILED, CPP_STRING(p), file, line); \
    } \
  while (0)

 
#define SPLIT_MIN	1		 
#define SPLIT_MID	9		 
#define SPLIT_MAX	12		 

 
#define COMBINE_MIN	1		 
#define COMBINE_MAX	(pagebucket - 1)	 

#define LESSCORE_MIN	8		 
#define LESSCORE_FRC	11		 

 
#define PREPOP_BIN	1
#define PREPOP_SIZE	64

#define STARTBUCK	0

 
#if defined (HAVE_MMAP)
#  if defined (MAP_ANON) && !defined (MAP_ANONYMOUS)
#    define MAP_ANONYMOUS MAP_ANON
#  endif
#endif

#if defined (HAVE_MMAP) && defined (MAP_ANONYMOUS)
#  define USE_MMAP	1
#endif

#if defined (USE_MMAP)
#  define MMAP_THRESHOLD	12	 
#else
#  define MMAP_THRESHOLD	(8 * SIZEOF_LONG)
#endif

 
#if USE_MMAP == 1 && defined (HAVE_MREMAP) && defined (MREMAP_MAYMOVE)
#  define USE_MREMAP 1
#endif

 

#define NBUCKETS	28

 
#define MALLOC_WRAPPER	0x01	 
#define MALLOC_INTERNAL	0x02	 
#define MALLOC_NOTRACE	0x04	 
#define MALLOC_NOREG	0x08	 

 
#define ERR_DUPFREE		0x01
#define ERR_UNALLOC		0x02
#define ERR_UNDERFLOW		0x04	
#define ERR_ASSERT_FAILED	0x08

 
#define IN_BUCKET(nb, nu)	((nb) <= binsizes[(nu)])

 
#define RIGHT_BUCKET(nb, nu) \
	(((nb) > binsizes[(nu)-1]) && ((nb) <= binsizes[(nu)]))

 

static union mhead *nextf[NBUCKETS];

 

static char busy[NBUCKETS];

static int pagesz;	 
static int pagebucket;	 
static int maxbuck;	 

static char *memtop;	 

static const unsigned long binsizes[NBUCKETS] = {
	32UL, 64UL, 128UL, 256UL, 512UL, 1024UL, 2048UL, 4096UL,
	8192UL, 16384UL, 32768UL, 65536UL, 131072UL, 262144UL, 524288UL,
	1048576UL, 2097152UL, 4194304UL, 8388608UL, 16777216UL, 33554432UL,
	67108864UL, 134217728UL, 268435456UL, 536870912UL, 1073741824UL,
	2147483648UL, 4294967295UL
};

 
#define binsize(x)	binsizes[(x)]

#define MAXALLOC_SIZE	binsizes[NBUCKETS-1]

#if !defined (errno)
extern int errno;
#endif

 
static PTR_T internal_malloc PARAMS((size_t, const char *, int, int));
static PTR_T internal_realloc PARAMS((PTR_T, size_t, const char *, int, int));
static void internal_free PARAMS((PTR_T, const char *, int, int));
static PTR_T internal_memalign PARAMS((size_t, size_t, const char *, int, int));
#ifndef NO_CALLOC
static PTR_T internal_calloc PARAMS((size_t, size_t, const char *, int, int));
static void internal_cfree PARAMS((PTR_T, const char *, int, int));
#endif
#ifndef NO_VALLOC
static PTR_T internal_valloc PARAMS((size_t, const char *, int, int));
#endif
static PTR_T internal_remap PARAMS((PTR_T, size_t, int, int));

#if defined (botch)
extern void botch ();
#else
static void botch PARAMS((const char *, const char *, int));
#endif
static void xbotch PARAMS((PTR_T, int, const char *, const char *, int));

#if !HAVE_DECL_SBRK
extern char *sbrk ();
#endif  

#ifdef SHELL
extern int running_trap;
extern int signal_is_trapped PARAMS((int));
#endif

#ifdef MALLOC_STATS
struct _malstats _mstats;
#endif  

 
int malloc_flags = 0;	 
int malloc_trace = 0;	 
int malloc_register = 0;	 

 
int malloc_mmap_threshold = MMAP_THRESHOLD;

#ifdef MALLOC_TRACE
char _malloc_trace_buckets[NBUCKETS];

 
extern void mtrace_alloc PARAMS((const char *, PTR_T, size_t, const char *, int));
extern void mtrace_free PARAMS((PTR_T, int, const char *, int));
#endif

#if !defined (botch)
static void
botch (s, file, line)
     const char *s;
     const char *file;
     int line;
{
  fprintf (stderr, _("malloc: failed assertion: %s\n"), s);
  (void)fflush (stderr);
  abort ();
}
#endif

 
static void
xbotch (mem, e, s, file, line)
     PTR_T mem;
     int e;
     const char *s;
     const char *file;
     int line;
{
  fprintf (stderr, _("\r\nmalloc: %s:%d: assertion botched\r\n"),
			file ? file : _("unknown"), line);
#ifdef MALLOC_REGISTER
  if (mem != NULL && malloc_register)
    mregister_describe_mem (mem, stderr);
#endif
  (void)fflush (stderr);
  botch(s, file, line);
}

 
static void
bcoalesce (nu)
     register int nu;
{
  register union mhead *mp, *mp1, *mp2;
  register int nbuck;
  unsigned long siz;

  nbuck = nu - 1;
  if (nextf[nbuck] == 0 || busy[nbuck])
    return;

  busy[nbuck] = 1;
  siz = binsize (nbuck);

  mp2 = mp1 = nextf[nbuck];
  mp = CHAIN (mp1);
  while (mp && mp != (union mhead *)((char *)mp1 + siz))
    {
      mp2 = mp1;
      mp1 = mp;
      mp = CHAIN (mp);
    }

  if (mp == 0)
    {
      busy[nbuck] = 0;
      return;
    }

   
  if (mp2 != mp1 && CHAIN(mp2) != mp1)
    {
      busy[nbuck] = 0;
      xbotch ((PTR_T)0, 0, "bcoalesce: CHAIN(mp2) != mp1", (char *)NULL, 0);
    }

#ifdef MALLOC_DEBUG
  if (CHAIN (mp1) != (union mhead *)((char *)mp1 + siz))
    {
      busy[nbuck] = 0;
      return;	 
    }
#endif

   
  if (mp1 == nextf[nbuck])
    nextf[nbuck] = CHAIN (mp);
  else
    CHAIN (mp2) = CHAIN (mp);
  busy[nbuck] = 0;

#ifdef MALLOC_STATS
  _mstats.tbcoalesce++;
  _mstats.ncoalesce[nbuck]++;
#endif

   
  mp1->mh_alloc = ISFREE;
  mp1->mh_index = nu;
  CHAIN (mp1) = nextf[nu];
  nextf[nu] = mp1;
}

 
static void
bsplit (nu)
     register int nu;
{
  register union mhead *mp;
  int nbuck, nblks, split_max;
  unsigned long siz;

  split_max = (maxbuck > SPLIT_MAX) ? maxbuck : SPLIT_MAX;

  if (nu >= SPLIT_MID)
    {
      for (nbuck = split_max; nbuck > nu; nbuck--)
	{
	  if (busy[nbuck] || nextf[nbuck] == 0)
	    continue;
	  break;
	}
    }
  else
    {
      for (nbuck = nu + 1; nbuck <= split_max; nbuck++)
	{
	  if (busy[nbuck] || nextf[nbuck] == 0)
	    continue;
	  break;
	}
    }

  if (nbuck > split_max || nbuck <= nu)
    return;

   

   
  busy[nbuck] = 1;
  mp = nextf[nbuck];
  nextf[nbuck] = CHAIN (mp);
  busy[nbuck] = 0;

#ifdef MALLOC_STATS
  _mstats.tbsplit++;
  _mstats.nsplit[nbuck]++;
#endif

   
  siz = binsize (nu);
  nblks = binsize (nbuck) / siz;

   
  nextf[nu] = mp;
  while (1)
    {
      mp->mh_alloc = ISFREE;
      mp->mh_index = nu;
      if (--nblks <= 0) break;
      CHAIN (mp) = (union mhead *)((char *)mp + siz);
      mp = (union mhead *)((char *)mp + siz);
    }
  CHAIN (mp) = 0;
}

 
static void
xsplit (mp, nu)
     union mhead *mp;
     int nu;
{
  union mhead *nh;
  int nbuck, nblks, split_max;
  unsigned long siz;

  nbuck = nu - 1;
  while (nbuck >= SPLIT_MIN && busy[nbuck])
    nbuck--;
  if (nbuck < SPLIT_MIN)
    return;

#ifdef MALLOC_STATS
  _mstats.tbsplit++;
  _mstats.nsplit[nu]++;
#endif

   
  siz = binsize (nu);			 
  nblks = siz / binsize (nbuck);	 

   
  siz = binsize (nbuck);		 
  nh = mp;
  while (1)
    {
      mp->mh_alloc = ISFREE;
      mp->mh_index = nbuck;
      if (--nblks <= 0) break;
      CHAIN (mp) = (union mhead *)((char *)mp + siz);
      mp = (union mhead *)((char *)mp + siz);
    }
  busy[nbuck] = 1;
  CHAIN (mp) = nextf[nbuck];
  nextf[nbuck] = nh;
  busy[nbuck] = 0;
}

void
_malloc_block_signals (setp, osetp)
     sigset_t *setp, *osetp;
{
#ifdef HAVE_POSIX_SIGNALS
  sigfillset (setp);
  sigemptyset (osetp);
  sigprocmask (SIG_BLOCK, setp, osetp);
#else
#  if defined (HAVE_BSD_SIGNALS)
  *osetp = sigsetmask (-1);
#  endif
#endif
}

void
_malloc_unblock_signals (setp, osetp)
     sigset_t *setp, *osetp;
{
#ifdef HAVE_POSIX_SIGNALS
  sigprocmask (SIG_SETMASK, osetp, (sigset_t *)NULL);
#else
#  if defined (HAVE_BSD_SIGNALS)
  sigsetmask (*osetp);
#  endif
#endif
}

#if defined (USE_LESSCORE)
   
static void
lesscore (nu)			 
     register int nu;		 
{
  long siz;

  siz = binsize (nu);
   
  sbrk (-siz);
  memtop -= siz;

#ifdef MALLOC_STATS
  _mstats.nsbrk++;
  _mstats.tsbrk -= siz;
  _mstats.nlesscore[nu]++;
#endif
}
#endif  

   
static void
morecore (nu)
     register int nu;		 
{
  register union mhead *mp;
  register int nblks;
  register long siz;
  long sbrk_amt;		 
  sigset_t set, oset;
  int blocked_sigs;

   
  blocked_sigs = 0;
#ifdef SHELL
#  if defined (SIGCHLD)
  if (running_trap || signal_is_trapped (SIGINT) || signal_is_trapped (SIGCHLD))
#  else
  if (running_trap || signal_is_trapped (SIGINT))
#  endif
#endif
    {
      _malloc_block_signals (&set, &oset);
      blocked_sigs = 1;
    }

  siz = binsize (nu);	 

  if (siz < 0)
    goto morecore_done;		 

#ifdef MALLOC_STATS
  _mstats.nmorecore[nu]++;
#endif

   
  if (nu >= SPLIT_MIN && nu <= malloc_mmap_threshold)
    {
      bsplit (nu);
      if (nextf[nu] != 0)
	goto morecore_done;
    }

   
  if (nu >= COMBINE_MIN && nu < COMBINE_MAX && nu <= malloc_mmap_threshold && busy[nu - 1] == 0 && nextf[nu - 1])
    {
      bcoalesce (nu);
      if (nextf[nu] != 0)
	goto morecore_done;
    }

   
  if (siz <= pagesz)
    {
      sbrk_amt = pagesz;
      nblks = sbrk_amt / siz;
    }
  else
    {
       
      sbrk_amt = siz & (pagesz - 1);
      if (sbrk_amt == 0)
	sbrk_amt = siz;
      else
	sbrk_amt = siz + pagesz - sbrk_amt;
      nblks = 1;
    }

#if defined (USE_MMAP)
  if (nu > malloc_mmap_threshold)
    {
      mp = (union mhead *)mmap (0, sbrk_amt, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      if ((void *)mp == MAP_FAILED)
	goto morecore_done;
      nextf[nu] = mp;
      mp->mh_alloc = ISFREE;
      mp->mh_index = nu;
      CHAIN (mp) = 0;
#ifdef MALLOC_STATS
      _mstats.nmmap++;
      _mstats.tmmap += sbrk_amt;
#endif
      goto morecore_done;
    }
#endif
	

#ifdef MALLOC_STATS
  _mstats.nsbrk++;
  _mstats.tsbrk += sbrk_amt;
#endif

  mp = (union mhead *) sbrk (sbrk_amt);

   
  if ((long)mp == -1)
    goto morecore_done;

  memtop += sbrk_amt;

   
  if ((long)mp & MALIGN_MASK)
    {
      mp = (union mhead *) (((long)mp + MALIGN_MASK) & ~MALIGN_MASK);
      nblks--;
    }

   
  nextf[nu] = mp;
  while (1)
    {
      mp->mh_alloc = ISFREE;
      mp->mh_index = nu;
      if (--nblks <= 0) break;
      CHAIN (mp) = (union mhead *)((char *)mp + siz);
      mp = (union mhead *)((char *)mp + siz);
    }
  CHAIN (mp) = 0;

morecore_done:
  if (blocked_sigs)
    _malloc_unblock_signals (&set, &oset);
}

static void
malloc_debug_dummy ()
{
  write (1, "malloc_debug_dummy\n", 19);
}

static int
pagealign ()
{
  register int nunits;
  register union mhead *mp;
  long sbrk_needed;
  char *curbrk;

  pagesz = getpagesize ();
  if (pagesz < MALLOC_PAGESIZE_MIN)
    pagesz = MALLOC_PAGESIZE_MIN;

   
  memtop = curbrk = sbrk (0);
  sbrk_needed = pagesz - ((long)curbrk & (pagesz - 1));	 
  if (sbrk_needed < 0)
    sbrk_needed += pagesz;

   
  if (sbrk_needed)
    {
#ifdef MALLOC_STATS
      _mstats.nsbrk++;
      _mstats.tsbrk += sbrk_needed;
#endif
      curbrk = sbrk (sbrk_needed);
      if ((long)curbrk == -1)
	return -1;
      memtop += sbrk_needed;

       
      curbrk += sbrk_needed & (PREPOP_SIZE - 1);
      sbrk_needed -= sbrk_needed & (PREPOP_SIZE - 1);
      nunits = sbrk_needed / PREPOP_SIZE;

      if (nunits > 0)
	{
	  mp = (union mhead *)curbrk;

	  nextf[PREPOP_BIN] = mp;
	  while (1)
	    {
	      mp->mh_alloc = ISFREE;
	      mp->mh_index = PREPOP_BIN;
	      if (--nunits <= 0) break;
	      CHAIN(mp) = (union mhead *)((char *)mp + PREPOP_SIZE);
	      mp = (union mhead *)((char *)mp + PREPOP_SIZE);
	    }
	  CHAIN(mp) = 0;
	}
    }

   
  for (nunits = 7; nunits < NBUCKETS; nunits++)
    if (pagesz <= binsize(nunits))
      break;
  pagebucket = nunits;

  return 0;
}
    
static PTR_T
internal_malloc (n, file, line, flags)		 
     size_t n;
     const char *file;
     int line, flags;
{
  register union mhead *p;
  register int nunits;
  register char *m, *z;
  MALLOC_SIZE_T nbytes;
  mguard_t mg;

   
  if (pagesz == 0)
    if (pagealign () < 0)
      return ((PTR_T)NULL);
 
   
#if SIZEOF_SIZE_T == 8
  if (ALLOCATED_BYTES(n) > MAXALLOC_SIZE)
    return ((PTR_T) NULL);
#endif
  nbytes = ALLOCATED_BYTES(n);
  nunits = (nbytes <= (pagesz >> 1)) ? STARTBUCK : pagebucket;
  for ( ; nunits < NBUCKETS; nunits++)
    if (nbytes <= binsize(nunits))
      break;

   
  if (nunits >= NBUCKETS)
    return ((PTR_T) NULL);

   
#ifdef MALLOC_STATS
  if (busy[nunits]) _mstats.nrecurse++;
#endif
  while (busy[nunits]) nunits++;
  busy[nunits] = 1;

  if (nunits > maxbuck)
    maxbuck = nunits;

   
  if (nextf[nunits] == 0)
    morecore (nunits);

   
  if ((p = nextf[nunits]) == NULL)
    {
      busy[nunits] = 0;
      return NULL;
    }
  nextf[nunits] = CHAIN (p);
  busy[nunits] = 0;

   
   
  if (p->mh_alloc != ISFREE || p->mh_index != nunits)
    xbotch ((PTR_T)(p+1), 0, _("malloc: block on free list clobbered"), file, line);

   
  p->mh_alloc = ISALLOC;
  p->mh_magic2 = MAGIC2;
  p->mh_nbytes = n;

   
  MALLOC_MEMSET ((char *)p->mh_magic8, MAGIC1, MAGIC8_NUMBYTES);

   
  mg.i = n;
  z = mg.s;
  m = (char *) (p + 1) + n;
  *m++ = *z++, *m++ = *z++, *m++ = *z++, *m++ = *z++;

#ifdef MEMSCRAMBLE
  if (n)
    MALLOC_MEMSET ((char *)(p + 1), 0xdf, n);	 
#endif
#ifdef MALLOC_STATS
  _mstats.nmalloc[nunits]++;
  _mstats.tmalloc[nunits]++;
  _mstats.nmal++;
  _mstats.bytesreq += n;
#endif  

#ifdef MALLOC_TRACE
  if (malloc_trace && (flags & MALLOC_NOTRACE) == 0)
    mtrace_alloc ("malloc", p + 1, n, file, line);
  else if (_malloc_trace_buckets[nunits])
    mtrace_alloc ("malloc", p + 1, n, file, line);
#endif

#ifdef MALLOC_REGISTER
  if (malloc_register && (flags & MALLOC_NOREG) == 0)
    mregister_alloc ("malloc", p + 1, n, file, line);
#endif

#ifdef MALLOC_WATCH
  if (_malloc_nwatch > 0)
    _malloc_ckwatch (p + 1, file, line, W_ALLOC, n);
#endif

#if defined (MALLOC_DEBUG)
  z = (char *) (p + 1);
   
  if ((unsigned long)z & MALIGN_MASK)
    fprintf (stderr, "malloc: %s:%d: warning: request for %d bytes not aligned on %d byte boundary\r\n",
	file ? file : _("unknown"), line, p->mh_nbytes, MALIGN_MASK+1);
#endif

  return (PTR_T) (p + 1);
}

static void
internal_free (mem, file, line, flags)
     PTR_T mem;
     const char *file;
     int line, flags;
{
  register union mhead *p;
  register char *ap, *z;
  register int nunits;
  register MALLOC_SIZE_T nbytes;
  MALLOC_SIZE_T ubytes;		 
  mguard_t mg;

  if ((ap = (char *)mem) == 0)
    return;

  p = (union mhead *) ap - 1;

  if (p->mh_alloc == ISMEMALIGN)
    {
      ap -= p->mh_nbytes;
      p = (union mhead *) ap - 1;
    }

#if defined (MALLOC_TRACE) || defined (MALLOC_REGISTER) || defined (MALLOC_WATCH)
  if (malloc_trace || malloc_register || _malloc_nwatch > 0)
    ubytes = p->mh_nbytes;
#endif

  if (p->mh_alloc != ISALLOC)
    {
      if (p->mh_alloc == ISFREE)
	xbotch (mem, ERR_DUPFREE,
		_("free: called with already freed block argument"), file, line);
      else
	xbotch (mem, ERR_UNALLOC,
		_("free: called with unallocated block argument"), file, line);
    }

  ASSERT (p->mh_magic2 == MAGIC2);

  nunits = p->mh_index;
  nbytes = ALLOCATED_BYTES(p->mh_nbytes);
   

  if (IN_BUCKET(nbytes, nunits) == 0)
    xbotch (mem, ERR_UNDERFLOW,
	    _("free: underflow detected; mh_nbytes out of range"), file, line);
  {
    int i;
    for (i = 0, z = p->mh_magic8; i < MAGIC8_NUMBYTES; i++)
      if (*z++ != MAGIC1)
	xbotch (mem, ERR_UNDERFLOW,
		_("free: underflow detected; magic8 corrupted"), file, line);
  }

  ap += p->mh_nbytes;
  z = mg.s;
  *z++ = *ap++, *z++ = *ap++, *z++ = *ap++, *z++ = *ap++;  
  if (mg.i != p->mh_nbytes)
    xbotch (mem, ERR_ASSERT_FAILED, _("free: start and end chunk sizes differ"), file, line);

#if defined (USE_MMAP)
  if (nunits > malloc_mmap_threshold)
    {
      munmap (p, binsize (nunits));
#if defined (MALLOC_STATS)
      _mstats.nlesscore[nunits]++;
#endif
      goto free_return;
    }
#endif

#if defined (USE_LESSCORE)
   
  if (nunits >= LESSCORE_MIN && ((char *)p + binsize(nunits) == memtop))
    {
       
      if ((nunits >= LESSCORE_FRC) || busy[nunits] || nextf[nunits] != 0)
	{
	  lesscore (nunits);
	   
	  goto free_return;
	}
    }
#endif  

#ifdef MEMSCRAMBLE
  if (p->mh_nbytes)
    MALLOC_MEMSET (mem, 0xcf, p->mh_nbytes);
#endif

  ASSERT (nunits < NBUCKETS);

  if (busy[nunits] == 1)
    {
      xsplit (p, nunits);	 
      goto free_return;
    }

  p->mh_alloc = ISFREE;
   
  busy[nunits] = 1;
   
  CHAIN (p) = nextf[nunits];
  nextf[nunits] = p;
  busy[nunits] = 0;

free_return:
  ;		 

#ifdef MALLOC_STATS
  _mstats.nmalloc[nunits]--;
  _mstats.nfre++;
#endif  

#ifdef MALLOC_TRACE
  if (malloc_trace && (flags & MALLOC_NOTRACE) == 0)
    mtrace_free (mem, ubytes, file, line);
  else if (_malloc_trace_buckets[nunits])
    mtrace_free (mem, ubytes, file, line);
#endif

#ifdef MALLOC_REGISTER
  if (malloc_register && (flags & MALLOC_NOREG) == 0)
    mregister_free (mem, ubytes, file, line);
#endif

#ifdef MALLOC_WATCH
  if (_malloc_nwatch > 0)
    _malloc_ckwatch (mem, file, line, W_FREE, ubytes);
#endif
}

#if USE_MREMAP == 1
 
static PTR_T
internal_remap (mem, n, nunits, flags)
     PTR_T mem;
     register size_t n;
     int nunits;
     int flags;
{
  register union mhead *p, *np;
  char *m, *z;
  mguard_t mg;
  MALLOC_SIZE_T nbytes;

  if (nunits >= NBUCKETS)	 
    return ((PTR_T) NULL);

  p = (union mhead *)mem - 1;

  m = (char *)mem + p->mh_nbytes;
  z = mg.s;
  *m++ = 0;  *m++ = 0;  *m++ = 0;  *m++ = 0;	 

  nbytes = ALLOCATED_BYTES(n);

  busy[nunits] = 1;
  np = (union mhead *)mremap (p, binsize (p->mh_index), binsize (nunits), MREMAP_MAYMOVE);
  busy[nunits] = 0;
  if (np == MAP_FAILED)
    return (PTR_T)NULL;

  if (np != p)
    {
      np->mh_alloc = ISALLOC;
      np->mh_magic2 = MAGIC2;
      MALLOC_MEMSET ((char *)np->mh_magic8, MAGIC1, MAGIC8_NUMBYTES);
    }
  np->mh_index = nunits;
  np->mh_nbytes = n;

  mg.i = n;
  z = mg.s;
  m = (char *)(np + 1) + n;
  *m++ = *z++, *m++ = *z++, *m++ = *z++, *m++ = *z++;

  return ((PTR_T)(np + 1));
}
#endif

static PTR_T
internal_realloc (mem, n, file, line, flags)
     PTR_T mem;
     register size_t n;
     const char *file;
     int line, flags;
{
  register union mhead *p;
  register MALLOC_SIZE_T tocopy;
  register MALLOC_SIZE_T nbytes;
  register int newunits, nunits;
  register char *m, *z;
  mguard_t mg;

#ifdef MALLOC_STATS
  _mstats.nrealloc++;
#endif

  if (n == 0)
    {
      internal_free (mem, file, line, MALLOC_INTERNAL);
      return (NULL);
    }
  if ((p = (union mhead *) mem) == 0)
    return internal_malloc (n, file, line, MALLOC_INTERNAL);

  p--;
  nunits = p->mh_index;
  ASSERT (nunits < NBUCKETS);

  if (p->mh_alloc != ISALLOC)
    xbotch (mem, ERR_UNALLOC,
	    _("realloc: called with unallocated block argument"), file, line);

  ASSERT (p->mh_magic2 == MAGIC2);
  nbytes = ALLOCATED_BYTES(p->mh_nbytes);
   
  if (IN_BUCKET(nbytes, nunits) == 0)
    xbotch (mem, ERR_UNDERFLOW,
	    _("realloc: underflow detected; mh_nbytes out of range"), file, line);
  {
    int i;
    for (i = 0, z = p->mh_magic8; i < MAGIC8_NUMBYTES; i++)
      if (*z++ != MAGIC1)
	xbotch (mem, ERR_UNDERFLOW,
		_("realloc: underflow detected; magic8 corrupted"), file, line);

  }

  m = (char *)mem + (tocopy = p->mh_nbytes);
  z = mg.s;
  *z++ = *m++, *z++ = *m++, *z++ = *m++, *z++ = *m++;
  if (mg.i != p->mh_nbytes)
    xbotch (mem, ERR_ASSERT_FAILED, _("realloc: start and end chunk sizes differ"), file, line);

#ifdef MALLOC_WATCH
  if (_malloc_nwatch > 0)
    _malloc_ckwatch (p + 1, file, line, W_REALLOC, n);
#endif
#ifdef MALLOC_STATS
  _mstats.bytesreq += (n < tocopy) ? 0 : n - tocopy;
#endif

   
  if (n == p->mh_nbytes)
    return mem;

#if SIZEOF_SIZE_T == 8
  if (ALLOCATED_BYTES(n) > MAXALLOC_SIZE)
    return ((PTR_T) NULL);
#endif
   
  nbytes = ALLOCATED_BYTES(n);

   
  if (RIGHT_BUCKET(nbytes, nunits) || RIGHT_BUCKET(nbytes, nunits-1))
    {
       
      m -= 4;

      *m++ = 0;  *m++ = 0;  *m++ = 0;  *m++ = 0;
      m = (char *)mem + (p->mh_nbytes = n);

      mg.i = n;
      z = mg.s;
      *m++ = *z++, *m++ = *z++, *m++ = *z++, *m++ = *z++;      

      return mem;
    }

  if (n < tocopy)
    tocopy = n;

#ifdef MALLOC_STATS
  _mstats.nrcopy++;
#endif

#if USE_MREMAP == 1
   
  if (nbytes > p->mh_nbytes)
    newunits = nunits;
  else
    newunits = (nbytes <= (pagesz >> 1)) ? STARTBUCK : pagebucket;
  for ( ; newunits < NBUCKETS; newunits++)
    if (nbytes <= binsize(newunits))
     break;

  if (nunits > malloc_mmap_threshold && newunits > malloc_mmap_threshold)
    {
      m = internal_remap (mem, n, newunits, MALLOC_INTERNAL);
      if (m == 0)
        return 0;
    }
  else
#endif  
    {
  if ((m = internal_malloc (n, file, line, MALLOC_INTERNAL|MALLOC_NOTRACE|MALLOC_NOREG)) == 0)
    return 0;
  FASTCOPY (mem, m, tocopy);
  internal_free (mem, file, line, MALLOC_INTERNAL);
    }

#ifdef MALLOC_TRACE
  if (malloc_trace && (flags & MALLOC_NOTRACE) == 0)
    mtrace_alloc ("realloc", m, n, file, line);
  else if (_malloc_trace_buckets[nunits])
    mtrace_alloc ("realloc", m, n, file, line);
#endif

#ifdef MALLOC_REGISTER
  if (malloc_register && (flags & MALLOC_NOREG) == 0)
    mregister_alloc ("realloc", m, n, file, line);
#endif

#ifdef MALLOC_WATCH
  if (_malloc_nwatch > 0)
    _malloc_ckwatch (m, file, line, W_RESIZED, n);
#endif

  return m;
}

static PTR_T
internal_memalign (alignment, size, file, line, flags)
     size_t alignment;
     size_t size;
     const char *file;
     int line, flags;
{
  register char *ptr;
  register char *aligned;
  register union mhead *p;

  ptr = internal_malloc (size + alignment, file, line, MALLOC_INTERNAL);

  if (ptr == 0)
    return 0;
   
  if (((long) ptr & (alignment - 1)) == 0)
    return ptr;
   
  aligned = (char *) (((long) ptr + alignment - 1) & (~alignment + 1));

   
  p = (union mhead *) aligned - 1;
  p->mh_nbytes = aligned - ptr;
  p->mh_alloc = ISMEMALIGN;

  return aligned;
}

int
posix_memalign (memptr, alignment, size)
     void **memptr;
     size_t alignment, size;
{
  void *mem;

   
  if ((alignment % sizeof (void *) != 0) || alignment == 0)
    return EINVAL;
  else if (powerof2 (alignment) == 0)
    return EINVAL;

  mem = internal_memalign (alignment, size, (char *)0, 0, 0);
  if (mem != 0)
    {
      *memptr = mem;
      return 0;
    }
  return ENOMEM;
}

size_t
malloc_usable_size (mem)
     void *mem;
{
  register union mhead *p;
  register char *ap;

  if ((ap = (char *)mem) == 0)
    return 0;

   
  p = (union mhead *) ap - 1;

  if (p->mh_alloc == ISMEMALIGN)
    {
      ap -= p->mh_nbytes;
      p = (union mhead *) ap - 1;
    }

   
  if (p->mh_alloc == ISFREE)
    return 0;
  
   
  return (p->mh_nbytes);
}

#if !defined (NO_VALLOC)
 
static PTR_T
internal_valloc (size, file, line, flags)
     size_t size;
     const char *file;
     int line, flags;
{
  return internal_memalign (getpagesize (), size, file, line, flags|MALLOC_INTERNAL);
}
#endif  

#ifndef NO_CALLOC
static PTR_T
internal_calloc (n, s, file, line, flags)
     size_t n, s;
     const char *file;
     int line, flags;
{
  size_t total;
  PTR_T result;

  total = n * s;
  result = internal_malloc (total, file, line, flags|MALLOC_INTERNAL);
  if (result)
    memset (result, 0, total);
  return result;  
}

static void
internal_cfree (p, file, line, flags)
     PTR_T p;
     const char *file;
     int line, flags;
{
  internal_free (p, file, line, flags|MALLOC_INTERNAL);
}
#endif  

#ifdef MALLOC_STATS
int
malloc_free_blocks (size)
     int size;
{
  int nfree;
  register union mhead *p;

  nfree = 0;
  for (p = nextf[size]; p; p = CHAIN (p))
    nfree++;

  return nfree;
}
#endif

#if defined (MALLOC_WRAPFUNCS)
PTR_T
sh_malloc (bytes, file, line)
     size_t bytes;
     const char *file;
     int line;
{
  return internal_malloc (bytes, file, line, MALLOC_WRAPPER);
}

PTR_T
sh_realloc (ptr, size, file, line)
     PTR_T ptr;
     size_t size;
     const char *file;
     int line;
{
  return internal_realloc (ptr, size, file, line, MALLOC_WRAPPER);
}

void
sh_free (mem, file, line)
     PTR_T mem;
     const char *file;
     int line;
{
  internal_free (mem, file, line, MALLOC_WRAPPER);
}

PTR_T
sh_memalign (alignment, size, file, line)
     size_t alignment;
     size_t size;
     const char *file;
     int line;
{
  return internal_memalign (alignment, size, file, line, MALLOC_WRAPPER);
}

#ifndef NO_CALLOC
PTR_T
sh_calloc (n, s, file, line)
     size_t n, s;
     const char *file;
     int line;
{
  return internal_calloc (n, s, file, line, MALLOC_WRAPPER);
}

void
sh_cfree (mem, file, line)
     PTR_T mem;
     const char *file;
     int line;
{
  internal_cfree (mem, file, line, MALLOC_WRAPPER);
}
#endif

#ifndef NO_VALLOC
PTR_T
sh_valloc (size, file, line)
     size_t size;
     const char *file;
     int line;
{
  return internal_valloc (size, file, line, MALLOC_WRAPPER);
}
#endif  

#endif  

 

PTR_T
malloc (size)
     size_t size;
{
  return internal_malloc (size, (char *)NULL, 0, 0);
}

PTR_T
realloc (mem, nbytes)
     PTR_T mem;
     size_t nbytes;
{
  return internal_realloc (mem, nbytes, (char *)NULL, 0, 0);
}

void
free (mem)
     PTR_T mem;
{
  internal_free (mem,  (char *)NULL, 0, 0);
}

PTR_T
memalign (alignment, size)
     size_t alignment;
     size_t size;
{
  return internal_memalign (alignment, size, (char *)NULL, 0, 0);
}

#ifndef NO_VALLOC
PTR_T
valloc (size)
     size_t size;
{
  return internal_valloc (size, (char *)NULL, 0, 0);
}
#endif

#ifndef NO_CALLOC
PTR_T
calloc (n, s)
     size_t n, s;
{
  return internal_calloc (n, s, (char *)NULL, 0, 0);
}

void
cfree (mem)
     PTR_T mem;
{
  internal_cfree (mem, (char *)NULL, 0, 0);
}
#endif
