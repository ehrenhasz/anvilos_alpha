
 

#include <linux/module.h>
#include <linux/trace_events.h>

 

static struct trace_event_file *gen_kprobe_test;
static struct trace_event_file *gen_kretprobe_test;

#define KPROBE_GEN_TEST_FUNC	"do_sys_open"

 
#if defined(CONFIG_X86_64) || defined(CONFIG_X86_32)
#define KPROBE_GEN_TEST_ARG0	"dfd=%ax"
#define KPROBE_GEN_TEST_ARG1	"filename=%dx"
#define KPROBE_GEN_TEST_ARG2	"flags=%cx"
#define KPROBE_GEN_TEST_ARG3	"mode=+4($stack)"

 
#elif defined(CONFIG_ARM64)
#define KPROBE_GEN_TEST_ARG0	"dfd=%x0"
#define KPROBE_GEN_TEST_ARG1	"filename=%x1"
#define KPROBE_GEN_TEST_ARG2	"flags=%x2"
#define KPROBE_GEN_TEST_ARG3	"mode=%x3"

 
#elif defined(CONFIG_ARM)
#define KPROBE_GEN_TEST_ARG0	"dfd=%r0"
#define KPROBE_GEN_TEST_ARG1	"filename=%r1"
#define KPROBE_GEN_TEST_ARG2	"flags=%r2"
#define KPROBE_GEN_TEST_ARG3	"mode=%r3"

 
#elif defined(CONFIG_RISCV)
#define KPROBE_GEN_TEST_ARG0	"dfd=%a0"
#define KPROBE_GEN_TEST_ARG1	"filename=%a1"
#define KPROBE_GEN_TEST_ARG2	"flags=%a2"
#define KPROBE_GEN_TEST_ARG3	"mode=%a3"

 
#else
#define KPROBE_GEN_TEST_ARG0	NULL
#define KPROBE_GEN_TEST_ARG1	NULL
#define KPROBE_GEN_TEST_ARG2	NULL
#define KPROBE_GEN_TEST_ARG3	NULL
#endif

static bool trace_event_file_is_valid(struct trace_event_file *input)
{
	return input && !IS_ERR(input);
}

 
static int __init test_gen_kprobe_cmd(void)
{
	struct dynevent_cmd cmd;
	char *buf;
	int ret;

	 
	buf = kzalloc(MAX_DYNEVENT_CMD_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	kprobe_event_cmd_init(&cmd, buf, MAX_DYNEVENT_CMD_LEN);

	 
	ret = kprobe_event_gen_cmd_start(&cmd, "gen_kprobe_test",
					 KPROBE_GEN_TEST_FUNC,
					 KPROBE_GEN_TEST_ARG0, KPROBE_GEN_TEST_ARG1);
	if (ret)
		goto out;

	 

	ret = kprobe_event_add_fields(&cmd, KPROBE_GEN_TEST_ARG2, KPROBE_GEN_TEST_ARG3);
	if (ret)
		goto out;

	 
	ret = kprobe_event_gen_cmd_end(&cmd);
	if (ret)
		goto out;

	 
	gen_kprobe_test = trace_get_event_file(NULL, "kprobes",
					       "gen_kprobe_test");
	if (IS_ERR(gen_kprobe_test)) {
		ret = PTR_ERR(gen_kprobe_test);
		goto delete;
	}

	 
	ret = trace_array_set_clr_event(gen_kprobe_test->tr,
					"kprobes", "gen_kprobe_test", true);
	if (ret) {
		trace_put_event_file(gen_kprobe_test);
		goto delete;
	}
 out:
	kfree(buf);
	return ret;
 delete:
	if (trace_event_file_is_valid(gen_kprobe_test))
		gen_kprobe_test = NULL;
	 
	kprobe_event_delete("gen_kprobe_test");
	goto out;
}

 
static int __init test_gen_kretprobe_cmd(void)
{
	struct dynevent_cmd cmd;
	char *buf;
	int ret;

	 
	buf = kzalloc(MAX_DYNEVENT_CMD_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	kprobe_event_cmd_init(&cmd, buf, MAX_DYNEVENT_CMD_LEN);

	 
	ret = kretprobe_event_gen_cmd_start(&cmd, "gen_kretprobe_test",
					    KPROBE_GEN_TEST_FUNC,
					    "$retval");
	if (ret)
		goto out;

	 
	ret = kretprobe_event_gen_cmd_end(&cmd);
	if (ret)
		goto out;

	 
	gen_kretprobe_test = trace_get_event_file(NULL, "kprobes",
						  "gen_kretprobe_test");
	if (IS_ERR(gen_kretprobe_test)) {
		ret = PTR_ERR(gen_kretprobe_test);
		goto delete;
	}

	 
	ret = trace_array_set_clr_event(gen_kretprobe_test->tr,
					"kprobes", "gen_kretprobe_test", true);
	if (ret) {
		trace_put_event_file(gen_kretprobe_test);
		goto delete;
	}
 out:
	kfree(buf);
	return ret;
 delete:
	if (trace_event_file_is_valid(gen_kretprobe_test))
		gen_kretprobe_test = NULL;
	 
	kprobe_event_delete("gen_kretprobe_test");
	goto out;
}

static int __init kprobe_event_gen_test_init(void)
{
	int ret;

	ret = test_gen_kprobe_cmd();
	if (ret)
		return ret;

	ret = test_gen_kretprobe_cmd();
	if (ret) {
		if (trace_event_file_is_valid(gen_kretprobe_test)) {
			WARN_ON(trace_array_set_clr_event(gen_kretprobe_test->tr,
							  "kprobes",
							  "gen_kretprobe_test", false));
			trace_put_event_file(gen_kretprobe_test);
		}
		WARN_ON(kprobe_event_delete("gen_kretprobe_test"));
	}

	return ret;
}

static void __exit kprobe_event_gen_test_exit(void)
{
	if (trace_event_file_is_valid(gen_kprobe_test)) {
		 
		WARN_ON(trace_array_set_clr_event(gen_kprobe_test->tr,
						  "kprobes",
						  "gen_kprobe_test", false));

		 
		trace_put_event_file(gen_kprobe_test);
	}


	 
	WARN_ON(kprobe_event_delete("gen_kprobe_test"));

	if (trace_event_file_is_valid(gen_kretprobe_test)) {
		 
		WARN_ON(trace_array_set_clr_event(gen_kretprobe_test->tr,
						  "kprobes",
						  "gen_kretprobe_test", false));

		 
		trace_put_event_file(gen_kretprobe_test);
	}


	 
	WARN_ON(kprobe_event_delete("gen_kretprobe_test"));
}

module_init(kprobe_event_gen_test_init)
module_exit(kprobe_event_gen_test_exit)

MODULE_AUTHOR("Tom Zanussi");
MODULE_DESCRIPTION("kprobe event generation test");
MODULE_LICENSE("GPL v2");
