
 

 
 
#include <linux/ihex.h>
#include <linux/slab.h>

 
 

#define PRISM2_USB_FWFILE	"prism2_ru.fw"
MODULE_FIRMWARE(PRISM2_USB_FWFILE);

#define S3DATA_MAX		5000
#define S3PLUG_MAX		200
#define S3CRC_MAX		200
#define S3INFO_MAX		50

#define S3ADDR_PLUG		(0xff000000UL)
#define S3ADDR_CRC		(0xff100000UL)
#define S3ADDR_INFO		(0xff200000UL)
#define S3ADDR_START		(0xff400000UL)

#define CHUNKS_MAX		100

#define WRITESIZE_MAX		4096

 
 

struct s3datarec {
	u32 len;
	u32 addr;
	u8 checksum;
	u8 *data;
};

struct s3plugrec {
	u32 itemcode;
	u32 addr;
	u32 len;
};

struct s3crcrec {
	u32 addr;
	u32 len;
	unsigned int dowrite;
};

struct s3inforec {
	u16 len;
	u16 type;
	union {
		struct hfa384x_compident version;
		struct hfa384x_caplevel compat;
		u16 buildseq;
		struct hfa384x_compident platform;
	} info;
};

struct pda {
	u8 buf[HFA384x_PDA_LEN_MAX];
	struct hfa384x_pdrec *rec[HFA384x_PDA_RECS_MAX];
	unsigned int nrec;
};

struct imgchunk {
	u32 addr;	 
	u32 len;	 
	u16 crc;	 
	u8 *data;
};

 
 

 
 

 
static unsigned int ns3data;
static struct s3datarec *s3data;

 
static unsigned int ns3plug;
static struct s3plugrec s3plug[S3PLUG_MAX];

 
static unsigned int ns3crc;
static struct s3crcrec s3crc[S3CRC_MAX];

 
static unsigned int ns3info;
static struct s3inforec s3info[S3INFO_MAX];

 
static u32 startaddr;

 
static unsigned int nfchunks;
static struct imgchunk fchunk[CHUNKS_MAX];

 
 
 
 
 

static struct pda pda;
static struct hfa384x_compident nicid;
static struct hfa384x_caplevel rfid;
static struct hfa384x_caplevel macid;
static struct hfa384x_caplevel priid;

 
 

static int prism2_fwapply(const struct ihex_binrec *rfptr,
			  struct wlandevice *wlandev);

static int read_fwfile(const struct ihex_binrec *rfptr);

static int mkimage(struct imgchunk *clist, unsigned int *ccnt);

static int read_cardpda(struct pda *pda, struct wlandevice *wlandev);

static int mkpdrlist(struct pda *pda);

static int plugimage(struct imgchunk *fchunk, unsigned int nfchunks,
		     struct s3plugrec *s3plug, unsigned int ns3plug,
		     struct pda *pda);

static int crcimage(struct imgchunk *fchunk, unsigned int nfchunks,
		    struct s3crcrec *s3crc, unsigned int ns3crc);

static int writeimage(struct wlandevice *wlandev, struct imgchunk *fchunk,
		      unsigned int nfchunks);

static void free_chunks(struct imgchunk *fchunk, unsigned int *nfchunks);

static void free_srecs(void);

