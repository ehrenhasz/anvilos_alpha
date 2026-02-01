 

#include "dm_services.h"
#include "include/logger_interface.h"

#include "bios_parser_interface.h"
#include "bios_parser.h"

#include "bios_parser2.h"


struct dc_bios *dal_bios_parser_create(
	struct bp_init_data *init,
	enum dce_version dce_version)
{
	struct dc_bios *bios = NULL;

	bios = firmware_parser_create(init, dce_version);

	 
	if (bios == NULL)
		bios = bios_parser_create(init, dce_version);

	return bios;
}

void dal_bios_parser_destroy(struct dc_bios **dcb)
{
	struct dc_bios *bios = *dcb;

	bios->funcs->bios_parser_destroy(dcb);
}

