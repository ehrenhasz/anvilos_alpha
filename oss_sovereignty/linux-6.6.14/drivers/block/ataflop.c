
 

#include <linux/module.h>

#include <linux/fd.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/blk-mq.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/wait.h>

#include <asm/atariints.h>
#include <asm/atari_stdma.h>
#include <asm/atari_stram.h>

#define	FD_MAX_UNITS 2

#undef DEBUG

static DEFINE_MUTEX(ataflop_mutex);
static struct request *fd_request;

 

 

#define FDCSELREG_STP   (0x80)    
#define FDCSELREG_TRA   (0x82)    
#define FDCSELREG_SEC   (0x84)    
#define FDCSELREG_DTA   (0x86)    

 

#define FDCREG_CMD		0
#define FDCREG_STATUS	0
#define FDCREG_TRACK	2
#define FDCREG_SECTOR	4
#define FDCREG_DATA		6

 

#define FDCCMD_RESTORE  (0x00)    
#define FDCCMD_SEEK     (0x10)    
#define FDCCMD_STEP     (0x20)    
#define FDCCMD_STIN     (0x40)    
#define FDCCMD_STOT     (0x60)    
#define FDCCMD_RDSEC    (0x80)    
#define FDCCMD_WRSEC    (0xa0)    
#define FDCCMD_RDADR    (0xc0)    
#define FDCCMD_RDTRA    (0xe0)    
#define FDCCMD_WRTRA    (0xf0)    
#define FDCCMD_FORCI    (0xd0)    

 

#define FDCCMDADD_SR6   (0x00)    
#define FDCCMDADD_SR12  (0x01)
#define FDCCMDADD_SR2   (0x02)
#define FDCCMDADD_SR3   (0x03)
#define FDCCMDADD_V     (0x04)    
#define FDCCMDADD_H     (0x08)    
#define FDCCMDADD_U     (0x10)    
#define FDCCMDADD_M     (0x10)    
#define FDCCMDADD_E     (0x04)    
#define FDCCMDADD_P     (0x02)    
#define FDCCMDADD_A0    (0x01)    

 

#define	FDCSTAT_MOTORON	(0x80)    
#define	FDCSTAT_WPROT	(0x40)    
#define	FDCSTAT_SPINUP	(0x20)    
#define	FDCSTAT_DELDAM	(0x20)    
#define	FDCSTAT_RECNF	(0x10)    
#define	FDCSTAT_CRC		(0x08)    
#define	FDCSTAT_TR00	(0x04)    
#define	FDCSTAT_LOST	(0x04)    
#define	FDCSTAT_IDX		(0x02)    
#define	FDCSTAT_DRQ		(0x02)    
#define	FDCSTAT_BUSY	(0x01)    


 
#define DSKSIDE     (0x01)

#define DSKDRVNONE  (0x06)
#define DSKDRV0     (0x02)
#define DSKDRV1     (0x04)

 
#define	FDCSTEP_6	0x00
#define	FDCSTEP_12	0x01
#define	FDCSTEP_2	0x02
#define	FDCSTEP_3	0x03

struct atari_format_descr {
	int track;		 
	int head;		 
	int sect_offset;	 
};

 
static struct atari_disk_type {
	const char	*name;
	unsigned	spt;		 
	unsigned	blocks;		 
	unsigned	fdc_speed;	 
	unsigned 	stretch;	 
} atari_disk_type[] = {
	{ "d360",  9, 720, 0, 0},	 
	{ "D360",  9, 720, 0, 1},	 
	{ "D720",  9,1440, 0, 0},	 
	{ "D820", 10,1640, 0, 0},	 
 
#define	MAX_TYPE_DD 3
	{ "h1200",15,2400, 3, 0},	 
	{ "H1440",18,2880, 3, 0},	 
	{ "H1640",20,3280, 3, 0},	 
 
#define	MAX_TYPE_HD 6
	{ "E2880",36,5760, 3, 0},	 
	{ "E3280",40,6560, 3, 0},	 
 
#define	MAX_TYPE_ED 8
 
	{ "H1680",21,3360, 3, 0},	 
	{ "h410",10,820, 0, 1},		 
	{ "h1476",18,2952, 3, 0},	 
	{ "H1722",21,3444, 3, 0},	 
	{ "h420",10,840, 0, 1},		 
	{ "H830",10,1660, 0, 0},	 
	{ "h1494",18,2952, 3, 0},	 
	{ "H1743",21,3486, 3, 0},	 
	{ "h880",11,1760, 0, 0},	 
	{ "D1040",13,2080, 0, 0},	 
	{ "D1120",14,2240, 0, 0},	 
	{ "h1600",20,3200, 3, 0},	 
	{ "H1760",22,3520, 3, 0},	 
	{ "H1920",24,3840, 3, 0},	 
	{ "E3200",40,6400, 3, 0},	 
	{ "E3520",44,7040, 3, 0},	 
	{ "E3840",48,7680, 3, 0},	 
	{ "H1840",23,3680, 3, 0},	 
	{ "D800",10,1600, 0, 0},	 
};

static int StartDiskType[] = {
	MAX_TYPE_DD,
	MAX_TYPE_HD,
	MAX_TYPE_ED
};

#define	TYPE_DD		0
#define	TYPE_HD		1
#define	TYPE_ED		2

static int DriveType = TYPE_HD;

static DEFINE_SPINLOCK(ataflop_lock);

 
static struct {
	int 	 index;
	unsigned drive_types;
} minor2disktype[] = {
	{  0, TYPE_DD },	 
	{  4, TYPE_HD },	 
	{  1, TYPE_DD },	 
	{  2, TYPE_DD },	 
	{  1, TYPE_DD },	 
	{  2, TYPE_DD },	 
	{  5, TYPE_HD },	 
	{  7, TYPE_ED },	 
 
	{  8, TYPE_ED },	 
	{  5, TYPE_HD },	 
	{  9, TYPE_HD },	 
	{ 10, TYPE_DD },	 
	{  3, TYPE_DD },	 
	{ 11, TYPE_HD },	 
	{ 12, TYPE_HD },	 
	{ 13, TYPE_DD },	 
	{ 14, TYPE_DD },	 
	{ 15, TYPE_HD },	 
	{ 16, TYPE_HD },	 
	{ 17, TYPE_DD },	 
	{ 18, TYPE_DD },	 
	{ 19, TYPE_DD },	 
	{ 20, TYPE_HD },	 
	{ 21, TYPE_HD },	 
	{ 22, TYPE_HD },	 
	{ 23, TYPE_ED },	 
	{ 24, TYPE_ED },	 
	{ 25, TYPE_ED },	 
	{ 26, TYPE_HD },	 
	{ 27, TYPE_DD },	 
	{  6, TYPE_HD },	 
};

#define NUM_DISK_MINORS ARRAY_SIZE(minor2disktype)

 
#define MAX_DISK_SIZE 3280

 
static struct atari_disk_type user_params[FD_MAX_UNITS];

 
static struct atari_disk_type default_params[FD_MAX_UNITS];

 
static struct atari_floppy_struct {
	int connected;				 
	int autoprobe;				 

	struct atari_disk_type	*disktype;	 

