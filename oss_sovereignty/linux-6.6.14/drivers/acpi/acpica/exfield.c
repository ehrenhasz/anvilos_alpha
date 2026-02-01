
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exfield")

 
#define ACPI_INVALID_PROTOCOL_ID        0x80
#define ACPI_MAX_PROTOCOL_ID            0x0F
static const u8 acpi_protocol_lengths[] = {
	ACPI_INVALID_PROTOCOL_ID,	 
	ACPI_INVALID_PROTOCOL_ID,	 
	0x00,			 
	ACPI_INVALID_PROTOCOL_ID,	 
	0x01,			 
	ACPI_INVALID_PROTOCOL_ID,	 
	0x01,			 
	ACPI_INVALID_PROTOCOL_ID,	 
	0x02,			 
	ACPI_INVALID_PROTOCOL_ID,	 
	0xFF,			 
	0xFF,			 
	0x02,			 
	0xFF,			 
	0xFF,			 
	0xFF			 
};

#define PCC_MASTER_SUBSPACE     3

 
#define GENERIC_SUBSPACE_COMMAND(a)     (4 == a || a == 5)
#define MASTER_SUBSPACE_COMMAND(a)      (12 <= a && a <= 15)

 

acpi_status
acpi_ex_get_protocol_buffer_length(u32 protocol_id, u32 *return_length)
{

	if ((protocol_id > ACPI_MAX_PROTOCOL_ID) ||
	    (acpi_protocol_lengths[protocol_id] == ACPI_INVALID_PROTOCOL_ID)) {
		ACPI_ERROR((AE_INFO,
			    "Invalid Field/AccessAs protocol ID: 0x%4.4X",
			    protocol_id));

		return (AE_AML_PROTOCOL);
	}

	*return_length = acpi_protocol_lengths[protocol_id];
	return (AE_OK);
}

 

acpi_status
acpi_ex_read_data_from_field(struct acpi_walk_state *walk_state,
			     union acpi_operand_object *obj_desc,
			     union acpi_operand_object **ret_buffer_desc)
{
	acpi_status status;
	union acpi_operand_object *buffer_desc;
	void *buffer;
	u32 buffer_length;

	ACPI_FUNCTION_TRACE_PTR(ex_read_data_from_field, obj_desc);

	 

	if (!obj_desc) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}
	if (!ret_buffer_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (obj_desc->common.type == ACPI_TYPE_BUFFER_FIELD) {
		 
		if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
			status = acpi_ds_get_buffer_field_arguments(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}
	} else if ((obj_desc->common.type == ACPI_TYPE_LOCAL_REGION_FIELD) &&
		   (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_SMBUS
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_GSBUS
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_IPMI
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_PLATFORM_RT
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_FIXED_HARDWARE)) {

		 

		status = acpi_ex_read_serial_bus(obj_desc, ret_buffer_desc);
		return_ACPI_STATUS(status);
	}

	 
	buffer_length =
	    (acpi_size)ACPI_ROUND_BITS_UP_TO_BYTES(obj_desc->field.bit_length);

	if (buffer_length > acpi_gbl_integer_byte_width ||
	    (obj_desc->common.type == ACPI_TYPE_BUFFER_FIELD &&
	     obj_desc->buffer_field.is_create_field)) {

		 

		buffer_desc = acpi_ut_create_buffer_object(buffer_length);
		if (!buffer_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}
		buffer = buffer_desc->buffer.pointer;
	} else {
		 

		buffer_desc = acpi_ut_create_integer_object((u64) 0);
		if (!buffer_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		buffer_length = acpi_gbl_integer_byte_width;
		buffer = &buffer_desc->integer.value;
	}

	if ((obj_desc->common.type == ACPI_TYPE_LOCAL_REGION_FIELD) &&
	    (obj_desc->field.region_obj->region.space_id ==
	     ACPI_ADR_SPACE_GPIO)) {

		 

		status = acpi_ex_read_gpio(obj_desc, buffer);
		goto exit;
	} else if ((obj_desc->common.type == ACPI_TYPE_LOCAL_REGION_FIELD) &&
		   (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_PLATFORM_COMM)) {
		 
		ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
				  "PCC FieldRead bits %u\n",
				  obj_desc->field.bit_length));

		memcpy(buffer,
		       obj_desc->field.region_obj->field.internal_pcc_buffer +
		       obj_desc->field.base_byte_offset,
		       (acpi_size)ACPI_ROUND_BITS_UP_TO_BYTES(obj_desc->field.
							      bit_length));

		*ret_buffer_desc = buffer_desc;
		return AE_OK;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldRead [TO]:   Obj %p, Type %X, Buf %p, ByteLen %X\n",
			  obj_desc, obj_desc->common.type, buffer,
			  buffer_length));
	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldRead [FROM]: BitLen %X, BitOff %X, ByteOff %X\n",
			  obj_desc->common_field.bit_length,
			  obj_desc->common_field.start_field_bit_offset,
			  obj_desc->common_field.base_byte_offset));

	 

	acpi_ex_acquire_global_lock(obj_desc->common_field.field_flags);

	 

	status = acpi_ex_extract_from_field(obj_desc, buffer, buffer_length);
	acpi_ex_release_global_lock(obj_desc->common_field.field_flags);

