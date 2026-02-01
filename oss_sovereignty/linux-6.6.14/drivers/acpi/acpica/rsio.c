
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acresrc.h"

#define _COMPONENT          ACPI_RESOURCES
ACPI_MODULE_NAME("rsio")

 
struct acpi_rsconvert_info acpi_rs_convert_io[5] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_IO,
	 ACPI_RS_SIZE(struct acpi_resource_io),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_io)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_IO,
	 sizeof(struct aml_resource_io),
	 0},

	 

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.io.io_decode),
	 AML_OFFSET(io.flags),
	 0},
	 
	{ACPI_RSC_MOVE8, ACPI_RS_OFFSET(data.io.alignment),
	 AML_OFFSET(io.alignment),
	 2},

	{ACPI_RSC_MOVE16, ACPI_RS_OFFSET(data.io.minimum),
	 AML_OFFSET(io.minimum),
	 2}
};

 

struct acpi_rsconvert_info acpi_rs_convert_fixed_io[4] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_FIXED_IO,
	 ACPI_RS_SIZE(struct acpi_resource_fixed_io),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_fixed_io)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_FIXED_IO,
	 sizeof(struct aml_resource_fixed_io),
	 0},
	 
	{ACPI_RSC_MOVE8, ACPI_RS_OFFSET(data.fixed_io.address_length),
	 AML_OFFSET(fixed_io.address_length),
	 1},

	{ACPI_RSC_MOVE16, ACPI_RS_OFFSET(data.fixed_io.address),
	 AML_OFFSET(fixed_io.address),
	 1}
};

 

struct acpi_rsconvert_info acpi_rs_convert_generic_reg[4] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_GENERIC_REGISTER,
	 ACPI_RS_SIZE(struct acpi_resource_generic_register),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_generic_reg)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_GENERIC_REGISTER,
	 sizeof(struct aml_resource_generic_register),
	 0},
	 
	{ACPI_RSC_MOVE8, ACPI_RS_OFFSET(data.generic_reg.space_id),
	 AML_OFFSET(generic_reg.address_space_id),
	 4},

	 

	{ACPI_RSC_MOVE64, ACPI_RS_OFFSET(data.generic_reg.address),
	 AML_OFFSET(generic_reg.address),
	 1}
};

 

struct acpi_rsconvert_info acpi_rs_convert_end_dpf[2] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_END_DEPENDENT,
	 ACPI_RS_SIZE_MIN,
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_end_dpf)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_END_DEPENDENT,
	 sizeof(struct aml_resource_end_dependent),
	 0}
};

 

struct acpi_rsconvert_info acpi_rs_convert_end_tag[2] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_END_TAG,
	 ACPI_RS_SIZE_MIN,
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_end_tag)},

	 
	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_END_TAG,
	 sizeof(struct aml_resource_end_tag),
	 0}
};

 

struct acpi_rsconvert_info acpi_rs_get_start_dpf[6] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_START_DEPENDENT,
	 ACPI_RS_SIZE(struct acpi_resource_start_dependent),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_get_start_dpf)},

	 

	{ACPI_RSC_SET8, ACPI_RS_OFFSET(data.start_dpf.compatibility_priority),
	 ACPI_ACCEPTABLE_CONFIGURATION,
	 2},

	 

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.start_dpf.descriptor_length),
	 AML_OFFSET(start_dpf.descriptor_type),
	 0},

	 

	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_AML_LENGTH, 0, 1},

	 

	{ACPI_RSC_2BITFLAG,
	 ACPI_RS_OFFSET(data.start_dpf.compatibility_priority),
	 AML_OFFSET(start_dpf.flags),
	 0},

	{ACPI_RSC_2BITFLAG,
	 ACPI_RS_OFFSET(data.start_dpf.performance_robustness),
	 AML_OFFSET(start_dpf.flags),
	 2}
};

 

struct acpi_rsconvert_info acpi_rs_set_start_dpf[10] = {
	 

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_START_DEPENDENT,
	 sizeof(struct aml_resource_start_dependent),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_set_start_dpf)},

	 

	{ACPI_RSC_2BITFLAG,
	 ACPI_RS_OFFSET(data.start_dpf.compatibility_priority),
	 AML_OFFSET(start_dpf.flags),
	 0},

	{ACPI_RSC_2BITFLAG,
	 ACPI_RS_OFFSET(data.start_dpf.performance_robustness),
	 AML_OFFSET(start_dpf.flags),
	 2},
	 
	{ACPI_RSC_EXIT_EQ, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.start_dpf.descriptor_length),
	 1},

	 

	{ACPI_RSC_LENGTH, 0, 0,
	 sizeof(struct aml_resource_start_dependent_noprio)},

	 
	{ACPI_RSC_EXIT_EQ, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.start_dpf.descriptor_length),
	 0},

	 

	{ACPI_RSC_LENGTH, 0, 0, sizeof(struct aml_resource_start_dependent)},

	 
	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.start_dpf.compatibility_priority),
	 ACPI_ACCEPTABLE_CONFIGURATION},

	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.start_dpf.performance_robustness),
	 ACPI_ACCEPTABLE_CONFIGURATION},

	 

	{ACPI_RSC_LENGTH, 0, 0,
	 sizeof(struct aml_resource_start_dependent_noprio)}
};
