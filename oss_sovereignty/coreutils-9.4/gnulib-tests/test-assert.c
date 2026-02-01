 

#include <config.h>

#define STATIC_ASSERT_TESTS \
  static_assert (2 + 2 == 4, "arithmetic does not work"); \
  static_assert (2 + 2 == 4); \
  static_assert (sizeof (char) == 1, "sizeof does not work"); \
  static_assert (sizeof (char) == 1)

STATIC_ASSERT_TESTS;

static char const *
assert (char const *p, int i)
{
  return p + i;
}

static char const *
f (char const *p)
{
  return assert (p, 0);
}

#include <assert.h>

STATIC_ASSERT_TESTS;

static int
g (void)
{
  assert (f ("this should work"));
  return 0;
}

#define NDEBUG 1
#include <assert.h>

STATIC_ASSERT_TESTS;

static int
h (void)
{
  assert (f ("this should work"));
  return 0;
}

int
main (void)
{
  STATIC_ASSERT_TESTS;
  f ("");
  g ();
  h ();
  return 0;
}
