
 

#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <ctype.h>

#include "misc.h"

#define PAGE_SIZE               sysconf(_SC_PAGESIZE)

 
int pvr;
u64 platform_extended_mask;

 
int ev_mask_pmcxsel, ev_shift_pmcxsel;		
int ev_mask_marked, ev_shift_marked;		
int ev_mask_comb, ev_shift_comb;		
int ev_mask_unit, ev_shift_unit;		
int ev_mask_pmc, ev_shift_pmc;			
int ev_mask_cache, ev_shift_cache;		
int ev_mask_sample, ev_shift_sample;		
int ev_mask_thd_sel, ev_shift_thd_sel;		
int ev_mask_thd_start, ev_shift_thd_start;	
int ev_mask_thd_stop, ev_shift_thd_stop;	
int ev_mask_thd_cmp, ev_shift_thd_cmp;		
int ev_mask_sm, ev_shift_sm;			
int ev_mask_rsq, ev_shift_rsq;			
int ev_mask_l2l3, ev_shift_l2l3;		
int ev_mask_mmcr3_src, ev_shift_mmcr3_src;	

static void init_ev_encodes(void)
{
	ev_mask_pmcxsel = 0xff;
	ev_shift_pmcxsel = 0;
	ev_mask_marked = 1;
	ev_shift_marked = 8;
	ev_mask_unit = 0xf;
	ev_shift_unit = 12;
	ev_mask_pmc = 0xf;
	ev_shift_pmc = 16;
	ev_mask_sample	= 0x1f;
	ev_shift_sample = 24;
	ev_mask_thd_sel = 0x7;
	ev_shift_thd_sel = 29;
	ev_mask_thd_start = 0xf;
	ev_shift_thd_start = 36;
	ev_mask_thd_stop = 0xf;
	ev_shift_thd_stop = 32;

	switch (pvr) {
	case POWER10:
		ev_mask_thd_cmp = 0x3ffff;
		ev_shift_thd_cmp = 0;
		ev_mask_rsq = 1;
		ev_shift_rsq = 9;
		ev_mask_comb = 3;
		ev_shift_comb = 10;
		ev_mask_cache = 3;
		ev_shift_cache = 20;
		ev_mask_sm = 0x3;
		ev_shift_sm = 22;
		ev_mask_l2l3 = 0x1f;
		ev_shift_l2l3 = 40;
		ev_mask_mmcr3_src = 0x7fff;
		ev_shift_mmcr3_src = 45;
		break;
	case POWER9:
		ev_mask_comb = 3;
		ev_shift_comb = 10;
		ev_mask_cache = 0xf;
		ev_shift_cache = 20;
		ev_mask_thd_cmp = 0x3ff;
		ev_shift_thd_cmp = 40;
		ev_mask_sm = 0x3;
		ev_shift_sm = 50;
		break;
	default:
		FAIL_IF_EXIT(1);
	}
}

 
static u64 perf_get_platform_reg_mask(void)
{
	if (have_hwcap2(PPC_FEATURE2_ARCH_3_1))
		return PERF_POWER10_MASK;
	if (have_hwcap2(PPC_FEATURE2_ARCH_3_00))
		return PERF_POWER9_MASK;

	return -1;
}

int check_extended_regs_support(void)
{
	int fd;
	struct event event;

	event_init(&event, 0x1001e);

	event.attr.type = 4;
	event.attr.sample_period = 1;
	event.attr.disabled = 1;
	event.attr.sample_type = PERF_SAMPLE_REGS_INTR;
	event.attr.sample_regs_intr = platform_extended_mask;

	fd = event_open(&event);
	if (fd != -1)
		return 0;

	return -1;
}

int platform_check_for_tests(void)
{
	pvr = PVR_VER(mfspr(SPRN_PVR));

	 
	if ((pvr != POWER10) && (pvr != POWER9))
		goto out;

	 
	if (!have_hwcap2(PPC_FEATURE2_EBB) || !have_hwcap2(PPC_FEATURE2_ARCH_3_00))
		goto out;

	return 0;

out:
	printf("%s: Tests unsupported for this platform\n", __func__);
	return -1;
}