	int track;		 
	unsigned int steprate;	 
	unsigned int wpstat;	 
	int flags;		 
	struct gendisk *disk[NUM_DISK_MINORS];
	bool registered[NUM_DISK_MINORS];
	int ref;
	int type;
	struct blk_mq_tag_set tag_set;
	int error_count;
} unit[FD_MAX_UNITS];

#define	UD	unit[drive]
#define	UDT	unit[drive].disktype
#define	SUD	unit[SelectedDrive]
#define	SUDT	unit[SelectedDrive].disktype


#define FDC_READ(reg) ({			\
     		\
    unsigned short __val;			\
     		\
    dma_wd.dma_mode_status = 0x80 | (reg);	\
    udelay(25);					\
    __val = dma_wd.fdc_acces_seccount;		\
    MFPDELAY();					\
     		\
    __val & 0xff;				\
})

#define FDC_WRITE(reg,val)			\
    do {					\
	 		\
	 		\
	dma_wd.dma_mode_status = 0x80 | (reg);	\
	udelay(25);				\
	dma_wd.fdc_acces_seccount = (val);	\
	MFPDELAY();				\
         	\
    } while(0)


 

static int MaxSectors[] = {
	11, 22, 44
};
static int BufferSize[] = {
	15*512, 30*512, 60*512
};

#define	BUFFER_SIZE	(BufferSize[DriveType])

unsigned char *DMABuffer;			   
static unsigned long PhysDMABuffer;    

static int UseTrackbuffer = -1;		   
module_param(UseTrackbuffer, int, 0);

unsigned char *TrackBuffer;			   
static unsigned long PhysTrackBuffer;  
static int BufferDrive, BufferSide, BufferTrack;
static int read_track;		 

#define	SECTOR_BUFFER(sec)	(TrackBuffer + ((sec)-1)*512)
#define	IS_BUFFERED(drive,side,track) \
    (BufferDrive == (drive) && BufferSide == (side) && BufferTrack == (track))

 
static int SelectedDrive = 0;
static int ReqCmd, ReqBlock;
static int ReqSide, ReqTrack, ReqSector, ReqCnt;
static int HeadSettleFlag = 0;
static unsigned char *ReqData, *ReqBuffer;
static int MotorOn = 0, MotorOffTrys;
static int IsFormatting = 0, FormatError;

static int UserSteprate[FD_MAX_UNITS] = { -1, -1 };
module_param_array(UserSteprate, int, NULL, 0);

static DECLARE_COMPLETION(format_wait);

static unsigned long changed_floppies = 0xff, fake_change = 0;
#define	CHECK_CHANGE_DELAY	HZ/2

#define	FD_MOTOR_OFF_DELAY	(3*HZ)
#define	FD_MOTOR_OFF_MAXTRY	(10*20)

#define FLOPPY_TIMEOUT		(6*HZ)
#define RECALIBRATE_ERRORS	4	 
#define MAX_ERRORS		8	 


 
static int Probing = 0;

 
static int NeedSeek = 0;


#ifdef DEBUG
#define DPRINT(a)	printk a
#else
#define DPRINT(a)
#endif

 

static void fd_select_side( int side );
static void fd_select_drive( int drive );
static void fd_deselect( void );
static void fd_motor_off_timer(struct timer_list *unused);
static void check_change(struct timer_list *unused);
static irqreturn_t floppy_irq (int irq, void *dummy);
static void fd_error( void );
static int do_format(int drive, int type, struct atari_format_descr *desc);
static void do_fd_action( int drive );
static void fd_calibrate( void );
static void fd_calibrate_done( int status );
static void fd_seek( void );
static void fd_seek_done( int status );
static void fd_rwsec( void );
static void fd_readtrack_check(struct timer_list *unused);
static void fd_rwsec_done( int status );
static void fd_rwsec_done1(int status);
static void fd_writetrack( void );
static void fd_writetrack_done( int status );
static void fd_times_out(struct timer_list *unused);
static void finish_fdc( void );
static void finish_fdc_done( int dummy );
static void setup_req_params( int drive );
static int fd_locked_ioctl(struct block_device *bdev, blk_mode_t mode,
		unsigned int cmd, unsigned long param);
static void fd_probe( int drive );
static int fd_test_drive_present( int drive );
static void config_types( void );
static int floppy_open(struct gendisk *disk, blk_mode_t mode);
static void floppy_release(struct gendisk *disk);

 

static DEFINE_TIMER(motor_off_timer, fd_motor_off_timer);
static DEFINE_TIMER(readtrack_timer, fd_readtrack_check);
static DEFINE_TIMER(timeout_timer, fd_times_out);
static DEFINE_TIMER(fd_timer, check_change);
	
static void fd_end_request_cur(blk_status_t err)
{
	DPRINT(("fd_end_request_cur(), bytes %d of %d\n",
		blk_rq_cur_bytes(fd_request),
		blk_rq_bytes(fd_request)));

	if (!blk_update_request(fd_request, err,
				blk_rq_cur_bytes(fd_request))) {
		DPRINT(("calling __blk_mq_end_request()\n"));
		__blk_mq_end_request(fd_request, err);
		fd_request = NULL;
	} else {
		 
		DPRINT(("calling blk_mq_requeue_request()\n"));
		blk_mq_requeue_request(fd_request, true);
		fd_request = NULL;
	}
}

static inline void start_motor_off_timer(void)
{
	mod_timer(&motor_off_timer, jiffies + FD_MOTOR_OFF_DELAY);
	MotorOffTrys = 0;
}

static inline void start_check_change_timer( void )
{
	mod_timer(&fd_timer, jiffies + CHECK_CHANGE_DELAY);
}

static inline void start_timeout(void)
{
	mod_timer(&timeout_timer, jiffies + FLOPPY_TIMEOUT);
}

static inline void stop_timeout(void)
{
	del_timer(&timeout_timer);
}

 

static void fd_select_side( int side )
{
	unsigned long flags;

	 
	local_irq_save(flags);
  
	sound_ym.rd_data_reg_sel = 14;  
	sound_ym.wd_data = (side == 0) ? sound_ym.rd_data_reg_sel | 0x01 :
	                                 sound_ym.rd_data_reg_sel & 0xfe;

	local_irq_restore(flags);
}


 

static void fd_select_drive( int drive )
{
	unsigned long flags;
	unsigned char tmp;
  
	if (drive == SelectedDrive)
	  return;

	 
	local_irq_save(flags);
	sound_ym.rd_data_reg_sel = 14;  
	tmp = sound_ym.rd_data_reg_sel;
	sound_ym.wd_data = (tmp | DSKDRVNONE) & ~(drive == 0 ? DSKDRV0 : DSKDRV1);
	atari_dont_touch_floppy_select = 1;
	local_irq_restore(flags);

	 
	FDC_WRITE( FDCREG_TRACK, UD.track );
	udelay(25);

	 
	if (UDT)
		if (ATARIHW_PRESENT(FDCSPEED))
			dma_wd.fdc_speed = UDT->fdc_speed;
	
	SelectedDrive = drive;
}


 

