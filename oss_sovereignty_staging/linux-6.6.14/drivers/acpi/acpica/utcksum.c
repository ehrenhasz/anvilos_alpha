
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acutils.h"

 

#define _COMPONENT          ACPI_CA_DISASSEMBLER
ACPI_MODULE_NAME("utcksum")

 
acpi_status acpi_ut_verify_checksum(struct acpi_table_header *table, u32 length)
{
	u8 checksum;

	 
	if (ACPI_COMPARE_NAMESEG(table->signature, ACPI_SIG_S3PT) ||
	    ACPI_COMPARE_NAMESEG(table->signature, ACPI_SIG_FACS)) {
		return (AE_OK);
	}

	 

	length = table->length;
	checksum =
	    acpi_ut_generate_checksum(ACPI_CAST_PTR(u8, table), length,
				      table->checksum);

	 

	if (checksum != table->checksum) {
		ACPI_BIOS_WARNING((AE_INFO,
				   "Incorrect checksum in table [%4.4s] - 0x%2.2X, "
				   "should be 0x%2.2X",
				   table->signature, table->checksum,
				   table->checksum - checksum));

#if (ACPI_CHECKSUM_ABORT)
		return (AE_BAD_CHECKSUM);
#endif
	}

	return (AE_OK);
}

 

acpi_status
acpi_ut_verify_cdat_checksum(struct acpi_table_cdat *cdat_table, u32 length)
{
	u8 checksum;

	 

	checksum = acpi_ut_generate_checksum(ACPI_CAST_PTR(u8, cdat_table),
					     cdat_table->length,
					     cdat_table->checksum);

	 

	if (checksum != cdat_table->checksum) {
		ACPI_BIOS_WARNING((AE_INFO,
				   "Incorrect checksum in table [%4.4s] - 0x%2.2X, "
				   "should be 0x%2.2X",
				   acpi_gbl_CDAT, cdat_table->checksum,
				   checksum));

#if (ACPI_CHECKSUM_ABORT)
		return (AE_BAD_CHECKSUM);
#endif
	}

	cdat_table->checksum = checksum;
	return (AE_OK);
}

 

u8 acpi_ut_generate_checksum(void *table, u32 length, u8 original_checksum)
{
	u8 checksum;

	 

	checksum = acpi_ut_checksum((u8 *)table, length);

	 

	checksum = (u8)(checksum - original_checksum);

	 

	checksum = (u8)(0 - checksum);
	return (checksum);
}

 

u8 acpi_ut_checksum(u8 *buffer, u32 length)
{
	u8 sum = 0;
	u8 *end = buffer + length;

	while (buffer < end) {
		sum = (u8)(sum + *(buffer++));
	}

	return (sum);
}