int check_pvr_for_sampling_tests(void)
{
	SKIP_IF(platform_check_for_tests());

	platform_extended_mask = perf_get_platform_reg_mask();
	 
	if (check_extended_regs_support())
		goto out;

	init_ev_encodes();
	return 0;

out:
	printf("%s: Sampling tests un-supported\n", __func__);
	return -1;
}

 
void *event_sample_buf_mmap(int fd, int mmap_pages)
{
	size_t page_size = sysconf(_SC_PAGESIZE);
	size_t mmap_size;
	void *buff;

	if (mmap_pages <= 0)
		return NULL;

	if (fd <= 0)
		return NULL;

	mmap_size =  page_size * (1 + mmap_pages);
	buff = mmap(NULL, mmap_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (buff == MAP_FAILED) {
		perror("mmap() failed.");
		return NULL;
	}
	return buff;
}

 
void *__event_read_samples(void *sample_buff, size_t *size, u64 *sample_count)
{
	size_t page_size = sysconf(_SC_PAGESIZE);
	struct perf_event_header *header = sample_buff + page_size;
	struct perf_event_mmap_page *metadata_page = sample_buff;
	unsigned long data_head, data_tail;

	 

	data_head = metadata_page->data_head;
	 
	mb();
	data_tail = metadata_page->data_tail;

	 
	if (sample_count)
		*sample_count = 0;

	while (1) {
		 
		if (data_head - data_tail < sizeof(header))
			return NULL;

		data_tail += sizeof(header);
		if (header->type == PERF_RECORD_SAMPLE) {
			*size = (header->size - sizeof(header));
			if (!sample_count)
				return sample_buff + page_size + data_tail;
			data_tail += *size;
			*sample_count += 1;
		} else {
			*size = (header->size - sizeof(header));
			if ((metadata_page->data_tail + *size) > metadata_page->data_head)
				data_tail = metadata_page->data_head;
			else
				data_tail += *size;
		}
		header = (struct perf_event_header *)((void *)header + header->size);
	}
	return NULL;
}

int collect_samples(void *sample_buff)
{
	u64 sample_count;
	size_t size = 0;

	__event_read_samples(sample_buff, &size, &sample_count);
	return sample_count;
}

static void *perf_read_first_sample(void *sample_buff, size_t *size)
{
	return __event_read_samples(sample_buff, size, NULL);
}

u64 *get_intr_regs(struct event *event, void *sample_buff)
{
	u64 type = event->attr.sample_type;
	u64 *intr_regs;
	size_t size = 0;

	if ((type ^ (PERF_SAMPLE_REGS_INTR | PERF_SAMPLE_BRANCH_STACK)) &&
			(type  ^ PERF_SAMPLE_REGS_INTR))
		return NULL;

	intr_regs = (u64 *)perf_read_first_sample(sample_buff, &size);
	if (!intr_regs)
		return NULL;

	if (type & PERF_SAMPLE_BRANCH_STACK) {
		 
		intr_regs += ((*intr_regs) * 3) + 1;
	}

	 
	++intr_regs;

	return intr_regs;
}

static const int __perf_reg_mask(const char *register_name)
{
	if (!strcmp(register_name, "R0"))
		return 0;
	else if (!strcmp(register_name, "R1"))
		return 1;
	else if (!strcmp(register_name, "R2"))
		return 2;
	else if (!strcmp(register_name, "R3"))
		return 3;
	else if (!strcmp(register_name, "R4"))
		return 4;
	else if (!strcmp(register_name, "R5"))
		return 5;
	else if (!strcmp(register_name, "R6"))
		return 6;
	else if (!strcmp(register_name, "R7"))
		return 7;
	else if (!strcmp(register_name, "R8"))
		return 8;
	else if (!strcmp(register_name, "R9"))
		return 9;
	else if (!strcmp(register_name, "R10"))
		return 10;
	else if (!strcmp(register_name, "R11"))
		return 11;
	else if (!strcmp(register_name, "R12"))
		return 12;
	else if (!strcmp(register_name, "R13"))
		return 13;
	else if (!strcmp(register_name, "R14"))
		return 14;
	else if (!strcmp(register_name, "R15"))
		return 15;
	else if (!strcmp(register_name, "R16"))
		return 16;
	else if (!strcmp(register_name, "R17"))
		return 17;
	else if (!strcmp(register_name, "R18"))
		return 18;
	else if (!strcmp(register_name, "R19"))
		return 19;
	else if (!strcmp(register_name, "R20"))
		return 20;
	else if (!strcmp(register_name, "R21"))
		return 21;
	else if (!strcmp(register_name, "R22"))
		return 22;
	else if (!strcmp(register_name, "R23"))
		return 23;
	else if (!strcmp(register_name, "R24"))
		return 24;
	else if (!strcmp(register_name, "R25"))
		return 25;
	else if (!strcmp(register_name, "R26"))
		return 26;
	else if (!strcmp(register_name, "R27"))
		return 27;
	else if (!strcmp(register_name, "R28"))
		return 28;
	else if (!strcmp(register_name, "R29"))
		return 29;
	else if (!strcmp(register_name, "R30"))
		return 30;
	else if (!strcmp(register_name, "R31"))
		return 31;
	else if (!strcmp(register_name, "NIP"))
		return 32;
	else if (!strcmp(register_name, "MSR"))
		return 33;
	else if (!strcmp(register_name, "ORIG_R3"))
		return 34;
	else if (!strcmp(register_name, "CTR"))
		return 35;
	else if (!strcmp(register_name, "LINK"))
		return 36;
	else if (!strcmp(register_name, "XER"))
		return 37;
	else if (!strcmp(register_name, "CCR"))
		return 38;
	else if (!strcmp(register_name, "SOFTE"))
		return 39;
	else if (!strcmp(register_name, "TRAP"))
		return 40;
	else if (!strcmp(register_name, "DAR"))
		return 41;
	else if (!strcmp(register_name, "DSISR"))
		return 42;
	else if (!strcmp(register_name, "SIER"))
		return 43;
	else if (!strcmp(register_name, "MMCRA"))
		return 44;
	else if (!strcmp(register_name, "MMCR0"))
		return 45;
	else if (!strcmp(register_name, "MMCR1"))
		return 46;
	else if (!strcmp(register_name, "MMCR2"))
		return 47;
	else if (!strcmp(register_name, "MMCR3"))
		return 48;
	else if (!strcmp(register_name, "SIER2"))
		return 49;
	else if (!strcmp(register_name, "SIER3"))
		return 50;
	else if (!strcmp(register_name, "PMC1"))
		return 51;
	else if (!strcmp(register_name, "PMC2"))
		return 52;
	else if (!strcmp(register_name, "PMC3"))
		return 53;
	else if (!strcmp(register_name, "PMC4"))
		return 54;
	else if (!strcmp(register_name, "PMC5"))
		return 55;
	else if (!strcmp(register_name, "PMC6"))
		return 56;
	else if (!strcmp(register_name, "SDAR"))
		return 57;
	else if (!strcmp(register_name, "SIAR"))
		return 58;
	else
		return -1;
}

u64 get_reg_value(u64 *intr_regs, char *register_name)
{
	int register_bit_position;

	register_bit_position = __perf_reg_mask(register_name);

	if (register_bit_position < 0 || (!((platform_extended_mask >>
			(register_bit_position - 1)) & 1)))
		return -1;

	return *(intr_regs + register_bit_position);
}

int get_thresh_cmp_val(struct event event)
{
	int exp = 0;
	u64 result = 0;
	u64 value;

	if (!have_hwcap2(PPC_FEATURE2_ARCH_3_1))
		return EV_CODE_EXTRACT(event.attr.config, thd_cmp);

	value = EV_CODE_EXTRACT(event.attr.config1, thd_cmp);

	if (!value)
		return value;

	 
	if (value > 261120)
		value = 261120;
	while ((64 - __builtin_clzl(value)) > 8) {
		exp++;
		value >>= 2;
	}

	 
	if (!(value & 0xC0) && exp)
		result = -1;
	else
		result = (exp << 8) | value;
	return result;
}

 
static bool auxv_generic_compat_pmu(void)
{
	int base_pvr = 0;

	if (!strcmp(auxv_base_platform(), "power9"))
		base_pvr = POWER9;
	else if (!strcmp(auxv_base_platform(), "power10"))
		base_pvr = POWER10;

	return (!base_pvr);
}

 
bool check_for_generic_compat_pmu(void)
{
	char pmu_name[256];

	memset(pmu_name, 0, sizeof(pmu_name));
	if (read_sysfs_file("bus/event_source/devices/cpu/caps/pmu_name",
		pmu_name, sizeof(pmu_name)) < 0)
		return auxv_generic_compat_pmu();

	if (!strcmp(pmu_name, "ISAv3"))
		return true;
	else
		return false;
}

 
bool check_for_compat_mode(void)
{
	char *platform = auxv_platform();
	char *base_platform = auxv_base_platform();

	return strcmp(platform, base_platform);
}
