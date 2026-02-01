 
 

#ifndef SELFTEST_KVM_SPARSEBIT_H
#define SELFTEST_KVM_SPARSEBIT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sparsebit;
typedef uint64_t sparsebit_idx_t;
typedef uint64_t sparsebit_num_t;

struct sparsebit *sparsebit_alloc(void);
void sparsebit_free(struct sparsebit **sbitp);
void sparsebit_copy(struct sparsebit *dstp, struct sparsebit *src);

bool sparsebit_is_set(struct sparsebit *sbit, sparsebit_idx_t idx);
bool sparsebit_is_set_num(struct sparsebit *sbit,
			  sparsebit_idx_t idx, sparsebit_num_t num);
bool sparsebit_is_clear(struct sparsebit *sbit, sparsebit_idx_t idx);
bool sparsebit_is_clear_num(struct sparsebit *sbit,
			    sparsebit_idx_t idx, sparsebit_num_t num);
sparsebit_num_t sparsebit_num_set(struct sparsebit *sbit);
bool sparsebit_any_set(struct sparsebit *sbit);
bool sparsebit_any_clear(struct sparsebit *sbit);
bool sparsebit_all_set(struct sparsebit *sbit);
bool sparsebit_all_clear(struct sparsebit *sbit);
sparsebit_idx_t sparsebit_first_set(struct sparsebit *sbit);
sparsebit_idx_t sparsebit_first_clear(struct sparsebit *sbit);
sparsebit_idx_t sparsebit_next_set(struct sparsebit *sbit, sparsebit_idx_t prev);
sparsebit_idx_t sparsebit_next_clear(struct sparsebit *sbit, sparsebit_idx_t prev);
sparsebit_idx_t sparsebit_next_set_num(struct sparsebit *sbit,
				       sparsebit_idx_t start, sparsebit_num_t num);
sparsebit_idx_t sparsebit_next_clear_num(struct sparsebit *sbit,
					 sparsebit_idx_t start, sparsebit_num_t num);

void sparsebit_set(struct sparsebit *sbitp, sparsebit_idx_t idx);
void sparsebit_set_num(struct sparsebit *sbitp, sparsebit_idx_t start,
		       sparsebit_num_t num);
void sparsebit_set_all(struct sparsebit *sbitp);

void sparsebit_clear(struct sparsebit *sbitp, sparsebit_idx_t idx);
void sparsebit_clear_num(struct sparsebit *sbitp,
			 sparsebit_idx_t start, sparsebit_num_t num);
void sparsebit_clear_all(struct sparsebit *sbitp);

void sparsebit_dump(FILE *stream, struct sparsebit *sbit,
		    unsigned int indent);
void sparsebit_validate_internal(struct sparsebit *sbit);

#ifdef __cplusplus
}
#endif

#endif  
