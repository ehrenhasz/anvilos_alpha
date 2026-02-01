
 

 

 

 

 

 

#ifndef NCR5380_io_delay
#define NCR5380_io_delay(x)
#endif

#ifndef NCR5380_acquire_dma_irq
#define NCR5380_acquire_dma_irq(x)	(1)
#endif

#ifndef NCR5380_release_dma_irq
#define NCR5380_release_dma_irq(x)
#endif

static unsigned int disconnect_mask = ~0;
module_param(disconnect_mask, int, 0444);

static int do_abort(struct Scsi_Host *, unsigned int);
static void do_reset(struct Scsi_Host *);
static void bus_reset_cleanup(struct Scsi_Host *);

 

static inline void initialize_SCp(struct scsi_cmnd *cmd)
{
	struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(cmd);

	if (scsi_bufflen(cmd)) {
		ncmd->buffer = scsi_sglist(cmd);
		ncmd->ptr = sg_virt(ncmd->buffer);
		ncmd->this_residual = ncmd->buffer->length;
	} else {
		ncmd->buffer = NULL;
		ncmd->ptr = NULL;
		ncmd->this_residual = 0;
	}

	ncmd->status = 0;
	ncmd->message = 0;
}

static inline void advance_sg_buffer(struct NCR5380_cmd *ncmd)
{
	struct scatterlist *s = ncmd->buffer;

	if (!ncmd->this_residual && s && !sg_is_last(s)) {
		ncmd->buffer = sg_next(s);
		ncmd->ptr = sg_virt(ncmd->buffer);
		ncmd->this_residual = ncmd->buffer->length;
	}
}

static inline void set_resid_from_SCp(struct scsi_cmnd *cmd)
{
	struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(cmd);
	int resid = ncmd->this_residual;
	struct scatterlist *s = ncmd->buffer;

	if (s)
		while (!sg_is_last(s)) {
			s = sg_next(s);
			resid += s->length;
		}
	scsi_set_resid(cmd, resid);
}

 

static int NCR5380_poll_politely2(struct NCR5380_hostdata *hostdata,
                                  unsigned int reg1, u8 bit1, u8 val1,
                                  unsigned int reg2, u8 bit2, u8 val2,
                                  unsigned long wait)
{
	unsigned long n = hostdata->poll_loops;
	unsigned long deadline = jiffies + wait;

	do {
		if ((NCR5380_read(reg1) & bit1) == val1)
			return 0;
		if ((NCR5380_read(reg2) & bit2) == val2)
			return 0;
		cpu_relax();
	} while (n--);

	if (!wait)
		return -ETIMEDOUT;

	 
	while (time_is_after_jiffies(deadline)) {
		schedule_timeout_uninterruptible(1);
		if ((NCR5380_read(reg1) & bit1) == val1)
			return 0;
		if ((NCR5380_read(reg2) & bit2) == val2)
			return 0;
	}

	return -ETIMEDOUT;
}

#if NDEBUG
static struct {
	unsigned char mask;
	const char *name;
} signals[] = {
	{SR_DBP, "PARITY"},
	{SR_RST, "RST"},
	{SR_BSY, "BSY"},
	{SR_REQ, "REQ"},
	{SR_MSG, "MSG"},
	{SR_CD, "CD"},
	{SR_IO, "IO"},
	{SR_SEL, "SEL"},
	{0, NULL}
},
basrs[] = {
	{BASR_END_DMA_TRANSFER, "END OF DMA"},
	{BASR_DRQ, "DRQ"},
	{BASR_PARITY_ERROR, "PARITY ERROR"},
	{BASR_IRQ, "IRQ"},
	{BASR_PHASE_MATCH, "PHASE MATCH"},
	{BASR_BUSY_ERROR, "BUSY ERROR"},
	{BASR_ATN, "ATN"},
	{BASR_ACK, "ACK"},
	{0, NULL}
},
icrs[] = {
	{ICR_ASSERT_RST, "ASSERT RST"},
	{ICR_ARBITRATION_PROGRESS, "ARB. IN PROGRESS"},
	{ICR_ARBITRATION_LOST, "LOST ARB."},
	{ICR_ASSERT_ACK, "ASSERT ACK"},
	{ICR_ASSERT_BSY, "ASSERT BSY"},
	{ICR_ASSERT_SEL, "ASSERT SEL"},
	{ICR_ASSERT_ATN, "ASSERT ATN"},
	{ICR_ASSERT_DATA, "ASSERT DATA"},
	{0, NULL}
},
mrs[] = {
	{MR_BLOCK_DMA_MODE, "BLOCK DMA MODE"},
	{MR_TARGET, "TARGET"},
	{MR_ENABLE_PAR_CHECK, "PARITY CHECK"},
	{MR_ENABLE_PAR_INTR, "PARITY INTR"},
	{MR_ENABLE_EOP_INTR, "EOP INTR"},
	{MR_MONITOR_BSY, "MONITOR BSY"},
	{MR_DMA_MODE, "DMA MODE"},
	{MR_ARBITRATE, "ARBITRATE"},
	{0, NULL}
};

 

static void NCR5380_print(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char status, basr, mr, icr, i;

	status = NCR5380_read(STATUS_REG);
	mr = NCR5380_read(MODE_REG);
	icr = NCR5380_read(INITIATOR_COMMAND_REG);
	basr = NCR5380_read(BUS_AND_STATUS_REG);

	printk(KERN_DEBUG "SR =   0x%02x : ", status);
	for (i = 0; signals[i].mask; ++i)
		if (status & signals[i].mask)
			printk(KERN_CONT "%s, ", signals[i].name);
	printk(KERN_CONT "\nBASR = 0x%02x : ", basr);
	for (i = 0; basrs[i].mask; ++i)
		if (basr & basrs[i].mask)
			printk(KERN_CONT "%s, ", basrs[i].name);
	printk(KERN_CONT "\nICR =  0x%02x : ", icr);
	for (i = 0; icrs[i].mask; ++i)
		if (icr & icrs[i].mask)
			printk(KERN_CONT "%s, ", icrs[i].name);
	printk(KERN_CONT "\nMR =   0x%02x : ", mr);
	for (i = 0; mrs[i].mask; ++i)
		if (mr & mrs[i].mask)
			printk(KERN_CONT "%s, ", mrs[i].name);
	printk(KERN_CONT "\n");
}

static struct {
	unsigned char value;
	const char *name;
} phases[] = {
	{PHASE_DATAOUT, "DATAOUT"},
	{PHASE_DATAIN, "DATAIN"},
	{PHASE_CMDOUT, "CMDOUT"},
	{PHASE_STATIN, "STATIN"},
	{PHASE_MSGOUT, "MSGOUT"},
	{PHASE_MSGIN, "MSGIN"},
	{PHASE_UNKNOWN, "UNKNOWN"}
};

 

