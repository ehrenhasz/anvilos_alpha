
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "amlcode.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_NAMESPACE
ACPI_MODULE_NAME("nsnames")

 
char *acpi_ns_get_external_pathname(struct acpi_namespace_node *node)
{
	char *name_buffer;

	ACPI_FUNCTION_TRACE_PTR(ns_get_external_pathname, node);

	name_buffer = acpi_ns_get_normalized_pathname(node, FALSE);
	return_PTR(name_buffer);
}

 

acpi_size acpi_ns_get_pathname_length(struct acpi_namespace_node *node)
{
	acpi_size size;

	 

	if (ACPI_GET_DESCRIPTOR_TYPE(node) != ACPI_DESC_TYPE_NAMED) {
		ACPI_ERROR((AE_INFO,
			    "Invalid/cached reference target node: %p, descriptor type %d",
			    node, ACPI_GET_DESCRIPTOR_TYPE(node)));
		return (0);
	}

	size = acpi_ns_build_normalized_path(node, NULL, 0, FALSE);
	return (size);
}

 

acpi_status
acpi_ns_handle_to_name(acpi_handle target_handle, struct acpi_buffer *buffer)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	const char *node_name;

	ACPI_FUNCTION_TRACE_PTR(ns_handle_to_name, target_handle);

	node = acpi_ns_validate_handle(target_handle);
	if (!node) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	status = acpi_ut_initialize_buffer(buffer, ACPI_PATH_SEGMENT_LENGTH);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	 

	node_name = acpi_ut_get_node_name(node);
	ACPI_COPY_NAMESEG(buffer->pointer, node_name);
	((char *)buffer->pointer)[ACPI_NAMESEG_SIZE] = 0;

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "%4.4s\n", (char *)buffer->pointer));
	return_ACPI_STATUS(AE_OK);
}

 

acpi_status
acpi_ns_handle_to_pathname(acpi_handle target_handle,
			   struct acpi_buffer *buffer, u8 no_trailing)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	acpi_size required_size;

	ACPI_FUNCTION_TRACE_PTR(ns_handle_to_pathname, target_handle);

	node = acpi_ns_validate_handle(target_handle);
	if (!node) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	required_size =
	    acpi_ns_build_normalized_path(node, NULL, 0, no_trailing);
	if (!required_size) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	 

	status = acpi_ut_initialize_buffer(buffer, required_size);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	 

	(void)acpi_ns_build_normalized_path(node, buffer->pointer,
					    (u32)required_size, no_trailing);

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "%s [%X]\n",
			  (char *)buffer->pointer, (u32) required_size));
	return_ACPI_STATUS(AE_OK);
}

 

u32
acpi_ns_build_normalized_path(struct acpi_namespace_node *node,
			      char *full_path, u32 path_size, u8 no_trailing)
{
	u32 length = 0, i;
	char name[ACPI_NAMESEG_SIZE];
	u8 do_no_trailing;
	char c, *left, *right;
	struct acpi_namespace_node *next_node;

	ACPI_FUNCTION_TRACE_PTR(ns_build_normalized_path, node);

#define ACPI_PATH_PUT8(path, size, byte, length)    \
	do {                                            \
		if ((length) < (size))                      \
		{                                           \
			(path)[(length)] = (byte);              \
		}                                           \
		(length)++;                                 \
	} while (0)

	 
	if (!full_path) {
		path_size = 0;
	}

	if (!node) {
		goto build_trailing_null;
	}

	next_node = node;
	while (next_node && next_node != acpi_gbl_root_node) {
		if (next_node != node) {
			ACPI_PATH_PUT8(full_path, path_size,
				       AML_DUAL_NAME_PREFIX, length);
		}

		ACPI_MOVE_32_TO_32(name, &next_node->name);
		do_no_trailing = no_trailing;
		for (i = 0; i < 4; i++) {
			c = name[4 - i - 1];
			if (do_no_trailing && c != '_') {
				do_no_trailing = FALSE;
			}
			if (!do_no_trailing) {
				ACPI_PATH_PUT8(full_path, path_size, c, length);
			}
		}

		next_node = next_node->parent;
	}

	ACPI_PATH_PUT8(full_path, path_size, AML_ROOT_PREFIX, length);

	 

	if (length <= path_size) {
		left = full_path;
		right = full_path + length - 1;

		while (left < right) {
			c = *left;
			*left++ = *right;
			*right-- = c;
		}
	}

	 

build_trailing_null:
	ACPI_PATH_PUT8(full_path, path_size, '\0', length);

#undef ACPI_PATH_PUT8

	return_UINT32(length);
}

 