static void fd_deselect( void )
{
	unsigned long flags;

	 
	local_irq_save(flags);
	atari_dont_touch_floppy_select = 0;
	sound_ym.rd_data_reg_sel=14;	 
	sound_ym.wd_data = (sound_ym.rd_data_reg_sel |
			    (MACH_IS_FALCON ? 3 : 7));  
	 
	SelectedDrive = -1;
	local_irq_restore(flags);
}


 

static void fd_motor_off_timer(struct timer_list *unused)
{
	unsigned char status;

	if (SelectedDrive < 0)
		 
		return;

	if (stdma_islocked())
		goto retry;

	status = FDC_READ( FDCREG_STATUS );

	if (!(status & 0x80)) {
		 
		MotorOn = 0;
		fd_deselect();
		return;
	}
	 

  retry:
	 
	mod_timer(&motor_off_timer,
		  jiffies + (MotorOffTrys++ < FD_MOTOR_OFF_MAXTRY ? HZ/20 : HZ/2));
}


 

static void check_change(struct timer_list *unused)
{
	static int    drive = 0;

	unsigned long flags;
	unsigned char old_porta;
	int			  stat;

	if (++drive > 1 || !UD.connected)
		drive = 0;

	 
	local_irq_save(flags);

	if (!stdma_islocked()) {
		sound_ym.rd_data_reg_sel = 14;
		old_porta = sound_ym.rd_data_reg_sel;
		sound_ym.wd_data = (old_porta | DSKDRVNONE) &
			               ~(drive == 0 ? DSKDRV0 : DSKDRV1);
		stat = !!(FDC_READ( FDCREG_STATUS ) & FDCSTAT_WPROT);
		sound_ym.wd_data = old_porta;

		if (stat != UD.wpstat) {
			DPRINT(( "wpstat[%d] = %d\n", drive, stat ));
			UD.wpstat = stat;
			set_bit (drive, &changed_floppies);
		}
	}
	local_irq_restore(flags);

	start_check_change_timer();
}

 
 

static inline void set_head_settle_flag(void)
{
	HeadSettleFlag = FDCCMDADD_E;
}

static inline int get_head_settle_flag(void)
{
	int	tmp = HeadSettleFlag;
	HeadSettleFlag = 0;
	return( tmp );
}

static inline void copy_buffer(void *from, void *to)
{
	ulong *p1 = (ulong *)from, *p2 = (ulong *)to;
	int cnt;

	for (cnt = 512/4; cnt; cnt--)
		*p2++ = *p1++;
}

 

static void (*FloppyIRQHandler)( int status ) = NULL;

static irqreturn_t floppy_irq (int irq, void *dummy)
{
	unsigned char status;
	void (*handler)( int );

	handler = xchg(&FloppyIRQHandler, NULL);

	if (handler) {
		nop();
		status = FDC_READ( FDCREG_STATUS );
		DPRINT(("FDC irq, status = %02x handler = %08lx\n",status,(unsigned long)handler));
		handler( status );
	}
	else {
		DPRINT(("FDC irq, no handler\n"));
	}
	return IRQ_HANDLED;
}


 

static void fd_error( void )
{
	if (IsFormatting) {
		IsFormatting = 0;
		FormatError = 1;
		complete(&format_wait);
		return;
	}

	if (!fd_request)
		return;

	unit[SelectedDrive].error_count++;
	if (unit[SelectedDrive].error_count >= MAX_ERRORS) {
		printk(KERN_ERR "fd%d: too many errors.\n", SelectedDrive );
		fd_end_request_cur(BLK_STS_IOERR);
		finish_fdc();
		return;
	}
	else if (unit[SelectedDrive].error_count == RECALIBRATE_ERRORS) {
		printk(KERN_WARNING "fd%d: recalibrating\n", SelectedDrive );
		if (SelectedDrive != -1)
			SUD.track = -1;
	}
	 
	atari_disable_irq( IRQ_MFP_FDC );

	setup_req_params( SelectedDrive );
	do_fd_action( SelectedDrive );

	atari_enable_irq( IRQ_MFP_FDC );
}



#define	SET_IRQ_HANDLER(proc) do { FloppyIRQHandler = (proc); } while(0)


 

#define FILL(n,val)		\
    do {			\
	memset( p, val, n );	\
	p += n;			\
    } while(0)

static int do_format(int drive, int type, struct atari_format_descr *desc)
{
	struct request_queue *q;
	unsigned char	*p;
	int sect, nsect;
	unsigned long	flags;
	int ret;

	if (type) {
		type--;
		if (type >= NUM_DISK_MINORS ||
		    minor2disktype[type].drive_types > DriveType) {
			finish_fdc();
			return -EINVAL;
		}
	}

	q = unit[drive].disk[type]->queue;
	blk_mq_freeze_queue(q);
	blk_mq_quiesce_queue(q);

	local_irq_save(flags);
	stdma_lock(floppy_irq, NULL);
	atari_turnon_irq( IRQ_MFP_FDC );  
	local_irq_restore(flags);

	if (type) {
		type = minor2disktype[type].index;
		UDT = &atari_disk_type[type];
	}

	if (!UDT || desc->track >= UDT->blocks/UDT->spt/2 || desc->head >= 2) {
		finish_fdc();
		ret = -EINVAL;
		goto out;
	}

	nsect = UDT->spt;
	p = TrackBuffer;
	 
	BufferDrive = -1;
	 
	del_timer( &motor_off_timer );

	FILL( 60 * (nsect / 9), 0x4e );
	for( sect = 0; sect < nsect; ++sect ) {
		FILL( 12, 0 );
		FILL( 3, 0xf5 );
		*p++ = 0xfe;
		*p++ = desc->track;
		*p++ = desc->head;
		*p++ = (nsect + sect - desc->sect_offset) % nsect + 1;
		*p++ = 2;
		*p++ = 0xf7;
		FILL( 22, 0x4e );
		FILL( 12, 0 );
		FILL( 3, 0xf5 );
		*p++ = 0xfb;
		FILL( 512, 0xe5 );
		*p++ = 0xf7;
		FILL( 40, 0x4e );
	}
	FILL( TrackBuffer+BUFFER_SIZE-p, 0x4e );

	IsFormatting = 1;
	FormatError = 0;
	ReqTrack = desc->track;
	ReqSide  = desc->head;
	do_fd_action( drive );

	wait_for_completion(&format_wait);

	finish_fdc();
	ret = FormatError ? -EIO : 0;
out:
	blk_mq_unquiesce_queue(q);
	blk_mq_unfreeze_queue(q);
	return ret;
}


 

static void do_fd_action( int drive )
{
	DPRINT(("do_fd_action\n"));
	
	if (UseTrackbuffer && !IsFormatting) {
	repeat:
	    if (IS_BUFFERED( drive, ReqSide, ReqTrack )) {
		if (ReqCmd == READ) {
		    copy_buffer( SECTOR_BUFFER(ReqSector), ReqData );
		    if (++ReqCnt < blk_rq_cur_sectors(fd_request)) {
			 
			setup_req_params( drive );
			goto repeat;
		    }
		    else {
			 
			fd_end_request_cur(BLK_STS_OK);
			finish_fdc();
			return;
		    }
		}
		else {
		     
		    copy_buffer( ReqData, SECTOR_BUFFER(ReqSector) );
		}
	    }
	}

	if (SelectedDrive != drive)
		fd_select_drive( drive );
    
	if (UD.track == -1)
		fd_calibrate();
	else if (UD.track != ReqTrack << UDT->stretch)
		fd_seek();
	else if (IsFormatting)
		fd_writetrack();
	else
		fd_rwsec();
}


 

