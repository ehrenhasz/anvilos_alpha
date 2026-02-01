
 

#include <acpi/acpi.h>
#include "accommon.h"
#include "acdebug.h"

#ifdef ACPI_APPLICATION
#include "acapps.h"
#endif

#define _COMPONENT          ACPI_CA_DEBUGGER
ACPI_MODULE_NAME("dbinput")

 
static u32 acpi_db_get_line(char *input_buffer);

static u32 acpi_db_match_command(char *user_command);

static void acpi_db_display_command_info(const char *command, u8 display_all);

static void acpi_db_display_help(char *command);

static u8
acpi_db_match_command_help(const char *command,
			   const struct acpi_db_command_help *help);

 
enum acpi_ex_debugger_commands {
	CMD_NOT_FOUND = 0,
	CMD_NULL,
	CMD_ALL,
	CMD_ALLOCATIONS,
	CMD_ARGS,
	CMD_ARGUMENTS,
	CMD_BREAKPOINT,
	CMD_BUSINFO,
	CMD_CALL,
	CMD_DEBUG,
	CMD_DISASSEMBLE,
	CMD_DISASM,
	CMD_DUMP,
	CMD_EVALUATE,
	CMD_EXECUTE,
	CMD_EXIT,
	CMD_FIELDS,
	CMD_FIND,
	CMD_GO,
	CMD_HANDLERS,
	CMD_HELP,
	CMD_HELP2,
	CMD_HISTORY,
	CMD_HISTORY_EXE,
	CMD_HISTORY_LAST,
	CMD_INFORMATION,
	CMD_INTEGRITY,
	CMD_INTO,
	CMD_LEVEL,
	CMD_LIST,
	CMD_LOCALS,
	CMD_LOCKS,
	CMD_METHODS,
	CMD_NAMESPACE,
	CMD_NOTIFY,
	CMD_OBJECTS,
	CMD_OSI,
	CMD_OWNER,
	CMD_PATHS,
	CMD_PREDEFINED,
	CMD_PREFIX,
	CMD_QUIT,
	CMD_REFERENCES,
	CMD_RESOURCES,
	CMD_RESULTS,
	CMD_SET,
	CMD_STATS,
	CMD_STOP,
	CMD_TABLES,
	CMD_TEMPLATE,
	CMD_TRACE,
	CMD_TREE,
	CMD_TYPE,
#ifdef ACPI_APPLICATION
	CMD_ENABLEACPI,
	CMD_EVENT,
	CMD_GPE,
	CMD_GPES,
	CMD_SCI,
	CMD_SLEEP,

	CMD_CLOSE,
	CMD_LOAD,
	CMD_OPEN,
	CMD_UNLOAD,

	CMD_TERMINATE,
	CMD_BACKGROUND,
	CMD_THREADS,

	CMD_TEST,
	CMD_INTERRUPT,
#endif
};

#define CMD_FIRST_VALID     2

 