static int validate_identity(void);

 
 

 
static int prism2_fwtry(struct usb_device *udev, struct wlandevice *wlandev)
{
	const struct firmware *fw_entry = NULL;

	netdev_info(wlandev->netdev, "prism2_usb: Checking for firmware %s\n",
		    PRISM2_USB_FWFILE);
	if (request_ihex_firmware(&fw_entry,
				  PRISM2_USB_FWFILE, &udev->dev) != 0) {
		netdev_info(wlandev->netdev,
			    "prism2_usb: Firmware not available, but not essential\n");
		netdev_info(wlandev->netdev,
			    "prism2_usb: can continue to use card anyway.\n");
		return 1;
	}

	netdev_info(wlandev->netdev,
		    "prism2_usb: %s will be processed, size %zu\n",
		    PRISM2_USB_FWFILE, fw_entry->size);
	prism2_fwapply((const struct ihex_binrec *)fw_entry->data, wlandev);

	release_firmware(fw_entry);
	return 0;
}

 
static int prism2_fwapply(const struct ihex_binrec *rfptr,
			  struct wlandevice *wlandev)
{
	signed int result = 0;
	struct p80211msg_dot11req_mibget getmsg;
	struct p80211itemd *item;
	u32 *data;

	 
	ns3data = 0;
	s3data = kcalloc(S3DATA_MAX, sizeof(*s3data), GFP_KERNEL);
	if (!s3data) {
		result = -ENOMEM;
		goto out;
	}

	ns3plug = 0;
	memset(s3plug, 0, sizeof(s3plug));
	ns3crc = 0;
	memset(s3crc, 0, sizeof(s3crc));
	ns3info = 0;
	memset(s3info, 0, sizeof(s3info));
	startaddr = 0;

	nfchunks = 0;
	memset(fchunk, 0, sizeof(fchunk));
	memset(&nicid, 0, sizeof(nicid));
	memset(&rfid, 0, sizeof(rfid));
	memset(&macid, 0, sizeof(macid));
	memset(&priid, 0, sizeof(priid));

	 
	memset(&pda, 0, sizeof(pda));
	pda.rec[0] = (struct hfa384x_pdrec *)pda.buf;
	pda.rec[0]->len = cpu_to_le16(2);	 
	pda.rec[0]->code = cpu_to_le16(HFA384x_PDR_END_OF_PDA);
	pda.nrec = 1;

	 
	 
	prism2sta_ifstate(wlandev, P80211ENUM_ifstate_fwload);

	 
	if (read_cardpda(&pda, wlandev)) {
		netdev_err(wlandev->netdev, "load_cardpda failed, exiting.\n");
		result = 1;
		goto out;
	}

	 
	memset(&getmsg, 0, sizeof(getmsg));
	getmsg.msgcode = DIDMSG_DOT11REQ_MIBGET;
	getmsg.msglen = sizeof(getmsg);
	strscpy(getmsg.devname, wlandev->name, sizeof(getmsg.devname));

	getmsg.mibattribute.did = DIDMSG_DOT11REQ_MIBGET_MIBATTRIBUTE;
	getmsg.mibattribute.status = P80211ENUM_msgitem_status_data_ok;
	getmsg.resultcode.did = DIDMSG_DOT11REQ_MIBGET_RESULTCODE;
	getmsg.resultcode.status = P80211ENUM_msgitem_status_no_value;

	item = (struct p80211itemd *)getmsg.mibattribute.data;
	item->did = DIDMIB_P2_NIC_PRISUPRANGE;
	item->status = P80211ENUM_msgitem_status_no_value;

	data = (u32 *)item->data;

	 
	prism2mgmt_mibset_mibget(wlandev, &getmsg);
	if (getmsg.resultcode.data != P80211ENUM_resultcode_success)
		netdev_err(wlandev->netdev, "Couldn't fetch PRI-SUP info\n");

	 
	priid.role = *data++;
	priid.id = *data++;
	priid.variant = *data++;
	priid.bottom = *data++;
	priid.top = *data++;

	 
	result = read_fwfile(rfptr);
	if (result) {
		netdev_err(wlandev->netdev,
			   "Failed to read the data exiting.\n");
		goto out;
	}

	result = validate_identity();
	if (result) {
		netdev_err(wlandev->netdev, "Incompatible firmware image.\n");
		goto out;
	}

