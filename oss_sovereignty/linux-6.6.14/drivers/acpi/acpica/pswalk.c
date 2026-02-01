
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acparser.h"

#define _COMPONENT          ACPI_PARSER
ACPI_MODULE_NAME("pswalk")

 
#include "amlcode.h"
void acpi_ps_delete_parse_tree(union acpi_parse_object *subtree_root)
{
	union acpi_parse_object *op = subtree_root;
	union acpi_parse_object *next = NULL;
	union acpi_parse_object *parent = NULL;
	u32 level = 0;

	ACPI_FUNCTION_TRACE_PTR(ps_delete_parse_tree, subtree_root);

	ACPI_DEBUG_PRINT((ACPI_DB_PARSE_TREES, " root %p\n", subtree_root));

	 

	while (op) {
		if (op != parent) {

			 

			if (ACPI_IS_DEBUG_ENABLED
			    (ACPI_LV_PARSE_TREES, _COMPONENT)) {

				 

				acpi_os_printf("      %*.s%s %p", (level * 4),
					       " ",
					       acpi_ps_get_opcode_name(op->
								       common.
								       aml_opcode),
					       op);

				if (op->named.aml_opcode == AML_INT_NAMEPATH_OP) {
					acpi_os_printf("  %4.4s",
						       op->common.value.string);
				}
				if (op->named.aml_opcode == AML_STRING_OP) {
					acpi_os_printf("  %s",
						       op->common.value.string);
				}
				acpi_os_printf("\n");
			}

			 

			next = acpi_ps_get_arg(op, 0);
			if (next) {

				 

				op = next;
				level++;
				continue;
			}
		}

		 

		next = op->common.next;
		parent = op->common.parent;

		acpi_ps_free_op(op);

		 

		if (op == subtree_root) {
			return_VOID;
		}

		if (next) {
			op = next;
		} else {
			level--;
			op = parent;
		}
	}

	return_VOID;
}
