


#ifndef _TEST_USER_RINGBUF_H
#define _TEST_USER_RINGBUF_H

#define TEST_OP_64 4
#define TEST_OP_32 2

enum test_msg_op {
	TEST_MSG_OP_INC64,
	TEST_MSG_OP_INC32,
	TEST_MSG_OP_MUL64,
	TEST_MSG_OP_MUL32,

	
	TEST_MSG_OP_NUM_OPS,
};

struct test_msg {
	enum test_msg_op msg_op;
	union {
		__s64 operand_64;
		__s32 operand_32;
	};
};

struct sample {
	int pid;
	int seq;
	long value;
	char comm[16];
};

#endif 