static void NCR5380_print_phase(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char status;
	int i;

	status = NCR5380_read(STATUS_REG);
	if (!(status & SR_REQ))
		shost_printk(KERN_DEBUG, instance, "REQ not asserted, phase unknown.\n");
	else {
		for (i = 0; (phases[i].value != PHASE_UNKNOWN) &&
		     (phases[i].value != (status & PHASE_MASK)); ++i)
			;
		shost_printk(KERN_DEBUG, instance, "phase %s\n", phases[i].name);
	}
}
#endif

 

static const char *NCR5380_info(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);

	return hostdata->info;
}

 

static int NCR5380_init(struct Scsi_Host *instance, int flags)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int i;
	unsigned long deadline;
	unsigned long accesses_per_ms;

	instance->max_lun = 7;

	hostdata->host = instance;
	hostdata->id_mask = 1 << instance->this_id;
	hostdata->id_higher_mask = 0;
	for (i = hostdata->id_mask; i <= 0x80; i <<= 1)
		if (i > hostdata->id_mask)
			hostdata->id_higher_mask |= i;
	for (i = 0; i < 8; ++i)
		hostdata->busy[i] = 0;
	hostdata->dma_len = 0;

	spin_lock_init(&hostdata->lock);
	hostdata->connected = NULL;
	hostdata->sensing = NULL;
	INIT_LIST_HEAD(&hostdata->autosense);
	INIT_LIST_HEAD(&hostdata->unissued);
	INIT_LIST_HEAD(&hostdata->disconnected);

	hostdata->flags = flags;

	INIT_WORK(&hostdata->main_task, NCR5380_main);
	hostdata->work_q = alloc_workqueue("ncr5380_%d",
	                        WQ_UNBOUND | WQ_MEM_RECLAIM,
				0, instance->host_no);
	if (!hostdata->work_q)
		return -ENOMEM;

	snprintf(hostdata->info, sizeof(hostdata->info),
		"%s, irq %d, io_port 0x%lx, base 0x%lx, can_queue %d, cmd_per_lun %d, sg_tablesize %d, this_id %d, flags { %s%s%s}",
		instance->hostt->name, instance->irq, hostdata->io_port,
		hostdata->base, instance->can_queue, instance->cmd_per_lun,
		instance->sg_tablesize, instance->this_id,
		hostdata->flags & FLAG_DMA_FIXUP     ? "DMA_FIXUP "     : "",
		hostdata->flags & FLAG_NO_PSEUDO_DMA ? "NO_PSEUDO_DMA " : "",
		hostdata->flags & FLAG_TOSHIBA_DELAY ? "TOSHIBA_DELAY " : "");

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(TARGET_COMMAND_REG, 0);
	NCR5380_write(SELECT_ENABLE_REG, 0);

	 
	i = 0;
	deadline = jiffies + 1;
	do {
		cpu_relax();
	} while (time_is_after_jiffies(deadline));
	deadline += msecs_to_jiffies(256);
	do {
		NCR5380_read(STATUS_REG);
		++i;
		cpu_relax();
	} while (time_is_after_jiffies(deadline));
	accesses_per_ms = i / 256;
	hostdata->poll_loops = NCR5380_REG_POLL_TIME * accesses_per_ms / 2;

	return 0;
}

 

static int NCR5380_maybe_reset_bus(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int pass;

	for (pass = 1; (NCR5380_read(STATUS_REG) & SR_BSY) && pass <= 6; ++pass) {
		switch (pass) {
		case 1:
		case 3:
		case 5:
			shost_printk(KERN_ERR, instance, "SCSI bus busy, waiting up to five seconds\n");
			NCR5380_poll_politely(hostdata,
			                      STATUS_REG, SR_BSY, 0, 5 * HZ);
			break;
		case 2:
			shost_printk(KERN_ERR, instance, "bus busy, attempting abort\n");
			do_abort(instance, 1);
			break;
		case 4:
			shost_printk(KERN_ERR, instance, "bus busy, attempting reset\n");
			do_reset(instance);
			 
			if (hostdata->flags & FLAG_TOSHIBA_DELAY)
				msleep(2500);
			else
				msleep(500);
			break;
		case 6:
			shost_printk(KERN_ERR, instance, "bus locked solid\n");
			return -ENXIO;
		}
	}
	return 0;
}

 

static void NCR5380_exit(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);

	cancel_work_sync(&hostdata->main_task);
	destroy_workqueue(hostdata->work_q);
}

 

static void complete_cmd(struct Scsi_Host *instance,
                         struct scsi_cmnd *cmd)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);

	dsprintk(NDEBUG_QUEUES, instance, "complete_cmd: cmd %p\n", cmd);

	if (hostdata->sensing == cmd) {
		 
		if (get_status_byte(cmd) != SAM_STAT_GOOD) {
			scsi_eh_restore_cmnd(cmd, &hostdata->ses);
		} else {
			scsi_eh_restore_cmnd(cmd, &hostdata->ses);
			set_status_byte(cmd, SAM_STAT_CHECK_CONDITION);
		}
		hostdata->sensing = NULL;
	}

	scsi_done(cmd);
}

 

static int NCR5380_queue_command(struct Scsi_Host *instance,
                                 struct scsi_cmnd *cmd)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(cmd);
	unsigned long flags;

#if (NDEBUG & NDEBUG_NO_WRITE)
	switch (cmd->cmnd[0]) {
	case WRITE_6:
	case WRITE_10:
		shost_printk(KERN_DEBUG, instance, "WRITE attempted with NDEBUG_NO_WRITE set\n");
		cmd->result = (DID_ERROR << 16);
		scsi_done(cmd);
		return 0;
	}
#endif  

	cmd->result = 0;

	spin_lock_irqsave(&hostdata->lock, flags);

	if (!NCR5380_acquire_dma_irq(instance)) {
		spin_unlock_irqrestore(&hostdata->lock, flags);

		return SCSI_MLQUEUE_HOST_BUSY;
	}

	 

	if (cmd->cmnd[0] == REQUEST_SENSE)
		list_add(&ncmd->list, &hostdata->unissued);
	else
		list_add_tail(&ncmd->list, &hostdata->unissued);

	spin_unlock_irqrestore(&hostdata->lock, flags);

	dsprintk(NDEBUG_QUEUES, instance, "command %p added to %s of queue\n",
	         cmd, (cmd->cmnd[0] == REQUEST_SENSE) ? "head" : "tail");

	 
	queue_work(hostdata->work_q, &hostdata->main_task);
	return 0;
}

static inline void maybe_release_dma_irq(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);

	 
	if (list_empty(&hostdata->disconnected) &&
	    list_empty(&hostdata->unissued) &&
	    list_empty(&hostdata->autosense) &&
	    !hostdata->connected &&
	    !hostdata->selecting) {
		NCR5380_release_dma_irq(instance);
	}
}

 