static void fd_calibrate( void )
{
	if (SUD.track >= 0) {
		fd_calibrate_done( 0 );
		return;
	}

	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = 0;    
	DPRINT(("fd_calibrate\n"));
	SET_IRQ_HANDLER( fd_calibrate_done );
	 
	FDC_WRITE( FDCREG_CMD, FDCCMD_RESTORE | SUD.steprate );

	NeedSeek = 1;
	MotorOn = 1;
	start_timeout();
	 
}


static void fd_calibrate_done( int status )
{
	DPRINT(("fd_calibrate_done()\n"));
	stop_timeout();
    
	 
	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = SUDT->fdc_speed;
	if (status & FDCSTAT_RECNF) {
		printk(KERN_ERR "fd%d: restore failed\n", SelectedDrive );
		fd_error();
	}
	else {
		SUD.track = 0;
		fd_seek();
	}
}
  
  
 
  
static void fd_seek( void )
{
	if (SUD.track == ReqTrack << SUDT->stretch) {
		fd_seek_done( 0 );
		return;
	}

	if (ATARIHW_PRESENT(FDCSPEED)) {
		dma_wd.fdc_speed = 0;	 
		MFPDELAY();
	}

	DPRINT(("fd_seek() to track %d\n",ReqTrack));
	FDC_WRITE( FDCREG_DATA, ReqTrack << SUDT->stretch);
	udelay(25);
	SET_IRQ_HANDLER( fd_seek_done );
	FDC_WRITE( FDCREG_CMD, FDCCMD_SEEK | SUD.steprate );

	MotorOn = 1;
	set_head_settle_flag();
	start_timeout();
	 
}


static void fd_seek_done( int status )
{
	DPRINT(("fd_seek_done()\n"));
	stop_timeout();
	
	 
	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = SUDT->fdc_speed;
	if (status & FDCSTAT_RECNF) {
		printk(KERN_ERR "fd%d: seek error (to track %d)\n",
				SelectedDrive, ReqTrack );
		 
		SUD.track = -1;
		fd_error();
	}
	else {
		SUD.track = ReqTrack << SUDT->stretch;
		NeedSeek = 0;
		if (IsFormatting)
			fd_writetrack();
		else
			fd_rwsec();
	}
}


 

static int MultReadInProgress = 0;


static void fd_rwsec( void )
{
	unsigned long paddr, flags;
	unsigned int  rwflag, old_motoron;
	unsigned int track;
	
	DPRINT(("fd_rwsec(), Sec=%d, Access=%c\n",ReqSector, ReqCmd == WRITE ? 'w' : 'r' ));
	if (ReqCmd == WRITE) {
		if (ATARIHW_PRESENT(EXTD_DMA)) {
			paddr = virt_to_phys(ReqData);
		}
		else {
			copy_buffer( ReqData, DMABuffer );
			paddr = PhysDMABuffer;
		}
		dma_cache_maintenance( paddr, 512, 1 );
		rwflag = 0x100;
	}
	else {
		if (read_track)
			paddr = PhysTrackBuffer;
		else
			paddr = ATARIHW_PRESENT(EXTD_DMA) ? 
				virt_to_phys(ReqData) : PhysDMABuffer;
		rwflag = 0;
	}

	fd_select_side( ReqSide );
  
	 
	FDC_WRITE( FDCREG_SECTOR, read_track ? 1 : ReqSector );
	MFPDELAY();
	 
	if (SUDT->stretch) {
		track = FDC_READ( FDCREG_TRACK);
		MFPDELAY();
		FDC_WRITE( FDCREG_TRACK, track >> SUDT->stretch);
	}
	udelay(25);
  
	 
	local_irq_save(flags);
	dma_wd.dma_lo = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	dma_wd.dma_md = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	if (ATARIHW_PRESENT(EXTD_DMA))
		st_dma_ext_dmahi = (unsigned short)paddr;
	else
		dma_wd.dma_hi = (unsigned char)paddr;
	MFPDELAY();
	local_irq_restore(flags);
  
	   
	dma_wd.dma_mode_status = 0x90 | rwflag;  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x90 | (rwflag ^ 0x100);  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x90 | rwflag;
	MFPDELAY();
  
	 
	dma_wd.fdc_acces_seccount = read_track ? SUDT->spt : 1;
  
	udelay(25);  
  
	 
	dma_wd.dma_mode_status = FDCSELREG_STP | rwflag;
	udelay(25);
	SET_IRQ_HANDLER( fd_rwsec_done );
	dma_wd.fdc_acces_seccount =
	  (get_head_settle_flag() |
	   (rwflag ? FDCCMD_WRSEC : (FDCCMD_RDSEC | (read_track ? FDCCMDADD_M : 0))));

	old_motoron = MotorOn;
	MotorOn = 1;
	NeedSeek = 1;
	 

	if (read_track) {
		 
		MultReadInProgress = 1;
		mod_timer(&readtrack_timer,
			   
			  jiffies + HZ/5 + (old_motoron ? 0 : HZ));
	}
	start_timeout();
}

    
static void fd_readtrack_check(struct timer_list *unused)
{
	unsigned long flags, addr, addr2;

	local_irq_save(flags);

	if (!MultReadInProgress) {
		 
		local_irq_restore(flags);
		return;
	}

	 
	 
	addr = 0;
	do {
		addr2 = addr;
		addr = dma_wd.dma_lo & 0xff;
		MFPDELAY();
		addr |= (dma_wd.dma_md & 0xff) << 8;
		MFPDELAY();
		if (ATARIHW_PRESENT( EXTD_DMA ))
			addr |= (st_dma_ext_dmahi & 0xffff) << 16;
		else
			addr |= (dma_wd.dma_hi & 0xff) << 16;
		MFPDELAY();
	} while(addr != addr2);
  
	if (addr >= PhysTrackBuffer + SUDT->spt*512) {
		 
		SET_IRQ_HANDLER( NULL );
		MultReadInProgress = 0;
		local_irq_restore(flags);
		DPRINT(("fd_readtrack_check(): done\n"));
		FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
		udelay(25);

		 
		fd_rwsec_done1(0);
	}
	else {
		 
		local_irq_restore(flags);
		DPRINT(("fd_readtrack_check(): not yet finished\n"));
		mod_timer(&readtrack_timer, jiffies + HZ/5/10);
	}
}


static void fd_rwsec_done( int status )
{
	DPRINT(("fd_rwsec_done()\n"));

	if (read_track) {
		del_timer(&readtrack_timer);
		if (!MultReadInProgress)
			return;
		MultReadInProgress = 0;
	}
	fd_rwsec_done1(status);
}