	if (startaddr == 0x00000000) {
		netdev_err(wlandev->netdev,
			   "Can't RAM download a Flash image!\n");
		result = 1;
		goto out;
	}

	 
	result = mkimage(fchunk, &nfchunks);
	if (result) {
		netdev_err(wlandev->netdev, "Failed to make image chunk.\n");
		goto free_chunks;
	}

	 
	result = plugimage(fchunk, nfchunks, s3plug, ns3plug, &pda);
	if (result) {
		netdev_err(wlandev->netdev, "Failed to plug data.\n");
		goto free_chunks;
	}

	 
	result = crcimage(fchunk, nfchunks, s3crc, ns3crc);
	if (result) {
		netdev_err(wlandev->netdev, "Failed to insert all CRCs\n");
		goto free_chunks;
	}

	 
	result = writeimage(wlandev, fchunk, nfchunks);
	if (result) {
		netdev_err(wlandev->netdev, "Failed to ramwrite image data.\n");
		goto free_chunks;
	}

	netdev_info(wlandev->netdev, "prism2_usb: firmware loading finished.\n");

free_chunks:
	 
	free_chunks(fchunk, &nfchunks);
	free_srecs();

out:
	return result;
}

 
static int crcimage(struct imgchunk *fchunk, unsigned int nfchunks,
		    struct s3crcrec *s3crc, unsigned int ns3crc)
{
	int result = 0;
	int i;
	int c;
	u32 crcstart;
	u32 cstart = 0;
	u32 cend;
	u8 *dest;
	u32 chunkoff;

