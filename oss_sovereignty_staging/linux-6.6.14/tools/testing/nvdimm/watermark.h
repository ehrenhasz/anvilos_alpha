

#ifndef _TEST_NVDIMM_WATERMARK_H_
#define _TEST_NVDIMM_WATERMARK_H_
int pmem_test(void);
int libnvdimm_test(void);
int acpi_nfit_test(void);
int device_dax_test(void);
int dax_pmem_test(void);
int dax_pmem_core_test(void);
int dax_pmem_compat_test(void);

 
#define nfit_test_watermark(x)				\
int x##_test(void)					\
{							\
	pr_debug("%s for nfit_test\n", KBUILD_MODNAME);	\
	return 0;					\
}							\
EXPORT_SYMBOL(x##_test)
#endif  
