 

 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bashjmp.h"
typedef struct _shtimer
{
  struct timeval tmout;

  int fd;
  int flags;

  int alrmflag;					 

  SigHandler *alrm_handler;
  SigHandler *old_handler;

  procenv_t jmpenv;

  int (*tm_handler) (struct _shtimer *);	 
  PTR_T	*data;					 
} sh_timer;

#define SHTIMER_ALARM	0x01			 
#define SHTIMER_SELECT	0x02
#define SHTIMER_LONGJMP	0x04

#define SHTIMER_SIGSET	0x100
#define SHTIMER_ALRMSET	0x200

extern sh_timer *shtimer_alloc (void);
extern void shtimer_flush (sh_timer *);
extern void shtimer_dispose (sh_timer *);

extern void shtimer_set (sh_timer *, time_t, long);
extern void shtimer_unset (sh_timer *);

extern void shtimer_cleanup (sh_timer *);
extern void shtimer_clear (sh_timer *);

extern int shtimer_chktimeout (sh_timer *);

extern int shtimer_select (sh_timer *);
extern int shtimer_alrm (sh_timer *);
