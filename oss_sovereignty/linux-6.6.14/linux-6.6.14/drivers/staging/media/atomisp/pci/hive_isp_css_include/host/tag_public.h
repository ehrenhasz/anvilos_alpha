#ifndef __TAG_PUBLIC_H_INCLUDED__
#define __TAG_PUBLIC_H_INCLUDED__
void
sh_css_create_tag_descr(int num_captures,
			unsigned int skip,
			int offset,
			unsigned int exp_id,
			struct sh_css_tag_descr *tag_descr);
unsigned int
sh_css_encode_tag_descr(struct sh_css_tag_descr *tag);
#endif  