	for (i = 0; i < ns3crc; i++) {
		if (!s3crc[i].dowrite)
			continue;
		crcstart = s3crc[i].addr;
		 
		for (c = 0; c < nfchunks; c++) {
			cstart = fchunk[c].addr;
			cend = fchunk[c].addr + fchunk[c].len;
			 
			 
			 
			 
			 

			 
			 
			if (crcstart - 2 >= cstart && crcstart < cend)
				break;
		}
		if (c >= nfchunks) {
			pr_err("Failed to find chunk for crcrec[%d], addr=0x%06x len=%d , aborting crc.\n",
			       i, s3crc[i].addr, s3crc[i].len);
			return 1;
		}

		 
		pr_debug("Adding crc @ 0x%06x\n", s3crc[i].addr - 2);
		chunkoff = crcstart - cstart - 2;
		dest = fchunk[c].data + chunkoff;
		*dest = 0xde;
		*(dest + 1) = 0xc0;
	}
	return result;
}

 
static void free_chunks(struct imgchunk *fchunk, unsigned int *nfchunks)
{
	int i;

	for (i = 0; i < *nfchunks; i++)
		kfree(fchunk[i].data);

	*nfchunks = 0;
	memset(fchunk, 0, sizeof(*fchunk));
}

 
static void free_srecs(void)
{
	ns3data = 0;
	kfree(s3data);
	ns3plug = 0;
	memset(s3plug, 0, sizeof(s3plug));
	ns3crc = 0;
	memset(s3crc, 0, sizeof(s3crc));
	ns3info = 0;
	memset(s3info, 0, sizeof(s3info));
	startaddr = 0;
}

 
static int mkimage(struct imgchunk *clist, unsigned int *ccnt)
{
	int result = 0;
	int i;
	int j;
	int currchunk = 0;
	u32 nextaddr = 0;
	u32 s3start;
	u32 s3end;
	u32 cstart = 0;
	u32 cend;
	u32 coffset;

	 
	*ccnt = 0;

	 
	for (i = 0; i < ns3data; i++) {
		if (s3data[i].addr == nextaddr) {
			 
			clist[currchunk].len += s3data[i].len;
			nextaddr += s3data[i].len;
		} else {
			 
			(*ccnt)++;
			currchunk = *ccnt - 1;
			clist[currchunk].addr = s3data[i].addr;
			clist[currchunk].len = s3data[i].len;
			nextaddr = s3data[i].addr + s3data[i].len;
			 
			 
			for (j = 0; j < ns3crc; j++) {
				if (s3crc[j].dowrite &&
				    s3crc[j].addr == clist[currchunk].addr) {
					clist[currchunk].addr -= 2;
					clist[currchunk].len += 2;
				}
			}
		}
	}

	 
	 

	 
	for (i = 0; i < *ccnt; i++) {
		clist[i].data = kzalloc(clist[i].len, GFP_KERNEL);
		if (!clist[i].data)
			return 1;

		pr_debug("chunk[%d]: addr=0x%06x len=%d\n",
			 i, clist[i].addr, clist[i].len);
	}

	 
	for (i = 0; i < ns3data; i++) {
		s3start = s3data[i].addr;
		s3end = s3start + s3data[i].len - 1;
		for (j = 0; j < *ccnt; j++) {
			cstart = clist[j].addr;
			cend = cstart + clist[j].len - 1;
			if (s3start >= cstart && s3end <= cend)
				break;
		}
		if (((unsigned int)j) >= (*ccnt)) {
			pr_err("s3rec(a=0x%06x,l=%d), no chunk match, exiting.\n",
			       s3start, s3data[i].len);
			return 1;
		}
		coffset = s3start - cstart;
		memcpy(clist[j].data + coffset, s3data[i].data, s3data[i].len);
	}

	return result;
}

 
static int mkpdrlist(struct pda *pda)
{
	__le16 *pda16 = (__le16 *)pda->buf;
	int curroff;		 

	pda->nrec = 0;
	curroff = 0;
	while (curroff < (HFA384x_PDA_LEN_MAX / 2 - 1) &&
	       le16_to_cpu(pda16[curroff + 1]) != HFA384x_PDR_END_OF_PDA) {
		pda->rec[pda->nrec] = (struct hfa384x_pdrec *)&pda16[curroff];

		if (le16_to_cpu(pda->rec[pda->nrec]->code) ==
		    HFA384x_PDR_NICID) {
			memcpy(&nicid, &pda->rec[pda->nrec]->data.nicid,
			       sizeof(nicid));
			le16_to_cpus(&nicid.id);
			le16_to_cpus(&nicid.variant);
			le16_to_cpus(&nicid.major);
			le16_to_cpus(&nicid.minor);
		}
		if (le16_to_cpu(pda->rec[pda->nrec]->code) ==
		    HFA384x_PDR_MFISUPRANGE) {
			memcpy(&rfid, &pda->rec[pda->nrec]->data.mfisuprange,
			       sizeof(rfid));
			le16_to_cpus(&rfid.id);
			le16_to_cpus(&rfid.variant);
			le16_to_cpus(&rfid.bottom);
			le16_to_cpus(&rfid.top);
		}
		if (le16_to_cpu(pda->rec[pda->nrec]->code) ==
		    HFA384x_PDR_CFISUPRANGE) {
			memcpy(&macid, &pda->rec[pda->nrec]->data.cfisuprange,
			       sizeof(macid));
			le16_to_cpus(&macid.id);
			le16_to_cpus(&macid.variant);
			le16_to_cpus(&macid.bottom);
			le16_to_cpus(&macid.top);
		}

		(pda->nrec)++;
		curroff += le16_to_cpu(pda16[curroff]) + 1;
	}
	if (curroff >= (HFA384x_PDA_LEN_MAX / 2 - 1)) {
		pr_err("no end record found or invalid lengths in PDR data, exiting. %x %d\n",
		       curroff, pda->nrec);
		return 1;
	}
	pda->rec[pda->nrec] = (struct hfa384x_pdrec *)&pda16[curroff];
	(pda->nrec)++;
	return 0;
}

 
static int plugimage(struct imgchunk *fchunk, unsigned int nfchunks,
		     struct s3plugrec *s3plug, unsigned int ns3plug,
		     struct pda *pda)
{
	int result = 0;
	int i;			 
	int j;			 
	int c;			 
	u32 pstart;
	u32 pend;
	u32 cstart = 0;
	u32 cend;
	u32 chunkoff;
	u8 *dest;

	 
	for (i = 0; i < ns3plug; i++) {
		pstart = s3plug[i].addr;
		pend = s3plug[i].addr + s3plug[i].len;
		j = -1;
		 
		if (s3plug[i].itemcode != 0xffffffffUL) {  
			for (j = 0; j < pda->nrec; j++) {
				if (s3plug[i].itemcode ==
				    le16_to_cpu(pda->rec[j]->code))
					break;
			}
		}
		if (j >= pda->nrec && j != -1) {  
			pr_warn("warning: Failed to find PDR for plugrec 0x%04x.\n",
				s3plug[i].itemcode);
			continue;	 

			 
		}

		 
		if (j != -1 && s3plug[i].len < le16_to_cpu(pda->rec[j]->len)) {
			pr_err("error: Plug vs. PDR len mismatch for plugrec 0x%04x, abort plugging.\n",
			       s3plug[i].itemcode);
			result = 1;
			continue;
		}

		 
		for (c = 0; c < nfchunks; c++) {
			cstart = fchunk[c].addr;
			cend = fchunk[c].addr + fchunk[c].len;
			if (pstart >= cstart && pend <= cend)
				break;
		}
		if (c >= nfchunks) {
			pr_err("error: Failed to find image chunk for plugrec 0x%04x.\n",
			       s3plug[i].itemcode);
			result = 1;
			continue;
		}

		 
		chunkoff = pstart - cstart;
		dest = fchunk[c].data + chunkoff;
		pr_debug("Plugging item 0x%04x @ 0x%06x, len=%d, cnum=%d coff=0x%06x\n",
			 s3plug[i].itemcode, pstart, s3plug[i].len,
			 c, chunkoff);

		if (j == -1) {	 
			memset(dest, 0, s3plug[i].len);
			strncpy(dest, PRISM2_USB_FWFILE, s3plug[i].len - 1);
		} else {	 
			memcpy(dest, &pda->rec[j]->data, s3plug[i].len);
		}
	}
	return result;
}

 
static int read_cardpda(struct pda *pda, struct wlandevice *wlandev)
{
	int result = 0;
	struct p80211msg_p2req_readpda *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	 
	msg->msgcode = DIDMSG_P2REQ_READPDA;
	msg->msglen = sizeof(msg);
	strscpy(msg->devname, wlandev->name, sizeof(msg->devname));
	msg->pda.did = DIDMSG_P2REQ_READPDA_PDA;
	msg->pda.len = HFA384x_PDA_LEN_MAX;
	msg->pda.status = P80211ENUM_msgitem_status_no_value;
	msg->resultcode.did = DIDMSG_P2REQ_READPDA_RESULTCODE;
	msg->resultcode.len = sizeof(u32);
	msg->resultcode.status = P80211ENUM_msgitem_status_no_value;

	if (prism2mgmt_readpda(wlandev, msg) != 0) {
		 
		result = -1;
	} else if (msg->resultcode.data == P80211ENUM_resultcode_success) {
		memcpy(pda->buf, msg->pda.data, HFA384x_PDA_LEN_MAX);
		result = mkpdrlist(pda);
	} else {
		 
		result = -1;
	}

	kfree(msg);
	return result;
}

 
static int read_fwfile(const struct ihex_binrec *record)
{
	int		i;
	int		rcnt = 0;
	u16		*tmpinfo;
	u16		*ptr16;
	u32		*ptr32, len, addr;

	pr_debug("Reading fw file ...\n");

	while (record) {
		rcnt++;

		len = be16_to_cpu(record->len);
		addr = be32_to_cpu(record->addr);

		 
		ptr32 = (u32 *)record->data;
		ptr16 = (u16 *)record->data;

		 
		switch (addr) {
		case S3ADDR_START:
			startaddr = *ptr32;
			pr_debug("  S7 start addr, record=%d addr=0x%08x\n",
				 rcnt,
				 startaddr);
			break;
		case S3ADDR_PLUG:
			s3plug[ns3plug].itemcode = *ptr32;
			s3plug[ns3plug].addr = *(ptr32 + 1);
			s3plug[ns3plug].len = *(ptr32 + 2);

			pr_debug("  S3 plugrec, record=%d itemcode=0x%08x addr=0x%08x len=%d\n",
				 rcnt,
				 s3plug[ns3plug].itemcode,
				 s3plug[ns3plug].addr,
				 s3plug[ns3plug].len);

			ns3plug++;
			if (ns3plug == S3PLUG_MAX) {
				pr_err("S3 plugrec limit reached - aborting\n");
				return 1;
			}
			break;
		case S3ADDR_CRC:
			s3crc[ns3crc].addr = *ptr32;
			s3crc[ns3crc].len = *(ptr32 + 1);
			s3crc[ns3crc].dowrite = *(ptr32 + 2);

			pr_debug("  S3 crcrec, record=%d addr=0x%08x len=%d write=0x%08x\n",
				 rcnt,
				 s3crc[ns3crc].addr,
				 s3crc[ns3crc].len,
				 s3crc[ns3crc].dowrite);
			ns3crc++;
			if (ns3crc == S3CRC_MAX) {
				pr_err("S3 crcrec limit reached - aborting\n");
				return 1;
			}
			break;
		case S3ADDR_INFO:
			s3info[ns3info].len = *ptr16;
			s3info[ns3info].type = *(ptr16 + 1);

			pr_debug("  S3 inforec, record=%d len=0x%04x type=0x%04x\n",
				 rcnt,
				 s3info[ns3info].len,
				 s3info[ns3info].type);
			if (((s3info[ns3info].len - 1) * sizeof(u16)) >
			   sizeof(s3info[ns3info].info)) {
				pr_err("S3 inforec length too long - aborting\n");
				return 1;
			}

			tmpinfo = (u16 *)&s3info[ns3info].info.version;
			pr_debug("            info=");
			for (i = 0; i < s3info[ns3info].len - 1; i++) {
				tmpinfo[i] = *(ptr16 + 2 + i);
				pr_debug("%04x ", tmpinfo[i]);
			}
			pr_debug("\n");

			ns3info++;
			if (ns3info == S3INFO_MAX) {
				pr_err("S3 inforec limit reached - aborting\n");
				return 1;
			}
			break;
		default:	 
			s3data[ns3data].addr = addr;
			s3data[ns3data].len = len;
			s3data[ns3data].data = (uint8_t *)record->data;
			ns3data++;
			if (ns3data == S3DATA_MAX) {
				pr_err("S3 datarec limit reached - aborting\n");
				return 1;
			}
			break;
		}
		record = ihex_next_binrec(record);
	}
	return 0;
}

 
static int writeimage(struct wlandevice *wlandev, struct imgchunk *fchunk,
		      unsigned int nfchunks)
{
	int result = 0;
	struct p80211msg_p2req_ramdl_state *rstmsg;
	struct p80211msg_p2req_ramdl_write *rwrmsg;
	u32 resultcode;
	int i;
	int j;
	unsigned int nwrites;
	u32 curroff;
	u32 currlen;
	u32 currdaddr;

	rstmsg = kzalloc(sizeof(*rstmsg), GFP_KERNEL);
	rwrmsg = kzalloc(sizeof(*rwrmsg), GFP_KERNEL);
	if (!rstmsg || !rwrmsg) {
		netdev_err(wlandev->netdev,
			   "%s: no memory for firmware download, aborting download\n",
			   __func__);
		result = -ENOMEM;
		goto free_result;
	}

	 
	strscpy(rstmsg->devname, wlandev->name, sizeof(rstmsg->devname));
	rstmsg->msgcode = DIDMSG_P2REQ_RAMDL_STATE;
	rstmsg->msglen = sizeof(*rstmsg);
	rstmsg->enable.did = DIDMSG_P2REQ_RAMDL_STATE_ENABLE;
	rstmsg->exeaddr.did = DIDMSG_P2REQ_RAMDL_STATE_EXEADDR;
	rstmsg->resultcode.did = DIDMSG_P2REQ_RAMDL_STATE_RESULTCODE;
	rstmsg->enable.status = P80211ENUM_msgitem_status_data_ok;
	rstmsg->exeaddr.status = P80211ENUM_msgitem_status_data_ok;
	rstmsg->resultcode.status = P80211ENUM_msgitem_status_no_value;
	rstmsg->enable.len = sizeof(u32);
	rstmsg->exeaddr.len = sizeof(u32);
	rstmsg->resultcode.len = sizeof(u32);

	strscpy(rwrmsg->devname, wlandev->name, sizeof(rwrmsg->devname));
	rwrmsg->msgcode = DIDMSG_P2REQ_RAMDL_WRITE;
	rwrmsg->msglen = sizeof(*rwrmsg);
	rwrmsg->addr.did = DIDMSG_P2REQ_RAMDL_WRITE_ADDR;
	rwrmsg->len.did = DIDMSG_P2REQ_RAMDL_WRITE_LEN;
	rwrmsg->data.did = DIDMSG_P2REQ_RAMDL_WRITE_DATA;
	rwrmsg->resultcode.did = DIDMSG_P2REQ_RAMDL_WRITE_RESULTCODE;
	rwrmsg->addr.status = P80211ENUM_msgitem_status_data_ok;
	rwrmsg->len.status = P80211ENUM_msgitem_status_data_ok;
	rwrmsg->data.status = P80211ENUM_msgitem_status_data_ok;
	rwrmsg->resultcode.status = P80211ENUM_msgitem_status_no_value;
	rwrmsg->addr.len = sizeof(u32);
	rwrmsg->len.len = sizeof(u32);
	rwrmsg->data.len = WRITESIZE_MAX;
	rwrmsg->resultcode.len = sizeof(u32);

	 
	pr_debug("Sending dl_state(enable) message.\n");
	rstmsg->enable.data = P80211ENUM_truth_true;
	rstmsg->exeaddr.data = startaddr;

	result = prism2mgmt_ramdl_state(wlandev, rstmsg);
	if (result) {
		netdev_err(wlandev->netdev,
			   "%s state enable failed w/ result=%d, aborting download\n",
			   __func__, result);
		goto free_result;
	}
	resultcode = rstmsg->resultcode.data;
	if (resultcode != P80211ENUM_resultcode_success) {
		netdev_err(wlandev->netdev,
			   "%s()->xxxdl_state msg indicates failure, w/ resultcode=%d, aborting download.\n",
			   __func__, resultcode);
		result = 1;
		goto free_result;
	}

	 
	for (i = 0; i < nfchunks; i++) {
		nwrites = fchunk[i].len / WRITESIZE_MAX;
		nwrites += (fchunk[i].len % WRITESIZE_MAX) ? 1 : 0;
		curroff = 0;
		for (j = 0; j < nwrites; j++) {
			 
			int lenleft = fchunk[i].len - (WRITESIZE_MAX * j);

			if (fchunk[i].len > WRITESIZE_MAX)
				currlen = WRITESIZE_MAX;
			else
				currlen = lenleft;
			curroff = j * WRITESIZE_MAX;
			currdaddr = fchunk[i].addr + curroff;
			 
			rwrmsg->addr.data = currdaddr;
			rwrmsg->len.data = currlen;
			memcpy(rwrmsg->data.data,
			       fchunk[i].data + curroff, currlen);

			 
			pr_debug
			    ("Sending xxxdl_write message addr=%06x len=%d.\n",
			     currdaddr, currlen);

			result = prism2mgmt_ramdl_write(wlandev, rwrmsg);

			 
			if (result) {
				netdev_err(wlandev->netdev,
					   "%s chunk write failed w/ result=%d, aborting download\n",
					   __func__, result);
				goto free_result;
			}
			resultcode = rstmsg->resultcode.data;
			if (resultcode != P80211ENUM_resultcode_success) {
				pr_err("%s()->xxxdl_write msg indicates failure, w/ resultcode=%d, aborting download.\n",
				       __func__, resultcode);
				result = 1;
				goto free_result;
			}
		}
	}

	 
	pr_debug("Sending dl_state(disable) message.\n");
	rstmsg->enable.data = P80211ENUM_truth_false;
	rstmsg->exeaddr.data = 0;

	result = prism2mgmt_ramdl_state(wlandev, rstmsg);
	if (result) {
		netdev_err(wlandev->netdev,
			   "%s state disable failed w/ result=%d, aborting download\n",
			   __func__, result);
		goto free_result;
	}
	resultcode = rstmsg->resultcode.data;
	if (resultcode != P80211ENUM_resultcode_success) {
		netdev_err(wlandev->netdev,
			   "%s()->xxxdl_state msg indicates failure, w/ resultcode=%d, aborting download.\n",
			   __func__, resultcode);
		result = 1;
		goto free_result;
	}

free_result:
	kfree(rstmsg);
	kfree(rwrmsg);
	return result;
}

