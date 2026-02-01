 

#include "dmub_reg.h"
#include "../dmub_srv.h"

struct dmub_reg_value_masks {
	uint32_t value;
	uint32_t mask;
};

static inline void
set_reg_field_value_masks(struct dmub_reg_value_masks *field_value_mask,
			  uint32_t value, uint32_t mask, uint8_t shift)
{
	field_value_mask->value =
		(field_value_mask->value & ~mask) | (mask & (value << shift));
	field_value_mask->mask = field_value_mask->mask | mask;
}

static void set_reg_field_values(struct dmub_reg_value_masks *field_value_mask,
				 uint32_t addr, int n, uint8_t shift1,
				 uint32_t mask1, uint32_t field_value1,
				 va_list ap)
{
	uint32_t shift, mask, field_value;
	int i = 1;

	 
	set_reg_field_value_masks(field_value_mask, field_value1, mask1,
				  shift1);

	while (i < n) {
		shift = va_arg(ap, uint32_t);
		mask = va_arg(ap, uint32_t);
		field_value = va_arg(ap, uint32_t);

		set_reg_field_value_masks(field_value_mask, field_value, mask,
					  shift);
		i++;
	}
}

static inline uint32_t get_reg_field_value_ex(uint32_t reg_value, uint32_t mask,
					      uint8_t shift)
{
	return (mask & reg_value) >> shift;
}

void dmub_reg_update(struct dmub_srv *srv, uint32_t addr, int n, uint8_t shift1,
		     uint32_t mask1, uint32_t field_value1, ...)
{
	struct dmub_reg_value_masks field_value_mask = { 0 };
	uint32_t reg_val;
	va_list ap;

	va_start(ap, field_value1);
	set_reg_field_values(&field_value_mask, addr, n, shift1, mask1,
			     field_value1, ap);
	va_end(ap);

	reg_val = srv->funcs.reg_read(srv->user_ctx, addr);
	reg_val = (reg_val & ~field_value_mask.mask) | field_value_mask.value;
	srv->funcs.reg_write(srv->user_ctx, addr, reg_val);
}

void dmub_reg_set(struct dmub_srv *srv, uint32_t addr, uint32_t reg_val, int n,
		  uint8_t shift1, uint32_t mask1, uint32_t field_value1, ...)
{
	struct dmub_reg_value_masks field_value_mask = { 0 };
	va_list ap;

	va_start(ap, field_value1);
	set_reg_field_values(&field_value_mask, addr, n, shift1, mask1,
			     field_value1, ap);
	va_end(ap);

	reg_val = (reg_val & ~field_value_mask.mask) | field_value_mask.value;
	srv->funcs.reg_write(srv->user_ctx, addr, reg_val);
}

void dmub_reg_get(struct dmub_srv *srv, uint32_t addr, uint8_t shift,
		  uint32_t mask, uint32_t *field_value)
{
	uint32_t reg_val = srv->funcs.reg_read(srv->user_ctx, addr);
	*field_value = get_reg_field_value_ex(reg_val, mask, shift);
}
