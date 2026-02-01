
 

#include <linux/module.h>
#include <linux/trace_events.h>

 

static struct trace_event_file *create_synth_test;
static struct trace_event_file *empty_synth_test;
static struct trace_event_file *gen_synth_test;

 
static int __init test_gen_synth_cmd(void)
{
	struct dynevent_cmd cmd;
	u64 vals[7];
	char *buf;
	int ret;

	 
	buf = kzalloc(MAX_DYNEVENT_CMD_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	synth_event_cmd_init(&cmd, buf, MAX_DYNEVENT_CMD_LEN);

	 
	ret = synth_event_gen_cmd_start(&cmd, "gen_synth_test", THIS_MODULE,
					"pid_t", "next_pid_field",
					"char[16]", "next_comm_field",
					"u64", "ts_ns",
					"u64", "ts_ms");
	if (ret)
		goto free;

	 

	ret = synth_event_add_field(&cmd, "unsigned int", "cpu");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "char[64]", "my_string_field");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "int", "my_int_field");
	if (ret)
		goto free;

	ret = synth_event_gen_cmd_end(&cmd);
	if (ret)
		goto free;

	 
	gen_synth_test = trace_get_event_file(NULL, "synthetic",
					      "gen_synth_test");
	if (IS_ERR(gen_synth_test)) {
		ret = PTR_ERR(gen_synth_test);
		goto delete;
	}

	 
	ret = trace_array_set_clr_event(gen_synth_test->tr,
					"synthetic", "gen_synth_test", true);
	if (ret) {
		trace_put_event_file(gen_synth_test);
		goto delete;
	}

	 

	vals[0] = 777;			 
	vals[1] = (u64)(long)"hula hoops";	 
	vals[2] = 1000000;		 
	vals[3] = 1000;			 
	vals[4] = raw_smp_processor_id();  
	vals[5] = (u64)(long)"thneed";	 
	vals[6] = 598;			 

	 
	ret = synth_event_trace_array(gen_synth_test, vals, ARRAY_SIZE(vals));
 free:
	kfree(buf);
	return ret;
 delete:
	 
	synth_event_delete("gen_synth_test");
	goto free;
}

 
static int __init test_empty_synth_event(void)
{
	struct dynevent_cmd cmd;
	u64 vals[7];
	char *buf;
	int ret;

	 
	buf = kzalloc(MAX_DYNEVENT_CMD_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	synth_event_cmd_init(&cmd, buf, MAX_DYNEVENT_CMD_LEN);

	 
	ret = synth_event_gen_cmd_start(&cmd, "empty_synth_test", THIS_MODULE);
	if (ret)
		goto free;

	 

	ret = synth_event_add_field(&cmd, "pid_t", "next_pid_field");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "char[16]", "next_comm_field");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "u64", "ts_ns");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "u64", "ts_ms");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "unsigned int", "cpu");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "char[64]", "my_string_field");
	if (ret)
		goto free;

	ret = synth_event_add_field(&cmd, "int", "my_int_field");
	if (ret)
		goto free;

	 

	ret = synth_event_gen_cmd_end(&cmd);
	if (ret)
		goto free;

	 
	empty_synth_test = trace_get_event_file(NULL, "synthetic",
						"empty_synth_test");
	if (IS_ERR(empty_synth_test)) {
		ret = PTR_ERR(empty_synth_test);
		goto delete;
	}

	 
	ret = trace_array_set_clr_event(empty_synth_test->tr,
					"synthetic", "empty_synth_test", true);
	if (ret) {
		trace_put_event_file(empty_synth_test);
		goto delete;
	}

	 

	vals[0] = 777;			 
	vals[1] = (u64)(long)"tiddlywinks";	 
	vals[2] = 1000000;		 
	vals[3] = 1000;			 
	vals[4] = raw_smp_processor_id();  
	vals[5] = (u64)(long)"thneed_2.0";	 
	vals[6] = 399;			 

	 
	ret = synth_event_trace_array(empty_synth_test, vals, ARRAY_SIZE(vals));
 free:
	kfree(buf);
	return ret;
 delete:
	 
	synth_event_delete("empty_synth_test");
	goto free;
}