static void fd_rwsec_done1(int status)
{
	unsigned int track;

	stop_timeout();
	
	 
	if (SUDT->stretch) {
		track = FDC_READ( FDCREG_TRACK);
		MFPDELAY();
		FDC_WRITE( FDCREG_TRACK, track << SUDT->stretch);
	}

	if (!UseTrackbuffer) {
		dma_wd.dma_mode_status = 0x90;
		MFPDELAY();
		if (!(dma_wd.dma_mode_status & 0x01)) {
			printk(KERN_ERR "fd%d: DMA error\n", SelectedDrive );
			goto err_end;
		}
	}
	MFPDELAY();

	if (ReqCmd == WRITE && (status & FDCSTAT_WPROT)) {
		printk(KERN_NOTICE "fd%d: is write protected\n", SelectedDrive );
		goto err_end;
	}	
	if ((status & FDCSTAT_RECNF) &&
	     
	    !(read_track && FDC_READ(FDCREG_SECTOR) > SUDT->spt)) {
		if (Probing) {
			if (SUDT > atari_disk_type) {
			    if (SUDT[-1].blocks > ReqBlock) {
				 
				SUDT--;
				set_capacity(unit[SelectedDrive].disk[0],
							SUDT->blocks);
			    } else
				Probing = 0;
			}
			else {
				if (SUD.flags & FTD_MSG)
					printk(KERN_INFO "fd%d: Auto-detected floppy type %s\n",
					       SelectedDrive, SUDT->name );
				Probing=0;
			}
		} else {	
 
			if (SUD.autoprobe) {
				SUDT = atari_disk_type + StartDiskType[DriveType];
				set_capacity(unit[SelectedDrive].disk[0],
							SUDT->blocks);
				Probing = 1;
			}
		}
		if (Probing) {
			if (ATARIHW_PRESENT(FDCSPEED)) {
				dma_wd.fdc_speed = SUDT->fdc_speed;
				MFPDELAY();
			}
			setup_req_params( SelectedDrive );
			BufferDrive = -1;
			do_fd_action( SelectedDrive );
			return;
		}

		printk(KERN_ERR "fd%d: sector %d not found (side %d, track %d)\n",
		       SelectedDrive, FDC_READ (FDCREG_SECTOR), ReqSide, ReqTrack );
		goto err_end;
	}
	if (status & FDCSTAT_CRC) {
		printk(KERN_ERR "fd%d: CRC error (side %d, track %d, sector %d)\n",
		       SelectedDrive, ReqSide, ReqTrack, FDC_READ (FDCREG_SECTOR) );
		goto err_end;
	}
	if (status & FDCSTAT_LOST) {
		printk(KERN_ERR "fd%d: lost data (side %d, track %d, sector %d)\n",
		       SelectedDrive, ReqSide, ReqTrack, FDC_READ (FDCREG_SECTOR) );
		goto err_end;
	}

	Probing = 0;
	
	if (ReqCmd == READ) {
		if (!read_track) {
			void *addr;
			addr = ATARIHW_PRESENT( EXTD_DMA ) ? ReqData : DMABuffer;
			dma_cache_maintenance( virt_to_phys(addr), 512, 0 );
			if (!ATARIHW_PRESENT( EXTD_DMA ))
				copy_buffer (addr, ReqData);
		} else {
			dma_cache_maintenance( PhysTrackBuffer, MaxSectors[DriveType] * 512, 0 );
			BufferDrive = SelectedDrive;
			BufferSide  = ReqSide;
			BufferTrack = ReqTrack;
			copy_buffer (SECTOR_BUFFER (ReqSector), ReqData);
		}
	}
  
	if (++ReqCnt < blk_rq_cur_sectors(fd_request)) {
		 
		setup_req_params( SelectedDrive );
		do_fd_action( SelectedDrive );
	}
	else {
		 
		fd_end_request_cur(BLK_STS_OK);
		finish_fdc();
	}
	return;
  
  err_end:
	BufferDrive = -1;
	fd_error();
}


static void fd_writetrack( void )
{
	unsigned long paddr, flags;
	unsigned int track;
	
	DPRINT(("fd_writetrack() Tr=%d Si=%d\n", ReqTrack, ReqSide ));

	paddr = PhysTrackBuffer;
	dma_cache_maintenance( paddr, BUFFER_SIZE, 1 );

	fd_select_side( ReqSide );
  
	 
	if (SUDT->stretch) {
		track = FDC_READ( FDCREG_TRACK);
		MFPDELAY();
		FDC_WRITE(FDCREG_TRACK,track >> SUDT->stretch);
	}
	udelay(40);
  
	 
	local_irq_save(flags);
	dma_wd.dma_lo = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	dma_wd.dma_md = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	if (ATARIHW_PRESENT( EXTD_DMA ))
		st_dma_ext_dmahi = (unsigned short)paddr;
	else
		dma_wd.dma_hi = (unsigned char)paddr;
	MFPDELAY();
	local_irq_restore(flags);
  
	   
	dma_wd.dma_mode_status = 0x190;  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x90;  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x190;
	MFPDELAY();
  
	 
	dma_wd.fdc_acces_seccount = BUFFER_SIZE/512;
	udelay(40);  
  
	 
	dma_wd.dma_mode_status = FDCSELREG_STP | 0x100;
	udelay(40);
	SET_IRQ_HANDLER( fd_writetrack_done );
	dma_wd.fdc_acces_seccount = FDCCMD_WRTRA | get_head_settle_flag(); 

	MotorOn = 1;
	start_timeout();
	 
}


static void fd_writetrack_done( int status )
{
	DPRINT(("fd_writetrack_done()\n"));

	stop_timeout();

	if (status & FDCSTAT_WPROT) {
		printk(KERN_NOTICE "fd%d: is write protected\n", SelectedDrive );
		goto err_end;
	}	
	if (status & FDCSTAT_LOST) {
		printk(KERN_ERR "fd%d: lost data (side %d, track %d)\n",
				SelectedDrive, ReqSide, ReqTrack );
		goto err_end;
	}

	complete(&format_wait);
	return;

  err_end:
	fd_error();
}

static void fd_times_out(struct timer_list *unused)
{
	atari_disable_irq( IRQ_MFP_FDC );
	if (!FloppyIRQHandler) goto end;  

	SET_IRQ_HANDLER( NULL );
	 
	if (UseTrackbuffer)
		del_timer( &readtrack_timer );
	FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
	udelay( 25 );
	
	printk(KERN_ERR "floppy timeout\n" );
	fd_error();
  end:
	atari_enable_irq( IRQ_MFP_FDC );
}


 

static void finish_fdc( void )
{
	if (!NeedSeek || !stdma_is_locked_by(floppy_irq)) {
		finish_fdc_done( 0 );
	}
	else {
		DPRINT(("finish_fdc: dummy seek started\n"));
		FDC_WRITE (FDCREG_DATA, SUD.track);
		SET_IRQ_HANDLER( finish_fdc_done );
		FDC_WRITE (FDCREG_CMD, FDCCMD_SEEK);
		MotorOn = 1;
		start_timeout();
		 
	  }
}


static void finish_fdc_done( int dummy )
{
	unsigned long flags;

	DPRINT(("finish_fdc_done entered\n"));
	stop_timeout();
	NeedSeek = 0;

	if (timer_pending(&fd_timer) && time_before(fd_timer.expires, jiffies + 5))
		 
		mod_timer(&fd_timer, jiffies + 5);
	else
		start_check_change_timer();
	start_motor_off_timer();

	local_irq_save(flags);
	if (stdma_is_locked_by(floppy_irq))
		stdma_release();
	local_irq_restore(flags);

	DPRINT(("finish_fdc() finished\n"));
}

 