static int validate_identity(void)
{
	int i;
	int result = 1;
	int trump = 0;

	pr_debug("NIC ID: %#x v%d.%d.%d\n",
		 nicid.id, nicid.major, nicid.minor, nicid.variant);
	pr_debug("MFI ID: %#x v%d %d->%d\n",
		 rfid.id, rfid.variant, rfid.bottom, rfid.top);
	pr_debug("CFI ID: %#x v%d %d->%d\n",
		 macid.id, macid.variant, macid.bottom, macid.top);
	pr_debug("PRI ID: %#x v%d %d->%d\n",
		 priid.id, priid.variant, priid.bottom, priid.top);

	for (i = 0; i < ns3info; i++) {
		switch (s3info[i].type) {
		case 1:
			pr_debug("Version:  ID %#x %d.%d.%d\n",
				 s3info[i].info.version.id,
				 s3info[i].info.version.major,
				 s3info[i].info.version.minor,
				 s3info[i].info.version.variant);
			break;
		case 2:
			pr_debug("Compat: Role %#x Id %#x v%d %d->%d\n",
				 s3info[i].info.compat.role,
				 s3info[i].info.compat.id,
				 s3info[i].info.compat.variant,
				 s3info[i].info.compat.bottom,
				 s3info[i].info.compat.top);

			 
			if ((s3info[i].info.compat.role == 1) &&
			    (s3info[i].info.compat.id == 2)) {
				if (s3info[i].info.compat.variant !=
				    macid.variant) {
					result = 2;
				}
			}

			 
			if ((s3info[i].info.compat.role == 1) &&
			    (s3info[i].info.compat.id == 3)) {
				if ((s3info[i].info.compat.bottom >
				     priid.top) ||
				    (s3info[i].info.compat.top <
				     priid.bottom)) {
					result = 3;
				}
			}
			 
			if ((s3info[i].info.compat.role == 1) &&
			    (s3info[i].info.compat.id == 4)) {
				 
			}

			break;
		case 3:
			pr_debug("Seq: %#x\n", s3info[i].info.buildseq);

			break;
		case 4:
			pr_debug("Platform:  ID %#x %d.%d.%d\n",
				 s3info[i].info.version.id,
				 s3info[i].info.version.major,
				 s3info[i].info.version.minor,
				 s3info[i].info.version.variant);

			if (nicid.id != s3info[i].info.version.id)
				continue;
			if (nicid.major != s3info[i].info.version.major)
				continue;
			if (nicid.minor != s3info[i].info.version.minor)
				continue;
			if ((nicid.variant != s3info[i].info.version.variant) &&
			    (nicid.id != 0x8008))
				continue;

			trump = 1;
			break;
		case 0x8001:
			pr_debug("name inforec len %d\n", s3info[i].len);

			break;
		default:
			pr_debug("Unknown inforec type %d\n", s3info[i].type);
		}
	}
	 

	if (trump && (result != 2))
		result = 0;
	return result;
}
