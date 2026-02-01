 

#include <config.h>

 
#include "windows-tls.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "windows-once.h"

void *
glwthread_tls_get (glwthread_tls_key_t key)
{
  return TlsGetValue (key);
}

int
glwthread_tls_set (glwthread_tls_key_t key, void *value)
{
  if (!TlsSetValue (key, value))
    return EINVAL;
  return 0;
}

 

static glwthread_once_t dtor_table_init_once = GLWTHREAD_ONCE_INIT;

static CRITICAL_SECTION dtor_table_lock;

struct dtor { glwthread_tls_key_t key; void (*destructor) (void *); };

 
static struct dtor *dtor_table;
 
static unsigned int dtors_count;
 
static unsigned int dtors_used;
 
static unsigned int dtors_allocated;
 

 
static unsigned int dtor_processing_threads;

static void
dtor_table_initialize (void)
{
  InitializeCriticalSection (&dtor_table_lock);
   
}

static void
dtor_table_ensure_initialized (void)
{
  glwthread_once (&dtor_table_init_once, dtor_table_initialize);
}

 
static void
dtor_table_shrink_used (void)
{
  unsigned int i = 0;
  unsigned int j = dtors_used;

  for (;;)
    {
      BOOL i_found = FALSE;
      BOOL j_found = FALSE;
       
      for (; i < dtors_count;)
        {
          if (dtor_table[i].destructor == NULL)
            {
              i_found = TRUE;
              break;
            }
          i++;
        }

       
      for (; j > dtors_count;)
        {
          j--;
          if (dtor_table[j].destructor != NULL)
            {
              j_found = TRUE;
              break;
            }
        }

      if (i_found != j_found)
         
        abort ();

      if (!i_found)
        break;

       
      dtor_table[i] = dtor_table[j];

      i++;
    }

  dtors_used = dtors_count;
}

void
glwthread_tls_process_destructors (void)
{
  unsigned int repeat;

  dtor_table_ensure_initialized ();

  EnterCriticalSection (&dtor_table_lock);
  if (dtor_processing_threads == 0)
    {
       
      if (dtors_used > dtors_count)
        dtor_table_shrink_used ();
    }
  dtor_processing_threads++;

  for (repeat = GLWTHREAD_DESTRUCTOR_ITERATIONS; repeat > 0; repeat--)
    {
      unsigned int destructors_run = 0;

       
      unsigned int i_limit = dtors_used;
      unsigned int i;

      for (i = 0; i < i_limit; i++)
        {
          struct dtor current = dtor_table[i];
          if (current.destructor != NULL)
            {
               
              void *current_value = glwthread_tls_get (current.key);
              if (current_value != NULL)
                {
                   
                  glwthread_tls_set (current.key, NULL);
                  LeaveCriticalSection (&dtor_table_lock);
                  current.destructor (current_value);
                  EnterCriticalSection (&dtor_table_lock);
                  destructors_run++;
                }
            }
        }

       
      if (destructors_run == 0)
        break;
    }

  dtor_processing_threads--;
  LeaveCriticalSection (&dtor_table_lock);
}

int
glwthread_tls_key_create (glwthread_tls_key_t *keyp, void (*destructor) (void *))
{
  if (destructor != NULL)
    {
      dtor_table_ensure_initialized ();

      EnterCriticalSection (&dtor_table_lock);
      if (dtor_processing_threads == 0)
        {
           
          if (dtors_used > dtors_count)
            dtor_table_shrink_used ();
        }

      while (dtors_used == dtors_allocated)
        {
           
          unsigned int new_allocated = 2 * dtors_allocated + 1;
          if (new_allocated < 7)
            new_allocated = 7;
          if (new_allocated <= dtors_allocated)  
            new_allocated = UINT_MAX;

          LeaveCriticalSection (&dtor_table_lock);
          {
            struct dtor *new_table =
              (struct dtor *) malloc (new_allocated * sizeof (struct dtor));
            if (new_table == NULL)
              return ENOMEM;
            EnterCriticalSection (&dtor_table_lock);
             
            if (dtors_used < new_allocated)
              {
                if (dtors_allocated < new_allocated)
                  {
                     
                    memcpy (new_table, dtor_table,
                            dtors_used * sizeof (struct dtor));
                    dtor_table = new_table;
                    dtors_allocated = new_allocated;
                  }
                else
                  {
                     
                    free (new_table);
                  }
                break;
              }
             
            free (new_table);
          }
        }
       
      {
         
        glwthread_tls_key_t key = TlsAlloc ();
        if (key == (DWORD)-1)
          {
            LeaveCriticalSection (&dtor_table_lock);
            return EAGAIN;
          }
         
        dtor_table[dtors_used].key = key;
        dtor_table[dtors_used].destructor = destructor;
        dtors_used++;
        dtors_count++;
        LeaveCriticalSection (&dtor_table_lock);
        *keyp = key;
      }
    }
  else
    {
       
      glwthread_tls_key_t key = TlsAlloc ();
      if (key == (DWORD)-1)
        return EAGAIN;
      *keyp = key;
    }
  return 0;
}

int
glwthread_tls_key_delete (glwthread_tls_key_t key)
{
   
  dtor_table_ensure_initialized ();

  EnterCriticalSection (&dtor_table_lock);
  if (dtor_processing_threads == 0)
    {
       
      if (dtors_used > dtors_count)
        dtor_table_shrink_used ();
       

       
      {
        unsigned int i_limit = dtors_used;
        unsigned int i;

        for (i = 0; i < i_limit; i++)
          if (dtor_table[i].key == key)
            {
              if (i < dtors_used - 1)
                 
                dtor_table[i] = dtor_table[dtors_used - 1];
              dtors_count = dtors_used = dtors_used - 1;
              break;
            }
      }
    }
  else
    {
       
      unsigned int i_limit = dtors_used;
      unsigned int i;

      for (i = 0; i < i_limit; i++)
        if (dtor_table[i].destructor != NULL  
            && dtor_table[i].key == key)
          {
             
            dtor_table[i].destructor = NULL;
            dtors_count = dtors_count - 1;
            break;
          }
    }
  LeaveCriticalSection (&dtor_table_lock);
   

  if (!TlsFree (key))
    return EINVAL;
  return 0;
}