static struct synth_field_desc create_synth_test_fields[] = {
	{ .type = "pid_t",		.name = "next_pid_field" },
	{ .type = "char[16]",		.name = "next_comm_field" },
	{ .type = "u64",		.name = "ts_ns" },
	{ .type = "char[]",		.name = "dynstring_field_1" },
	{ .type = "u64",		.name = "ts_ms" },
	{ .type = "unsigned int",	.name = "cpu" },
	{ .type = "char[64]",		.name = "my_string_field" },
	{ .type = "char[]",		.name = "dynstring_field_2" },
	{ .type = "int",		.name = "my_int_field" },
};

 
static int __init test_create_synth_event(void)
{
	u64 vals[9];
	int ret;

	 
	ret = synth_event_create("create_synth_test",
				 create_synth_test_fields,
				 ARRAY_SIZE(create_synth_test_fields),
				 THIS_MODULE);
	if (ret)
		goto out;

	 
	create_synth_test = trace_get_event_file(NULL, "synthetic",
						 "create_synth_test");
	if (IS_ERR(create_synth_test)) {
		ret = PTR_ERR(create_synth_test);
		goto delete;
	}

	 
	ret = trace_array_set_clr_event(create_synth_test->tr,
					"synthetic", "create_synth_test", true);
	if (ret) {
		trace_put_event_file(create_synth_test);
		goto delete;
	}

	 

	vals[0] = 777;			 
	vals[1] = (u64)(long)"tiddlywinks";	 
	vals[2] = 1000000;		 
	vals[3] = (u64)(long)"xrayspecs";	 
	vals[4] = 1000;			 
	vals[5] = raw_smp_processor_id();  
	vals[6] = (u64)(long)"thneed";	 
	vals[7] = (u64)(long)"kerplunk";	 
	vals[8] = 398;			 

	 
	ret = synth_event_trace_array(create_synth_test, vals, ARRAY_SIZE(vals));
 out:
	return ret;
 delete:
	 
	synth_event_delete("create_synth_test");

	goto out;
}

 
static int __init test_add_next_synth_val(void)
{
	struct synth_event_trace_state trace_state;
	int ret;

	 
	ret = synth_event_trace_start(gen_synth_test, &trace_state);
	if (ret)
		return ret;

	 

	 
	ret = synth_event_add_next_val(777, &trace_state);
	if (ret)
		goto out;

	 
	ret = synth_event_add_next_val((u64)(long)"slinky", &trace_state);
	if (ret)
		goto out;

	 
	ret = synth_event_add_next_val(1000000, &trace_state);
	if (ret)
		goto out;

	 
	ret = synth_event_add_next_val(1000, &trace_state);
	if (ret)
		goto out;

	 
	ret = synth_event_add_next_val(raw_smp_processor_id(), &trace_state);
	if (ret)
		goto out;

	 
	ret = synth_event_add_next_val((u64)(long)"thneed_2.01", &trace_state);
	if (ret)
		goto out;

	 
	ret = synth_event_add_next_val(395, &trace_state);
 out:
	 
	ret = synth_event_trace_end(&trace_state);

	return ret;
}

 
static int __init test_add_synth_val(void)
{
	struct synth_event_trace_state trace_state;
	int ret;

	 
	ret = synth_event_trace_start(gen_synth_test, &trace_state);
	if (ret)
		return ret;

	 

	ret = synth_event_add_val("ts_ns", 1000000, &trace_state);
	if (ret)
		goto out;

	ret = synth_event_add_val("ts_ms", 1000, &trace_state);
	if (ret)
		goto out;

	ret = synth_event_add_val("cpu", raw_smp_processor_id(), &trace_state);
	if (ret)
		goto out;

	ret = synth_event_add_val("next_pid_field", 777, &trace_state);
	if (ret)
		goto out;

	ret = synth_event_add_val("next_comm_field", (u64)(long)"silly putty",
				  &trace_state);
	if (ret)
		goto out;

	ret = synth_event_add_val("my_string_field", (u64)(long)"thneed_9",
				  &trace_state);
	if (ret)
		goto out;

	ret = synth_event_add_val("my_int_field", 3999, &trace_state);
 out:
	 
	ret = synth_event_trace_end(&trace_state);

	return ret;
}

 
static int __init test_trace_synth_event(void)
{
	int ret;

	 
	ret = synth_event_trace(create_synth_test, 9,	 
				(u64)444,		 
				(u64)(long)"clackers",	 
				(u64)1000000,		 
				(u64)(long)"viewmaster", 
				(u64)1000,		 
				(u64)raw_smp_processor_id(),  
				(u64)(long)"Thneed",	 
				(u64)(long)"yoyos",	 
				(u64)999);		 
	return ret;
}

