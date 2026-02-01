 

 

#include <config.h>

#include <alloca.h>

#include <string.h>
#include <stdlib.h>

 
#if !(defined __GNUC__ || defined __clang__)

 
# ifndef alloca

 

#  ifndef STACK_DIRECTION
#   define STACK_DIRECTION      0        
#  endif

#  if STACK_DIRECTION != 0

#   define STACK_DIR    STACK_DIRECTION  

#  else  

static int stack_dir;            
#   define STACK_DIR    stack_dir

static int
find_stack_direction (int *addr, int depth)
{
  int dir, dummy = 0;
  if (! addr)
    addr = &dummy;
  *addr = addr < &dummy ? 1 : addr == &dummy ? 0 : -1;
  dir = depth ? find_stack_direction (addr, depth - 1) : 0;
  return dir + dummy;
}

#  endif  

 

#  ifndef       ALIGN_SIZE
#   define ALIGN_SIZE   sizeof(double)
#  endif

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

 

void *
alloca (size_t size)
{
  auto char probe;               
  register char *depth = &probe;

#  if STACK_DIRECTION == 0
  if (STACK_DIR == 0)            
    STACK_DIR = find_stack_direction (NULL, (size & 1) + 20);
#  endif

   

  {
    register header *hp;         

    for (hp = last_alloca_header; hp != NULL;)
      if ((STACK_DIR > 0 && hp->h.deep > depth)
          || (STACK_DIR < 0 && hp->h.deep < depth))
        {
          register header *np = hp->h.next;

          free (hp);             

          hp = np;               
        }
      else
        break;                   

    last_alloca_header = hp;     
  }

  if (size == 0)
    return NULL;                 

   

  {
     
    register header *new;

    size_t combined_size = sizeof (header) + size;
    if (combined_size < sizeof (header))
      memory_full ();

    new = malloc (combined_size);

    if (! new)
      memory_full ();

    new->h.next = last_alloca_header;
    new->h.deep = depth;

    last_alloca_header = new;

     

    return (void *) (new + 1);
  }
}

# endif  
#endif  