static const struct acpi_db_command_info acpi_gbl_db_commands[] = {
	{"<NOT FOUND>", 0},
	{"<NULL>", 0},
	{"ALL", 1},
	{"ALLOCATIONS", 0},
	{"ARGS", 0},
	{"ARGUMENTS", 0},
	{"BREAKPOINT", 1},
	{"BUSINFO", 0},
	{"CALL", 0},
	{"DEBUG", 1},
	{"DISASSEMBLE", 1},
	{"DISASM", 1},
	{"DUMP", 1},
	{"EVALUATE", 1},
	{"EXECUTE", 1},
	{"EXIT", 0},
	{"FIELDS", 1},
	{"FIND", 1},
	{"GO", 0},
	{"HANDLERS", 0},
	{"HELP", 0},
	{"?", 0},
	{"HISTORY", 0},
	{"!", 1},
	{"!!", 0},
	{"INFORMATION", 0},
	{"INTEGRITY", 0},
	{"INTO", 0},
	{"LEVEL", 0},
	{"LIST", 0},
	{"LOCALS", 0},
	{"LOCKS", 0},
	{"METHODS", 0},
	{"NAMESPACE", 0},
	{"NOTIFY", 2},
	{"OBJECTS", 0},
	{"OSI", 0},
	{"OWNER", 1},
	{"PATHS", 0},
	{"PREDEFINED", 0},
	{"PREFIX", 0},
	{"QUIT", 0},
	{"REFERENCES", 1},
	{"RESOURCES", 0},
	{"RESULTS", 0},
	{"SET", 3},
	{"STATS", 1},
	{"STOP", 0},
	{"TABLES", 0},
	{"TEMPLATE", 1},
	{"TRACE", 1},
	{"TREE", 0},
	{"TYPE", 1},
#ifdef ACPI_APPLICATION
	{"ENABLEACPI", 0},
	{"EVENT", 1},
	{"GPE", 1},
	{"GPES", 0},
	{"SCI", 0},
	{"SLEEP", 0},

	{"CLOSE", 0},
	{"LOAD", 1},
	{"OPEN", 1},
	{"UNLOAD", 1},

	{"TERMINATE", 0},
	{"BACKGROUND", 1},
	{"THREADS", 3},

	{"TEST", 1},
	{"INTERRUPT", 1},
#endif
	{NULL, 0}
};

 
static const struct acpi_db_command_help acpi_gbl_db_command_help[] = {
	{0, "\nNamespace Access:", "\n"},
	{1, "  Businfo", "Display system bus info\n"},
	{1, "  Disassemble <Method>", "Disassemble a control method\n"},
	{1, "  Find <AcpiName> (? is wildcard)",
	 "Find ACPI name(s) with wildcards\n"},
	{1, "  Integrity", "Validate namespace integrity\n"},
	{1, "  Methods", "Display list of loaded control methods\n"},
	{1, "  Fields <AddressSpaceId>",
	 "Display list of loaded field units by space ID\n"},
	{1, "  Namespace [Object] [Depth]",
	 "Display loaded namespace tree/subtree\n"},
	{1, "  Notify <Object> <Value>", "Send a notification on Object\n"},
	{1, "  Objects [ObjectType]",
	 "Display summary of all objects or just given type\n"},
	{1, "  Owner <OwnerId> [Depth]",
	 "Display loaded namespace by object owner\n"},
	{1, "  Paths", "Display full pathnames of namespace objects\n"},
	{1, "  Predefined", "Check all predefined names\n"},
	{1, "  Prefix [<Namepath>]", "Set or Get current execution prefix\n"},
	{1, "  References <Addr>", "Find all references to object at addr\n"},
	{1, "  Resources [DeviceName]",
	 "Display Device resources (no arg = all devices)\n"},
	{1, "  Set N <NamedObject> <Value>", "Set value for named integer\n"},
	{1, "  Template <Object>", "Format/dump a Buffer/ResourceTemplate\n"},
	{1, "  Type <Object>", "Display object type\n"},

	{0, "\nControl Method Execution:", "\n"},
	{1, "  All <NameSeg>", "Evaluate all objects named NameSeg\n"},
	{1, "  Evaluate <Namepath> [Arguments]",
	 "Evaluate object or control method\n"},
	{1, "  Execute <Namepath> [Arguments]", "Synonym for Evaluate\n"},
#ifdef ACPI_APPLICATION
	{1, "  Background <Namepath> [Arguments]",
	 "Evaluate object/method in a separate thread\n"},
	{1, "  Thread <Threads><Loops><NamePath>",
	 "Spawn threads to execute method(s)\n"},
#endif
	{1, "  Debug <Namepath> [Arguments]", "Single-Step a control method\n"},
	{7, "  [Arguments] formats:", "Control method argument formats\n"},
	{1, "     Hex Integer", "Integer\n"},
	{1, "     \"Ascii String\"", "String\n"},
	{1, "     (Hex Byte List)", "Buffer\n"},
	{1, "         (01 42 7A BF)", "Buffer example (4 bytes)\n"},
	{1, "     [Package Element List]", "Package\n"},
	{1, "         [0x01 0x1234 \"string\"]",
	 "Package example (3 elements)\n"},

	{0, "\nMiscellaneous:", "\n"},
	{1, "  Allocations", "Display list of current memory allocations\n"},
	{2, "  Dump <Address>|<Namepath>", "\n"},
	{0, "       [Byte|Word|Dword|Qword]",
	 "Display ACPI objects or memory\n"},
	{1, "  Handlers", "Info about global handlers\n"},
	{1, "  Help [Command]", "This help screen or individual command\n"},
	{1, "  History", "Display command history buffer\n"},
	{1, "  Level <DebugLevel>] [console]",
	 "Get/Set debug level for file or console\n"},
	{1, "  Locks", "Current status of internal mutexes\n"},
	{1, "  Osi [Install|Remove <name>]",
	 "Display or modify global _OSI list\n"},
	{1, "  Quit or Exit", "Exit this command\n"},
	{8, "  Stats <SubCommand>",
	 "Display namespace and memory statistics\n"},
	{1, "     Allocations", "Display list of current memory allocations\n"},
	{1, "     Memory", "Dump internal memory lists\n"},
	{1, "     Misc", "Namespace search and mutex stats\n"},
	{1, "     Objects", "Summary of namespace objects\n"},
	{1, "     Sizes", "Sizes for each of the internal objects\n"},
	{1, "     Stack", "Display CPU stack usage\n"},
	{1, "     Tables", "Info about current ACPI table(s)\n"},
	{1, "  Tables", "Display info about loaded ACPI tables\n"},
#ifdef ACPI_APPLICATION
	{1, "  Terminate", "Delete namespace and all internal objects\n"},
#endif
	{1, "  ! <CommandNumber>", "Execute command from history buffer\n"},
	{1, "  !!", "Execute last command again\n"},

	{0, "\nMethod and Namespace Debugging:", "\n"},
	{5, "  Trace <State> [<Namepath>] [Once]",
	 "Trace control method execution\n"},
	{1, "     Enable", "Enable all messages\n"},
	{1, "     Disable", "Disable tracing\n"},
	{1, "     Method", "Enable method execution messages\n"},
	{1, "     Opcode", "Enable opcode execution messages\n"},
	{3, "  Test <TestName>", "Invoke a debug test\n"},
	{1, "     Objects", "Read/write/compare all namespace data objects\n"},
	{1, "     Predefined",
	 "Validate all ACPI predefined names (_STA, etc.)\n"},
	{1, "  Execute predefined",
	 "Execute all predefined (public) methods\n"},

	{0, "\nControl Method Single-Step Execution:", "\n"},
	{1, "  Arguments (or Args)", "Display method arguments\n"},
	{1, "  Breakpoint <AmlOffset>", "Set an AML execution breakpoint\n"},
	{1, "  Call", "Run to next control method invocation\n"},
	{1, "  Go", "Allow method to run to completion\n"},
	{1, "  Information", "Display info about the current method\n"},
	{1, "  Into", "Step into (not over) a method call\n"},
	{1, "  List [# of Aml Opcodes]", "Display method ASL statements\n"},
	{1, "  Locals", "Display method local variables\n"},
	{1, "  Results", "Display method result stack\n"},
	{1, "  Set <A|L> <#> <Value>", "Set method data (Arguments/Locals)\n"},
	{1, "  Stop", "Terminate control method\n"},
	{1, "  Tree", "Display control method calling tree\n"},
	{1, "  <Enter>", "Single step next AML opcode (over calls)\n"},

#ifdef ACPI_APPLICATION
	{0, "\nFile Operations:", "\n"},
	{1, "  Close", "Close debug output file\n"},
	{1, "  Load <Input Filename>", "Load ACPI table from a file\n"},
	{1, "  Open <Output Filename>", "Open a file for debug output\n"},
	{1, "  Unload <Namepath>",
	 "Unload an ACPI table via namespace object\n"},

	{0, "\nHardware Simulation:", "\n"},
	{1, "  EnableAcpi", "Enable ACPI (hardware) mode\n"},
	{1, "  Event <F|G> <Value>", "Generate AcpiEvent (Fixed/GPE)\n"},
	{1, "  Gpe <GpeNum> [GpeBlockDevice]", "Simulate a GPE\n"},
	{1, "  Gpes", "Display info on all GPE devices\n"},
	{1, "  Sci", "Generate an SCI\n"},
	{1, "  Sleep [SleepState]", "Simulate sleep/wake sequence(s) (0-5)\n"},
	{1, "  Interrupt <GSIV>", "Simulate an interrupt\n"},
#endif
	{0, NULL, NULL}
};

 

