 
 

#ifndef __ACSTRUCT_H__
#define __ACSTRUCT_H__

 

 

 
#define ACPI_NEXT_OP_DOWNWARD       1
#define ACPI_NEXT_OP_UPWARD         2

 
#define ACPI_WALK_NON_METHOD        0
#define ACPI_WALK_METHOD            0x01
#define ACPI_WALK_METHOD_RESTART    0x02

struct acpi_walk_state {
	struct acpi_walk_state *next;	 
	u8 descriptor_type;	 
	u8 walk_type;
	u16 opcode;		 
	u8 next_op_info;	 
	u8 num_operands;	 
	u8 operand_index;	 
	acpi_owner_id owner_id;	 
	u8 last_predicate;	 
	u8 current_result;
	u8 return_used;
	u8 scope_depth;
	u8 pass_number;		 
	u8 namespace_override;	 
	u8 result_size;		 
	u8 result_count;	 
	u8 *aml;
	u32 arg_types;
	u32 method_breakpoint;	 
	u32 user_breakpoint;	 
	u32 parse_flags;

	struct acpi_parse_state parser_state;	 
	u32 prev_arg_types;
	u32 arg_count;		 
	u16 method_nesting_depth;
	u8 method_is_nested;

	struct acpi_namespace_node arguments[ACPI_METHOD_NUM_ARGS];	 
	struct acpi_namespace_node local_variables[ACPI_METHOD_NUM_LOCALS];	 
	union acpi_operand_object *operands[ACPI_OBJ_NUM_OPERANDS + 1];	 
	union acpi_operand_object **params;

	u8 *aml_last_while;
	union acpi_operand_object **caller_return_desc;
	union acpi_generic_state *control_state;	 
	struct acpi_namespace_node *deferred_node;	 
	union acpi_operand_object *implicit_return_obj;
	struct acpi_namespace_node *method_call_node;	 
	union acpi_parse_object *method_call_op;	 
	union acpi_operand_object *method_desc;	 
	struct acpi_namespace_node *method_node;	 
	char *method_pathname;	 
	union acpi_parse_object *op;	 
	const struct acpi_opcode_info *op_info;	 
	union acpi_parse_object *origin;	 
	union acpi_operand_object *result_obj;
	union acpi_generic_state *results;	 
	union acpi_operand_object *return_desc;	 
	union acpi_generic_state *scope_info;	 
	union acpi_parse_object *prev_op;	 
	union acpi_parse_object *next_op;	 
	struct acpi_thread_state *thread;
	acpi_parse_downwards descending_callback;
	acpi_parse_upwards ascending_callback;
};

 

struct acpi_init_walk_info {
	u32 table_index;
	u32 object_count;
	u32 method_count;
	u32 serial_method_count;
	u32 non_serial_method_count;
	u32 serialized_method_count;
	u32 device_count;
	u32 op_region_count;
	u32 field_count;
	u32 buffer_count;
	u32 package_count;
	u32 op_region_init;
	u32 field_init;
	u32 buffer_init;
	u32 package_init;
	acpi_owner_id owner_id;
};

struct acpi_get_devices_info {
	acpi_walk_callback user_function;
	void *context;
	const char *hid;
};

union acpi_aml_operands {
	union acpi_operand_object *operands[7];

	struct {
		struct acpi_object_integer *type;
		struct acpi_object_integer *code;
		struct acpi_object_integer *argument;

	} fatal;

	struct {
		union acpi_operand_object *source;
		struct acpi_object_integer *index;
		union acpi_operand_object *target;

	} index;

	struct {
		union acpi_operand_object *source;
		struct acpi_object_integer *index;
		struct acpi_object_integer *length;
		union acpi_operand_object *target;

	} mid;
};

 
struct acpi_evaluate_info {
	 

	struct acpi_namespace_node *prefix_node;	 
	const char *relative_pathname;	 
	union acpi_operand_object **parameters;	 

	struct acpi_namespace_node *node;	 
	union acpi_operand_object *obj_desc;	 
	char *full_pathname;	 

	const union acpi_predefined_info *predefined;	 
	union acpi_operand_object *return_object;	 
	union acpi_operand_object *parent_package;	 

	u32 return_flags;	 
	u32 return_btype;	 
	u16 param_count;	 
	u16 node_flags;		 
	u8 pass_number;		 
	u8 return_object_type;	 
	u8 flags;		 
};

 

#define ACPI_IGNORE_RETURN_VALUE    1

 

#define ACPI_OBJECT_REPAIRED        1
#define ACPI_OBJECT_WRAPPED         2

 

struct acpi_device_walk_info {
	struct acpi_table_desc *table_desc;
	struct acpi_evaluate_info *evaluate_info;
	u32 device_count;
	u32 num_STA;
	u32 num_INI;
};

 

struct acpi_region_walk_info {
	u32 debug_level;
	u32 count;
	acpi_owner_id owner_id;
	u8 display_type;
	u32 address_space_id;
};

 

struct acpi_walk_info {
	u32 debug_level;
	u32 count;
	acpi_owner_id owner_id;
	u8 display_type;
};

 

#define ACPI_DISPLAY_SUMMARY        (u8) 0
#define ACPI_DISPLAY_OBJECTS        (u8) 1
#define ACPI_DISPLAY_MASK           (u8) 1

#define ACPI_DISPLAY_SHORT          (u8) 2

#endif