static unsigned int floppy_check_events(struct gendisk *disk,
					unsigned int clearing)
{
	struct atari_floppy_struct *p = disk->private_data;
	unsigned int drive = p - unit;
	if (test_bit (drive, &fake_change)) {
		 
		return DISK_EVENT_MEDIA_CHANGE;
	}
	if (test_bit (drive, &changed_floppies)) {
		 
		return DISK_EVENT_MEDIA_CHANGE;
	}
	if (UD.wpstat) {
		 
		return DISK_EVENT_MEDIA_CHANGE;
	}

	return 0;
}

static int floppy_revalidate(struct gendisk *disk)
{
	struct atari_floppy_struct *p = disk->private_data;
	unsigned int drive = p - unit;

	if (test_bit(drive, &changed_floppies) ||
	    test_bit(drive, &fake_change) || !p->disktype) {
		if (UD.flags & FTD_MSG)
			printk(KERN_ERR "floppy: clear format %p!\n", UDT);
		BufferDrive = -1;
		clear_bit(drive, &fake_change);
		clear_bit(drive, &changed_floppies);
		 
		if (default_params[drive].blocks == 0)
			UDT = NULL;
		else
			UDT = &default_params[drive];
	}
	return 0;
}


 

static void setup_req_params( int drive )
{
	int block = ReqBlock + ReqCnt;

	ReqTrack = block / UDT->spt;
	ReqSector = block - ReqTrack * UDT->spt + 1;
	ReqSide = ReqTrack & 1;
	ReqTrack >>= 1;
	ReqData = ReqBuffer + 512 * ReqCnt;

	if (UseTrackbuffer)
		read_track = (ReqCmd == READ && unit[drive].error_count == 0);
	else
		read_track = 0;

	DPRINT(("Request params: Si=%d Tr=%d Se=%d Data=%08lx\n",ReqSide,
			ReqTrack, ReqSector, (unsigned long)ReqData ));
}

static blk_status_t ataflop_queue_rq(struct blk_mq_hw_ctx *hctx,
				     const struct blk_mq_queue_data *bd)
{
	struct atari_floppy_struct *floppy = bd->rq->q->disk->private_data;
	int drive = floppy - unit;
	int type = floppy->type;

	DPRINT(("Queue request: drive %d type %d sectors %d of %d last %d\n",
		drive, type, blk_rq_cur_sectors(bd->rq),
		blk_rq_sectors(bd->rq), bd->last));

	spin_lock_irq(&ataflop_lock);
	if (fd_request) {
		spin_unlock_irq(&ataflop_lock);
		return BLK_STS_DEV_RESOURCE;
	}
	if (!stdma_try_lock(floppy_irq, NULL))  {
		spin_unlock_irq(&ataflop_lock);
		return BLK_STS_RESOURCE;
	}
	fd_request = bd->rq;
	unit[drive].error_count = 0;
	blk_mq_start_request(fd_request);

	atari_disable_irq( IRQ_MFP_FDC );

	IsFormatting = 0;

	if (!UD.connected) {
		 
		printk(KERN_ERR "Unknown Device: fd%d\n", drive );
		fd_end_request_cur(BLK_STS_IOERR);
		stdma_release();
		goto out;
	}
		
	if (type == 0) {
		if (!UDT) {
			Probing = 1;
			UDT = atari_disk_type + StartDiskType[DriveType];
			set_capacity(bd->rq->q->disk, UDT->blocks);
			UD.autoprobe = 1;
		}
	} 
	else {
		 
		if (--type >= NUM_DISK_MINORS) {
			printk(KERN_WARNING "fd%d: invalid disk format", drive );
			fd_end_request_cur(BLK_STS_IOERR);
			stdma_release();
			goto out;
		}
		if (minor2disktype[type].drive_types > DriveType)  {
			printk(KERN_WARNING "fd%d: unsupported disk format", drive );
			fd_end_request_cur(BLK_STS_IOERR);
			stdma_release();
			goto out;
		}
		type = minor2disktype[type].index;
		UDT = &atari_disk_type[type];
		set_capacity(bd->rq->q->disk, UDT->blocks);
		UD.autoprobe = 0;
	}

	 
	del_timer( &motor_off_timer );
		
	ReqCnt = 0;
	ReqCmd = rq_data_dir(fd_request);
	ReqBlock = blk_rq_pos(fd_request);
	ReqBuffer = bio_data(fd_request->bio);
	setup_req_params( drive );
	do_fd_action( drive );

	atari_enable_irq( IRQ_MFP_FDC );

out:
	spin_unlock_irq(&ataflop_lock);
	return BLK_STS_OK;
}

