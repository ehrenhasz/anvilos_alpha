
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utdecode")

 
const u8 acpi_gbl_ns_properties[ACPI_NUM_NS_TYPES] = {
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NEWSCOPE,	 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NEWSCOPE,	 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NEWSCOPE,	 
	ACPI_NS_NEWSCOPE,	 
	ACPI_NS_NEWSCOPE,	 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,	 
	ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,	 
	ACPI_NS_NEWSCOPE,	 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL,		 
	ACPI_NS_NORMAL		 
};

 

 

const char *acpi_gbl_region_types[ACPI_NUM_PREDEFINED_REGIONS] = {
	"SystemMemory",		 
	"SystemIO",		 
	"PCI_Config",		 
	"EmbeddedControl",	 
	"SMBus",		 
	"SystemCMOS",		 
	"PCIBARTarget",		 
	"IPMI",			 
	"GeneralPurposeIo",	 
	"GenericSerialBus",	 
	"PCC",			 
	"PlatformRtMechanism"	 
};

const char *acpi_ut_get_region_name(u8 space_id)
{

	if (space_id >= ACPI_USER_REGION_BEGIN) {
		return ("UserDefinedRegion");
	} else if (space_id == ACPI_ADR_SPACE_DATA_TABLE) {
		return ("DataTable");
	} else if (space_id == ACPI_ADR_SPACE_FIXED_HARDWARE) {
		return ("FunctionalFixedHW");
	} else if (space_id >= ACPI_NUM_PREDEFINED_REGIONS) {
		return ("InvalidSpaceId");
	}

	return (acpi_gbl_region_types[space_id]);
}

 

 

static const char *acpi_gbl_event_types[ACPI_NUM_FIXED_EVENTS] = {
	"PM_Timer",
	"GlobalLock",
	"PowerButton",
	"SleepButton",
	"RealTimeClock",
};

const char *acpi_ut_get_event_name(u32 event_id)
{

	if (event_id > ACPI_EVENT_MAX) {
		return ("InvalidEventID");
	}

	return (acpi_gbl_event_types[event_id]);
}

 

 
static const char acpi_gbl_bad_type[] = "UNDEFINED";

 

static const char *acpi_gbl_ns_type_names[] = {
	  "Untyped",
	  "Integer",
	  "String",
	  "Buffer",
	  "Package",
	  "FieldUnit",
	  "Device",
	  "Event",
	  "Method",
	  "Mutex",
	  "Region",
	  "Power",
	  "Processor",
	  "Thermal",
	  "BufferField",
	  "DdbHandle",
	  "DebugObject",
	  "RegionField",
	  "BankField",
	  "IndexField",
	  "Reference",
	  "Alias",
	  "MethodAlias",
	  "Notify",
	  "AddrHandler",
	  "ResourceDesc",
	  "ResourceFld",
	  "Scope",
	  "Extra",
	  "Data",
	  "Invalid"
};

const char *acpi_ut_get_type_name(acpi_object_type type)
{

	if (type > ACPI_TYPE_INVALID) {
		return (acpi_gbl_bad_type);
	}

	return (acpi_gbl_ns_type_names[type]);
}

const char *acpi_ut_get_object_type_name(union acpi_operand_object *obj_desc)
{
	ACPI_FUNCTION_TRACE(ut_get_object_type_name);

	if (!obj_desc) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Null Object Descriptor\n"));
		return_STR("[NULL Object Descriptor]");
	}

	 

	if ((ACPI_GET_DESCRIPTOR_TYPE(obj_desc) != ACPI_DESC_TYPE_OPERAND) &&
	    (ACPI_GET_DESCRIPTOR_TYPE(obj_desc) != ACPI_DESC_TYPE_NAMED)) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Invalid object descriptor type: 0x%2.2X [%s] (%p)\n",
				  ACPI_GET_DESCRIPTOR_TYPE(obj_desc),
				  acpi_ut_get_descriptor_name(obj_desc),
				  obj_desc));

		return_STR("Invalid object");
	}

	return_STR(acpi_ut_get_type_name(obj_desc->common.type));
}

 

const char *acpi_ut_get_node_name(void *object)
{
	struct acpi_namespace_node *node = (struct acpi_namespace_node *)object;

	 

	if (!object) {
		return ("NULL");
	}

	 

	if ((object == ACPI_ROOT_OBJECT) || (object == acpi_gbl_root_node)) {
		return ("\"\\\" ");
	}

	 

	if (ACPI_GET_DESCRIPTOR_TYPE(node) != ACPI_DESC_TYPE_NAMED) {
		return ("####");
	}

	 
	acpi_ut_repair_name(node->name.ascii);

	 

	return (node->name.ascii);
}

 

 

static const char *acpi_gbl_desc_type_names[] = {
	  "Not a Descriptor",
	  "Cached Object",
	  "State-Generic",
	  "State-Update",
	  "State-Package",
	  "State-Control",
	  "State-RootParseScope",
	  "State-ParseScope",
	  "State-WalkScope",
	  "State-Result",
	  "State-Notify",
	  "State-Thread",
	  "Tree Walk State",
	  "Parse Tree Op",
	  "Operand Object",
	  "Namespace Node"
};

