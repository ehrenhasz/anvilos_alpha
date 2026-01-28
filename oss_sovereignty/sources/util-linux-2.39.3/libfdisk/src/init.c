
#include "fdiskP.h"




UL_DEBUG_DEFINE_MASK(libfdisk);
UL_DEBUG_DEFINE_MASKNAMES(libfdisk) =
{
	{ "all",	LIBFDISK_DEBUG_ALL,	"info about all subsystems" },
	{ "ask",	LIBFDISK_DEBUG_ASK,	"fdisk dialogs" },
	{ "help",	LIBFDISK_DEBUG_HELP,	"this help" },
	{ "cxt",	LIBFDISK_DEBUG_CXT,	"library context (handler)" },
	{ "label",	LIBFDISK_DEBUG_LABEL,	"disk label utils" },
	{ "part",	LIBFDISK_DEBUG_PART,	"partition utils" },
	{ "parttype",	LIBFDISK_DEBUG_PARTTYPE,"partition type utils" },
	{ "script",	LIBFDISK_DEBUG_SCRIPT,	"sfdisk-like scripts" },
	{ "tab",	LIBFDISK_DEBUG_TAB,	"table utils"},
	{ "wipe",       LIBFDISK_DEBUG_WIPE,    "wipe area utils" },
	{ "item",       LIBFDISK_DEBUG_ITEM,    "disklabel items" },
	{ "gpt",        LIBFDISK_DEBUG_GPT,     "GPT subsystems" },
	{ NULL, 0 }
};


void fdisk_init_debug(int mask)
{
	if (libfdisk_debug_mask)
		return;

	__UL_INIT_DEBUG_FROM_ENV(libfdisk, LIBFDISK_DEBUG_, mask, LIBFDISK_DEBUG);


	if (libfdisk_debug_mask != LIBFDISK_DEBUG_INIT
	    && libfdisk_debug_mask != (LIBFDISK_DEBUG_HELP|LIBFDISK_DEBUG_INIT)) {
		const char *ver = NULL;

		fdisk_get_library_version(&ver);

		DBG(INIT, ul_debug("library debug mask: 0x%04x", libfdisk_debug_mask));
		DBG(INIT, ul_debug("library version: %s", ver));
	}

	ON_DBG(HELP, ul_debug_print_masks("LIBFDISK_DEBUG",
				UL_DEBUG_MASKNAMES(libfdisk)));
}
