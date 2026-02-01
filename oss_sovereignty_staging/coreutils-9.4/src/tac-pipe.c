 
#include "assure.h"

 
#define BUFFER_SIZE (8)

#define LEN(X, I) ((X)->p[(I)].one_past_end - (X)->p[(I)].start)
#define EMPTY(X) ((X)->n_bufs == 1 && LEN (X, 0) == 0)

#define ONE_PAST_END(X, I) ((X)->p[(I)].one_past_end)

struct Line_ptr
{
  size_t i;
  char *ptr;
};
typedef struct Line_ptr Line_ptr;

struct B_pair
{
  char *start;
  char *one_past_end;
};

struct Buf
{
  size_t n_bufs;
  struct obstack obs;
  struct B_pair *p;
};
typedef struct Buf Buf;

static bool
buf_init_from_stdin (Buf *x, char eol_byte)
{
  bool last_byte_is_eol_byte = true;
  bool ok = true;

#define OBS (&(x->obs))
  obstack_init (OBS);

  while (true)
    {
      char *buf = (char *) malloc (BUFFER_SIZE);
      size_t bytes_read;

      if (buf == nullptr)
        {
           
           
          ok = false;
          break;
        }
      bytes_read = full_read (STDIN_FILENO, buf, BUFFER_SIZE);
      if (bytes_read != buffer_size && errno != 0)
        error (EXIT_FAILURE, errno, _("read error"));

      {
        struct B_pair bp;
        bp.start = buf;
        bp.one_past_end = buf + bytes_read;
        obstack_grow (OBS, &bp, sizeof (bp));
      }

      if (bytes_read != 0)
        last_byte_is_eol_byte = (buf[bytes_read - 1] == eol_byte);

      if (bytes_read < BUFFER_SIZE)
        break;
    }

  if (ok)
    {
       
      if (!last_byte_is_eol_byte)
        {
          char *buf = malloc (1);
          if (buf == nullptr)
            {
               
              ok = false;
            }
          else
            {
              struct B_pair bp;
              *buf = eol_byte;
              bp.start = buf;
              bp.one_past_end = buf + 1;
              obstack_grow (OBS, &bp, sizeof (bp));
            }
        }
    }

  x->n_bufs = obstack_object_size (OBS) / sizeof (x->p[0]);
  x->p = (struct B_pair *) obstack_finish (OBS);

   
  if (x->n_bufs >= 2
      && x->p[x->n_bufs - 1].start == x->p[x->n_bufs - 1].one_past_end)
    free (x->p[--(x->n_bufs)].start);

  return ok;
}

static void
buf_free (Buf *x)
{
  for (size_t i = 0; i < x->n_bufs; i++)
    free (x->p[i].start);
  obstack_free (OBS, nullptr);
}

Line_ptr
line_ptr_decrement (const Buf *x, const Line_ptr *lp)
{
  Line_ptr lp_new;

  if (lp->ptr > x->p[lp->i].start)
    {
      lp_new.i = lp->i;
      lp_new.ptr = lp->ptr - 1;
    }
  else
    {
      affirm (lp->i > 0);
      lp_new.i = lp->i - 1;
      lp_new.ptr = ONE_PAST_END (x, lp->i - 1) - 1;
    }
  return lp_new;
}

Line_ptr
line_ptr_increment (const Buf *x, const Line_ptr *lp)
{
  Line_ptr lp_new;

  affirm (lp->ptr <= ONE_PAST_END (x, lp->i) - 1);
  if (lp->ptr < ONE_PAST_END (x, lp->i) - 1)
    {
      lp_new.i = lp->i;
      lp_new.ptr = lp->ptr + 1;
    }
  else
    {
      affirm (lp->i < x->n_bufs - 1);
      lp_new.i = lp->i + 1;
      lp_new.ptr = x->p[lp->i + 1].start;
    }
  return lp_new;
}

static bool
find_bol (const Buf *x,
          const Line_ptr *last_bol, Line_ptr *new_bol, char eol_byte)
{
  size_t i;
  Line_ptr tmp;
  char *last_bol_ptr;

  if (last_bol->ptr == x->p[0].start)
    return false;

  tmp = line_ptr_decrement (x, last_bol);
  last_bol_ptr = tmp.ptr;
  i = tmp.i;
  while (true)
    {
      char *nl = memrchr (x->p[i].start, last_bol_ptr, eol_byte);
      if (nl)
        {
          Line_ptr nl_pos;
          nl_pos.i = i;
          nl_pos.ptr = nl;
          *new_bol = line_ptr_increment (x, &nl_pos);
          return true;
        }

      if (i == 0)
        break;

      --i;
      last_bol_ptr = ONE_PAST_END (x, i);
    }

   
  if (last_bol->ptr != x->p[0].start)
    {
      new_bol->i = 0;
      new_bol->ptr = x->p[0].start;
      return true;
    }

  return false;
}

static void
print_line (FILE *out_stream, const Buf *x,
            const Line_ptr *bol, const Line_ptr *bol_next)
{
  for (size_t i = bol->i; i <= bol_next->i; i++)
    {
      char *a = (i == bol->i ? bol->ptr : x->p[i].start);
      char *b = (i == bol_next->i ? bol_next->ptr : ONE_PAST_END (x, i));
      fwrite (a, 1, b - a, out_stream);
    }
}

static bool
tac_mem ()
{
  Buf x;
  Line_ptr bol;
  char eol_byte = '\n';

  if (! buf_init_from_stdin (&x, eol_byte))
    {
      buf_free (&x);
      return false;
    }

   
  if (EMPTY (&x))
    return true;

   
  bol.i = x.n_bufs - 1;
  bol.ptr = ONE_PAST_END (&x, bol.i);

  while (true)
    {
      Line_ptr new_bol;
      if (! find_bol (&x, &bol, &new_bol, eol_byte))
        break;
      print_line (stdout, &x, &new_bol, &bol);
      bol = new_bol;
    }
  return true;
}
