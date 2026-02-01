 

#include <config.h>

#include <scratch_buffer.h>

#include <string.h>
#include "macros.h"

static int
byte_at (unsigned long long int i)
{
  return ((i % 13) + ((i * i) % 251)) & 0xff;
}

int
main ()
{
   
  {
    size_t sizes[] = { 100, 1000, 10000, 100000 };
    size_t s;
    for (s = 0; s < SIZEOF (sizes); s++)
      {
        size_t size = sizes[s];
        struct scratch_buffer buf;
        bool ok;
        size_t i;

        scratch_buffer_init (&buf);

        ok = scratch_buffer_set_array_size (&buf, size, 1);
        ASSERT (ok);

        for (i = 0; i < size; i++)
          ((unsigned char *) buf.data)[i] = byte_at (i);

        memset (buf.data, 'x', buf.length);
        memset (buf.data, 'y', size);

        scratch_buffer_free (&buf);
      }
  }

   
  {
    size_t sizes[] = { 100, 1000, 10000, 100000 };
    size_t s;
    for (s = 0; s < SIZEOF (sizes); s++)
      {
        size_t size = sizes[s];
        struct scratch_buffer buf;
        bool ok;
        size_t i;

        scratch_buffer_init (&buf);

        while (buf.length < size)
          {
            ok = scratch_buffer_grow (&buf);
            ASSERT (ok);
          }

        for (i = 0; i < size; i++)
          ((unsigned char *) buf.data)[i] = byte_at (i);

        memset (buf.data, 'x', buf.length);
        memset (buf.data, 'y', size);

        scratch_buffer_free (&buf);
      }
  }

   
  {
    size_t sizes[] = { 100, 1000, 10000, 100000 };
    struct scratch_buffer buf;
    size_t s;
    size_t size;
    bool ok;
    size_t i;

    scratch_buffer_init (&buf);

    s = 0;
    size = sizes[s];
    ok = scratch_buffer_set_array_size (&buf, size, 1);
    ASSERT (ok);

    for (i = 0; i < size; i++)
      ((unsigned char *) buf.data)[i] = byte_at (i);

    for (; s < SIZEOF (sizes); s++)
      {
        size_t oldsize = size;
        size = sizes[s];

        while (buf.length < size)
          {
            ok = scratch_buffer_grow_preserve (&buf);
            ASSERT (ok);
          }

        for (i = 0; i < oldsize; i++)
          ASSERT(((unsigned char *) buf.data)[i] == byte_at (i));
        for (i = oldsize; i < size; i++)
          ((unsigned char *) buf.data)[i] = byte_at (i);
      }

    scratch_buffer_free (&buf);
  }

  return 0;
}
