#ifndef _PSERIES_RTAS_FADUMP_H
#define _PSERIES_RTAS_FADUMP_H
#define RTAS_FADUMP_MIN_BOOT_MEM	((0x1UL << 28) + (0x1UL << 26))
#define RTAS_FADUMP_CPU_STATE_DATA	0x0001
#define RTAS_FADUMP_HPTE_REGION		0x0002
#define RTAS_FADUMP_REAL_MODE_REGION	0x0011
#define RTAS_FADUMP_REQUEST_FLAG	0x00000001
#define RTAS_FADUMP_ERROR_FLAG		0x2000
struct rtas_fadump_section {
	__be32	request_flag;
	__be16	source_data_type;
	__be16	error_flags;
	__be64	source_address;
	__be64	source_len;
	__be64	bytes_dumped;
	__be64	destination_address;
};
struct rtas_fadump_section_header {
	__be32	dump_format_version;
	__be16	dump_num_sections;
	__be16	dump_status_flag;
	__be32	offset_first_dump_section;
	__be32	dd_block_size;
	__be64	dd_block_offset;
	__be64	dd_num_blocks;
	__be32	dd_offset_disk_path;
	__be32	max_time_auto;
};
struct rtas_fadump_mem_struct {
	struct rtas_fadump_section_header	header;
	struct rtas_fadump_section		cpu_state_data;
	struct rtas_fadump_section		hpte_region;
	struct rtas_fadump_section		rmr_region;
};
struct rtas_fadump_reg_save_area_header {
	__be64		magic_number;
	__be32		version;
	__be32		num_cpu_offset;
};
struct rtas_fadump_reg_entry {
	__be64		reg_id;
	__be64		reg_value;
};
#define RTAS_FADUMP_SKIP_TO_NEXT_CPU(reg_entry)				\
({									\
	while (be64_to_cpu(reg_entry->reg_id) !=			\
	       fadump_str_to_u64("CPUEND"))				\
		reg_entry++;						\
	reg_entry++;							\
})
#define RTAS_FADUMP_CPU_ID_MASK			((1UL << 32) - 1)
#endif  