static struct scsi_cmnd *dequeue_next_cmd(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	struct NCR5380_cmd *ncmd;
	struct scsi_cmnd *cmd;

	if (hostdata->sensing || list_empty(&hostdata->autosense)) {
		list_for_each_entry(ncmd, &hostdata->unissued, list) {
			cmd = NCR5380_to_scmd(ncmd);
			dsprintk(NDEBUG_QUEUES, instance, "dequeue: cmd=%p target=%d busy=0x%02x lun=%llu\n",
			         cmd, scmd_id(cmd), hostdata->busy[scmd_id(cmd)], cmd->device->lun);

			if (!(hostdata->busy[scmd_id(cmd)] & (1 << cmd->device->lun))) {
				list_del(&ncmd->list);
				dsprintk(NDEBUG_QUEUES, instance,
				         "dequeue: removed %p from issue queue\n", cmd);
				return cmd;
			}
		}
	} else {
		 
		ncmd = list_first_entry(&hostdata->autosense,
		                        struct NCR5380_cmd, list);
		list_del(&ncmd->list);
		cmd = NCR5380_to_scmd(ncmd);
		dsprintk(NDEBUG_QUEUES, instance,
		         "dequeue: removed %p from autosense queue\n", cmd);
		scsi_eh_prep_cmnd(cmd, &hostdata->ses, NULL, 0, ~0);
		hostdata->sensing = cmd;
		return cmd;
	}
	return NULL;
}

static void requeue_cmd(struct Scsi_Host *instance, struct scsi_cmnd *cmd)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(cmd);

	if (hostdata->sensing == cmd) {
		scsi_eh_restore_cmnd(cmd, &hostdata->ses);
		list_add(&ncmd->list, &hostdata->autosense);
		hostdata->sensing = NULL;
	} else
		list_add(&ncmd->list, &hostdata->unissued);
}

 

static void NCR5380_main(struct work_struct *work)
{
	struct NCR5380_hostdata *hostdata =
		container_of(work, struct NCR5380_hostdata, main_task);
	struct Scsi_Host *instance = hostdata->host;
	int done;

	do {
		done = 1;

		spin_lock_irq(&hostdata->lock);
		while (!hostdata->connected && !hostdata->selecting) {
			struct scsi_cmnd *cmd = dequeue_next_cmd(instance);

			if (!cmd)
				break;

			dsprintk(NDEBUG_MAIN, instance, "main: dequeued %p\n", cmd);

			 
			 

			if (!NCR5380_select(instance, cmd)) {
				dsprintk(NDEBUG_MAIN, instance, "main: select complete\n");
			} else {
				dsprintk(NDEBUG_MAIN | NDEBUG_QUEUES, instance,
				         "main: select failed, returning %p to queue\n", cmd);
				requeue_cmd(instance, cmd);
			}
		}
		if (hostdata->connected && !hostdata->dma_len) {
			dsprintk(NDEBUG_MAIN, instance, "main: performing information transfer\n");
			NCR5380_information_transfer(instance);
			done = 0;
		}
		if (!hostdata->connected) {
			NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
			maybe_release_dma_irq(instance);
		}
		spin_unlock_irq(&hostdata->lock);
		if (!done)
			cond_resched();
	} while (!done);
}

 

static void NCR5380_dma_complete(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(hostdata->connected);
	int transferred;
	unsigned char **data;
	int *count;
	int saved_data = 0, overrun = 0;
	unsigned char p;

	if (hostdata->read_overruns) {
		p = ncmd->phase;
		if (p & SR_IO) {
			udelay(10);
			if ((NCR5380_read(BUS_AND_STATUS_REG) &
			     (BASR_PHASE_MATCH | BASR_ACK)) ==
			    (BASR_PHASE_MATCH | BASR_ACK)) {
				saved_data = NCR5380_read(INPUT_DATA_REG);
				overrun = 1;
				dsprintk(NDEBUG_DMA, instance, "read overrun handled\n");
			}
		}
	}

#ifdef CONFIG_SUN3
	if (sun3scsi_dma_finish(hostdata->connected->sc_data_direction)) {
		pr_err("scsi%d: overrun in UDC counter -- not prepared to deal with this!\n",
		       instance->host_no);
		BUG();
	}

	if ((NCR5380_read(BUS_AND_STATUS_REG) & (BASR_PHASE_MATCH | BASR_ACK)) ==
	    (BASR_PHASE_MATCH | BASR_ACK)) {
		pr_err("scsi%d: BASR %02x\n", instance->host_no,
		       NCR5380_read(BUS_AND_STATUS_REG));
		pr_err("scsi%d: bus stuck in data phase -- probably a single byte overrun!\n",
		       instance->host_no);
		BUG();
	}
#endif

	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	NCR5380_read(RESET_PARITY_INTERRUPT_REG);

	transferred = hostdata->dma_len - NCR5380_dma_residual(hostdata);
	hostdata->dma_len = 0;

	data = (unsigned char **)&ncmd->ptr;
	count = &ncmd->this_residual;
	*data += transferred;
	*count -= transferred;

	if (hostdata->read_overruns) {
		int cnt, toPIO;

		if ((NCR5380_read(STATUS_REG) & PHASE_MASK) == p && (p & SR_IO)) {
			cnt = toPIO = hostdata->read_overruns;
			if (overrun) {
				dsprintk(NDEBUG_DMA, instance,
				         "Got an input overrun, using saved byte\n");
				*(*data)++ = saved_data;
				(*count)--;
				cnt--;
				toPIO--;
			}
			if (toPIO > 0) {
				dsprintk(NDEBUG_DMA, instance,
				         "Doing %d byte PIO to 0x%p\n", cnt, *data);
				NCR5380_transfer_pio(instance, &p, &cnt, data, 0);
				*count -= toPIO - cnt;
			}
		}
	}
}

 

