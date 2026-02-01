
 

#include <acpi/acpi.h>
#include "accommon.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utstrsuppt")

 
static acpi_status
acpi_ut_insert_digit(u64 *accumulated_value, u32 base, int ascii_digit);

static acpi_status
acpi_ut_strtoul_multiply64(u64 multiplicand, u32 base, u64 *out_product);

static acpi_status acpi_ut_strtoul_add64(u64 addend1, u32 digit, u64 *out_sum);

 

acpi_status acpi_ut_convert_octal_string(char *string, u64 *return_value_ptr)
{
	u64 accumulated_value = 0;
	acpi_status status = AE_OK;

	 

	while (*string) {
		 
		if (!(ACPI_IS_OCTAL_DIGIT(*string))) {
#ifdef ACPI_ASL_COMPILER
			status = AE_BAD_OCTAL_CONSTANT;
#endif
			break;
		}

		 

		status = acpi_ut_insert_digit(&accumulated_value, 8, *string);
		if (ACPI_FAILURE(status)) {
			status = AE_OCTAL_OVERFLOW;
			break;
		}

		string++;
	}

	 

	*return_value_ptr = accumulated_value;
	return (status);
}

 

acpi_status acpi_ut_convert_decimal_string(char *string, u64 *return_value_ptr)
{
	u64 accumulated_value = 0;
	acpi_status status = AE_OK;

	 

	while (*string) {
		 
		if (!isdigit((int)*string)) {
#ifdef ACPI_ASL_COMPILER
			status = AE_BAD_DECIMAL_CONSTANT;
#endif
			break;
		}

		 

		status = acpi_ut_insert_digit(&accumulated_value, 10, *string);
		if (ACPI_FAILURE(status)) {
			status = AE_DECIMAL_OVERFLOW;
			break;
		}

		string++;
	}

	 

	*return_value_ptr = accumulated_value;
	return (status);
}

 

acpi_status acpi_ut_convert_hex_string(char *string, u64 *return_value_ptr)
{
	u64 accumulated_value = 0;
	acpi_status status = AE_OK;

	 

	while (*string) {
		 
		if (!isxdigit((int)*string)) {
#ifdef ACPI_ASL_COMPILER
			status = AE_BAD_HEX_CONSTANT;
#endif
			break;
		}

		 

		status = acpi_ut_insert_digit(&accumulated_value, 16, *string);
		if (ACPI_FAILURE(status)) {
			status = AE_HEX_OVERFLOW;
			break;
		}

		string++;
	}

	 

	*return_value_ptr = accumulated_value;
	return (status);
}

 

char acpi_ut_remove_leading_zeros(char **string)
{

	while (**string == ACPI_ASCII_ZERO) {
		*string += 1;
	}

	return (**string);
}

 

char acpi_ut_remove_whitespace(char **string)
{

	while (isspace((u8)**string)) {
		*string += 1;
	}

	return (**string);
}

 

u8 acpi_ut_detect_hex_prefix(char **string)
{
	char *initial_position = *string;

	acpi_ut_remove_hex_prefix(string);
	if (*string != initial_position) {
		return (TRUE);	 
	}

	return (FALSE);		 
}

 

void acpi_ut_remove_hex_prefix(char **string)
{
	if ((**string == ACPI_ASCII_ZERO) &&
	    (tolower((int)*(*string + 1)) == 'x')) {
		*string += 2;	 
	}
}

 

u8 acpi_ut_detect_octal_prefix(char **string)
{

	if (**string == ACPI_ASCII_ZERO) {
		*string += 1;	 
		return (TRUE);
	}

	return (FALSE);		 
}

 

static acpi_status
acpi_ut_insert_digit(u64 *accumulated_value, u32 base, int ascii_digit)
{
	acpi_status status;
	u64 product;

	 

	status = acpi_ut_strtoul_multiply64(*accumulated_value, base, &product);
	if (ACPI_FAILURE(status)) {
		return (status);
	}

	 

	status =
	    acpi_ut_strtoul_add64(product,
				  acpi_ut_ascii_char_to_hex(ascii_digit),
				  accumulated_value);

	return (status);
}

 

static acpi_status
acpi_ut_strtoul_multiply64(u64 multiplicand, u32 base, u64 *out_product)
{
	u64 product;
	u64 quotient;

	 

	*out_product = 0;
	if (!multiplicand || !base) {
		return (AE_OK);
	}

	 
	acpi_ut_short_divide(ACPI_UINT64_MAX, base, &quotient, NULL);
	if (multiplicand > quotient) {
		return (AE_NUMERIC_OVERFLOW);
	}

	product = multiplicand * base;

	 

	if ((acpi_gbl_integer_bit_width == 32) && (product > ACPI_UINT32_MAX)) {
		return (AE_NUMERIC_OVERFLOW);
	}

	*out_product = product;
	return (AE_OK);
}

 

static acpi_status acpi_ut_strtoul_add64(u64 addend1, u32 digit, u64 *out_sum)
{
	u64 sum;

	 

	if ((addend1 > 0) && (digit > (ACPI_UINT64_MAX - addend1))) {
		return (AE_NUMERIC_OVERFLOW);
	}

	sum = addend1 + digit;

	 

	if ((acpi_gbl_integer_bit_width == 32) && (sum > ACPI_UINT32_MAX)) {
		return (AE_NUMERIC_OVERFLOW);
	}

	*out_sum = sum;
	return (AE_OK);
}
