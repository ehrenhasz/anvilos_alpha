 

#include <config.h>

#include "ignore-value.h"

#include <stdio.h>

#include "attribute.h"

struct s { int i; };
NODISCARD static char doChar (void);
NODISCARD static int doInt (void);
NODISCARD static off_t doOff (void);
NODISCARD static void *doPtr (void);
NODISCARD static struct s doStruct (void);

static char
doChar (void)
{
  return 0;
}

static int
doInt (void)
{
  return 0;
}

static off_t
doOff (void)
{
  return 0;
}

static void *
doPtr (void)
{
  return NULL;
}

static struct s
doStruct (void)
{
  static struct s s1;
  return s1;
}

int
main (void)
{
   
  ignore_value (doChar ());
  ignore_value (doInt ());
  ignore_value (doOff ());
  ignore_value (doPtr ());
  ignore_value (doStruct ());
  return 0;
}