static irqreturn_t __maybe_unused NCR5380_intr(int irq, void *dev_id)
{
	struct Scsi_Host *instance = dev_id;
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int handled = 0;
	unsigned char basr;
	unsigned long flags;

	spin_lock_irqsave(&hostdata->lock, flags);

	basr = NCR5380_read(BUS_AND_STATUS_REG);
	if (basr & BASR_IRQ) {
		unsigned char mr = NCR5380_read(MODE_REG);
		unsigned char sr = NCR5380_read(STATUS_REG);

		dsprintk(NDEBUG_INTR, instance, "IRQ %d, BASR 0x%02x, SR 0x%02x, MR 0x%02x\n",
		         irq, basr, sr, mr);

		if ((mr & MR_DMA_MODE) || (mr & MR_MONITOR_BSY)) {
			 

			dsprintk(NDEBUG_INTR, instance, "interrupt in DMA mode\n");

			if (hostdata->connected) {
				NCR5380_dma_complete(instance);
				queue_work(hostdata->work_q, &hostdata->main_task);
			} else {
				NCR5380_write(MODE_REG, MR_BASE);
				NCR5380_read(RESET_PARITY_INTERRUPT_REG);
			}
		} else if ((NCR5380_read(CURRENT_SCSI_DATA_REG) & hostdata->id_mask) &&
		    (sr & (SR_SEL | SR_IO | SR_BSY | SR_RST)) == (SR_SEL | SR_IO)) {
			 
			NCR5380_write(SELECT_ENABLE_REG, 0);
			NCR5380_read(RESET_PARITY_INTERRUPT_REG);

			dsprintk(NDEBUG_INTR, instance, "interrupt with SEL and IO\n");

			if (!hostdata->connected) {
				NCR5380_reselect(instance);
				queue_work(hostdata->work_q, &hostdata->main_task);
			}
			if (!hostdata->connected)
				NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		} else {
			 
			NCR5380_read(RESET_PARITY_INTERRUPT_REG);

			if (sr & SR_RST) {
				 
				shost_printk(KERN_WARNING, instance,
					     "bus reset interrupt\n");
				bus_reset_cleanup(instance);
			} else {
				dsprintk(NDEBUG_INTR, instance, "unknown interrupt\n");
			}
#ifdef SUN3_SCSI_VME
			dregs->csr |= CSR_DMA_ENABLE;
#endif
		}
		handled = 1;
	} else {
		dsprintk(NDEBUG_INTR, instance, "interrupt without IRQ bit\n");
#ifdef SUN3_SCSI_VME
		dregs->csr |= CSR_DMA_ENABLE;
#endif
	}

	spin_unlock_irqrestore(&hostdata->lock, flags);

	return IRQ_RETVAL(handled);
}

 

static bool NCR5380_select(struct Scsi_Host *instance, struct scsi_cmnd *cmd)
	__releases(&hostdata->lock) __acquires(&hostdata->lock)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char tmp[3], phase;
	unsigned char *data;
	int len;
	int err;
	bool ret = true;
	bool can_disconnect = instance->irq != NO_IRQ &&
			      cmd->cmnd[0] != REQUEST_SENSE &&
			      (disconnect_mask & BIT(scmd_id(cmd)));

	NCR5380_dprint(NDEBUG_ARBITRATION, instance);
	dsprintk(NDEBUG_ARBITRATION, instance, "starting arbitration, id = %d\n",
	         instance->this_id);

	 
	hostdata->selecting = cmd;

	 

	NCR5380_write(TARGET_COMMAND_REG, 0);

	 

	NCR5380_write(OUTPUT_DATA_REG, hostdata->id_mask);
	NCR5380_write(MODE_REG, MR_ARBITRATE);

	 

	spin_unlock_irq(&hostdata->lock);
	err = NCR5380_poll_politely2(hostdata, MODE_REG, MR_ARBITRATE, 0,
	                INITIATOR_COMMAND_REG, ICR_ARBITRATION_PROGRESS,
	                                       ICR_ARBITRATION_PROGRESS, HZ);
	spin_lock_irq(&hostdata->lock);
	if (!(NCR5380_read(MODE_REG) & MR_ARBITRATE)) {
		 
		goto out;
	}
	if (!hostdata->selecting) {
		 
		NCR5380_write(MODE_REG, MR_BASE);
		return false;
	}
	if (err < 0) {
		NCR5380_write(MODE_REG, MR_BASE);
		shost_printk(KERN_ERR, instance,
		             "select: arbitration timeout\n");
		goto out;
	}
	spin_unlock_irq(&hostdata->lock);

	 
	udelay(3);

	 
	if ((NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST) ||
	    (NCR5380_read(CURRENT_SCSI_DATA_REG) & hostdata->id_higher_mask) ||
	    (NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST)) {
		NCR5380_write(MODE_REG, MR_BASE);
		dsprintk(NDEBUG_ARBITRATION, instance, "lost arbitration, deasserting MR_ARBITRATE\n");
		spin_lock_irq(&hostdata->lock);
		goto out;
	}

	 
	NCR5380_write(INITIATOR_COMMAND_REG,
		      ICR_BASE | ICR_ASSERT_SEL | ICR_ASSERT_BSY);

	 

	if (hostdata->flags & FLAG_TOSHIBA_DELAY)
		udelay(15);
	else
		udelay(2);

	spin_lock_irq(&hostdata->lock);

	 
	if (!(NCR5380_read(MODE_REG) & MR_ARBITRATE))
		goto out;

	if (!hostdata->selecting) {
		NCR5380_write(MODE_REG, MR_BASE);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		return false;
	}

	dsprintk(NDEBUG_ARBITRATION, instance, "won arbitration\n");

	 

	NCR5380_write(OUTPUT_DATA_REG, hostdata->id_mask | (1 << scmd_id(cmd)));

	 

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_BSY |
	              ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_SEL);
	NCR5380_write(MODE_REG, MR_BASE);

	 
	NCR5380_write(SELECT_ENABLE_REG, 0);

	spin_unlock_irq(&hostdata->lock);

	 
	udelay(1);         

	 
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA |
	              ICR_ASSERT_ATN | ICR_ASSERT_SEL);

	 

	udelay(1);

	dsprintk(NDEBUG_SELECTION, instance, "selecting target %d\n", scmd_id(cmd));

	 

	err = NCR5380_poll_politely(hostdata, STATUS_REG, SR_BSY, SR_BSY,
	                            msecs_to_jiffies(250));

	if ((NCR5380_read(STATUS_REG) & (SR_SEL | SR_IO)) == (SR_SEL | SR_IO)) {
		spin_lock_irq(&hostdata->lock);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		NCR5380_reselect(instance);
		shost_printk(KERN_ERR, instance, "reselection after won arbitration?\n");
		goto out;
	}

	if (err < 0) {
		spin_lock_irq(&hostdata->lock);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

		 
		if (!hostdata->selecting)
			return false;

		cmd->result = DID_BAD_TARGET << 16;
		complete_cmd(instance, cmd);
		dsprintk(NDEBUG_SELECTION, instance,
			"target did not respond within 250ms\n");
		ret = false;
		goto out;
	}

	 

	udelay(1);

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);

	 

	 

	err = NCR5380_poll_politely(hostdata, STATUS_REG, SR_REQ, SR_REQ, HZ);
	spin_lock_irq(&hostdata->lock);
	if (err < 0) {
		shost_printk(KERN_ERR, instance, "select: REQ timeout\n");
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		goto out;
	}
	if (!hostdata->selecting) {
		do_abort(instance, 0);
		return false;
	}

	dsprintk(NDEBUG_SELECTION, instance, "target %d selected, going into MESSAGE OUT phase.\n",
	         scmd_id(cmd));
	tmp[0] = IDENTIFY(can_disconnect, cmd->device->lun);

	len = 1;
	data = tmp;
	phase = PHASE_MSGOUT;
	NCR5380_transfer_pio(instance, &phase, &len, &data, 0);
	if (len) {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		cmd->result = DID_ERROR << 16;
		complete_cmd(instance, cmd);
		dsprintk(NDEBUG_SELECTION, instance, "IDENTIFY message transfer failed\n");
		ret = false;
		goto out;
	}

	dsprintk(NDEBUG_SELECTION, instance, "nexus established.\n");

	hostdata->connected = cmd;
	hostdata->busy[cmd->device->id] |= 1 << cmd->device->lun;

