#ifndef _BRCM_MAC80211_IF_H_
#define _BRCM_MAC80211_IF_H_
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include "ucode_loader.h"
#include "led.h"
#define BRCMS_LEGACY_5G_RATE_OFFSET	4
#define BRCMS_SET_SHORTSLOT_OVERRIDE		146
struct brcms_timer {
	struct delayed_work dly_wrk;
	struct brcms_info *wl;
	void (*fn) (void *);	 
	void *arg;		 
	uint ms;
	bool periodic;
	bool set;		 
	struct brcms_timer *next;	 
#ifdef DEBUG
	char *name;		 
#endif
};
struct brcms_if {
	uint subunit;		 
	struct pci_dev *pci_dev;
};
#define MAX_FW_IMAGES		4
struct brcms_firmware {
	u32 fw_cnt;
	const struct firmware *fw_bin[MAX_FW_IMAGES];
	const struct firmware *fw_hdr[MAX_FW_IMAGES];
	u32 hdr_num_entries[MAX_FW_IMAGES];
};
struct brcms_info {
	struct brcms_pub *pub;		 
	struct brcms_c_info *wlc;	 
	u32 magic;
	int irq;
	spinlock_t lock;	 
	spinlock_t isr_lock;	 
	wait_queue_head_t tx_flush_wq;
	atomic_t callbacks;	 
	struct brcms_timer *timers;	 
	struct tasklet_struct tasklet;	 
	bool resched;		 
	struct brcms_firmware fw;
	struct wiphy *wiphy;
	struct brcms_ucode ucode;
	bool mute_tx;
	struct brcms_led radio_led;
	struct led_classdev led_dev;
};
void brcms_init(struct brcms_info *wl);
uint brcms_reset(struct brcms_info *wl);
void brcms_intrson(struct brcms_info *wl);
u32 brcms_intrsoff(struct brcms_info *wl);
void brcms_intrsrestore(struct brcms_info *wl, u32 macintmask);
int brcms_up(struct brcms_info *wl);
void brcms_down(struct brcms_info *wl);
void brcms_txflowcontrol(struct brcms_info *wl, struct brcms_if *wlif,
			 bool state, int prio);
bool brcms_rfkill_set_hw_state(struct brcms_info *wl);
struct brcms_timer *brcms_init_timer(struct brcms_info *wl,
				     void (*fn) (void *arg), void *arg,
				     const char *name);
void brcms_free_timer(struct brcms_timer *timer);
void brcms_add_timer(struct brcms_timer *timer, uint ms, int periodic);
bool brcms_del_timer(struct brcms_timer *timer);
void brcms_dpc(struct tasklet_struct *t);
void brcms_timer(struct brcms_timer *t);
void brcms_fatal_error(struct brcms_info *wl);
#endif				 
