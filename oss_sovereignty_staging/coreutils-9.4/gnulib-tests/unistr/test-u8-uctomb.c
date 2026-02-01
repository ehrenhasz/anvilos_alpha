 

#include <config.h>

#include "unistr.h"

#include "macros.h"

#define MAGIC 0xBA

int
main ()
{
   
  {
    ucs4_t uc;

    for (uc = 0; uc < 0x80; uc++)
      {
        uint8_t buf[5] = { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
        int ret;

        ret = u8_uctomb (buf, uc, 0);
        ASSERT (ret == -2);
        ASSERT (buf[0] == MAGIC);

        ret = u8_uctomb (buf, uc, 1);
        ASSERT (ret == 1);
        ASSERT (buf[0] == uc);
        ASSERT (buf[1] == MAGIC);
      }
  }

   
  {
    ucs4_t uc = 0x00D7;
    uint8_t buf[5] = { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
    int ret;

    ret = u8_uctomb (buf, uc, 0);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);

    ret = u8_uctomb (buf, uc, 1);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);

    ret = u8_uctomb (buf, uc, 2);
    ASSERT (ret == 2);
    ASSERT (buf[0] == 0xC3);
    ASSERT (buf[1] == 0x97);
    ASSERT (buf[2] == MAGIC);
  }

   
  {
    ucs4_t uc = 0x20AC;
    uint8_t buf[5] = { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
    int ret;

    ret = u8_uctomb (buf, uc, 0);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);

    ret = u8_uctomb (buf, uc, 1);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);

    ret = u8_uctomb (buf, uc, 2);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);
    ASSERT (buf[1] == MAGIC);

    ret = u8_uctomb (buf, uc, 3);
    ASSERT (ret == 3);
    ASSERT (buf[0] == 0xE2);
    ASSERT (buf[1] == 0x82);
    ASSERT (buf[2] == 0xAC);
    ASSERT (buf[3] == MAGIC);
  }

   
  {
    ucs4_t uc = 0x10FFFD;
    uint8_t buf[5] = { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
    int ret;

    ret = u8_uctomb (buf, uc, 0);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);

    ret = u8_uctomb (buf, uc, 1);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);

    ret = u8_uctomb (buf, uc, 2);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);
    ASSERT (buf[1] == MAGIC);

    ret = u8_uctomb (buf, uc, 3);
    ASSERT (ret == -2);
    ASSERT (buf[0] == MAGIC);
    ASSERT (buf[1] == MAGIC);
    ASSERT (buf[2] == MAGIC);

    ret = u8_uctomb (buf, uc, 4);
    ASSERT (ret == 4);
    ASSERT (buf[0] == 0xF4);
    ASSERT (buf[1] == 0x8F);
    ASSERT (buf[2] == 0xBF);
    ASSERT (buf[3] == 0xBD);
    ASSERT (buf[4] == MAGIC);
  }

   
  {
    ucs4_t invalid[] = { 0x110000, 0xD800, 0xDBFF, 0xDC00, 0xDFFF };
    uint8_t buf[5] = { MAGIC, MAGIC, MAGIC, MAGIC, MAGIC };
    size_t i;

    for (i = 0; i < SIZEOF (invalid); i++)
      {
        ucs4_t uc = invalid[i];
        int n;

        for (n = 0; n <= 4; n++)
          {
            int ret = u8_uctomb (buf, uc, n);
            ASSERT (ret == -1);
            ASSERT (buf[0] == MAGIC);
            ASSERT (buf[1] == MAGIC);
            ASSERT (buf[2] == MAGIC);
            ASSERT (buf[3] == MAGIC);
            ASSERT (buf[4] == MAGIC);
          }
      }
  }

  return 0;
}
