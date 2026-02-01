
 

 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"
#include "acparser.h"
#include "acdispat.h"
#include "amlcode.h"
#include "acconvert.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_PARSER
ACPI_MODULE_NAME("psloop")

 
static acpi_status
acpi_ps_get_arguments(struct acpi_walk_state *walk_state,
		      u8 * aml_op_start, union acpi_parse_object *op);

 

static acpi_status
acpi_ps_get_arguments(struct acpi_walk_state *walk_state,
		      u8 * aml_op_start, union acpi_parse_object *op)
{
	acpi_status status = AE_OK;
	union acpi_parse_object *arg = NULL;

	ACPI_FUNCTION_TRACE_PTR(ps_get_arguments, walk_state);

	ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
			  "Get arguments for opcode [%s]\n",
			  op->common.aml_op_name));

	switch (op->common.aml_opcode) {
	case AML_BYTE_OP:	 
	case AML_WORD_OP:	 
	case AML_DWORD_OP:	 
	case AML_QWORD_OP:	 
	case AML_STRING_OP:	 

		 

		acpi_ps_get_next_simple_arg(&(walk_state->parser_state),
					    GET_CURRENT_ARG_TYPE(walk_state->
								 arg_types),
					    op);
		break;

	case AML_INT_NAMEPATH_OP:	 

		status = acpi_ps_get_next_namepath(walk_state,
						   &(walk_state->parser_state),
						   op,
						   ACPI_POSSIBLE_METHOD_CALL);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		walk_state->arg_types = 0;
		break;

	default:
		 
		while (GET_CURRENT_ARG_TYPE(walk_state->arg_types) &&
		       !walk_state->arg_count) {
			walk_state->aml = walk_state->parser_state.aml;

			switch (op->common.aml_opcode) {
			case AML_METHOD_OP:
			case AML_BUFFER_OP:
			case AML_PACKAGE_OP:
			case AML_VARIABLE_PACKAGE_OP:
			case AML_WHILE_OP:

				break;

			default:

				ASL_CV_CAPTURE_COMMENTS(walk_state);
				break;
			}

			status =
			    acpi_ps_get_next_arg(walk_state,
						 &(walk_state->parser_state),
						 GET_CURRENT_ARG_TYPE
						 (walk_state->arg_types), &arg);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}

			if (arg) {
				acpi_ps_append_arg(op, arg);
			}

			INCREMENT_ARG_LIST(walk_state->arg_types);
		}

		ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
				  "Final argument count: %8.8X pass %u\n",
				  walk_state->arg_count,
				  walk_state->pass_number));

		 

		switch (op->common.aml_opcode) {
		case AML_METHOD_OP:
			 
			op->named.data = walk_state->parser_state.aml;
			op->named.length = (u32)
			    (walk_state->parser_state.pkg_end -
			     walk_state->parser_state.aml);

			 

			walk_state->parser_state.aml =
			    walk_state->parser_state.pkg_end;
			walk_state->arg_count = 0;
			break;

		case AML_BUFFER_OP:
		case AML_PACKAGE_OP:
		case AML_VARIABLE_PACKAGE_OP:

			if ((op->common.parent) &&
			    (op->common.parent->common.aml_opcode ==
			     AML_NAME_OP)
			    && (walk_state->pass_number <=
				ACPI_IMODE_LOAD_PASS2)) {
				ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
						  "Setup Package/Buffer: Pass %u, AML Ptr: %p\n",
						  walk_state->pass_number,
						  aml_op_start));

				 
				op->named.data = aml_op_start;
				op->named.length = (u32)
				    (walk_state->parser_state.pkg_end -
				     aml_op_start);

				 

				walk_state->parser_state.aml =
				    walk_state->parser_state.pkg_end;
				walk_state->arg_count = 0;
			}
			break;

		case AML_WHILE_OP:

			if (walk_state->control_state) {
				walk_state->control_state->control.package_end =
				    walk_state->parser_state.pkg_end;
			}
			break;

		default:

			 

			break;
		}

		break;
	}

	return_ACPI_STATUS(AE_OK);
}

 