static u8
acpi_db_match_command_help(const char *command,
			   const struct acpi_db_command_help *help)
{
	char *invocation = help->invocation;
	u32 line_count;

	 

	if (*invocation != ' ') {
		return (FALSE);
	}

	while (*invocation == ' ') {
		invocation++;
	}

	 

	while ((*command) && (*invocation) && (*invocation != ' ')) {
		if (tolower((int)*command) != tolower((int)*invocation)) {
			return (FALSE);
		}

		invocation++;
		command++;
	}

	 

	line_count = help->line_count;
	while (line_count) {
		acpi_os_printf("%-38s : %s", help->invocation,
			       help->description);
		help++;
		line_count--;
	}

	return (TRUE);
}

 

static void acpi_db_display_command_info(const char *command, u8 display_all)
{
	const struct acpi_db_command_help *next;
	u8 matched;

	next = acpi_gbl_db_command_help;
	while (next->invocation) {
		matched = acpi_db_match_command_help(command, next);
		if (!display_all && matched) {
			return;
		}

		next++;
	}
}

 

static void acpi_db_display_help(char *command)
{
	const struct acpi_db_command_help *next = acpi_gbl_db_command_help;

	if (!command) {

		 

		acpi_os_printf("\nSummary of AML Debugger Commands\n\n");

		while (next->invocation) {
			acpi_os_printf("%-38s%s", next->invocation,
				       next->description);
			next++;
		}
		acpi_os_printf("\n");

	} else {
		 

		acpi_db_display_command_info(command, TRUE);
	}
}

 