static int fd_locked_ioctl(struct block_device *bdev, blk_mode_t mode,
		    unsigned int cmd, unsigned long param)
{
	struct gendisk *disk = bdev->bd_disk;
	struct atari_floppy_struct *floppy = disk->private_data;
	int drive = floppy - unit;
	int type = floppy->type;
	struct atari_format_descr fmt_desc;
	struct atari_disk_type *dtp;
	struct floppy_struct getprm;
	int settype;
	struct floppy_struct setprm;
	void __user *argp = (void __user *)param;

	switch (cmd) {
	case FDGETPRM:
		if (type) {
			if (--type >= NUM_DISK_MINORS)
				return -ENODEV;
			if (minor2disktype[type].drive_types > DriveType)
				return -ENODEV;
			type = minor2disktype[type].index;
			dtp = &atari_disk_type[type];
			if (UD.flags & FTD_MSG)
			    printk (KERN_ERR "floppy%d: found dtp %p name %s!\n",
			        drive, dtp, dtp->name);
		}
		else {
			if (!UDT)
				return -ENXIO;
			else
				dtp = UDT;
		}
		memset((void *)&getprm, 0, sizeof(getprm));
		getprm.size = dtp->blocks;
		getprm.sect = dtp->spt;
		getprm.head = 2;
		getprm.track = dtp->blocks/dtp->spt/2;
		getprm.stretch = dtp->stretch;
		if (copy_to_user(argp, &getprm, sizeof(getprm)))
			return -EFAULT;
		return 0;
	}
	switch (cmd) {
	case FDSETPRM:
	case FDDEFPRM:
	         

		 
		if (floppy->ref != 1 && floppy->ref != -1)
			return -EBUSY;
		if (copy_from_user(&setprm, argp, sizeof(setprm)))
			return -EFAULT;
		 

		if (floppy_check_events(disk, 0))
		        floppy_revalidate(disk);

		if (UD.flags & FTD_MSG)
		    printk (KERN_INFO "floppy%d: setting size %d spt %d str %d!\n",
			drive, setprm.size, setprm.sect, setprm.stretch);

		 
		if (type) {
		         
			finish_fdc();
			return -EINVAL;
		}

		 

		for (settype = 0; settype < NUM_DISK_MINORS; settype++) {
			int setidx = 0;
			if (minor2disktype[settype].drive_types > DriveType) {
				 
				continue;
			}
			setidx = minor2disktype[settype].index;
			dtp = &atari_disk_type[setidx];

			 
			if (   dtp->blocks  == setprm.size 
			    && dtp->spt     == setprm.sect
			    && dtp->stretch == setprm.stretch ) {
				if (UD.flags & FTD_MSG)
				    printk (KERN_INFO "floppy%d: setting %s %p!\n",
				        drive, dtp->name, dtp);
				UDT = dtp;
				set_capacity(disk, UDT->blocks);

				if (cmd == FDDEFPRM) {
				   
				  default_params[drive].name    = dtp->name;
				  default_params[drive].spt     = dtp->spt;
				  default_params[drive].blocks  = dtp->blocks;
				  default_params[drive].fdc_speed = dtp->fdc_speed;
				  default_params[drive].stretch = dtp->stretch;
				}
				
				return 0;
			}

		}

		 

	       	if (cmd == FDDEFPRM) {
			 
			dtp = &default_params[drive];
		} else
			 
			dtp = &user_params[drive];

		dtp->name   = "user format";
		dtp->blocks = setprm.size;
		dtp->spt    = setprm.sect;
		if (setprm.sect > 14) 
			dtp->fdc_speed = 3;
		else
			dtp->fdc_speed = 0;
		dtp->stretch = setprm.stretch;

		if (UD.flags & FTD_MSG)
			printk (KERN_INFO "floppy%d: blk %d spt %d str %d!\n",
				drive, dtp->blocks, dtp->spt, dtp->stretch);

		 
		if (setprm.track != dtp->blocks/dtp->spt/2 ||
		    setprm.head != 2) {
			finish_fdc();
			return -EINVAL;
		}

		UDT = dtp;
		set_capacity(disk, UDT->blocks);

		return 0;
	case FDMSGON:
		UD.flags |= FTD_MSG;
		return 0;
	case FDMSGOFF:
		UD.flags &= ~FTD_MSG;
		return 0;
	case FDSETEMSGTRESH:
		return -EINVAL;
	case FDFMTBEG:
		return 0;
	case FDFMTTRK:
		if (floppy->ref != 1 && floppy->ref != -1)
			return -EBUSY;
		if (copy_from_user(&fmt_desc, argp, sizeof(fmt_desc)))
			return -EFAULT;
		return do_format(drive, type, &fmt_desc);
	case FDCLRPRM:
		UDT = NULL;
		 
		default_params[drive].blocks  = 0;
		set_capacity(disk, MAX_DISK_SIZE * 2);
		fallthrough;
	case FDFMTEND:
	case FDFLUSH:
		 
		BufferDrive = -1;
		set_bit(drive, &fake_change);
		if (disk_check_media_change(disk))
			floppy_revalidate(disk);
		return 0;
	default:
		return -EINVAL;
	}
}

static int fd_ioctl(struct block_device *bdev, blk_mode_t mode,
			     unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&ataflop_mutex);
	ret = fd_locked_ioctl(bdev, mode, cmd, arg);
	mutex_unlock(&ataflop_mutex);

	return ret;
}

 

static void __init fd_probe( int drive )
{
	UD.connected = 0;
	UDT  = NULL;

	if (!fd_test_drive_present( drive ))
		return;

	UD.connected = 1;
	UD.track     = 0;
	switch( UserSteprate[drive] ) {
	case 2:
		UD.steprate = FDCSTEP_2;
		break;
	case 3:
		UD.steprate = FDCSTEP_3;
		break;
	case 6:
		UD.steprate = FDCSTEP_6;
		break;
	case 12:
		UD.steprate = FDCSTEP_12;
		break;
	default:  
		if (ATARIHW_PRESENT( FDCSPEED ) || MACH_IS_MEDUSA)
			UD.steprate = FDCSTEP_3;
		else
			UD.steprate = FDCSTEP_6;
		break;
	}
	MotorOn = 1;	 
}


 

static int __init fd_test_drive_present( int drive )
{
	unsigned long timeout;
	unsigned char status;
	int ok;
	
	if (drive >= (MACH_IS_FALCON ? 1 : 2)) return( 0 );
	fd_select_drive( drive );

	 
	atari_turnoff_irq( IRQ_MFP_FDC );
	FDC_WRITE (FDCREG_TRACK, 0xff00);
	FDC_WRITE( FDCREG_CMD, FDCCMD_RESTORE | FDCCMDADD_H | FDCSTEP_6 );

	timeout = jiffies + 2*HZ+HZ/2;
	while (time_before(jiffies, timeout))
		if (!(st_mfp.par_dt_reg & 0x20))
			break;

	status = FDC_READ( FDCREG_STATUS );
	ok = (status & FDCSTAT_TR00) != 0;

	 
	FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
	udelay(500);
	status = FDC_READ( FDCREG_STATUS );
	udelay(20);

	if (ok) {
		 
		FDC_WRITE( FDCREG_DATA, 0 );
		FDC_WRITE( FDCREG_CMD, FDCCMD_SEEK );
		while( st_mfp.par_dt_reg & 0x20 )
			;
		status = FDC_READ( FDCREG_STATUS );
	}

	atari_turnon_irq( IRQ_MFP_FDC );
	return( ok );
}


 

static void __init config_types( void )
{
	int drive, cnt = 0;

	 
	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = 0;

	printk(KERN_INFO "Probing floppy drive(s):\n");
	for( drive = 0; drive < FD_MAX_UNITS; drive++ ) {
		fd_probe( drive );
		if (UD.connected) {
			printk(KERN_INFO "fd%d\n", drive);
			++cnt;
		}
	}

	if (FDC_READ( FDCREG_STATUS ) & FDCSTAT_BUSY) {
		 
		FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
		udelay(500);
		FDC_READ( FDCREG_STATUS );
		udelay(20);
	}
	
	if (cnt > 0) {
		start_motor_off_timer();
		if (cnt == 1) fd_select_drive( 0 );
		start_check_change_timer();
	}
}

 

static int floppy_open(struct gendisk *disk, blk_mode_t mode)
{
	struct atari_floppy_struct *p = disk->private_data;
	int type = disk->first_minor >> 2;

	DPRINT(("fd_open: type=%d\n",type));
	if (p->ref && p->type != type)
		return -EBUSY;

	if (p->ref == -1 || (p->ref && mode & BLK_OPEN_EXCL))
		return -EBUSY;
	if (mode & BLK_OPEN_EXCL)
		p->ref = -1;
	else
		p->ref++;

	p->type = type;

	if (mode & BLK_OPEN_NDELAY)
		return 0;

	if (mode & (BLK_OPEN_READ | BLK_OPEN_WRITE)) {
		if (disk_check_media_change(disk))
			floppy_revalidate(disk);
		if (mode & BLK_OPEN_WRITE) {
			if (p->wpstat) {
				if (p->ref < 0)
					p->ref = 0;
				else
					p->ref--;
				return -EROFS;
			}
		}
	}
	return 0;
}

static int floppy_unlocked_open(struct gendisk *disk, blk_mode_t mode)
{
	int ret;

	mutex_lock(&ataflop_mutex);
	ret = floppy_open(disk, mode);
	mutex_unlock(&ataflop_mutex);

	return ret;
}

