
 

#include <acpi/acpi.h>
#include "accommon.h"

#define _COMPONENT          ACPI_COMPILER
ACPI_MODULE_NAME("utuuid")

#if (defined ACPI_ASL_COMPILER || defined ACPI_EXEC_APP || defined ACPI_HELP_APP)
 
const u8 acpi_gbl_map_to_uuid_offset[UUID_BUFFER_LENGTH] = {
	6, 4, 2, 0, 11, 9, 16, 14, 19, 21, 24, 26, 28, 30, 32, 34
};

 

void acpi_ut_convert_string_to_uuid(char *in_string, u8 *uuid_buffer)
{
	u32 i;

	for (i = 0; i < UUID_BUFFER_LENGTH; i++) {
		uuid_buffer[i] =
		    (acpi_ut_ascii_char_to_hex
		     (in_string[acpi_gbl_map_to_uuid_offset[i]]) << 4);

		uuid_buffer[i] |=
		    acpi_ut_ascii_char_to_hex(in_string
					      [acpi_gbl_map_to_uuid_offset[i] +
					       1]);
	}
}

 

acpi_status acpi_ut_convert_uuid_to_string(char *uuid_buffer, char *out_string)
{
	u32 i;

	if (!uuid_buffer || !out_string) {
		return (AE_BAD_PARAMETER);
	}

	for (i = 0; i < UUID_BUFFER_LENGTH; i++) {
		out_string[acpi_gbl_map_to_uuid_offset[i]] =
		    acpi_ut_hex_to_ascii_char(uuid_buffer[i], 4);

		out_string[acpi_gbl_map_to_uuid_offset[i] + 1] =
		    acpi_ut_hex_to_ascii_char(uuid_buffer[i], 0);
	}

	 

	out_string[UUID_HYPHEN1_OFFSET] =
	    out_string[UUID_HYPHEN2_OFFSET] =
	    out_string[UUID_HYPHEN3_OFFSET] =
	    out_string[UUID_HYPHEN4_OFFSET] = '-';

	out_string[UUID_STRING_LENGTH] = 0;	 
	return (AE_OK);
}
#endif