char *acpi_db_get_next_token(char *string,
			     char **next, acpi_object_type *return_type)
{
	char *start;
	u32 depth;
	acpi_object_type type = ACPI_TYPE_INTEGER;

	 

	if (!string || !(*string)) {
		return (NULL);
	}

	 

	while (*string && isspace((int)*string)) {
		string++;
	}

	if (!(*string)) {
		return (NULL);
	}

	switch (*string) {
	case '"':

		 

		string++;
		start = string;
		type = ACPI_TYPE_STRING;

		 

		while (*string && (*string != '"')) {
			string++;
		}
		break;

	case '(':

		 

		string++;
		start = string;
		type = ACPI_TYPE_BUFFER;

		 

		while (*string && (*string != ')')) {
			string++;
		}
		break;

	case '{':

		 

		string++;
		start = string;
		type = ACPI_TYPE_FIELD_UNIT;

		 

		while (*string && (*string != '}')) {
			string++;
		}
		break;

	case '[':

		 

		string++;
		depth = 1;
		start = string;
		type = ACPI_TYPE_PACKAGE;

		 

		while (*string) {

			 

			if (*string == '"') {
				 

				string++;
				while (*string && (*string != '"')) {
					string++;
				}
				if (!(*string)) {
					break;
				}
			} else if (*string == '[') {
				depth++;	 
			} else if (*string == ']') {
				depth--;
				if (depth == 0) {	 
					break;
				}
			}

			string++;
		}
		break;

	default:

		start = string;

		 

		while (*string && !isspace((int)*string)) {
			string++;
		}
		break;
	}

	if (!(*string)) {
		*next = NULL;
	} else {
		*string = 0;
		*next = string + 1;
	}

	*return_type = type;
	return (start);
}

 