#ifdef SUN3_SCSI_VME
	dregs->csr |= CSR_INTR;
#endif

	initialize_SCp(cmd);

	ret = false;

out:
	if (!hostdata->selecting)
		return false;
	hostdata->selecting = NULL;
	return ret;
}

 

 

static int NCR5380_transfer_pio(struct Scsi_Host *instance,
				unsigned char *phase, int *count,
				unsigned char **data, unsigned int can_sleep)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char p = *phase, tmp;
	int c = *count;
	unsigned char *d = *data;

	 

	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(p));

	do {
		 

		if (NCR5380_poll_politely(hostdata, STATUS_REG, SR_REQ, SR_REQ,
					  HZ * can_sleep) < 0)
			break;

		dsprintk(NDEBUG_HANDSHAKE, instance, "REQ asserted\n");

		 
		if ((NCR5380_read(STATUS_REG) & PHASE_MASK) != p) {
			dsprintk(NDEBUG_PIO, instance, "phase mismatch\n");
			NCR5380_dprint_phase(NDEBUG_PIO, instance);
			break;
		}

		 
		if (!(p & SR_IO))
			NCR5380_write(OUTPUT_DATA_REG, *d);
		else
			*d = NCR5380_read(CURRENT_SCSI_DATA_REG);

		++d;

		 

		if (!(p & SR_IO)) {
			if (!((p & SR_MSG) && c > 1)) {
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA);
				NCR5380_dprint(NDEBUG_PIO, instance);
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE |
				              ICR_ASSERT_DATA | ICR_ASSERT_ACK);
			} else {
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE |
				              ICR_ASSERT_DATA | ICR_ASSERT_ATN);
				NCR5380_dprint(NDEBUG_PIO, instance);
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE |
				              ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
			}
		} else {
			NCR5380_dprint(NDEBUG_PIO, instance);
			NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
		}

		if (NCR5380_poll_politely(hostdata,
		                          STATUS_REG, SR_REQ, 0, 5 * HZ * can_sleep) < 0)
			break;

		dsprintk(NDEBUG_HANDSHAKE, instance, "REQ negated, handshake complete\n");

 
		if (!(p == PHASE_MSGIN && c == 1)) {
			if (p == PHASE_MSGOUT && c > 1)
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
			else
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		}
	} while (--c);

	dsprintk(NDEBUG_PIO, instance, "residual %d\n", c);

	*count = c;
	*data = d;
	tmp = NCR5380_read(STATUS_REG);
	 
	if ((tmp & SR_REQ) || ((tmp & SR_IO) && c == 0))
		*phase = tmp & PHASE_MASK;
	else
		*phase = PHASE_UNKNOWN;

	if (!c || (*phase == p))
		return 0;
	else
		return -1;
}

 

static void do_reset(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata __maybe_unused *hostdata = shost_priv(instance);
	unsigned long flags;

	local_irq_save(flags);
	NCR5380_write(TARGET_COMMAND_REG,
	              PHASE_SR_TO_TCR(NCR5380_read(STATUS_REG) & PHASE_MASK));
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_RST);
	udelay(50);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	(void)NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	local_irq_restore(flags);
}

 

static int do_abort(struct Scsi_Host *instance, unsigned int can_sleep)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char *msgptr, phase, tmp;
	int len;
	int rc;

	 
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);

	 

	rc = NCR5380_poll_politely(hostdata, STATUS_REG, SR_REQ, SR_REQ,
				   10 * HZ * can_sleep);
	if (rc < 0)
		goto out;

	tmp = NCR5380_read(STATUS_REG) & PHASE_MASK;

	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));

	if (tmp != PHASE_MSGOUT) {
		NCR5380_write(INITIATOR_COMMAND_REG,
		              ICR_BASE | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
		rc = NCR5380_poll_politely(hostdata, STATUS_REG, SR_REQ, 0,
					   3 * HZ * can_sleep);
		if (rc < 0)
			goto out;
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
	}

	tmp = ABORT;
	msgptr = &tmp;
	len = 1;
	phase = PHASE_MSGOUT;
	NCR5380_transfer_pio(instance, &phase, &len, &msgptr, can_sleep);
	if (len)
		rc = -ENXIO;

	 

out:
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	return rc;
}

 


static int NCR5380_transfer_dma(struct Scsi_Host *instance,
				unsigned char *phase, int *count,
				unsigned char **data)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int c = *count;
	unsigned char p = *phase;
	unsigned char *d = *data;
	unsigned char tmp;
	int result = 0;

	if ((tmp = (NCR5380_read(STATUS_REG) & PHASE_MASK)) != p) {
		*phase = tmp;
		return -1;
	}

	NCR5380_to_ncmd(hostdata->connected)->phase = p;

	if (p & SR_IO) {
		if (hostdata->read_overruns)
			c -= hostdata->read_overruns;
		else if (hostdata->flags & FLAG_DMA_FIXUP)
			--c;
	}

	dsprintk(NDEBUG_DMA, instance, "initializing DMA %s: length %d, address %p\n",
	         (p & SR_IO) ? "receive" : "send", c, d);

#ifdef CONFIG_SUN3
	 
	sun3scsi_dma_start(c, *data);
#endif

	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(p));
	NCR5380_write(MODE_REG, MR_BASE | MR_DMA_MODE | MR_MONITOR_BSY |
	                        MR_ENABLE_EOP_INTR);

	if (!(hostdata->flags & FLAG_LATE_DMA_SETUP)) {
		 
		if (p & SR_IO)
			result = NCR5380_dma_recv_setup(hostdata, d, c);
		else
			result = NCR5380_dma_send_setup(hostdata, d, c);
	}

	 

	if (p & SR_IO) {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		NCR5380_io_delay(1);
		NCR5380_write(START_DMA_INITIATOR_RECEIVE_REG, 0);
	} else {
		NCR5380_io_delay(1);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA);
		NCR5380_io_delay(1);
		NCR5380_write(START_DMA_SEND_REG, 0);
		NCR5380_io_delay(1);
	}

#ifdef CONFIG_SUN3
#ifdef SUN3_SCSI_VME
	dregs->csr |= CSR_DMA_ENABLE;
#endif
	sun3_dma_active = 1;