acpi_status acpi_ps_parse_loop(struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;
	union acpi_parse_object *op = NULL;	 
	struct acpi_parse_state *parser_state;
	u8 *aml_op_start = NULL;
	u8 opcode_length;

	ACPI_FUNCTION_TRACE_PTR(ps_parse_loop, walk_state);

	if (walk_state->descending_callback == NULL) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	parser_state = &walk_state->parser_state;
	walk_state->arg_types = 0;

#ifndef ACPI_CONSTANT_EVAL_ONLY

	if (walk_state->walk_type & ACPI_WALK_METHOD_RESTART) {

		 

		if (acpi_ps_has_completed_scope(parser_state)) {
			 
			if ((parser_state->scope->parse_scope.op) &&
			    ((parser_state->scope->parse_scope.op->common.
			      aml_opcode == AML_IF_OP)
			     || (parser_state->scope->parse_scope.op->common.
				 aml_opcode == AML_WHILE_OP))
			    && (walk_state->control_state)
			    && (walk_state->control_state->common.state ==
				ACPI_CONTROL_PREDICATE_EXECUTING)) {
				 
				walk_state->op = NULL;
				status =
				    acpi_ds_get_predicate_value(walk_state,
								ACPI_TO_POINTER
								(TRUE));
				if (ACPI_FAILURE(status)
				    && !ACPI_CNTL_EXCEPTION(status)) {
					if (status == AE_AML_NO_RETURN_VALUE) {
						ACPI_EXCEPTION((AE_INFO, status,
								"Invoked method did not return a value"));
					}

					ACPI_EXCEPTION((AE_INFO, status,
							"GetPredicate Failed"));
					return_ACPI_STATUS(status);
				}

				status =
				    acpi_ps_next_parse_state(walk_state, op,
							     status);
			}

			acpi_ps_pop_scope(parser_state, &op,
					  &walk_state->arg_types,
					  &walk_state->arg_count);
			ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
					  "Popped scope, Op=%p\n", op));
		} else if (walk_state->prev_op) {

			 

			op = walk_state->prev_op;
			walk_state->arg_types = walk_state->prev_arg_types;
		}
	}