static int __init synth_event_gen_test_init(void)
{
	int ret;

	ret = test_gen_synth_cmd();
	if (ret)
		return ret;

	ret = test_empty_synth_event();
	if (ret) {
		WARN_ON(trace_array_set_clr_event(gen_synth_test->tr,
						  "synthetic",
						  "gen_synth_test", false));
		trace_put_event_file(gen_synth_test);
		WARN_ON(synth_event_delete("gen_synth_test"));
		goto out;
	}

	ret = test_create_synth_event();
	if (ret) {
		WARN_ON(trace_array_set_clr_event(gen_synth_test->tr,
						  "synthetic",
						  "gen_synth_test", false));
		trace_put_event_file(gen_synth_test);
		WARN_ON(synth_event_delete("gen_synth_test"));

		WARN_ON(trace_array_set_clr_event(empty_synth_test->tr,
						  "synthetic",
						  "empty_synth_test", false));
		trace_put_event_file(empty_synth_test);
		WARN_ON(synth_event_delete("empty_synth_test"));
		goto out;
	}

	ret = test_add_next_synth_val();
	WARN_ON(ret);

	ret = test_add_synth_val();
	WARN_ON(ret);

	ret = test_trace_synth_event();
	WARN_ON(ret);

	 
	trace_array_set_clr_event(gen_synth_test->tr,
				  "synthetic",
				  "gen_synth_test", false);
	trace_array_set_clr_event(empty_synth_test->tr,
				  "synthetic",
				  "empty_synth_test", false);
	trace_array_set_clr_event(create_synth_test->tr,
				  "synthetic",
				  "create_synth_test", false);
 out:
	return ret;
}

static void __exit synth_event_gen_test_exit(void)
{
	 
	WARN_ON(trace_array_set_clr_event(gen_synth_test->tr,
					  "synthetic",
					  "gen_synth_test", false));

	 
	trace_put_event_file(gen_synth_test);

	 
	WARN_ON(synth_event_delete("gen_synth_test"));

	 
	WARN_ON(trace_array_set_clr_event(empty_synth_test->tr,
					  "synthetic",
					  "empty_synth_test", false));

	 
	trace_put_event_file(empty_synth_test);

	 
	WARN_ON(synth_event_delete("empty_synth_test"));

	 
	WARN_ON(trace_array_set_clr_event(create_synth_test->tr,
					  "synthetic",
					  "create_synth_test", false));

	 
	trace_put_event_file(create_synth_test);

	 
	WARN_ON(synth_event_delete("create_synth_test"));
}

module_init(synth_event_gen_test_init)
module_exit(synth_event_gen_test_exit)

MODULE_AUTHOR("Tom Zanussi");
MODULE_DESCRIPTION("synthetic event generation test");
MODULE_LICENSE("GPL v2");