static void floppy_release(struct gendisk *disk)
{
	struct atari_floppy_struct *p = disk->private_data;
	mutex_lock(&ataflop_mutex);
	if (p->ref < 0)
		p->ref = 0;
	else if (!p->ref--) {
		printk(KERN_ERR "floppy_release with fd_ref == 0");
		p->ref = 0;
	}
	mutex_unlock(&ataflop_mutex);
}

static const struct block_device_operations floppy_fops = {
	.owner		= THIS_MODULE,
	.open		= floppy_unlocked_open,
	.release	= floppy_release,
	.ioctl		= fd_ioctl,
	.check_events	= floppy_check_events,
};

static const struct blk_mq_ops ataflop_mq_ops = {
	.queue_rq = ataflop_queue_rq,
};

static int ataflop_alloc_disk(unsigned int drive, unsigned int type)
{
	struct gendisk *disk;

	disk = blk_mq_alloc_disk(&unit[drive].tag_set, NULL);
	if (IS_ERR(disk))
		return PTR_ERR(disk);

	disk->major = FLOPPY_MAJOR;
	disk->first_minor = drive + (type << 2);
	disk->minors = 1;
	sprintf(disk->disk_name, "fd%d", drive);
	disk->fops = &floppy_fops;
	disk->flags |= GENHD_FL_NO_PART;
	disk->events = DISK_EVENT_MEDIA_CHANGE;
	disk->private_data = &unit[drive];
	set_capacity(disk, MAX_DISK_SIZE * 2);

	unit[drive].disk[type] = disk;
	return 0;
}

static void ataflop_probe(dev_t dev)
{
	int drive = MINOR(dev) & 3;
	int type  = MINOR(dev) >> 2;

	if (type)
		type--;

	if (drive >= FD_MAX_UNITS || type >= NUM_DISK_MINORS)
		return;
	if (unit[drive].disk[type])
		return;
	if (ataflop_alloc_disk(drive, type))
		return;
	if (add_disk(unit[drive].disk[type]))
		goto cleanup_disk;
	unit[drive].registered[type] = true;
	return;

cleanup_disk:
	put_disk(unit[drive].disk[type]);
	unit[drive].disk[type] = NULL;
}

static void atari_floppy_cleanup(void)
{
	int i;
	int type;

	for (i = 0; i < FD_MAX_UNITS; i++) {
		for (type = 0; type < NUM_DISK_MINORS; type++) {
			if (!unit[i].disk[type])
				continue;
			del_gendisk(unit[i].disk[type]);
			put_disk(unit[i].disk[type]);
		}
		blk_mq_free_tag_set(&unit[i].tag_set);
	}

	del_timer_sync(&fd_timer);
	atari_stram_free(DMABuffer);
}

static void atari_cleanup_floppy_disk(struct atari_floppy_struct *fs)
{
	int type;

	for (type = 0; type < NUM_DISK_MINORS; type++) {
		if (!fs->disk[type])
			continue;
		if (fs->registered[type])
			del_gendisk(fs->disk[type]);
		put_disk(fs->disk[type]);
	}
	blk_mq_free_tag_set(&fs->tag_set);
}

static int __init atari_floppy_init (void)
{
	int i;
	int ret;

	if (!MACH_IS_ATARI)
		 
		return -ENODEV;

	for (i = 0; i < FD_MAX_UNITS; i++) {
		memset(&unit[i].tag_set, 0, sizeof(unit[i].tag_set));
		unit[i].tag_set.ops = &ataflop_mq_ops;
		unit[i].tag_set.nr_hw_queues = 1;
		unit[i].tag_set.nr_maps = 1;
		unit[i].tag_set.queue_depth = 2;
		unit[i].tag_set.numa_node = NUMA_NO_NODE;
		unit[i].tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
		ret = blk_mq_alloc_tag_set(&unit[i].tag_set);
		if (ret)
			goto err;

		ret = ataflop_alloc_disk(i, 0);
		if (ret) {
			blk_mq_free_tag_set(&unit[i].tag_set);
			goto err;
		}
	}

	if (UseTrackbuffer < 0)
		 
		UseTrackbuffer = !MACH_IS_MEDUSA;

	 
	SelectedDrive = -1;
	BufferDrive = -1;

	DMABuffer = atari_stram_alloc(BUFFER_SIZE+512, "ataflop");
	if (!DMABuffer) {
		printk(KERN_ERR "atari_floppy_init: cannot get dma buffer\n");
		ret = -ENOMEM;
		goto err;
	}
	TrackBuffer = DMABuffer + 512;
	PhysDMABuffer = atari_stram_to_phys(DMABuffer);
	PhysTrackBuffer = virt_to_phys(TrackBuffer);
	BufferDrive = BufferSide = BufferTrack = -1;

	for (i = 0; i < FD_MAX_UNITS; i++) {
		unit[i].track = -1;
		unit[i].flags = 0;
		ret = add_disk(unit[i].disk[0]);
		if (ret)
			goto err_out_dma;
		unit[i].registered[0] = true;
	}

	printk(KERN_INFO "Atari floppy driver: max. %cD, %strack buffering\n",
	       DriveType == 0 ? 'D' : DriveType == 1 ? 'H' : 'E',
	       UseTrackbuffer ? "" : "no ");
	config_types();

	ret = __register_blkdev(FLOPPY_MAJOR, "fd", ataflop_probe);
	if (ret) {
		printk(KERN_ERR "atari_floppy_init: cannot register block device\n");
		atari_floppy_cleanup();
	}
	return ret;

err_out_dma:
	atari_stram_free(DMABuffer);
err:
	while (--i >= 0)
		atari_cleanup_floppy_disk(&unit[i]);

	return ret;
}

#ifndef MODULE
static int __init atari_floppy_setup(char *str)
{
	int ints[3 + FD_MAX_UNITS];
	int i;

	if (!MACH_IS_ATARI)
		return 0;

	str = get_options(str, 3 + FD_MAX_UNITS, ints);
	
	if (ints[0] < 1) {
		printk(KERN_ERR "ataflop_setup: no arguments!\n" );
		return 0;
	}
	else if (ints[0] > 2+FD_MAX_UNITS) {
		printk(KERN_ERR "ataflop_setup: too many arguments\n" );
	}

	if (ints[1] < 0 || ints[1] > 2)
		printk(KERN_ERR "ataflop_setup: bad drive type\n" );
	else
		DriveType = ints[1];

	if (ints[0] >= 2)
		UseTrackbuffer = (ints[2] > 0);

	for( i = 3; i <= ints[0] && i-3 < FD_MAX_UNITS; ++i ) {
		if (ints[i] != 2 && ints[i] != 3 && ints[i] != 6 && ints[i] != 12)
			printk(KERN_ERR "ataflop_setup: bad steprate\n" );
		else
			UserSteprate[i-3] = ints[i];
	}
	return 1;
}

__setup("floppy=", atari_floppy_setup);
#endif

static void __exit atari_floppy_exit(void)
{
	unregister_blkdev(FLOPPY_MAJOR, "fd");
	atari_floppy_cleanup();
}

module_init(atari_floppy_init)
module_exit(atari_floppy_exit)

MODULE_LICENSE("GPL");
