
#ifndef IOU_OP_DEF_H
#define IOU_OP_DEF_H

struct io_issue_def {
	 
	unsigned		needs_file : 1;
	 
	unsigned		plug : 1;
	 
	unsigned		hash_reg_file : 1;
	 
	unsigned		unbound_nonreg_file : 1;
	 
	unsigned		pollin : 1;
	unsigned		pollout : 1;
	unsigned		poll_exclusive : 1;
	 
	unsigned		buffer_select : 1;
	 
	unsigned		not_supported : 1;
	 
	unsigned		audit_skip : 1;
	 
	unsigned		ioprio : 1;
	 
	unsigned		iopoll : 1;
	 
	unsigned		iopoll_queue : 1;
	 
	unsigned		manual_alloc : 1;

	int (*issue)(struct io_kiocb *, unsigned int);
	int (*prep)(struct io_kiocb *, const struct io_uring_sqe *);
};

struct io_cold_def {
	 
	unsigned short		async_size;

	const char		*name;

	int (*prep_async)(struct io_kiocb *);
	void (*cleanup)(struct io_kiocb *);
	void (*fail)(struct io_kiocb *);
};

extern const struct io_issue_def io_issue_defs[];
extern const struct io_cold_def io_cold_defs[];

void io_uring_optable_init(void);
#endif