char *acpi_ns_get_normalized_pathname(struct acpi_namespace_node *node,
				      u8 no_trailing)
{
	char *name_buffer;
	acpi_size size;

	ACPI_FUNCTION_TRACE_PTR(ns_get_normalized_pathname, node);

	 

	size = acpi_ns_build_normalized_path(node, NULL, 0, no_trailing);
	if (!size) {
		return_PTR(NULL);
	}

	 

	name_buffer = ACPI_ALLOCATE_ZEROED(size);
	if (!name_buffer) {
		ACPI_ERROR((AE_INFO, "Could not allocate %u bytes", (u32)size));
		return_PTR(NULL);
	}

	 

	(void)acpi_ns_build_normalized_path(node, name_buffer, (u32)size,
					    no_trailing);

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_NAMES, "%s: Path \"%s\"\n",
			      ACPI_GET_FUNCTION_NAME, name_buffer));

	return_PTR(name_buffer);
}

 

char *acpi_ns_build_prefixed_pathname(union acpi_generic_state *prefix_scope,
				      const char *internal_path)
{
	acpi_status status;
	char *full_path = NULL;
	char *external_path = NULL;
	char *prefix_path = NULL;
	acpi_size prefix_path_length = 0;

	 

	if (prefix_scope && prefix_scope->scope.node) {
		prefix_path =
		    acpi_ns_get_normalized_pathname(prefix_scope->scope.node,
						    TRUE);
		if (prefix_path) {
			prefix_path_length = strlen(prefix_path);
		}
	}

	status = acpi_ns_externalize_name(ACPI_UINT32_MAX, internal_path,
					  NULL, &external_path);
	if (ACPI_FAILURE(status)) {
		goto cleanup;
	}

	 

	full_path =
	    ACPI_ALLOCATE_ZEROED(prefix_path_length + strlen(external_path) +
				 2);
	if (!full_path) {
		goto cleanup;
	}

	 

	if (prefix_path && (*external_path != '\\') && (*external_path != '^')) {
		strcat(full_path, prefix_path);
		if (prefix_path[1]) {
			strcat(full_path, ".");
		}
	}

	acpi_ns_normalize_pathname(external_path);
	strcat(full_path, external_path);

cleanup:
	if (prefix_path) {
		ACPI_FREE(prefix_path);
	}
	if (external_path) {
		ACPI_FREE(external_path);
	}

	return (full_path);
}

 

void acpi_ns_normalize_pathname(char *original_path)
{
	char *input_path = original_path;
	char *new_path_buffer;
	char *new_path;
	u32 i;

	 

	new_path_buffer = ACPI_ALLOCATE_ZEROED(strlen(input_path) + 1);
	new_path = new_path_buffer;
	if (!new_path_buffer) {
		return;
	}

	 

	if (*input_path == '\\') {
		*new_path = *input_path;
		new_path++;
		input_path++;
	}

	while (*input_path == '^') {
		*new_path = *input_path;
		new_path++;
		input_path++;
	}

	 

	while (*input_path) {

		 

		for (i = 0; (i < ACPI_NAMESEG_SIZE) && *input_path; i++) {
			if ((i == 0) || (*input_path != '_')) {	 
				*new_path = *input_path;
				new_path++;
			}

			input_path++;
		}

		 

		if (*input_path == '.') {
			*new_path = *input_path;
			new_path++;
			input_path++;
		}
	}

	*new_path = 0;
	strcpy(original_path, new_path_buffer);
	ACPI_FREE(new_path_buffer);
}