const char *acpi_ut_get_descriptor_name(void *object)
{

	if (!object) {
		return ("NULL OBJECT");
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(object) > ACPI_DESC_TYPE_MAX) {
		return ("Not a Descriptor");
	}

	return (acpi_gbl_desc_type_names[ACPI_GET_DESCRIPTOR_TYPE(object)]);
}

 

 

static const char *acpi_gbl_ref_class_names[] = {
	  "Local",
	  "Argument",
	  "RefOf",
	  "Index",
	  "DdbHandle",
	  "Named Object",
	  "Debug"
};

const char *acpi_ut_get_reference_name(union acpi_operand_object *object)
{

	if (!object) {
		return ("NULL Object");
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(object) != ACPI_DESC_TYPE_OPERAND) {
		return ("Not an Operand object");
	}

	if (object->common.type != ACPI_TYPE_LOCAL_REFERENCE) {
		return ("Not a Reference object");
	}

	if (object->reference.class > ACPI_REFCLASS_MAX) {
		return ("Unknown Reference class");
	}

	return (acpi_gbl_ref_class_names[object->reference.class]);
}

 

 

static const char *acpi_gbl_mutex_names[ACPI_NUM_MUTEX] = {
	"ACPI_MTX_Interpreter",
	"ACPI_MTX_Namespace",
	"ACPI_MTX_Tables",
	"ACPI_MTX_Events",
	"ACPI_MTX_Caches",
	"ACPI_MTX_Memory",
};

const char *acpi_ut_get_mutex_name(u32 mutex_id)
{

	if (mutex_id > ACPI_MAX_MUTEX) {
		return ("Invalid Mutex ID");
	}

	return (acpi_gbl_mutex_names[mutex_id]);
}

#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

 

 

 

static const char *acpi_gbl_generic_notify[ACPI_GENERIC_NOTIFY_MAX + 1] = {
	  "Bus Check",
	  "Device Check",
	  "Device Wake",
	  "Eject Request",
	  "Device Check Light",
	  "Frequency Mismatch",
	  "Bus Mode Mismatch",
	  "Power Fault",
	  "Capabilities Check",
	  "Device PLD Check",
	  "Reserved",
	  "System Locality Update",
								  "Reserved (was previously Shutdown Request)",
								 
	  "System Resource Affinity Update",
								  "Heterogeneous Memory Attributes Update",
								 
						  "Error Disconnect Recover"
						 
};

static const char *acpi_gbl_device_notify[5] = {
	  "Status Change",
	  "Information Change",
	  "Device-Specific Change",
	  "Device-Specific Change",
	  "Reserved"
};

static const char *acpi_gbl_processor_notify[5] = {
	  "Performance Capability Change",
	  "C-State Change",
	  "Throttling Capability Change",
	  "Guaranteed Change",
	  "Minimum Excursion"
};

static const char *acpi_gbl_thermal_notify[5] = {
	  "Thermal Status Change",
	  "Thermal Trip Point Change",
	  "Thermal Device List Change",
	  "Thermal Relationship Change",
	  "Reserved"
};

const char *acpi_ut_get_notify_name(u32 notify_value, acpi_object_type type)
{

	 

	if (notify_value <= ACPI_GENERIC_NOTIFY_MAX) {
		return (acpi_gbl_generic_notify[notify_value]);
	}

	 

	if (notify_value <= ACPI_MAX_SYS_NOTIFY) {
		return ("Reserved");
	}

	 

	if (notify_value <= ACPI_SPECIFIC_NOTIFY_MAX) {
		switch (type) {
		case ACPI_TYPE_ANY:
		case ACPI_TYPE_DEVICE:
			return (acpi_gbl_device_notify[notify_value - 0x80]);

		case ACPI_TYPE_PROCESSOR:
			return (acpi_gbl_processor_notify[notify_value - 0x80]);

		case ACPI_TYPE_THERMAL:
			return (acpi_gbl_thermal_notify[notify_value - 0x80]);

		default:
			return ("Target object type does not support notifies");
		}
	}

	 

	if (notify_value <= ACPI_MAX_DEVICE_SPECIFIC_NOTIFY) {
		return ("Device-Specific");
	}

	 

	return ("Hardware-Specific");
}

 

static const char *acpi_gbl_argument_type[20] = {
	  "Unknown ARGP",
	  "ByteData",
	  "ByteList",
	  "CharList",
	  "DataObject",
	  "DataObjectList",
	  "DWordData",
	  "FieldList",
	  "Name",
	  "NameString",
	  "ObjectList",
	  "PackageLength",
	  "SuperName",
	  "Target",
	  "TermArg",
	  "TermList",
	  "WordData",
	  "QWordData",
	  "SimpleName",
	  "NameOrRef"
};

const char *acpi_ut_get_argument_type_name(u32 arg_type)
{

	if (arg_type > ARGP_MAX) {
		return ("Unknown ARGP");
	}

	return (acpi_gbl_argument_type[arg_type]);
}

#endif

 

u8 acpi_ut_valid_object_type(acpi_object_type type)
{

	if (type > ACPI_TYPE_LOCAL_MAX) {

		 

		return (FALSE);
	}

	return (TRUE);
}