#endif

	if (hostdata->flags & FLAG_LATE_DMA_SETUP) {
		 
		if (p & SR_IO)
			result = NCR5380_dma_recv_setup(hostdata, d, c);
		else
			result = NCR5380_dma_send_setup(hostdata, d, c);
	}

	 
	if (result < 0)
		return result;

	 
	if (result > 0) {
		hostdata->dma_len = result;
		return 0;
	}

	 
	hostdata->dma_len = c;

 

	if (hostdata->flags & FLAG_DMA_FIXUP) {
		if (p & SR_IO) {
			 

			if (NCR5380_poll_politely(hostdata, BUS_AND_STATUS_REG,
			                          BASR_DRQ, BASR_DRQ, 0) < 0) {
				result = -1;
				shost_printk(KERN_ERR, instance, "PDMA read: DRQ timeout\n");
			}
			if (NCR5380_poll_politely(hostdata, STATUS_REG,
			                          SR_REQ, 0, 0) < 0) {
				result = -1;
				shost_printk(KERN_ERR, instance, "PDMA read: !REQ timeout\n");
			}
			d[*count - 1] = NCR5380_read(INPUT_DATA_REG);
		} else {
			 
			if (NCR5380_poll_politely2(hostdata,
			     BUS_AND_STATUS_REG, BASR_DRQ, BASR_DRQ,
			     BUS_AND_STATUS_REG, BASR_PHASE_MATCH, 0, 0) < 0) {
				result = -1;
				shost_printk(KERN_ERR, instance, "PDMA write: DRQ and phase timeout\n");
			}
		}
	}

	NCR5380_dma_complete(instance);
	return result;
}

 

static void NCR5380_information_transfer(struct Scsi_Host *instance)
	__releases(&hostdata->lock) __acquires(&hostdata->lock)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char msgout = NOP;
	int sink = 0;
	int len;
	int transfersize;
	unsigned char *data;
	unsigned char phase, tmp, extended_msg[10], old_phase = 0xff;
	struct scsi_cmnd *cmd;

#ifdef SUN3_SCSI_VME
	dregs->csr |= CSR_INTR;
#endif

	while ((cmd = hostdata->connected)) {
		struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(cmd);

		tmp = NCR5380_read(STATUS_REG);
		 
		if (tmp & SR_REQ) {
			phase = (tmp & PHASE_MASK);
			if (phase != old_phase) {
				old_phase = phase;
				NCR5380_dprint_phase(NDEBUG_INFORMATION, instance);
			}
#ifdef CONFIG_SUN3
			if (phase == PHASE_CMDOUT &&
			    sun3_dma_setup_done != cmd) {
				int count;

				advance_sg_buffer(ncmd);

				count = sun3scsi_dma_xfer_len(hostdata, cmd);

				if (count > 0) {
					if (cmd->sc_data_direction == DMA_TO_DEVICE)
						sun3scsi_dma_send_setup(hostdata,
									ncmd->ptr, count);
					else
						sun3scsi_dma_recv_setup(hostdata,
									ncmd->ptr, count);
					sun3_dma_setup_done = cmd;
				}
#ifdef SUN3_SCSI_VME
				dregs->csr |= CSR_INTR;
#endif
			}
#endif  

			if (sink && (phase != PHASE_MSGOUT)) {
				NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));

				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN |
				              ICR_ASSERT_ACK);
				while (NCR5380_read(STATUS_REG) & SR_REQ)
					;
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE |
				              ICR_ASSERT_ATN);
				sink = 0;
				continue;
			}

			switch (phase) {
			case PHASE_DATAOUT:
#if (NDEBUG & NDEBUG_NO_DATAOUT)
				shost_printk(KERN_DEBUG, instance, "NDEBUG_NO_DATAOUT set, attempted DATAOUT aborted\n");
				sink = 1;
				do_abort(instance, 0);
				cmd->result = DID_ERROR << 16;
				complete_cmd(instance, cmd);
				hostdata->connected = NULL;
				hostdata->busy[scmd_id(cmd)] &= ~(1 << cmd->device->lun);
				return;
#endif
			case PHASE_DATAIN:
				 

				advance_sg_buffer(ncmd);
				dsprintk(NDEBUG_INFORMATION, instance,
					"this residual %d, sg ents %d\n",
					ncmd->this_residual,
					sg_nents(ncmd->buffer));

				 

				transfersize = 0;
				if (!cmd->device->borken)
					transfersize = NCR5380_dma_xfer_len(hostdata, cmd);

				if (transfersize > 0) {
					len = transfersize;
					if (NCR5380_transfer_dma(instance, &phase,
					    &len, (unsigned char **)&ncmd->ptr)) {
						 
						scmd_printk(KERN_INFO, cmd,
							"switching to slow handshake\n");
						cmd->device->borken = 1;
						do_reset(instance);
						bus_reset_cleanup(instance);
					}
				} else {
					 
					transfersize = min(ncmd->this_residual,
							   NCR5380_PIO_CHUNK_SIZE);
					len = transfersize;
					NCR5380_transfer_pio(instance, &phase, &len,
							     (unsigned char **)&ncmd->ptr,
							     0);
					ncmd->this_residual -= transfersize - len;
				}
#ifdef CONFIG_SUN3
				if (sun3_dma_setup_done == cmd)
					sun3_dma_setup_done = NULL;
#endif
				return;
			case PHASE_MSGIN:
				len = 1;
				data = &tmp;
				NCR5380_transfer_pio(instance, &phase, &len, &data, 0);
				ncmd->message = tmp;

				switch (tmp) {
				case ABORT:
					set_host_byte(cmd, DID_ABORT);
					fallthrough;
				case COMMAND_COMPLETE:
					 
					sink = 1;
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					dsprintk(NDEBUG_QUEUES, instance,
					         "COMMAND COMPLETE %p target %d lun %llu\n",
					         cmd, scmd_id(cmd), cmd->device->lun);

					hostdata->connected = NULL;
					hostdata->busy[scmd_id(cmd)] &= ~(1 << cmd->device->lun);

					set_status_byte(cmd, ncmd->status);

					set_resid_from_SCp(cmd);

					if (cmd->cmnd[0] == REQUEST_SENSE)
						complete_cmd(instance, cmd);
					else {
						if (ncmd->status == SAM_STAT_CHECK_CONDITION ||
						    ncmd->status == SAM_STAT_COMMAND_TERMINATED) {
							dsprintk(NDEBUG_QUEUES, instance, "autosense: adding cmd %p to tail of autosense queue\n",
							         cmd);
							list_add_tail(&ncmd->list,
							              &hostdata->autosense);
						} else
							complete_cmd(instance, cmd);
					}

					 
					NCR5380_write(TARGET_COMMAND_REG, 0);

					return;
				case MESSAGE_REJECT:
					 
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					switch (hostdata->last_message) {
					case HEAD_OF_QUEUE_TAG:
					case ORDERED_QUEUE_TAG:
					case SIMPLE_QUEUE_TAG:
						cmd->device->simple_tags = 0;
						hostdata->busy[cmd->device->id] |= (1 << (cmd->device->lun & 0xFF));
						break;
					default:
						break;
					}
					break;
				case DISCONNECT:
					 
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					hostdata->connected = NULL;
					list_add(&ncmd->list, &hostdata->disconnected);
					dsprintk(NDEBUG_INFORMATION | NDEBUG_QUEUES,
					         instance, "connected command %p for target %d lun %llu moved to disconnected queue\n",
					         cmd, scmd_id(cmd), cmd->device->lun);

					 
					NCR5380_write(TARGET_COMMAND_REG, 0);

#ifdef SUN3_SCSI_VME
					dregs->csr |= CSR_DMA_ENABLE;
#endif
					return;
					 
				case SAVE_POINTERS:
				case RESTORE_POINTERS:
					 
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					break;
				case EXTENDED_MESSAGE:
					 
					extended_msg[0] = EXTENDED_MESSAGE;
					 
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

					spin_unlock_irq(&hostdata->lock);

					dsprintk(NDEBUG_EXTENDED, instance, "receiving extended message\n");

					len = 2;
					data = extended_msg + 1;
					phase = PHASE_MSGIN;
					NCR5380_transfer_pio(instance, &phase, &len, &data, 1);
					dsprintk(NDEBUG_EXTENDED, instance, "length %d, code 0x%02x\n",
					         (int)extended_msg[1],
					         (int)extended_msg[2]);

					if (!len && extended_msg[1] > 0 &&
					    extended_msg[1] <= sizeof(extended_msg) - 2) {
						 
						NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
						len = extended_msg[1] - 1;
						data = extended_msg + 3;
						phase = PHASE_MSGIN;

						NCR5380_transfer_pio(instance, &phase, &len, &data, 1);
						dsprintk(NDEBUG_EXTENDED, instance, "message received, residual %d\n",
						         len);

						switch (extended_msg[2]) {
						case EXTENDED_SDTR:
						case EXTENDED_WDTR:
							tmp = 0;
						}
					} else if (len) {
						shost_printk(KERN_ERR, instance, "error receiving extended message\n");
						tmp = 0;
					} else {
						shost_printk(KERN_NOTICE, instance, "extended message code %02x length %d is too long\n",
						             extended_msg[2], extended_msg[1]);
						tmp = 0;
					}

					spin_lock_irq(&hostdata->lock);
					if (!hostdata->connected)
						return;

					 
					fallthrough;
				default:
					 
					if (tmp == EXTENDED_MESSAGE)
						scmd_printk(KERN_INFO, cmd,
						            "rejecting unknown extended message code %02x, length %d\n",
						            extended_msg[2], extended_msg[1]);
					else if (tmp)
						scmd_printk(KERN_INFO, cmd,
						            "rejecting unknown message code %02x\n",
						            tmp);

					msgout = MESSAGE_REJECT;
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
					break;
				}  
				break;
			case PHASE_MSGOUT:
				len = 1;
				data = &msgout;
				hostdata->last_message = msgout;
				NCR5380_transfer_pio(instance, &phase, &len, &data, 0);
				if (msgout == ABORT) {
					hostdata->connected = NULL;
					hostdata->busy[scmd_id(cmd)] &= ~(1 << cmd->device->lun);
					cmd->result = DID_ERROR << 16;
					complete_cmd(instance, cmd);
					return;
				}
				msgout = NOP;
				break;
			case PHASE_CMDOUT:
				len = cmd->cmd_len;
				data = cmd->cmnd;
				 
				NCR5380_transfer_pio(instance, &phase, &len, &data, 0);
				break;
			case PHASE_STATIN:
				len = 1;
				data = &tmp;
				NCR5380_transfer_pio(instance, &phase, &len, &data, 0);
				ncmd->status = tmp;
				break;
			default:
				shost_printk(KERN_ERR, instance, "unknown phase\n");
				NCR5380_dprint(NDEBUG_ANY, instance);
			}  
		} else {
			spin_unlock_irq(&hostdata->lock);
			NCR5380_poll_politely(hostdata, STATUS_REG, SR_REQ, SR_REQ, HZ);
			spin_lock_irq(&hostdata->lock);
		}
	}
}

 

