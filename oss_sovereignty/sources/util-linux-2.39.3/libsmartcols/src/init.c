



#include <stdarg.h>

#include "smartcolsP.h"

UL_DEBUG_DEFINE_MASK(libsmartcols);
UL_DEBUG_DEFINE_MASKNAMES(libsmartcols) =
{
	{ "all", SCOLS_DEBUG_ALL,	"info about all subsystems" },
	{ "buff", SCOLS_DEBUG_BUFF,	"output buffer utils" },
	{ "cell", SCOLS_DEBUG_CELL,	"table cell utils" },
	{ "col", SCOLS_DEBUG_COL,	"cols utils" },
	{ "help", SCOLS_DEBUG_HELP,	"this help" },
	{ "group", SCOLS_DEBUG_GROUP,	"lines grouping utils" },
	{ "line", SCOLS_DEBUG_LINE,	"table line utils" },
	{ "tab", SCOLS_DEBUG_TAB,	"table utils" },
	{ NULL, 0, NULL }
};


void scols_init_debug(int mask)
{
	if (libsmartcols_debug_mask)
		return;

	__UL_INIT_DEBUG_FROM_ENV(libsmartcols, SCOLS_DEBUG_, mask, LIBSMARTCOLS_DEBUG);

	if (libsmartcols_debug_mask != SCOLS_DEBUG_INIT
	    && libsmartcols_debug_mask != (SCOLS_DEBUG_HELP|SCOLS_DEBUG_INIT)) {
		const char *ver = NULL;

		scols_get_library_version(&ver);

		DBG(INIT, ul_debug("library debug mask: 0x%04x", libsmartcols_debug_mask));
		DBG(INIT, ul_debug("library version: %s", ver));
	}
	ON_DBG(HELP, ul_debug_print_masks("LIBSMARTCOLS_DEBUG",
				UL_DEBUG_MASKNAMES(libsmartcols)));
}
