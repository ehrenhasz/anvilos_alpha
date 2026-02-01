
 

#include "gcc-common.h"

__visible int plugin_is_GPL_compatible;

static int track_frame_size = -1;
static bool build_for_x86 = false;
static const char track_function[] = "stackleak_track_stack";
static bool disable = false;
static bool verbose = false;

 
static GTY(()) tree track_function_decl;

static struct plugin_info stackleak_plugin_info = {
	.version = PLUGIN_VERSION,
	.help = "track-min-size=nn\ttrack stack for functions with a stack frame size >= nn bytes\n"
		"arch=target_arch\tspecify target build arch\n"
		"disable\t\tdo not activate the plugin\n"
		"verbose\t\tprint info about the instrumentation\n"
};

static void add_stack_tracking_gcall(gimple_stmt_iterator *gsi, bool after)
{
	gimple stmt;
	gcall *gimple_call;
	cgraph_node_ptr node;
	basic_block bb;

	 
	stmt = gimple_build_call(track_function_decl, 0);
	gimple_call = as_a_gcall(stmt);
	if (after)
		gsi_insert_after(gsi, gimple_call, GSI_CONTINUE_LINKING);
	else
		gsi_insert_before(gsi, gimple_call, GSI_SAME_STMT);

	 
	bb = gimple_bb(gimple_call);
	node = cgraph_get_create_node(track_function_decl);
	gcc_assert(node);
	cgraph_create_edge(cgraph_get_node(current_function_decl), node,
			gimple_call, bb->count,
			compute_call_stmt_bb_frequency(current_function_decl, bb));
}

static bool is_alloca(gimple stmt)
{
	if (gimple_call_builtin_p(stmt, BUILT_IN_ALLOCA))
		return true;

	if (gimple_call_builtin_p(stmt, BUILT_IN_ALLOCA_WITH_ALIGN))
		return true;

	return false;
}

static tree get_current_stack_pointer_decl(void)
{
	varpool_node_ptr node;

	FOR_EACH_VARIABLE(node) {
		tree var = NODE_DECL(node);
		tree name = DECL_NAME(var);

		if (DECL_NAME_LENGTH(var) != sizeof("current_stack_pointer") - 1)
			continue;

		if (strcmp(IDENTIFIER_POINTER(name), "current_stack_pointer"))
			continue;

		return var;
	}

	if (verbose) {
		fprintf(stderr, "stackleak: missing current_stack_pointer in %s()\n",
			DECL_NAME_POINTER(current_function_decl));
	}
	return NULL_TREE;
}

static void add_stack_tracking_gasm(gimple_stmt_iterator *gsi, bool after)
{
	gasm *asm_call = NULL;
	tree sp_decl, input;
	vec<tree, va_gc> *inputs = NULL;

	 
	gcc_assert(build_for_x86);

	 
	sp_decl = get_current_stack_pointer_decl();
	if (sp_decl == NULL_TREE) {
		add_stack_tracking_gcall(gsi, after);
		return;
	}
	input = build_tree_list(NULL_TREE, build_const_char_string(2, "r"));
	input = chainon(NULL_TREE, build_tree_list(input, sp_decl));
	vec_safe_push(inputs, input);
	asm_call = gimple_build_asm_vec("call stackleak_track_stack",
					inputs, NULL, NULL, NULL);
	gimple_asm_set_volatile(asm_call, true);
	if (after)
		gsi_insert_after(gsi, asm_call, GSI_CONTINUE_LINKING);
	else
		gsi_insert_before(gsi, asm_call, GSI_SAME_STMT);
	update_stmt(asm_call);
}

static void add_stack_tracking(gimple_stmt_iterator *gsi, bool after)
{
	 
	if (lookup_attribute_spec(get_identifier("no_caller_saved_registers")))
		add_stack_tracking_gasm(gsi, after);
	else
		add_stack_tracking_gcall(gsi, after);
}

 
static unsigned int stackleak_instrument_execute(void)
{
	basic_block bb, entry_bb;
	bool prologue_instrumented = false, is_leaf = true;
	gimple_stmt_iterator gsi = { 0 };

	 
	gcc_assert(single_succ_p(ENTRY_BLOCK_PTR_FOR_FN(cfun)));
	entry_bb = single_succ(ENTRY_BLOCK_PTR_FOR_FN(cfun));

	 
	FOR_EACH_BB_FN(bb, cfun) {
		for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
			gimple stmt;

			stmt = gsi_stmt(gsi);

			 
			if (is_gimple_call(stmt))
				is_leaf = false;

			if (!is_alloca(stmt))
				continue;

			if (verbose) {
				fprintf(stderr, "stackleak: be careful, alloca() in %s()\n",
					DECL_NAME_POINTER(current_function_decl));
			}

			 
			add_stack_tracking(&gsi, true);
			if (bb == entry_bb)
				prologue_instrumented = true;
		}
	}

	if (prologue_instrumented)
		return 0;

	 
	if (is_leaf &&
	    !TREE_PUBLIC(current_function_decl) &&
	    DECL_DECLARED_INLINE_P(current_function_decl)) {
		return 0;
	}

	if (is_leaf &&
	    !strncmp(IDENTIFIER_POINTER(DECL_NAME(current_function_decl)),
		     "_paravirt_", 10)) {
		return 0;
	}

	 
	bb = entry_bb;
	if (!single_pred_p(bb)) {
		 
		split_edge(single_succ_edge(ENTRY_BLOCK_PTR_FOR_FN(cfun)));
		gcc_assert(single_succ_p(ENTRY_BLOCK_PTR_FOR_FN(cfun)));
		bb = single_succ(ENTRY_BLOCK_PTR_FOR_FN(cfun));
	}
	gsi = gsi_after_labels(bb);
	add_stack_tracking(&gsi, false);

	return 0;
}