exit:
	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(buffer_desc);
	} else {
		*ret_buffer_desc = buffer_desc;
	}

	return_ACPI_STATUS(status);
}

 

acpi_status
acpi_ex_write_data_to_field(union acpi_operand_object *source_desc,
			    union acpi_operand_object *obj_desc,
			    union acpi_operand_object **result_desc)
{
	acpi_status status;
	u32 buffer_length;
	u32 data_length;
	void *buffer;

	ACPI_FUNCTION_TRACE_PTR(ex_write_data_to_field, obj_desc);

	 

	if (!source_desc || !obj_desc) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	if (obj_desc->common.type == ACPI_TYPE_BUFFER_FIELD) {
		 
		if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
			status = acpi_ds_get_buffer_field_arguments(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}
	} else if ((obj_desc->common.type == ACPI_TYPE_LOCAL_REGION_FIELD) &&
		   (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_GPIO)) {

		 

		status = acpi_ex_write_gpio(source_desc, obj_desc, result_desc);
		return_ACPI_STATUS(status);
	} else if ((obj_desc->common.type == ACPI_TYPE_LOCAL_REGION_FIELD) &&
		   (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_SMBUS
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_GSBUS
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_IPMI
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_PLATFORM_RT
		    || obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_FIXED_HARDWARE)) {

		 

		status =
		    acpi_ex_write_serial_bus(source_desc, obj_desc,
					     result_desc);
		return_ACPI_STATUS(status);
	} else if ((obj_desc->common.type == ACPI_TYPE_LOCAL_REGION_FIELD) &&
		   (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_PLATFORM_COMM)) {
		 
		data_length =
		    (acpi_size)ACPI_ROUND_BITS_UP_TO_BYTES(obj_desc->field.
							   bit_length);
		memcpy(obj_desc->field.region_obj->field.internal_pcc_buffer +
		       obj_desc->field.base_byte_offset,
		       source_desc->buffer.pointer, data_length);

		if (MASTER_SUBSPACE_COMMAND(obj_desc->field.base_byte_offset)) {

			 

			ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
					  "PCC COMD field has been written. Invoking PCC handler now.\n"));

			status =
			    acpi_ex_access_region(obj_desc, 0,
						  (u64 *)obj_desc->field.
						  region_obj->field.
						  internal_pcc_buffer,
						  ACPI_WRITE);
			return_ACPI_STATUS(status);
		}
		return (AE_OK);
	}

	 

	switch (source_desc->common.type) {
	case ACPI_TYPE_INTEGER:

		buffer = &source_desc->integer.value;
		buffer_length = sizeof(source_desc->integer.value);
		break;

	case ACPI_TYPE_BUFFER:

		buffer = source_desc->buffer.pointer;
		buffer_length = source_desc->buffer.length;
		break;

	case ACPI_TYPE_STRING:

		buffer = source_desc->string.pointer;
		buffer_length = source_desc->string.length;
		break;

	default:
		return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldWrite [FROM]: Obj %p (%s:%X), Buf %p, ByteLen %X\n",
			  source_desc,
			  acpi_ut_get_type_name(source_desc->common.type),
			  source_desc->common.type, buffer, buffer_length));

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldWrite [TO]:   Obj %p (%s:%X), BitLen %X, BitOff %X, ByteOff %X\n",
			  obj_desc,
			  acpi_ut_get_type_name(obj_desc->common.type),
			  obj_desc->common.type,
			  obj_desc->common_field.bit_length,
			  obj_desc->common_field.start_field_bit_offset,
			  obj_desc->common_field.base_byte_offset));

	 

	acpi_ex_acquire_global_lock(obj_desc->common_field.field_flags);

	 

	status = acpi_ex_insert_into_field(obj_desc, buffer, buffer_length);
	acpi_ex_release_global_lock(obj_desc->common_field.field_flags);
	return_ACPI_STATUS(status);
}
