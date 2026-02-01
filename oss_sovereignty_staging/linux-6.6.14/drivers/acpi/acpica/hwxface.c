
 

#define EXPORT_ACPI_INTERFACES

#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_HARDWARE
ACPI_MODULE_NAME("hwxface")

 
acpi_status acpi_reset(void)
{
	struct acpi_generic_address *reset_reg;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_reset);

	reset_reg = &acpi_gbl_FADT.reset_register;

	 

	if (!(acpi_gbl_FADT.flags & ACPI_FADT_RESET_REGISTER) ||
	    !reset_reg->address) {
		return_ACPI_STATUS(AE_NOT_EXIST);
	}

	if (reset_reg->space_id == ACPI_ADR_SPACE_SYSTEM_IO) {
		 
		status = acpi_os_write_port((acpi_io_address)reset_reg->address,
					    acpi_gbl_FADT.reset_value,
					    ACPI_RESET_REGISTER_WIDTH);
	} else {
		 

		status = acpi_hw_write(acpi_gbl_FADT.reset_value, reset_reg);
	}

	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_reset)

 
acpi_status acpi_read(u64 *return_value, struct acpi_generic_address *reg)
{
	acpi_status status;

	ACPI_FUNCTION_NAME(acpi_read);

	status = acpi_hw_read(return_value, reg);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_read)

 
acpi_status acpi_write(u64 value, struct acpi_generic_address *reg)
{
	acpi_status status;

	ACPI_FUNCTION_NAME(acpi_write);

	status = acpi_hw_write(value, reg);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_write)

#if (!ACPI_REDUCED_HARDWARE)
 
acpi_status acpi_read_bit_register(u32 register_id, u32 *return_value)
{
	struct acpi_bit_register_info *bit_reg_info;
	u32 register_value;
	u32 value;
	acpi_status status;

	ACPI_FUNCTION_TRACE_U32(acpi_read_bit_register, register_id);

	 

	bit_reg_info = acpi_hw_get_bit_register_info(register_id);
	if (!bit_reg_info) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	status = acpi_hw_register_read(bit_reg_info->parent_register,
				       &register_value);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	 

	value = ((register_value & bit_reg_info->access_bit_mask)
		 >> bit_reg_info->bit_position);

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "BitReg %X, ParentReg %X, Actual %8.8X, ReturnValue %8.8X\n",
			  register_id, bit_reg_info->parent_register,
			  register_value, value));

	*return_value = value;
	return_ACPI_STATUS(AE_OK);
}

ACPI_EXPORT_SYMBOL(acpi_read_bit_register)

 
acpi_status acpi_write_bit_register(u32 register_id, u32 value)
{
	struct acpi_bit_register_info *bit_reg_info;
	acpi_cpu_flags lock_flags;
	u32 register_value;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_U32(acpi_write_bit_register, register_id);

	 

	bit_reg_info = acpi_hw_get_bit_register_info(register_id);
	if (!bit_reg_info) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	lock_flags = acpi_os_acquire_raw_lock(acpi_gbl_hardware_lock);

	 
	if (bit_reg_info->parent_register != ACPI_REGISTER_PM1_STATUS) {
		 
		status = acpi_hw_register_read(bit_reg_info->parent_register,
					       &register_value);
		if (ACPI_FAILURE(status)) {
			goto unlock_and_exit;
		}

		 
		ACPI_REGISTER_INSERT_VALUE(register_value,
					   bit_reg_info->bit_position,
					   bit_reg_info->access_bit_mask,
					   value);

		status = acpi_hw_register_write(bit_reg_info->parent_register,
						register_value);
	} else {
		 
		register_value = ACPI_REGISTER_PREPARE_BITS(value,
							    bit_reg_info->
							    bit_position,
							    bit_reg_info->
							    access_bit_mask);

		 

		if (register_value) {
			status =
			    acpi_hw_register_write(ACPI_REGISTER_PM1_STATUS,
						   register_value);
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_IO,
			  "BitReg %X, ParentReg %X, Value %8.8X, Actual %8.8X\n",
			  register_id, bit_reg_info->parent_register, value,
			  register_value));

unlock_and_exit:

	acpi_os_release_raw_lock(acpi_gbl_hardware_lock, lock_flags);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_write_bit_register)
#endif				 
 
acpi_status
acpi_get_sleep_type_data(u8 sleep_state, u8 *sleep_type_a, u8 *sleep_type_b)
{
	acpi_status status;
	struct acpi_evaluate_info *info;
	union acpi_operand_object **elements;

	ACPI_FUNCTION_TRACE(acpi_get_sleep_type_data);

	 

	if ((sleep_state > ACPI_S_STATES_MAX) || !sleep_type_a || !sleep_type_b) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
	if (!info) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	 
	info->relative_pathname = acpi_gbl_sleep_state_names[sleep_state];

	status = acpi_ns_evaluate(info);
	if (ACPI_FAILURE(status)) {
		if (status == AE_NOT_FOUND) {

			 

			goto final_cleanup;
		}

		goto warning_cleanup;
	}

	 

	if (!info->return_object) {
		ACPI_ERROR((AE_INFO, "No Sleep State object returned from [%s]",
			    info->relative_pathname));
		status = AE_AML_NO_RETURN_VALUE;
		goto warning_cleanup;
	}

	 

	if (info->return_object->common.type != ACPI_TYPE_PACKAGE) {
		ACPI_ERROR((AE_INFO,
			    "Sleep State return object is not a Package"));
		status = AE_AML_OPERAND_TYPE;
		goto return_value_cleanup;
	}

	 
	elements = info->return_object->package.elements;
	switch (info->return_object->package.count) {
	case 0:

		status = AE_AML_PACKAGE_LIMIT;
		break;

	case 1:

		if (elements[0]->common.type != ACPI_TYPE_INTEGER) {
			status = AE_AML_OPERAND_TYPE;
			break;
		}

		 

		*sleep_type_a = (u8)elements[0]->integer.value;
		*sleep_type_b = (u8)(elements[0]->integer.value >> 8);
		break;

	case 2:
	default:

		if ((elements[0]->common.type != ACPI_TYPE_INTEGER) ||
		    (elements[1]->common.type != ACPI_TYPE_INTEGER)) {
			status = AE_AML_OPERAND_TYPE;
			break;
		}

		 

		*sleep_type_a = (u8)elements[0]->integer.value;
		*sleep_type_b = (u8)elements[1]->integer.value;
		break;
	}

return_value_cleanup:
	acpi_ut_remove_reference(info->return_object);

warning_cleanup:
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status,
				"While evaluating Sleep State [%s]",
				info->relative_pathname));
	}

final_cleanup:
	ACPI_FREE(info);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_sleep_type_data)