static bool large_stack_frame(void)
{
#if BUILDING_GCC_VERSION >= 8000
	return maybe_ge(get_frame_size(), track_frame_size);
#else
	return (get_frame_size() >= track_frame_size);
#endif
}

static void remove_stack_tracking_gcall(void)
{
	rtx_insn *insn, *next;

	 
	for (insn = get_insns(); insn; insn = next) {
		rtx body;

		next = NEXT_INSN(insn);

		 
		if (!CALL_P(insn))
			continue;

		 
		body = PATTERN(insn);

		if (GET_CODE(body) == PARALLEL)
			body = XVECEXP(body, 0, 0);

		if (GET_CODE(body) != CALL)
			continue;

		 
		body = XEXP(body, 0);
		if (GET_CODE(body) != MEM)
			continue;

		body = XEXP(body, 0);
		if (GET_CODE(body) != SYMBOL_REF)
			continue;

		if (SYMBOL_REF_DECL(body) != track_function_decl)
			continue;

		 
		delete_insn_and_edges(insn);
#if BUILDING_GCC_VERSION < 8000
		if (GET_CODE(next) == NOTE &&
		    NOTE_KIND(next) == NOTE_INSN_CALL_ARG_LOCATION) {
			insn = next;
			next = NEXT_INSN(insn);
			delete_insn_and_edges(insn);
		}
#endif
	}
}

static bool remove_stack_tracking_gasm(void)
{
	bool removed = false;
	rtx_insn *insn, *next;

	 
	gcc_assert(build_for_x86);

	 
	for (insn = get_insns(); insn; insn = next) {
		rtx body;

		next = NEXT_INSN(insn);

		 
		if (!NONJUMP_INSN_P(insn))
			continue;

		 
		body = PATTERN(insn);

		if (GET_CODE(body) != PARALLEL)
			continue;

		body = XVECEXP(body, 0, 0);

		if (GET_CODE(body) != ASM_OPERANDS)
			continue;

		if (strcmp(ASM_OPERANDS_TEMPLATE(body),
						"call stackleak_track_stack")) {
			continue;
		}

		delete_insn_and_edges(insn);
		gcc_assert(!removed);
		removed = true;
	}

	return removed;
}

 
static unsigned int stackleak_cleanup_execute(void)
{
	const char *fn = DECL_NAME_POINTER(current_function_decl);
	bool removed = false;

	 
	if (cfun->calls_alloca) {
		if (verbose)
			fprintf(stderr, "stackleak: instrument %s(): calls_alloca\n", fn);
		return 0;
	}

	 
	if (large_stack_frame()) {
		if (verbose)
			fprintf(stderr, "stackleak: instrument %s()\n", fn);
		return 0;
	}

	if (lookup_attribute_spec(get_identifier("no_caller_saved_registers")))
		removed = remove_stack_tracking_gasm();

	if (!removed)
		remove_stack_tracking_gcall();

	return 0;
}

 
static inline bool string_equal(tree node, const char *string, int length)
{
	if (TREE_STRING_LENGTH(node) < length)
		return false;
	if (TREE_STRING_LENGTH(node) > length + 1)
		return false;
	if (TREE_STRING_LENGTH(node) == length + 1 &&
	    TREE_STRING_POINTER(node)[length] != '\0')
		return false;
	return !memcmp(TREE_STRING_POINTER(node), string, length);
}
#define STRING_EQUAL(node, str)	string_equal(node, str, strlen(str))

