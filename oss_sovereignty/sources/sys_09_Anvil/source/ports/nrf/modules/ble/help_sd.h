

#ifndef HELP_SD_H__
#define HELP_SD_H__

#include "bluetooth_conf.h"

#if MICROPY_PY_BLE

#define HELP_TEXT_SD \
"If compiled with SD=<softdevice> the additional commands are\n" \
"available:\n" \
"  ble.enable()    -- enable bluetooth stack\n" \
"  ble.disable()   -- disable bluetooth stack\n" \
"  ble.enabled()   -- check whether bluetooth stack is enabled\n" \
"  ble.address()   -- return device address as text string\n" \
"\n"

#else
#define HELP_TEXT_SD
#endif 

#endif
