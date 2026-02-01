 

#include <config.h>

 
#include "windows-once.h"

#include <stdlib.h>

void
glwthread_once (glwthread_once_t *once_control, void (*initfunction) (void))
{
  if (once_control->inited <= 0)
    {
      if (InterlockedIncrement (&once_control->started) == 0)
        {
           
          InitializeCriticalSection (&once_control->lock);
          EnterCriticalSection (&once_control->lock);
          once_control->inited = 0;
          initfunction ();
          once_control->inited = 1;
          LeaveCriticalSection (&once_control->lock);
        }
      else
        {
           
          InterlockedDecrement (&once_control->started);
           
          while (once_control->inited < 0)
            Sleep (0);
          if (once_control->inited <= 0)
            {
               
              EnterCriticalSection (&once_control->lock);
              LeaveCriticalSection (&once_control->lock);
              if (!(once_control->inited > 0))
                abort ();
            }
        }
    }
}
