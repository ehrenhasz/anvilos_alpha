
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acparser.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("dspkginit")

 
static void
acpi_ds_resolve_package_element(union acpi_operand_object **element);

 

acpi_status
acpi_ds_build_internal_package_obj(struct acpi_walk_state *walk_state,
				   union acpi_parse_object *op,
				   u32 element_count,
				   union acpi_operand_object **obj_desc_ptr)
{
	union acpi_parse_object *arg;
	union acpi_parse_object *parent;
	union acpi_operand_object *obj_desc = NULL;
	acpi_status status = AE_OK;
	u8 module_level_code = FALSE;
	u16 reference_count;
	u32 index;
	u32 i;

	ACPI_FUNCTION_TRACE(ds_build_internal_package_obj);

	 

	if (walk_state->parse_flags & ACPI_PARSE_MODULE_LEVEL) {
		module_level_code = TRUE;
	}

	 

	parent = op->common.parent;
	while ((parent->common.aml_opcode == AML_PACKAGE_OP) ||
	       (parent->common.aml_opcode == AML_VARIABLE_PACKAGE_OP)) {
		parent = parent->common.parent;
	}

	 
	obj_desc = *obj_desc_ptr;
	if (!obj_desc) {
		obj_desc = acpi_ut_create_internal_object(ACPI_TYPE_PACKAGE);
		*obj_desc_ptr = obj_desc;
		if (!obj_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		obj_desc->package.node = parent->common.node;
	}

	if (obj_desc->package.flags & AOPOBJ_DATA_VALID) {	 
		return_ACPI_STATUS(AE_OK);
	}

	 
	if (!obj_desc->package.elements) {
		obj_desc->package.elements = ACPI_ALLOCATE_ZEROED(((acpi_size)
								   element_count
								   +
								   1) *
								  sizeof(void
									 *));

		if (!obj_desc->package.elements) {
			acpi_ut_delete_object_desc(obj_desc);
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		obj_desc->package.count = element_count;
	}

	 

	arg = op->common.value.arg;
	arg = arg->common.next;

	 
	if (module_level_code) {
		obj_desc->package.aml_start = walk_state->aml;
		obj_desc->package.aml_length = 0;

		ACPI_DEBUG_PRINT_RAW((ACPI_DB_PARSE,
				      "%s: Deferring resolution of Package elements\n",
				      ACPI_GET_FUNCTION_NAME));
	}

	 
	for (i = 0; arg && (i < element_count); i++) {
		if (arg->common.aml_opcode == AML_INT_RETURN_VALUE_OP) {
			if (!arg->common.node) {
				 
				ACPI_EXCEPTION((AE_INFO, AE_SUPPORT,
						"Expressions within package elements are not supported"));

				 

				acpi_ut_remove_reference(walk_state->results->
							 results.obj_desc[0]);
				return_ACPI_STATUS(AE_SUPPORT);
			}

			if (arg->common.node->type == ACPI_TYPE_METHOD) {
				 
				arg->common.aml_opcode = AML_INT_NAMEPATH_OP;
				status =
				    acpi_ds_build_internal_object(walk_state,
								  arg,
								  &obj_desc->
								  package.
								  elements[i]);
			} else {
				 

				obj_desc->package.elements[i] =
				    ACPI_CAST_PTR(union acpi_operand_object,
						  arg->common.node);
			}
		} else {
			status =
			    acpi_ds_build_internal_object(walk_state, arg,
							  &obj_desc->package.
							  elements[i]);
			if (status == AE_NOT_FOUND) {
				ACPI_ERROR((AE_INFO, "%-48s",
					    "****DS namepath not found"));
			}

			if (!module_level_code) {
				 
				acpi_ds_init_package_element(0,
							     obj_desc->package.
							     elements[i], NULL,
							     &obj_desc->package.
							     elements[i]);
			}
		}

		if (*obj_desc_ptr) {

			 

			reference_count =
			    (*obj_desc_ptr)->common.reference_count;
			if (reference_count > 1) {

				 
				 

				for (index = 0;
				     index < ((u32)reference_count - 1);
				     index++) {
					acpi_ut_add_reference((obj_desc->
							       package.
							       elements[i]));
				}
			}
		}

		arg = arg->common.next;
	}

	 

	if (arg) {
		 
		while (arg) {
			 
			if (arg->common.node) {
				acpi_ut_remove_reference(ACPI_CAST_PTR
							 (union
							  acpi_operand_object,
							  arg->common.node));
				arg->common.node = NULL;
			}

			 

			i++;
			arg = arg->common.next;
		}

		ACPI_INFO(("Actual Package length (%u) is larger than "
			   "NumElements field (%u), truncated",
			   i, element_count));
	} else if (i < element_count) {
		 
		ACPI_DEBUG_PRINT_RAW((ACPI_DB_INFO,
				      "%s: Package List length (%u) smaller than NumElements "
				      "count (%u), padded with null elements\n",
				      ACPI_GET_FUNCTION_NAME, i,
				      element_count));
	}

	 

	if (!module_level_code) {
		obj_desc->package.flags |= AOPOBJ_DATA_VALID;
	}

	op->common.node = ACPI_CAST_PTR(struct acpi_namespace_node, obj_desc);
	return_ACPI_STATUS(status);
}

 

acpi_status
acpi_ds_init_package_element(u8 object_type,
			     union acpi_operand_object *source_object,
			     union acpi_generic_state *state, void *context)
{
	union acpi_operand_object **element_ptr;

	ACPI_FUNCTION_TRACE(ds_init_package_element);

	if (!source_object) {
		return_ACPI_STATUS(AE_OK);
	}

	 
	if (context) {

		 

		element_ptr = (union acpi_operand_object **)context;
	} else {
		 

		element_ptr = state->pkg.this_target_obj;
	}

	 

	if (source_object->common.type == ACPI_TYPE_LOCAL_REFERENCE) {

		 

		acpi_ds_resolve_package_element(element_ptr);
	} else if (source_object->common.type == ACPI_TYPE_PACKAGE) {
		source_object->package.flags |= AOPOBJ_DATA_VALID;
	}

	return_ACPI_STATUS(AE_OK);
}

 

static void
acpi_ds_resolve_package_element(union acpi_operand_object **element_ptr)
{
	acpi_status status;
	acpi_status status2;
	union acpi_generic_state scope_info;
	union acpi_operand_object *element = *element_ptr;
	struct acpi_namespace_node *resolved_node;
	struct acpi_namespace_node *original_node;
	char *external_path = "";
	acpi_object_type type;

	ACPI_FUNCTION_TRACE(ds_resolve_package_element);

	 

	if (element->reference.resolved) {
		ACPI_DEBUG_PRINT_RAW((ACPI_DB_PARSE,
				      "%s: Package element is already resolved\n",
				      ACPI_GET_FUNCTION_NAME));

		return_VOID;
	}

	 

	scope_info.scope.node = element->reference.node;	 

	status = acpi_ns_lookup(&scope_info, (char *)element->reference.aml,
				ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
				ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
				NULL, &resolved_node);
	if (ACPI_FAILURE(status)) {
		if ((status == AE_NOT_FOUND)
		    && acpi_gbl_ignore_package_resolution_errors) {
			 

			 

			acpi_ut_remove_reference(*element_ptr);
			*element_ptr = NULL;
			return_VOID;
		}

		status2 = acpi_ns_externalize_name(ACPI_UINT32_MAX,
						   (char *)element->reference.
						   aml, NULL, &external_path);

		ACPI_EXCEPTION((AE_INFO, status,
				"While resolving a named reference package element - %s",
				external_path));
		if (ACPI_SUCCESS(status2)) {
			ACPI_FREE(external_path);
		}

		 

		acpi_ut_remove_reference(*element_ptr);
		*element_ptr = NULL;
		return_VOID;
	} else if (resolved_node->type == ACPI_TYPE_ANY) {

		 

		ACPI_ERROR((AE_INFO,
			    "Could not resolve named package element [%4.4s] in [%4.4s]",
			    resolved_node->name.ascii,
			    scope_info.scope.node->name.ascii));
		*element_ptr = NULL;
		return_VOID;
	}

	 
	if (resolved_node->type == ACPI_TYPE_LOCAL_ALIAS) {
		resolved_node = ACPI_CAST_PTR(struct acpi_namespace_node,
					      resolved_node->object);
	}

	 

	element->reference.resolved = TRUE;
	element->reference.node = resolved_node;
	type = element->reference.node->type;

	 
	original_node = resolved_node;
	status = acpi_ex_resolve_node_to_value(&resolved_node, NULL);
	if (ACPI_FAILURE(status)) {
		return_VOID;
	}

	switch (type) {
		 
	case ACPI_TYPE_DEVICE:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_METHOD:
		break;

	case ACPI_TYPE_MUTEX:
	case ACPI_TYPE_POWER:
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_EVENT:
	case ACPI_TYPE_REGION:

		 

		acpi_ut_remove_reference(original_node->object);
		break;

	default:
		 
		acpi_ut_remove_reference(element);
		*element_ptr = (union acpi_operand_object *)resolved_node;
		break;
	}

	return_VOID;
}