static void NCR5380_reselect(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned char target_mask;
	unsigned char lun;
	unsigned char msg[3];
	struct NCR5380_cmd *ncmd;
	struct scsi_cmnd *tmp;

	 

	NCR5380_write(MODE_REG, MR_BASE);

	target_mask = NCR5380_read(CURRENT_SCSI_DATA_REG) & ~(hostdata->id_mask);
	if (!target_mask || target_mask & (target_mask - 1)) {
		shost_printk(KERN_WARNING, instance,
			     "reselect: bad target_mask 0x%02x\n", target_mask);
		return;
	}

	 

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_BSY);
	if (NCR5380_poll_politely(hostdata,
	                          STATUS_REG, SR_SEL, 0, 0) < 0) {
		shost_printk(KERN_ERR, instance, "reselect: !SEL timeout\n");
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		return;
	}
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

	 

	if (NCR5380_poll_politely(hostdata,
	                          STATUS_REG, SR_REQ, SR_REQ, 0) < 0) {
		if ((NCR5380_read(STATUS_REG) & (SR_BSY | SR_SEL)) == 0)
			 
			return;
		shost_printk(KERN_ERR, instance, "reselect: REQ timeout\n");
		do_abort(instance, 0);
		return;
	}

#ifdef CONFIG_SUN3
	 
	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(PHASE_MSGIN));

	 
	msg[0] = NCR5380_read(CURRENT_SCSI_DATA_REG);
#else
	{
		int len = 1;
		unsigned char *data = msg;
		unsigned char phase = PHASE_MSGIN;

		NCR5380_transfer_pio(instance, &phase, &len, &data, 0);

		if (len) {
			do_abort(instance, 0);
			return;
		}
	}
#endif  

	if (!(msg[0] & 0x80)) {
		shost_printk(KERN_ERR, instance, "expecting IDENTIFY message, got ");
		spi_print_msg(msg);
		printk("\n");
		do_abort(instance, 0);
		return;
	}
	lun = msg[0] & 0x07;

	 

	 

	tmp = NULL;
	list_for_each_entry(ncmd, &hostdata->disconnected, list) {
		struct scsi_cmnd *cmd = NCR5380_to_scmd(ncmd);

		if (target_mask == (1 << scmd_id(cmd)) &&
		    lun == (u8)cmd->device->lun) {
			list_del(&ncmd->list);
			tmp = cmd;
			break;
		}
	}

	if (tmp) {
		dsprintk(NDEBUG_RESELECTION | NDEBUG_QUEUES, instance,
		         "reselect: removed %p from disconnected queue\n", tmp);
	} else {
		int target = ffs(target_mask) - 1;

		shost_printk(KERN_ERR, instance, "target bitmask 0x%02x lun %d not in disconnected queue.\n",
		             target_mask, lun);
		 
		if (do_abort(instance, 0) == 0)
			hostdata->busy[target] &= ~(1 << lun);
		return;
	}

