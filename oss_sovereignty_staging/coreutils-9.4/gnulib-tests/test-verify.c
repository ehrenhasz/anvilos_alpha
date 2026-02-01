 

#include <config.h>

#include "verify.h"

#ifndef EXP_FAIL
# define EXP_FAIL 0
#endif

 

int gx;
enum { A, B, C };

#if EXP_FAIL == 1
verify (gx >= 0);                  
#endif
verify (C == 2);                   
#if EXP_FAIL == 2
verify (1 + 1 == 3);               
#endif
verify (1 == 1); verify (1 == 1);  

enum
{
  item = verify_expr (1 == 1, 10 * 0 + 17)  
};

static int
function (int n)
{
#if EXP_FAIL == 3
  verify (n >= 0);                   
#endif
  verify (C == 2);                   
#if EXP_FAIL == 4
  verify (1 + 1 == 3);               
#endif
  verify (1 == 1); verify (1 == 1);  

  if (n)
    return ((void) verify_expr (1 == 1, 1), verify_expr (1 == 1, 8));  
#if EXP_FAIL == 5
  return verify_expr (1 == 2, 5);  
#endif
  return 0;
}

 

static int
f (int a)
{
  return a;
}

typedef struct { unsigned int context : 4; unsigned int halt : 1; } state;

void test_assume_expressions (state *s);
int test_assume_optimization (int x);
_Noreturn void test_assume_noreturn (void);

void
test_assume_expressions (state *s)
{
   
  assume (f (1));
   
  assume (s->halt);
}

int
test_assume_optimization (int x)
{
   
  assume (x >= 4);
  return (x > 1 ? x + 3 : 2 * x + 10);
}

_Noreturn void
test_assume_noreturn (void)
{
   
  assume (0);
}

 
int
main (void)
{
  state s = { 0, 1 };
  test_assume_expressions (&s);
  test_assume_optimization (5);
  return !(function (0) == 0 && function (1) == 8);
}
