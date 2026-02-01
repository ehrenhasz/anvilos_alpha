 
 

 
#define PAC_AUTOGAIN_IGNORE_FRAMES	2

static const unsigned char pac_sof_marker[5] =
		{ 0xff, 0xff, 0x00, 0xff, 0x96 };

 

static unsigned char *pac_find_sof(struct gspca_dev *gspca_dev, u8 *sof_read,
					unsigned char *m, int len)
{
	int i;

	 
	for (i = 0; i < len; i++) {
		switch (*sof_read) {
		case 0:
			if (m[i] == 0xff)
				*sof_read = 1;
			break;
		case 1:
			if (m[i] == 0xff)
				*sof_read = 2;
			else
				*sof_read = 0;
			break;
		case 2:
			switch (m[i]) {
			case 0x00:
				*sof_read = 3;
				break;
			case 0xff:
				 
				break;
			default:
				*sof_read = 0;
			}
			break;
		case 3:
			if (m[i] == 0xff)
				*sof_read = 4;
			else
				*sof_read = 0;
			break;
		case 4:
			switch (m[i]) {
			case 0x96:
				 
				gspca_dbg(gspca_dev, D_FRAM,
					  "SOF found, bytes to analyze: %u - Frame starts at byte #%u\n",
					  len, i + 1);
				*sof_read = 0;
				return m + i + 1;
				break;
			case 0xff:
				*sof_read = 2;
				break;
			default:
				*sof_read = 0;
			}
			break;
		default:
			*sof_read = 0;
		}
	}

	return NULL;
}