#ifdef CONFIG_SUN3
	if (sun3_dma_setup_done != tmp) {
		int count;

		advance_sg_buffer(ncmd);

		count = sun3scsi_dma_xfer_len(hostdata, tmp);

		if (count > 0) {
			if (tmp->sc_data_direction == DMA_TO_DEVICE)
				sun3scsi_dma_send_setup(hostdata,
							ncmd->ptr, count);
			else
				sun3scsi_dma_recv_setup(hostdata,
							ncmd->ptr, count);
			sun3_dma_setup_done = tmp;
		}
	}

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
#endif  

	 
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

	hostdata->connected = tmp;
	dsprintk(NDEBUG_RESELECTION, instance, "nexus established, target %d, lun %llu\n",
	         scmd_id(tmp), tmp->device->lun);
}

 

static bool list_find_cmd(struct list_head *haystack,
                          struct scsi_cmnd *needle)
{
	struct NCR5380_cmd *ncmd;

	list_for_each_entry(ncmd, haystack, list)
		if (NCR5380_to_scmd(ncmd) == needle)
			return true;
	return false;
}

 

static bool list_del_cmd(struct list_head *haystack,
                         struct scsi_cmnd *needle)
{
	if (list_find_cmd(haystack, needle)) {
		struct NCR5380_cmd *ncmd = NCR5380_to_ncmd(needle);

		list_del(&ncmd->list);
		return true;
	}
	return false;
}

 

static int NCR5380_abort(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *instance = cmd->device->host;
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned long flags;
	int result = SUCCESS;

	spin_lock_irqsave(&hostdata->lock, flags);

#if (NDEBUG & NDEBUG_ANY)
	scmd_printk(KERN_INFO, cmd, __func__);
#endif
	NCR5380_dprint(NDEBUG_ANY, instance);
	NCR5380_dprint_phase(NDEBUG_ANY, instance);

	if (list_del_cmd(&hostdata->unissued, cmd)) {
		dsprintk(NDEBUG_ABORT, instance,
		         "abort: removed %p from issue queue\n", cmd);
		cmd->result = DID_ABORT << 16;
		scsi_done(cmd);  
		goto out;
	}

	if (hostdata->selecting == cmd) {
		dsprintk(NDEBUG_ABORT, instance,
		         "abort: cmd %p == selecting\n", cmd);
		hostdata->selecting = NULL;
		cmd->result = DID_ABORT << 16;
		complete_cmd(instance, cmd);
		goto out;
	}

	if (list_del_cmd(&hostdata->disconnected, cmd)) {
		dsprintk(NDEBUG_ABORT, instance,
		         "abort: removed %p from disconnected list\n", cmd);
		 
		set_host_byte(cmd, DID_ERROR);
		complete_cmd(instance, cmd);
		result = FAILED;
		goto out;
	}

	if (hostdata->connected == cmd) {
		dsprintk(NDEBUG_ABORT, instance, "abort: cmd %p is connected\n", cmd);
		hostdata->connected = NULL;
		hostdata->dma_len = 0;
		if (do_abort(instance, 0) < 0) {
			set_host_byte(cmd, DID_ERROR);
			complete_cmd(instance, cmd);
			result = FAILED;
			goto out;
		}
		set_host_byte(cmd, DID_ABORT);
		complete_cmd(instance, cmd);
		goto out;
	}

	if (list_del_cmd(&hostdata->autosense, cmd)) {
		dsprintk(NDEBUG_ABORT, instance,
		         "abort: removed %p from sense queue\n", cmd);
		complete_cmd(instance, cmd);
	}

out:
	if (result == FAILED)
		dsprintk(NDEBUG_ABORT, instance, "abort: failed to abort %p\n", cmd);
	else {
		hostdata->busy[scmd_id(cmd)] &= ~(1 << cmd->device->lun);
		dsprintk(NDEBUG_ABORT, instance, "abort: successfully aborted %p\n", cmd);
	}

	queue_work(hostdata->work_q, &hostdata->main_task);
	spin_unlock_irqrestore(&hostdata->lock, flags);

	return result;
}


static void bus_reset_cleanup(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int i;
	struct NCR5380_cmd *ncmd;

	 
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(TARGET_COMMAND_REG, 0);
	NCR5380_write(SELECT_ENABLE_REG, 0);

	 

	if (hostdata->selecting) {
		hostdata->selecting->result = DID_RESET << 16;
		complete_cmd(instance, hostdata->selecting);
		hostdata->selecting = NULL;
	}

	list_for_each_entry(ncmd, &hostdata->disconnected, list) {
		struct scsi_cmnd *cmd = NCR5380_to_scmd(ncmd);

		set_host_byte(cmd, DID_RESET);
		complete_cmd(instance, cmd);
	}
	INIT_LIST_HEAD(&hostdata->disconnected);

	list_for_each_entry(ncmd, &hostdata->autosense, list) {
		struct scsi_cmnd *cmd = NCR5380_to_scmd(ncmd);

		scsi_done(cmd);
	}
	INIT_LIST_HEAD(&hostdata->autosense);

	if (hostdata->connected) {
		set_host_byte(hostdata->connected, DID_RESET);
		complete_cmd(instance, hostdata->connected);
		hostdata->connected = NULL;
	}

	for (i = 0; i < 8; ++i)
		hostdata->busy[i] = 0;
	hostdata->dma_len = 0;

	queue_work(hostdata->work_q, &hostdata->main_task);
}

 

static int NCR5380_host_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *instance = cmd->device->host;
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	unsigned long flags;
	struct NCR5380_cmd *ncmd;

	spin_lock_irqsave(&hostdata->lock, flags);

#if (NDEBUG & NDEBUG_ANY)
	shost_printk(KERN_INFO, instance, __func__);
#endif
	NCR5380_dprint(NDEBUG_ANY, instance);
	NCR5380_dprint_phase(NDEBUG_ANY, instance);

	list_for_each_entry(ncmd, &hostdata->unissued, list) {
		struct scsi_cmnd *scmd = NCR5380_to_scmd(ncmd);

		scmd->result = DID_RESET << 16;
		scsi_done(scmd);
	}
	INIT_LIST_HEAD(&hostdata->unissued);

	do_reset(instance);
	bus_reset_cleanup(instance);

	spin_unlock_irqrestore(&hostdata->lock, flags);

	return SUCCESS;
}