static bool stackleak_gate(void)
{
	tree section;

	section = lookup_attribute("section",
				   DECL_ATTRIBUTES(current_function_decl));
	if (section && TREE_VALUE(section)) {
		section = TREE_VALUE(TREE_VALUE(section));

		if (STRING_EQUAL(section, ".init.text"))
			return false;
		if (STRING_EQUAL(section, ".devinit.text"))
			return false;
		if (STRING_EQUAL(section, ".cpuinit.text"))
			return false;
		if (STRING_EQUAL(section, ".meminit.text"))
			return false;
		if (STRING_EQUAL(section, ".noinstr.text"))
			return false;
		if (STRING_EQUAL(section, ".entry.text"))
			return false;
	}

	return track_frame_size >= 0;
}

 
static void stackleak_start_unit(void *gcc_data __unused,
				 void *user_data __unused)
{
	tree fntype;

	 
	fntype = build_function_type_list(void_type_node, NULL_TREE);
	track_function_decl = build_fn_decl(track_function, fntype);
	DECL_ASSEMBLER_NAME(track_function_decl);  
	TREE_PUBLIC(track_function_decl) = 1;
	TREE_USED(track_function_decl) = 1;
	DECL_EXTERNAL(track_function_decl) = 1;
	DECL_ARTIFICIAL(track_function_decl) = 1;
	DECL_PRESERVE_P(track_function_decl) = 1;
}

 
static bool stackleak_instrument_gate(void)
{
	return stackleak_gate();
}

#define PASS_NAME stackleak_instrument
#define PROPERTIES_REQUIRED PROP_gimple_leh | PROP_cfg
#define TODO_FLAGS_START TODO_verify_ssa | TODO_verify_flow | TODO_verify_stmts
#define TODO_FLAGS_FINISH TODO_verify_ssa | TODO_verify_stmts | TODO_dump_func \
			| TODO_update_ssa | TODO_rebuild_cgraph_edges
#include "gcc-generate-gimple-pass.h"

static bool stackleak_cleanup_gate(void)
{
	return stackleak_gate();
}

#define PASS_NAME stackleak_cleanup
#define TODO_FLAGS_FINISH TODO_dump_func
#include "gcc-generate-rtl-pass.h"

 
__visible int plugin_init(struct plugin_name_args *plugin_info,
			  struct plugin_gcc_version *version)
{
	const char * const plugin_name = plugin_info->base_name;
	const int argc = plugin_info->argc;
	const struct plugin_argument * const argv = plugin_info->argv;
	int i = 0;

	 
	static const struct ggc_root_tab gt_ggc_r_gt_stackleak[] = {
		{
			.base = &track_function_decl,
			.nelt = 1,
			.stride = sizeof(track_function_decl),
			.cb = &gt_ggc_mx_tree_node,
			.pchw = &gt_pch_nx_tree_node
		},
		LAST_GGC_ROOT_TAB
	};

	 
	PASS_INFO(stackleak_instrument, "optimized", 1,
						PASS_POS_INSERT_BEFORE);

	 
	PASS_INFO(stackleak_cleanup, "*free_cfg", 1, PASS_POS_INSERT_BEFORE);

	if (!plugin_default_version_check(version, &gcc_version)) {
		error(G_("incompatible gcc/plugin versions"));
		return 1;
	}

	 
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i].key, "track-min-size")) {
			if (!argv[i].value) {
				error(G_("no value supplied for option '-fplugin-arg-%s-%s'"),
					plugin_name, argv[i].key);
				return 1;
			}

			track_frame_size = atoi(argv[i].value);
			if (track_frame_size < 0) {
				error(G_("invalid option argument '-fplugin-arg-%s-%s=%s'"),
					plugin_name, argv[i].key, argv[i].value);
				return 1;
			}
		} else if (!strcmp(argv[i].key, "arch")) {
			if (!argv[i].value) {
				error(G_("no value supplied for option '-fplugin-arg-%s-%s'"),
					plugin_name, argv[i].key);
				return 1;
			}

			if (!strcmp(argv[i].value, "x86"))
				build_for_x86 = true;
		} else if (!strcmp(argv[i].key, "disable")) {
			disable = true;
		} else if (!strcmp(argv[i].key, "verbose")) {
			verbose = true;
		} else {
			error(G_("unknown option '-fplugin-arg-%s-%s'"),
					plugin_name, argv[i].key);
			return 1;
		}
	}

	if (disable) {
		if (verbose)
			fprintf(stderr, "stackleak: disabled for this translation unit\n");
		return 0;
	}

	 
	register_callback(plugin_name, PLUGIN_INFO, NULL,
						&stackleak_plugin_info);

	 
	register_callback(plugin_name, PLUGIN_START_UNIT,
					&stackleak_start_unit, NULL);

	 
	register_callback(plugin_name, PLUGIN_REGISTER_GGC_ROOTS, NULL,
					(void *)&gt_ggc_r_gt_stackleak);

	 
	register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
					&stackleak_instrument_pass_info);
	register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
					&stackleak_cleanup_pass_info);

	return 0;
}