static u32 acpi_db_get_line(char *input_buffer)
{
	u32 i;
	u32 count;
	char *next;
	char *this;

	if (acpi_ut_safe_strcpy
	    (acpi_gbl_db_parsed_buf, sizeof(acpi_gbl_db_parsed_buf),
	     input_buffer)) {
		acpi_os_printf
		    ("Buffer overflow while parsing input line (max %u characters)\n",
		     (u32)sizeof(acpi_gbl_db_parsed_buf));
		return (0);
	}

	this = acpi_gbl_db_parsed_buf;
	for (i = 0; i < ACPI_DEBUGGER_MAX_ARGS; i++) {
		acpi_gbl_db_args[i] = acpi_db_get_next_token(this, &next,
							     &acpi_gbl_db_arg_types
							     [i]);
		if (!acpi_gbl_db_args[i]) {
			break;
		}

		this = next;
	}

	 

	acpi_ut_strupr(acpi_gbl_db_args[0]);

	count = i;
	if (count) {
		count--;	 
	}

	return (count);
}

 

static u32 acpi_db_match_command(char *user_command)
{
	u32 i;

	if (!user_command || user_command[0] == 0) {
		return (CMD_NULL);
	}

	for (i = CMD_FIRST_VALID; acpi_gbl_db_commands[i].name; i++) {
		if (strstr
		    (ACPI_CAST_PTR(char, acpi_gbl_db_commands[i].name),
		     user_command) == acpi_gbl_db_commands[i].name) {
			return (i);
		}
	}

	 

	return (CMD_NOT_FOUND);
}

 

acpi_status
acpi_db_command_dispatch(char *input_buffer,
			 struct acpi_walk_state *walk_state,
			 union acpi_parse_object *op)
{
	u32 temp;
	u64 temp64;
	u32 command_index;
	u32 param_count;
	char *command_line;
	acpi_status status = AE_CTRL_TRUE;

	 

	if (acpi_gbl_db_terminate_loop) {
		return (AE_CTRL_TERMINATE);
	}

	 

	param_count = acpi_db_get_line(input_buffer);
	command_index = acpi_db_match_command(acpi_gbl_db_args[0]);

	 
	if (command_index != CMD_HISTORY_LAST) {
		acpi_db_add_to_history(input_buffer);
	}

	 

	if (param_count < acpi_gbl_db_commands[command_index].min_args) {
		acpi_os_printf
		    ("%u parameters entered, [%s] requires %u parameters\n",
		     param_count, acpi_gbl_db_commands[command_index].name,
		     acpi_gbl_db_commands[command_index].min_args);

		acpi_db_display_command_info(acpi_gbl_db_commands
					     [command_index].name, FALSE);
		return (AE_CTRL_TRUE);
	}

	 

