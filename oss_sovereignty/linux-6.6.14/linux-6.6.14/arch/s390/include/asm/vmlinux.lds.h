#include <asm/page.h>
#define BOOT_DATA							\
	. = ALIGN(PAGE_SIZE);						\
	.boot.data : {							\
		__boot_data_start = .;					\
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.boot.data*)))		\
		__boot_data_end = .;					\
	}
#define BOOT_DATA_PRESERVED						\
	. = ALIGN(PAGE_SIZE);						\
	.boot.preserved.data : {					\
		__boot_data_preserved_start = .;			\
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.boot.preserved.data*))) \
		__boot_data_preserved_end = .;				\
	}