#endif

	 

	while ((parser_state->aml < parser_state->aml_end) || (op)) {
		ASL_CV_CAPTURE_COMMENTS(walk_state);

		aml_op_start = parser_state->aml;
		if (!op) {
			status =
			    acpi_ps_create_op(walk_state, aml_op_start, &op);
			if (ACPI_FAILURE(status)) {
				 
				if ((walk_state->
				     parse_flags & ACPI_PARSE_MODULE_LEVEL)
				    && ((status == AE_ALREADY_EXISTS)
					|| (status == AE_NOT_FOUND))) {
					status = AE_OK;
				}
				if (status == AE_CTRL_PARSE_CONTINUE) {
					continue;
				}

				if (status == AE_CTRL_PARSE_PENDING) {
					status = AE_OK;
				}

				if (status == AE_CTRL_TERMINATE) {
					return_ACPI_STATUS(status);
				}

				status =
				    acpi_ps_complete_op(walk_state, &op,
							status);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}
				if (acpi_ns_opens_scope
				    (acpi_ps_get_opcode_info
				     (walk_state->opcode)->object_type)) {
					 
					ACPI_INFO(("Skipping parse of AML opcode: %s (0x%4.4X)", acpi_ps_get_opcode_name(walk_state->opcode), walk_state->opcode));

					 
					opcode_length = 1;
					if ((walk_state->opcode & 0xFF00) ==
					    AML_EXTENDED_OPCODE) {
						opcode_length = 2;
					}
					walk_state->parser_state.aml =
					    walk_state->aml + opcode_length;

					walk_state->parser_state.aml =
					    acpi_ps_get_next_package_end
					    (&walk_state->parser_state);
					walk_state->aml =
					    walk_state->parser_state.aml;
				}

				continue;
			}

			acpi_ex_start_trace_opcode(op, walk_state);
		}

		 
		walk_state->arg_count = 0;

		switch (op->common.aml_opcode) {
		case AML_BYTE_OP:
		case AML_WORD_OP:
		case AML_DWORD_OP:
		case AML_QWORD_OP:

			break;

		default:

			ASL_CV_CAPTURE_COMMENTS(walk_state);
			break;
		}

		 

		if (walk_state->arg_types) {

			 

			status =
			    acpi_ps_get_arguments(walk_state, aml_op_start, op);
			if (ACPI_FAILURE(status)) {
				status =
				    acpi_ps_complete_op(walk_state, &op,
							status);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}
				if ((walk_state->control_state) &&
				    ((walk_state->control_state->control.
				      opcode == AML_IF_OP)
				     || (walk_state->control_state->control.
					 opcode == AML_WHILE_OP))) {
					 
					parser_state->aml =
					    walk_state->control_state->control.
					    aml_predicate_start + 1;
					parser_state->aml =
					    acpi_ps_get_next_package_end
					    (parser_state);
					walk_state->aml = parser_state->aml;

					ACPI_ERROR((AE_INFO,
						    "Skipping While/If block"));
					if (*walk_state->aml == AML_ELSE_OP) {
						ACPI_ERROR((AE_INFO,
							    "Skipping Else block"));
						walk_state->parser_state.aml =
						    walk_state->aml + 1;
						walk_state->parser_state.aml =
						    acpi_ps_get_next_package_end
						    (parser_state);
						walk_state->aml =
						    parser_state->aml;
					}
					ACPI_FREE(acpi_ut_pop_generic_state
						  (&walk_state->control_state));
				}
				op = NULL;
				continue;
			}
		}

		 

		ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
				  "Parseloop: argument count: %8.8X\n",
				  walk_state->arg_count));

		if (walk_state->arg_count) {
			 
			status = acpi_ps_push_scope(parser_state, op,
						    walk_state->arg_types,
						    walk_state->arg_count);
			if (ACPI_FAILURE(status)) {
				status =
				    acpi_ps_complete_op(walk_state, &op,
							status);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				continue;
			}

			op = NULL;
			continue;
		}

		 
		walk_state->op_info =
		    acpi_ps_get_opcode_info(op->common.aml_opcode);
		if (walk_state->op_info->flags & AML_NAMED) {
			if (op->common.aml_opcode == AML_REGION_OP ||
			    op->common.aml_opcode == AML_DATA_REGION_OP) {
				 
				op->named.length =
				    (u32) (parser_state->aml - op->named.data);
			}
		}

		if (walk_state->op_info->flags & AML_CREATE) {
			 
			op->named.length =
			    (u32) (parser_state->aml - op->named.data);
		}

		if (op->common.aml_opcode == AML_BANK_FIELD_OP) {
			 
			op->named.length =
			    (u32) (parser_state->aml - op->named.data);
		}

		 

		if (walk_state->ascending_callback != NULL) {
			walk_state->op = op;
			walk_state->opcode = op->common.aml_opcode;

			status = walk_state->ascending_callback(walk_state);
			status =
			    acpi_ps_next_parse_state(walk_state, op, status);
			if (status == AE_CTRL_PENDING) {
				status = AE_OK;
			} else
			    if ((walk_state->
				 parse_flags & ACPI_PARSE_MODULE_LEVEL)
				&& (ACPI_AML_EXCEPTION(status)
				    || status == AE_ALREADY_EXISTS
				    || status == AE_NOT_FOUND)) {
				 
				status = AE_OK;
			}
		}

		status = acpi_ps_complete_op(walk_state, &op, status);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

	}			 

	status = acpi_ps_complete_final_op(walk_state, op, status);
	return_ACPI_STATUS(status);
}
