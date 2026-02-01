 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

 
#if !defined (__GNUC__) || __GNUC__ < 2

#include <bashtypes.h>		 

 
#ifndef alloca

#ifdef emacs
#ifdef static
 
#ifndef STACK_DIRECTION
you
lose
-- must know STACK_DIRECTION at compile-time
#endif  
#endif  
#endif  

 

#if defined (CRAY) && defined (CRAY_STACKSEG_END)
long i00afunc ();
#define ADDRESS_FUNCTION(arg) (char *) i00afunc (&(arg))
#else
#define ADDRESS_FUNCTION(arg) &(arg)
#endif  

#if __STDC__
typedef void *pointer;
#else
typedef char *pointer;
#endif

#define	NULL	0

 

#ifndef emacs
#define malloc xmalloc
extern pointer xmalloc ();
#endif

 

#ifndef STACK_DIRECTION
#define	STACK_DIRECTION	0	 
#endif

#if STACK_DIRECTION != 0

#define	STACK_DIR	STACK_DIRECTION	 

#else  

static int stack_dir;		 
#define	STACK_DIR	stack_dir

static void
find_stack_direction ()
{
  static char *addr = NULL;	 
  auto char dummy;		 

  if (addr == NULL)
    {				 
      addr = ADDRESS_FUNCTION (dummy);

      find_stack_direction ();	 
    }
  else
    {
       
      if (ADDRESS_FUNCTION (dummy) > addr)
	stack_dir = 1;		 
      else
	stack_dir = -1;		 
    }
}

#endif  

 

#ifndef	ALIGN_SIZE
#define	ALIGN_SIZE	sizeof(double)
#endif

typedef union hdr
{
  char align[ALIGN_SIZE];	 
  struct
    {
      union hdr *next;		 
      char *deep;		 
    } h;
} header;

static header *last_alloca_header = NULL;	 

 

pointer
alloca (size)
     size_t size;
{
  auto char probe;		 
  register char *depth = ADDRESS_FUNCTION (probe);

#if STACK_DIRECTION == 0
  if (STACK_DIR == 0)		 
    find_stack_direction ();
#endif

   

  {
    register header *hp;	 

    for (hp = last_alloca_header; hp != NULL;)
      if ((STACK_DIR > 0 && hp->h.deep > depth)
	  || (STACK_DIR < 0 && hp->h.deep < depth))
	{
	  register header *np = hp->h.next;

	  free ((pointer) hp);	 

	  hp = np;		 
	}
      else
	break;			 

    last_alloca_header = hp;	 
  }

  if (size == 0)
    return NULL;		 

   

  {
    register pointer new = malloc (sizeof (header) + size);
     

    ((header *) new)->h.next = last_alloca_header;
    ((header *) new)->h.deep = depth;

    last_alloca_header = (header *) new;

     

    return (pointer) ((char *) new + sizeof (header));
  }
}

#if defined (CRAY) && defined (CRAY_STACKSEG_END)

#ifdef DEBUG_I00AFUNC
#include <stdio.h>
#endif

#ifndef CRAY_STACK
#define CRAY_STACK
#ifndef CRAY2
 
struct stack_control_header
  {
    long shgrow:32;		 
    long shaseg:32;		 
    long shhwm:32;		 
    long shsize:32;		 
  };

 

struct stack_segment_linkage
  {
    long ss[0200];		 
    long sssize:32;		 
    long ssbase:32;		 
    long:32;
    long sspseg:32;		 
    long:32;
    long sstcpt:32;		 
    long sscsnm;		 
    long ssusr1;		 
    long ssusr2;		 
    long sstpid;		 
    long ssgvup;		 
    long sscray[7];		 
    long ssa0;
    long ssa1;
    long ssa2;
    long ssa3;
    long ssa4;
    long ssa5;
    long ssa6;
    long ssa7;
    long sss0;
    long sss1;
    long sss2;
    long sss3;
    long sss4;
    long sss5;
    long sss6;
    long sss7;
  };

#else  
 
struct stk_stat
  {
    long now;			 
    long maxc;			 
    long high_water;		 
    long overflows;		 
    long hits;			 
    long extends;		 
    long stko_mallocs;		 
    long underflows;		 
    long stko_free;		 
    long stkm_free;		 
    long segments;		 
    long maxs;			 
    long pad_size;		 
    long current_address;	 
    long current_size;		 
    long initial_address;	 
    long initial_size;		 
  };

 

struct stk_trailer
  {
    long this_address;		 
    long this_size;		 
    long unknown2;
    long unknown3;
    long link;			 
    long unknown5;
    long unknown6;
    long unknown7;
    long unknown8;
    long unknown9;
    long unknown10;
    long unknown11;
    long unknown12;
    long unknown13;
    long unknown14;
  };

#endif  
#endif  

#ifdef CRAY2
 

static long
i00afunc (long *address)
{
  struct stk_stat status;
  struct stk_trailer *trailer;
  long *block, size;
  long result = 0;

   

  STKSTAT (&status);

   

  trailer = (struct stk_trailer *) (status.current_address
				    + status.current_size
				    - 15);

   

  if (trailer == 0)
    abort ();

   

  while (trailer != 0)
    {
      block = (long *) trailer->this_address;
      size = trailer->this_size;
      if (block == 0 || size == 0)
	abort ();
      trailer = (struct stk_trailer *) trailer->link;
      if ((block <= address) && (address < (block + size)))
	break;
    }

   

  result = address - block;

  if (trailer == 0)
    {
      return result;
    }

  do
    {
      if (trailer->this_size <= 0)
	abort ();
      result += trailer->this_size;
      trailer = (struct stk_trailer *) trailer->link;
    }
  while (trailer != 0);

   

  return (result);
}

#else  
 

static long
i00afunc (long address)
{
  long stkl = 0;

  long size, pseg, this_segment, stack;
  long result = 0;

  struct stack_segment_linkage *ssptr;

   

   
  stkl = CRAY_STACKSEG_END ();
  ssptr = (struct stack_segment_linkage *) stkl;

   

  pseg = ssptr->sspseg;
  size = ssptr->sssize;

  this_segment = stkl - size;

   

  while (!(this_segment <= address && address <= stkl))
    {
#ifdef DEBUG_I00AFUNC
      fprintf (stderr, "%011o %011o %011o\n", this_segment, address, stkl);
#endif
      if (pseg == 0)
	break;
      stkl = stkl - pseg;
      ssptr = (struct stack_segment_linkage *) stkl;
      size = ssptr->sssize;
      pseg = ssptr->sspseg;
      this_segment = stkl - size;
    }

  result = address - this_segment;

   

  while (pseg != 0)
    {
#ifdef DEBUG_I00AFUNC
      fprintf (stderr, "%011o %011o\n", pseg, size);
#endif
      stkl = stkl - pseg;
      ssptr = (struct stack_segment_linkage *) stkl;
      size = ssptr->sssize;
      pseg = ssptr->sspseg;
      result += size;
    }
  return (result);
}

#endif  
#endif  

#endif  
#endif  
