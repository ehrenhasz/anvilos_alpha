 

#ifndef _SPL_WAIT_H
#define	_SPL_WAIT_H

#include <linux/sched.h>
#include <linux/wait.h>

#ifndef HAVE_WAIT_ON_BIT_ACTION
#define	spl_wait_on_bit(word, bit, mode)	wait_on_bit(word, bit, mode)
#else

static inline int
spl_bit_wait(void *word)
{
	schedule();
	return (0);
}

#define	spl_wait_on_bit(word, bit, mode)		\
	wait_on_bit(word, bit, spl_bit_wait, mode)

#endif  

#ifdef HAVE_WAIT_QUEUE_ENTRY_T
typedef wait_queue_head_t	spl_wait_queue_head_t;
typedef wait_queue_entry_t	spl_wait_queue_entry_t;
#else
typedef wait_queue_head_t	spl_wait_queue_head_t;
typedef wait_queue_t		spl_wait_queue_entry_t;
#endif

#endif  
