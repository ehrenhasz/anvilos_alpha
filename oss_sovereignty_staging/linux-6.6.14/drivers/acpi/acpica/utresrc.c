
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acresrc.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utresrc")

 
const u8 acpi_gbl_resource_aml_sizes[] = {
	 

	0,
	0,
	0,
	0,
	ACPI_AML_SIZE_SMALL(struct aml_resource_irq),
	ACPI_AML_SIZE_SMALL(struct aml_resource_dma),
	ACPI_AML_SIZE_SMALL(struct aml_resource_start_dependent),
	ACPI_AML_SIZE_SMALL(struct aml_resource_end_dependent),
	ACPI_AML_SIZE_SMALL(struct aml_resource_io),
	ACPI_AML_SIZE_SMALL(struct aml_resource_fixed_io),
	ACPI_AML_SIZE_SMALL(struct aml_resource_fixed_dma),
	0,
	0,
	0,
	ACPI_AML_SIZE_SMALL(struct aml_resource_vendor_small),
	ACPI_AML_SIZE_SMALL(struct aml_resource_end_tag),

	 

	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_memory24),
	ACPI_AML_SIZE_LARGE(struct aml_resource_generic_register),
	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_vendor_large),
	ACPI_AML_SIZE_LARGE(struct aml_resource_memory32),
	ACPI_AML_SIZE_LARGE(struct aml_resource_fixed_memory32),
	ACPI_AML_SIZE_LARGE(struct aml_resource_address32),
	ACPI_AML_SIZE_LARGE(struct aml_resource_address16),
	ACPI_AML_SIZE_LARGE(struct aml_resource_extended_irq),
	ACPI_AML_SIZE_LARGE(struct aml_resource_address64),
	ACPI_AML_SIZE_LARGE(struct aml_resource_extended_address64),
	ACPI_AML_SIZE_LARGE(struct aml_resource_gpio),
	ACPI_AML_SIZE_LARGE(struct aml_resource_pin_function),
	ACPI_AML_SIZE_LARGE(struct aml_resource_common_serialbus),
	ACPI_AML_SIZE_LARGE(struct aml_resource_pin_config),
	ACPI_AML_SIZE_LARGE(struct aml_resource_pin_group),
	ACPI_AML_SIZE_LARGE(struct aml_resource_pin_group_function),
	ACPI_AML_SIZE_LARGE(struct aml_resource_pin_group_config),
	ACPI_AML_SIZE_LARGE(struct aml_resource_clock_input),

};

const u8 acpi_gbl_resource_aml_serial_bus_sizes[] = {
	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_i2c_serialbus),
	ACPI_AML_SIZE_LARGE(struct aml_resource_spi_serialbus),
	ACPI_AML_SIZE_LARGE(struct aml_resource_uart_serialbus),
	ACPI_AML_SIZE_LARGE(struct aml_resource_csi2_serialbus),
};

 
static const u8 acpi_gbl_resource_types[] = {
	 

	0,
	0,
	0,
	0,
	ACPI_SMALL_VARIABLE_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_SMALL_VARIABLE_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	0,
	0,
	0,
	ACPI_VARIABLE_LENGTH,	 
	ACPI_FIXED_LENGTH,	 

	 

	0,
	ACPI_FIXED_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	0,
	ACPI_VARIABLE_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_FIXED_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
	ACPI_VARIABLE_LENGTH,	 
};

 

