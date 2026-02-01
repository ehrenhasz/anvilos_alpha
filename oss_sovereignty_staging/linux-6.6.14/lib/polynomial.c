
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/polynomial.h>

 

 
long polynomial_calc(const struct polynomial *poly, long data)
{
	const struct polynomial_term *term = poly->terms;
	long total_divider = poly->total_divider ?: 1;
	long tmp, ret = 0;
	int deg;

	 
	do {
		tmp = term->coef;
		for (deg = 0; deg < term->deg; ++deg)
			tmp = mult_frac(tmp, data, term->divider);
		ret += tmp / term->divider_leftover;
	} while ((term++)->deg);

	return ret / total_divider;
}
EXPORT_SYMBOL_GPL(polynomial_calc);

MODULE_DESCRIPTION("Generic polynomial calculations");
MODULE_LICENSE("GPL");
