
 

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/nls.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>


 
int
usb_gadget_get_string (const struct usb_gadget_strings *table, int id, u8 *buf)
{
	struct usb_string	*s;
	int			len;

	 
	if (id == 0) {
		buf [0] = 4;
		buf [1] = USB_DT_STRING;
		buf [2] = (u8) table->language;
		buf [3] = (u8) (table->language >> 8);
		return 4;
	}
	for (s = table->strings; s && s->s; s++)
		if (s->id == id)
			break;

	 
	if (!s || !s->s)
		return -EINVAL;

	 
	len = min((size_t)USB_MAX_STRING_LEN, strlen(s->s));
	len = utf8s_to_utf16s(s->s, len, UTF16_LITTLE_ENDIAN,
			(wchar_t *) &buf[2], USB_MAX_STRING_LEN);
	if (len < 0)
		return -EINVAL;
	buf [0] = (len + 1) * 2;
	buf [1] = USB_DT_STRING;
	return buf [0];
}
EXPORT_SYMBOL_GPL(usb_gadget_get_string);

 
bool usb_validate_langid(u16 langid)
{
	u16 primary_lang = langid & 0x3ff;	 
	u16 sub_lang = langid >> 10;		 

	switch (primary_lang) {
	case 0:
	case 0x62 ... 0xfe:
	case 0x100 ... 0x3ff:
		return false;
	}
	if (!sub_lang)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(usb_validate_langid);
