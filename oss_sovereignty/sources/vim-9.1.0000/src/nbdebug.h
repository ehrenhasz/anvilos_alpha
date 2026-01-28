


#ifndef NBDEBUG_H
#define NBDEBUG_H

#ifdef NBDEBUG

# ifndef ASSERT
#  define ASSERT(c) \
    if (!(c)) \
    { \
	fprintf(stderr, "Assertion failed: line %d, file %s\n", \
		__LINE__, __FILE__); \
	fflush(stderr); \
	abort(); \
    }
# endif

# define nbdebug(a) nbdbg a

# define NB_TRACE		0x00000001
# define NB_TRACE_VERBOSE	0x00000002
# define NB_TRACE_COLONCMD	0x00000004
# define NB_PRINT		0x00000008
# define NB_DEBUG_ALL		0xffffffff

# define NBDLEVEL(flags)	(nb_debug != NULL && (nb_dlevel & (flags)))

# define NBDEBUG_TRACE	1

typedef enum {
		WT_ENV = 1,		
		WT_WAIT,		
		WT_STOP			
} WtWait;


void		 nbdbg(char *, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

void nbdebug_wait(u_int wait_flags, char *wait_var, u_int wait_secs);
void nbdebug_log_init(char *log_var, char *level_var);

extern FILE	*nb_debug;
extern u_int	 nb_dlevel;		

#else		

# ifndef ASSERT
#  define ASSERT(c)
# endif



void
nbdbg(
	char		*fmt,
	...)
{
}

#endif 
#endif 