acpi_status
acpi_ut_walk_aml_resources(struct acpi_walk_state *walk_state,
			   u8 *aml,
			   acpi_size aml_length,
			   acpi_walk_aml_callback user_function, void **context)
{
	acpi_status status;
	u8 *end_aml;
	u8 resource_index;
	u32 length;
	u32 offset = 0;
	u8 end_tag[2] = { 0x79, 0x00 };

	ACPI_FUNCTION_TRACE(ut_walk_aml_resources);

	 

	if (aml_length < sizeof(struct aml_resource_end_tag)) {
		return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
	}

	 

	end_aml = aml + aml_length;

	 

	while (aml < end_aml) {

		 

		status =
		    acpi_ut_validate_resource(walk_state, aml, &resource_index);
		if (ACPI_FAILURE(status)) {
			 
			return_ACPI_STATUS(status);
		}

		 

		length = acpi_ut_get_descriptor_length(aml);

		 

		if (user_function) {
			status =
			    user_function(aml, length, offset, resource_index,
					  context);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}

		 

		if (acpi_ut_get_resource_type(aml) ==
		    ACPI_RESOURCE_NAME_END_TAG) {
			 
			if ((aml + 1) >= end_aml) {
				return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
			}

			 

			 

			if (!user_function) {
				*context = aml;
			}

			 

			return_ACPI_STATUS(AE_OK);
		}

		aml += length;
		offset += length;
	}

	 

	if (user_function) {

		 

		(void)acpi_ut_validate_resource(walk_state, end_tag,
						&resource_index);
		status =
		    user_function(end_tag, 2, offset, resource_index, context);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
}

 

acpi_status
acpi_ut_validate_resource(struct acpi_walk_state *walk_state,
			  void *aml, u8 *return_index)
{
	union aml_resource *aml_resource;
	u8 resource_type;
	u8 resource_index;
	acpi_rs_length resource_length;
	acpi_rs_length minimum_resource_length;

	ACPI_FUNCTION_ENTRY();

	 
	resource_type = ACPI_GET8(aml);

	 
	if (resource_type & ACPI_RESOURCE_NAME_LARGE) {

		 

		if (resource_type > ACPI_RESOURCE_NAME_LARGE_MAX) {
			goto invalid_resource;
		}

		 
		resource_index = (u8) (resource_type - 0x70);
	} else {
		 
		resource_index = (u8)
		    ((resource_type & ACPI_RESOURCE_NAME_SMALL_MASK) >> 3);
	}

	 
	if (!acpi_gbl_resource_types[resource_index]) {
		goto invalid_resource;
	}

	 
	resource_length = acpi_ut_get_resource_length(aml);
	minimum_resource_length = acpi_gbl_resource_aml_sizes[resource_index];

	 

	switch (acpi_gbl_resource_types[resource_index]) {
	case ACPI_FIXED_LENGTH:

		 

		if (resource_length != minimum_resource_length) {
			goto bad_resource_length;
		}
		break;

	case ACPI_VARIABLE_LENGTH:

		 

		if (resource_length < minimum_resource_length) {
			goto bad_resource_length;
		}
		break;

	case ACPI_SMALL_VARIABLE_LENGTH:

		 

		if ((resource_length > minimum_resource_length) ||
		    (resource_length < (minimum_resource_length - 1))) {
			goto bad_resource_length;
		}
		break;

	default:

		 

		goto invalid_resource;
	}

	aml_resource = ACPI_CAST_PTR(union aml_resource, aml);
	if (resource_type == ACPI_RESOURCE_NAME_SERIAL_BUS) {

		 

		struct aml_resource_common_serialbus common_serial_bus;
		memcpy(&common_serial_bus, aml_resource,
		       sizeof(common_serial_bus));

		 

		if ((common_serial_bus.type == 0) ||
		    (common_serial_bus.type > AML_RESOURCE_MAX_SERIALBUSTYPE)) {
			if (walk_state) {
				ACPI_ERROR((AE_INFO,
					    "Invalid/unsupported SerialBus resource descriptor: BusType 0x%2.2X",
					    common_serial_bus.type));
			}
			return (AE_AML_INVALID_RESOURCE_TYPE);
		}
	}

	 

	if (return_index) {
		*return_index = resource_index;
	}

	return (AE_OK);

invalid_resource:

	if (walk_state) {
		ACPI_ERROR((AE_INFO,
			    "Invalid/unsupported resource descriptor: Type 0x%2.2X",
			    resource_type));
	}
	return (AE_AML_INVALID_RESOURCE_TYPE);

bad_resource_length:

	if (walk_state) {
		ACPI_ERROR((AE_INFO,
			    "Invalid resource descriptor length: Type "
			    "0x%2.2X, Length 0x%4.4X, MinLength 0x%4.4X",
			    resource_type, resource_length,
			    minimum_resource_length));
	}
	return (AE_AML_BAD_RESOURCE_LENGTH);
}

 

u8 acpi_ut_get_resource_type(void *aml)
{
	ACPI_FUNCTION_ENTRY();

	 
	if (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_LARGE) {

		 

		return (ACPI_GET8(aml));
	} else {
		 

		return ((u8) (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_SMALL_MASK));
	}
}

 

u16 acpi_ut_get_resource_length(void *aml)
{
	acpi_rs_length resource_length;

	ACPI_FUNCTION_ENTRY();

	 
	if (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_LARGE) {

		 

		ACPI_MOVE_16_TO_16(&resource_length, ACPI_ADD_PTR(u8, aml, 1));

	} else {
		 

		resource_length = (u16) (ACPI_GET8(aml) &
					 ACPI_RESOURCE_NAME_SMALL_LENGTH_MASK);
	}

	return (resource_length);
}

 

u8 acpi_ut_get_resource_header_length(void *aml)
{
	ACPI_FUNCTION_ENTRY();

	 

	if (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_LARGE) {
		return (sizeof(struct aml_resource_large_header));
	} else {
		return (sizeof(struct aml_resource_small_header));
	}
}

 

u32 acpi_ut_get_descriptor_length(void *aml)
{
	ACPI_FUNCTION_ENTRY();

	 
	return (acpi_ut_get_resource_length(aml) +
		acpi_ut_get_resource_header_length(aml));
}

 

acpi_status
acpi_ut_get_resource_end_tag(union acpi_operand_object *obj_desc, u8 **end_tag)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_get_resource_end_tag);

	 

	if (!obj_desc->buffer.length) {
		*end_tag = obj_desc->buffer.pointer;
		return_ACPI_STATUS(AE_OK);
	}

	 

	status = acpi_ut_walk_aml_resources(NULL, obj_desc->buffer.pointer,
					    obj_desc->buffer.length, NULL,
					    (void **)end_tag);

	return_ACPI_STATUS(status);
}
