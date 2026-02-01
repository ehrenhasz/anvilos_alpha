 
 

#ifndef IGT_ATOMIC_H
#define IGT_ATOMIC_H

struct igt_atomic_section {
	const char *name;
	void (*critical_section_begin)(void);
	void (*critical_section_end)(void);
};

extern const struct igt_atomic_section igt_atomic_phases[];

#endif  
