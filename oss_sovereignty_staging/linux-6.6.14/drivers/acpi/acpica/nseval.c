
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acparser.h"
#include "acinterp.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nseval")

 
acpi_status acpi_ns_evaluate(struct acpi_evaluate_info *info)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ns_evaluate);

	if (!info) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (!info->node) {
		 
		status =
		    acpi_ns_get_node(info->prefix_node, info->relative_pathname,
				     ACPI_NS_NO_UPSEARCH, &info->node);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	 
	if (acpi_ns_get_type(info->node) == ACPI_TYPE_LOCAL_METHOD_ALIAS) {
		info->node =
		    ACPI_CAST_PTR(struct acpi_namespace_node,
				  info->node->object);
	}

	 

	info->return_object = NULL;
	info->node_flags = info->node->flags;
	info->obj_desc = acpi_ns_get_attached_object(info->node);

	ACPI_DEBUG_PRINT((ACPI_DB_NAMES, "%s [%p] Value %p\n",
			  info->relative_pathname, info->node,
			  acpi_ns_get_attached_object(info->node)));

	 

	info->predefined =
	    acpi_ut_match_predefined_method(info->node->name.ascii);

	 

	info->full_pathname = acpi_ns_get_normalized_pathname(info->node, TRUE);
	if (!info->full_pathname) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	 

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_EVALUATION,
			      "%-26s:  %s (%s)\n", "   Enter evaluation",
			      &info->full_pathname[1],
			      acpi_ut_get_type_name(info->node->type)));

	 

	info->param_count = 0;
	if (info->parameters) {
		while (info->parameters[info->param_count]) {
			info->param_count++;
		}

		 

		if (info->param_count > ACPI_METHOD_NUM_ARGS) {
			ACPI_WARN_PREDEFINED((AE_INFO, info->full_pathname,
					      ACPI_WARN_ALWAYS,
					      "Excess arguments (%u) - using only %u",
					      info->param_count,
					      ACPI_METHOD_NUM_ARGS));

			info->param_count = ACPI_METHOD_NUM_ARGS;
		}
	}

	 
	acpi_ns_check_acpi_compliance(info->full_pathname, info->node,
				      info->predefined);

	 
	acpi_ns_check_argument_count(info->full_pathname, info->node,
				     info->param_count, info->predefined);

	 

	acpi_ns_check_argument_types(info);

	 
	switch (acpi_ns_get_type(info->node)) {
	case ACPI_TYPE_ANY:
	case ACPI_TYPE_DEVICE:
	case ACPI_TYPE_EVENT:
	case ACPI_TYPE_MUTEX:
	case ACPI_TYPE_REGION:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_LOCAL_SCOPE:
		 
		ACPI_ERROR((AE_INFO,
			    "%s: This object type [%s] "
			    "never contains data and cannot be evaluated",
			    info->full_pathname,
			    acpi_ut_get_type_name(info->node->type)));

		status = AE_TYPE;
		goto cleanup;

	case ACPI_TYPE_METHOD:
		 

		 

		if (!info->obj_desc) {
			ACPI_ERROR((AE_INFO,
				    "%s: Method has no attached sub-object",
				    info->full_pathname));
			status = AE_NULL_OBJECT;
			goto cleanup;
		}

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "**** Execute method [%s] at AML address %p length %X\n",
				  info->full_pathname,
				  info->obj_desc->method.aml_start + 1,
				  info->obj_desc->method.aml_length - 1));

		 
		acpi_ex_enter_interpreter();
		status = acpi_ps_execute_method(info);
		acpi_ex_exit_interpreter();
		break;

	default:
		 

		 
		acpi_ex_enter_interpreter();

		 

		info->return_object =
		    ACPI_CAST_PTR(union acpi_operand_object, info->node);

		status =
		    acpi_ex_resolve_node_to_value(ACPI_CAST_INDIRECT_PTR
						  (struct acpi_namespace_node,
						   &info->return_object), NULL);
		acpi_ex_exit_interpreter();

		if (ACPI_FAILURE(status)) {
			info->return_object = NULL;
			goto cleanup;
		}

		ACPI_DEBUG_PRINT((ACPI_DB_NAMES, "Returned object %p [%s]\n",
				  info->return_object,
				  acpi_ut_get_object_type_name(info->
							       return_object)));

		status = AE_CTRL_RETURN_VALUE;	 
		break;
	}

	 
	(void)acpi_ns_check_return_value(info->node, info, info->param_count,
					 status, &info->return_object);

	 

	if (status == AE_CTRL_RETURN_VALUE) {

		 

		if (info->flags & ACPI_IGNORE_RETURN_VALUE) {
			acpi_ut_remove_reference(info->return_object);
			info->return_object = NULL;
		}

		 

		status = AE_OK;
	} else if (ACPI_FAILURE(status)) {

		 

		if (info->return_object) {
			acpi_ut_remove_reference(info->return_object);
			info->return_object = NULL;
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_NAMES,
			  "*** Completed evaluation of object %s ***\n",
			  info->relative_pathname));

cleanup:
	 

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_EVALUATION,
			      "%-26s:  %s\n", "   Exit evaluation",
			      &info->full_pathname[1]));

	 
	ACPI_FREE(info->full_pathname);
	info->full_pathname = NULL;
	return_ACPI_STATUS(status);
}