	switch (command_index) {
	case CMD_NULL:

		if (op) {
			return (AE_OK);
		}
		break;

	case CMD_ALL:

		acpi_os_printf("Executing all objects with NameSeg: %s\n",
			       acpi_gbl_db_args[1]);
		acpi_db_execute(acpi_gbl_db_args[1], &acpi_gbl_db_args[2],
				&acpi_gbl_db_arg_types[2],
				EX_NO_SINGLE_STEP | EX_ALL);
		break;

	case CMD_ALLOCATIONS:

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
		acpi_ut_dump_allocations((u32)-1, NULL);
#endif
		break;

	case CMD_ARGS:
	case CMD_ARGUMENTS:

		acpi_db_display_arguments();
		break;

	case CMD_BREAKPOINT:

		acpi_db_set_method_breakpoint(acpi_gbl_db_args[1], walk_state,
					      op);
		break;

	case CMD_BUSINFO:

		acpi_db_get_bus_info();
		break;

	case CMD_CALL:

		acpi_db_set_method_call_breakpoint(op);
		status = AE_OK;
		break;

	case CMD_DEBUG:

		acpi_db_execute(acpi_gbl_db_args[1],
				&acpi_gbl_db_args[2], &acpi_gbl_db_arg_types[2],
				EX_SINGLE_STEP);
		break;

	case CMD_DISASSEMBLE:
	case CMD_DISASM:

#ifdef ACPI_DISASSEMBLER
		(void)acpi_db_disassemble_method(acpi_gbl_db_args[1]);
#else
		acpi_os_printf
		    ("The AML Disassembler is not configured/present\n");
#endif
		break;

	case CMD_DUMP:

		acpi_db_decode_and_display_object(acpi_gbl_db_args[1],
						  acpi_gbl_db_args[2]);
		break;

	case CMD_EVALUATE:
	case CMD_EXECUTE:

		acpi_db_execute(acpi_gbl_db_args[1],
				&acpi_gbl_db_args[2], &acpi_gbl_db_arg_types[2],
				EX_NO_SINGLE_STEP);
		break;

	case CMD_FIND:

		status = acpi_db_find_name_in_namespace(acpi_gbl_db_args[1]);
		break;

	case CMD_FIELDS:

		status = acpi_ut_strtoul64(acpi_gbl_db_args[1], &temp64);

		if (ACPI_FAILURE(status)
		    || temp64 >= ACPI_NUM_PREDEFINED_REGIONS) {
			acpi_os_printf
			    ("Invalid address space ID: must be between 0 and %u inclusive\n",
			     ACPI_NUM_PREDEFINED_REGIONS - 1);
			return (AE_OK);
		}

		status = acpi_db_display_fields((u32)temp64);
		break;

	case CMD_GO:

		acpi_gbl_cm_single_step = FALSE;
		return (AE_OK);

	case CMD_HANDLERS:

		acpi_db_display_handlers();
		break;

	case CMD_HELP:
	case CMD_HELP2:

		acpi_db_display_help(acpi_gbl_db_args[1]);
		break;

	case CMD_HISTORY:

		acpi_db_display_history();
		break;

	case CMD_HISTORY_EXE:	 

		command_line = acpi_db_get_from_history(acpi_gbl_db_args[1]);
		if (!command_line) {
			return (AE_CTRL_TRUE);
		}

		status = acpi_db_command_dispatch(command_line, walk_state, op);
		return (status);

	case CMD_HISTORY_LAST:	 

		command_line = acpi_db_get_from_history(NULL);
		if (!command_line) {
			return (AE_CTRL_TRUE);
		}

		status = acpi_db_command_dispatch(command_line, walk_state, op);
		return (status);

	case CMD_INFORMATION:

		acpi_db_display_method_info(op);
		break;

	case CMD_INTEGRITY:

		acpi_db_check_integrity();
		break;

	case CMD_INTO:

		if (op) {
			acpi_gbl_cm_single_step = TRUE;
			return (AE_OK);
		}
		break;

	case CMD_LEVEL:

		if (param_count == 0) {
			acpi_os_printf
			    ("Current debug level for file output is:    %8.8X\n",
			     acpi_gbl_db_debug_level);
			acpi_os_printf
			    ("Current debug level for console output is: %8.8X\n",
			     acpi_gbl_db_console_debug_level);
		} else if (param_count == 2) {
			temp = acpi_gbl_db_console_debug_level;
			acpi_gbl_db_console_debug_level =
			    strtoul(acpi_gbl_db_args[1], NULL, 16);
			acpi_os_printf
			    ("Debug Level for console output was %8.8X, now %8.8X\n",
			     temp, acpi_gbl_db_console_debug_level);
		} else {
			temp = acpi_gbl_db_debug_level;
			acpi_gbl_db_debug_level =
			    strtoul(acpi_gbl_db_args[1], NULL, 16);
			acpi_os_printf
			    ("Debug Level for file output was %8.8X, now %8.8X\n",
			     temp, acpi_gbl_db_debug_level);
		}
		break;

	case CMD_LIST:

#ifdef ACPI_DISASSEMBLER
		acpi_db_disassemble_aml(acpi_gbl_db_args[1], op);
#else
		acpi_os_printf
		    ("The AML Disassembler is not configured/present\n");
#endif
		break;

	case CMD_LOCKS:

		acpi_db_display_locks();
		break;

	case CMD_LOCALS:

		acpi_db_display_locals();
		break;

	case CMD_METHODS:

		status = acpi_db_display_objects("METHOD", acpi_gbl_db_args[1]);
		break;

	case CMD_NAMESPACE:

		acpi_db_dump_namespace(acpi_gbl_db_args[1],
				       acpi_gbl_db_args[2]);
		break;

	case CMD_NOTIFY:

		temp = strtoul(acpi_gbl_db_args[2], NULL, 0);
		acpi_db_send_notify(acpi_gbl_db_args[1], temp);
		break;

	case CMD_OBJECTS:

		acpi_ut_strupr(acpi_gbl_db_args[1]);
		status =
		    acpi_db_display_objects(acpi_gbl_db_args[1],
					    acpi_gbl_db_args[2]);
		break;

	case CMD_OSI:

		acpi_db_display_interfaces(acpi_gbl_db_args[1],
					   acpi_gbl_db_args[2]);
		break;

	case CMD_OWNER:

		acpi_db_dump_namespace_by_owner(acpi_gbl_db_args[1],
						acpi_gbl_db_args[2]);
		break;

	case CMD_PATHS:

		acpi_db_dump_namespace_paths();
		break;

	case CMD_PREFIX:

		acpi_db_set_scope(acpi_gbl_db_args[1]);
		break;

	case CMD_REFERENCES:

		acpi_db_find_references(acpi_gbl_db_args[1]);
		break;

	case CMD_RESOURCES:

		acpi_db_display_resources(acpi_gbl_db_args[1]);
		break;

	case CMD_RESULTS:

		acpi_db_display_results();
		break;

	case CMD_SET:

		acpi_db_set_method_data(acpi_gbl_db_args[1],
					acpi_gbl_db_args[2],
					acpi_gbl_db_args[3]);
		break;

	case CMD_STATS:

		status = acpi_db_display_statistics(acpi_gbl_db_args[1]);
		break;

	case CMD_STOP:

		return (AE_NOT_IMPLEMENTED);

	case CMD_TABLES:

		acpi_db_display_table_info(acpi_gbl_db_args[1]);
		break;

	case CMD_TEMPLATE:

		acpi_db_display_template(acpi_gbl_db_args[1]);
		break;

	case CMD_TRACE:

		acpi_db_trace(acpi_gbl_db_args[1], acpi_gbl_db_args[2],
			      acpi_gbl_db_args[3]);
		break;

	case CMD_TREE:

		acpi_db_display_calling_tree();
		break;

	case CMD_TYPE:

		acpi_db_display_object_type(acpi_gbl_db_args[1]);
		break;

#ifdef ACPI_APPLICATION

		 

	case CMD_ENABLEACPI:
#if (!ACPI_REDUCED_HARDWARE)

		status = acpi_enable();
		if (ACPI_FAILURE(status)) {
			acpi_os_printf("AcpiEnable failed (Status=%X)\n",
				       status);
			return (status);
		}
#endif				 
		break;

	case CMD_EVENT:

		acpi_os_printf("Event command not implemented\n");
		break;

	case CMD_INTERRUPT:

		acpi_db_generate_interrupt(acpi_gbl_db_args[1]);
		break;

	case CMD_GPE:

		acpi_db_generate_gpe(acpi_gbl_db_args[1], acpi_gbl_db_args[2]);
		break;

	case CMD_GPES:

		acpi_db_display_gpes();
		break;

	case CMD_SCI:

		acpi_db_generate_sci();
		break;

	case CMD_SLEEP:

		status = acpi_db_sleep(acpi_gbl_db_args[1]);
		break;

		 

	case CMD_CLOSE:

		acpi_db_close_debug_file();
		break;

	case CMD_LOAD:{
			struct acpi_new_table_desc *list_head = NULL;

			status =
			    ac_get_all_tables_from_file(acpi_gbl_db_args[1],
							ACPI_GET_ALL_TABLES,
							&list_head);
			if (ACPI_SUCCESS(status)) {
				acpi_db_load_tables(list_head);
			}
		}
		break;

	case CMD_OPEN:

		acpi_db_open_debug_file(acpi_gbl_db_args[1]);
		break;

		 

	case CMD_TERMINATE:

		acpi_db_set_output_destination(ACPI_DB_REDIRECTABLE_OUTPUT);
		acpi_ut_subsystem_shutdown();

		 

		acpi_gbl_db_terminate_loop = TRUE;
		 
		break;

	case CMD_BACKGROUND:

		acpi_db_create_execution_thread(acpi_gbl_db_args[1],
						&acpi_gbl_db_args[2],
						&acpi_gbl_db_arg_types[2]);
		break;

	case CMD_THREADS:

		acpi_db_create_execution_threads(acpi_gbl_db_args[1],
						 acpi_gbl_db_args[2],
						 acpi_gbl_db_args[3]);
		break;

		 

	case CMD_PREDEFINED:

		acpi_db_check_predefined_names();
		break;

	case CMD_TEST:

		acpi_db_execute_test(acpi_gbl_db_args[1]);
		break;

	case CMD_UNLOAD:

		acpi_db_unload_acpi_table(acpi_gbl_db_args[1]);
		break;
#endif

	case CMD_EXIT:
	case CMD_QUIT:

		if (op) {
			acpi_os_printf("Method execution terminated\n");
			return (AE_CTRL_TERMINATE);
		}

		if (!acpi_gbl_db_output_to_file) {
			acpi_dbg_level = ACPI_DEBUG_DEFAULT;
		}
#ifdef ACPI_APPLICATION
		acpi_db_close_debug_file();
#endif
		acpi_gbl_db_terminate_loop = TRUE;
		return (AE_CTRL_TERMINATE);

	case CMD_NOT_FOUND:
	default:

		acpi_os_printf("%s: unknown command\n", acpi_gbl_db_args[0]);
		return (AE_CTRL_TRUE);
	}

	if (ACPI_SUCCESS(status)) {
		status = AE_CTRL_TRUE;
	}

	return (status);
}

 

void ACPI_SYSTEM_XFACE acpi_db_execute_thread(void *context)
{

	(void)acpi_db_user_commands();
	acpi_gbl_db_threads_terminated = TRUE;
}

 

acpi_status acpi_db_user_commands(void)
{
	acpi_status status = AE_OK;

	acpi_os_printf("\n");

	 

	while (!acpi_gbl_db_terminate_loop) {

		 

		status = acpi_os_wait_command_ready();
		if (ACPI_FAILURE(status)) {
			break;
		}

		 

		acpi_gbl_method_executing = FALSE;
		acpi_gbl_step_to_next_call = FALSE;

		(void)acpi_db_command_dispatch(acpi_gbl_db_line_buf, NULL,
					       NULL);

		 

		status = acpi_os_notify_command_complete();
		if (ACPI_FAILURE(status)) {
			break;
		}
	}

	if (ACPI_FAILURE(status) && status != AE_CTRL_TERMINATE) {
		ACPI_EXCEPTION((AE_INFO, status, "While parsing command line"));
	}
	return (status);
}
