
 

#include <linux/kernel.h>
#include <linux/soc/mediatek/mtk_wed.h>

const struct mtk_wed_ops __rcu *mtk_soc_wed_ops;
EXPORT_SYMBOL_GPL(mtk_soc_wed_ops);
