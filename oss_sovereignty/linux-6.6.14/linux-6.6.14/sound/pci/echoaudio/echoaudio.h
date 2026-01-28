#ifndef _ECHOAUDIO_H_
#define _ECHOAUDIO_H_
#include "echoaudio_dsp.h"
#define VENDOR_ID		0x1057
#define DEVICE_ID_56301		0x1801
#define DEVICE_ID_56361		0x3410
#define SUBVENDOR_ID		0xECC0
#define DARLA20			0x0010
#define GINA20			0x0020
#define LAYLA20			0x0030
#define DARLA24			0x0040
#define GINA24			0x0050
#define LAYLA24			0x0060
#define MONA			0x0070
#define MIA			0x0080
#define INDIGO			0x0090
#define INDIGO_IO		0x00a0
#define INDIGO_DJ		0x00b0
#define DC8			0x00c0
#define INDIGO_IOX		0x00d0
#define INDIGO_DJX		0x00e0
#define ECHO3G			0x0100
#define ECHO_MAXAUDIOINPUTS	32	 
#define ECHO_MAXAUDIOOUTPUTS	32	 
#define ECHO_MAXAUDIOPIPES	32	 
#define E3G_MAX_OUTPUTS		16
#define ECHO_MAXMIDIJACKS	1	 
#define ECHO_MIDI_QUEUE_SZ 	512	 
#define ECHO_MTC_QUEUE_SZ	32	 
#define MIDI_ACTIVITY_TIMEOUT_USEC	200000
#define ECHO_CLOCK_INTERNAL		0
#define ECHO_CLOCK_WORD			1
#define ECHO_CLOCK_SUPER		2
#define ECHO_CLOCK_SPDIF		3
#define ECHO_CLOCK_ADAT			4
#define ECHO_CLOCK_ESYNC		5
#define ECHO_CLOCK_ESYNC96		6
#define ECHO_CLOCK_MTC			7
#define ECHO_CLOCK_NUMBER		8
#define ECHO_CLOCKS			0xffff
#define ECHO_CLOCK_BIT_INTERNAL		(1 << ECHO_CLOCK_INTERNAL)
#define ECHO_CLOCK_BIT_WORD		(1 << ECHO_CLOCK_WORD)
#define ECHO_CLOCK_BIT_SUPER		(1 << ECHO_CLOCK_SUPER)
#define ECHO_CLOCK_BIT_SPDIF		(1 << ECHO_CLOCK_SPDIF)
#define ECHO_CLOCK_BIT_ADAT		(1 << ECHO_CLOCK_ADAT)
#define ECHO_CLOCK_BIT_ESYNC		(1 << ECHO_CLOCK_ESYNC)
#define ECHO_CLOCK_BIT_ESYNC96		(1 << ECHO_CLOCK_ESYNC96)
#define ECHO_CLOCK_BIT_MTC		(1<<ECHO_CLOCK_MTC)
#define DIGITAL_MODE_NONE			0xFF
#define DIGITAL_MODE_SPDIF_RCA			0
#define DIGITAL_MODE_SPDIF_OPTICAL		1
#define DIGITAL_MODE_ADAT			2
#define DIGITAL_MODE_SPDIF_CDROM		3
#define DIGITAL_MODES				4
#define ECHOCAPS_HAS_DIGITAL_MODE_SPDIF_RCA	(1 << DIGITAL_MODE_SPDIF_RCA)
#define ECHOCAPS_HAS_DIGITAL_MODE_SPDIF_OPTICAL	(1 << DIGITAL_MODE_SPDIF_OPTICAL)
#define ECHOCAPS_HAS_DIGITAL_MODE_ADAT		(1 << DIGITAL_MODE_ADAT)
#define ECHOCAPS_HAS_DIGITAL_MODE_SPDIF_CDROM	(1 << DIGITAL_MODE_SPDIF_CDROM)
#define EXT_3GBOX_NC			0x01	 
#define EXT_3GBOX_NOT_SET		0x02	 
#define ECHOGAIN_MUTED		(-128)	 
#define ECHOGAIN_MINOUT		(-128)	 
#define ECHOGAIN_MAXOUT		(6)	 
#define ECHOGAIN_MININP		(-50)	 
#define ECHOGAIN_MAXINP		(50)	 
#define PIPE_STATE_STOPPED	0	 
#define PIPE_STATE_PAUSED	1	 
#define PIPE_STATE_STARTED	2	 
#define PIPE_STATE_PENDING	3	 
struct audiopipe {
	volatile __le32 *dma_counter;	 
	u32 last_period;                 
	u32 last_counter;		 
	u32 position;			 
	short index;			 
	short interleave;
	struct snd_dma_buffer sgpage;	 
	struct snd_pcm_hardware hw;
	struct snd_pcm_hw_constraint_list constr;
	short sglist_head;
	char state;			 
};
struct audioformat {
	u8 interleave;			 
	u8 bits_per_sample;		 
	char mono_to_stereo;		 
	char data_are_bigendian;	 
};
struct echoaudio {
	spinlock_t lock;
	struct snd_pcm_substream *substream[DSP_MAXPIPES];
	struct mutex mode_mutex;
	u16 num_digital_modes, digital_mode_list[6];
	u16 num_clock_sources, clock_source_list[10];
	unsigned int opencount;   
	struct snd_kcontrol *clock_src_ctl;
	struct snd_pcm *analog_pcm, *digital_pcm;
	struct snd_card *card;
	const char *card_name;
	struct pci_dev *pci;
	unsigned long dsp_registers_phys;
	struct resource *iores;
	struct snd_dma_buffer *commpage_dma_buf;
	int irq;
#ifdef ECHOCARD_HAS_MIDI
	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *midi_in, *midi_out;
#endif
	struct timer_list timer;
	char tinuse;				 
	char midi_full;				 
	char can_set_rate;                       
	char rate_set;                           
	struct comm_page *comm_page;	 
	u32 pipe_alloc_mask;		 
	u32 pipe_cyclic_mask;		 
	u32 sample_rate;		 
	u8 digital_mode;		 
	u8 spdif_status;		 
	u8 clock_state;			 
	u8 input_clock;			 
	u8 output_clock;		 
	char meters_enabled;		 
	char asic_loaded;		 
	char bad_board;			 
	char professional_spdif;	 
	char non_audio_spdif;		 
	char digital_in_automute;	 
	char has_phantom_power;
	char hasnt_input_nominal_level;	 
	char phantom_power;		 
	char has_midi;
	char midi_input_enabled;
#ifdef ECHOCARD_ECHO3G
	char px_digital_out, px_analog_in, px_digital_in, px_num;
	char bx_digital_out, bx_analog_in, bx_digital_in, bx_num;
#endif
	char nominal_level[ECHO_MAXAUDIOPIPES];	 
	s8 input_gain[ECHO_MAXAUDIOINPUTS];	 
	s8 output_gain[ECHO_MAXAUDIOOUTPUTS];	 
	s8 monitor_gain[ECHO_MAXAUDIOOUTPUTS][ECHO_MAXAUDIOINPUTS];
	s8 vmixer_gain[ECHO_MAXAUDIOOUTPUTS][ECHO_MAXAUDIOOUTPUTS];
	u16 digital_modes;		 
	u16 input_clock_types;		 
	u16 output_clock_types;		 
	u16 device_id, subdevice_id;
	u16 *dsp_code;			 
	short dsp_code_to_load;		 
	short asic_code;		 
	u32 comm_page_phys;			 
	u32 __iomem *dsp_registers;		 
	u32 active_mask;			 
#ifdef CONFIG_PM_SLEEP
	const struct firmware *fw_cache[8];	 
#endif
#ifdef ECHOCARD_HAS_MIDI
	u16 mtc_state;				 
	u8 midi_buffer[MIDI_IN_BUFFER_SIZE];
#endif
};
static int init_dsp_comm_page(struct echoaudio *chip);
static int init_line_levels(struct echoaudio *chip);
static int free_pipes(struct echoaudio *chip, struct audiopipe *pipe);
static int load_firmware(struct echoaudio *chip);
static int wait_handshake(struct echoaudio *chip);
static int send_vector(struct echoaudio *chip, u32 command);
static int get_firmware(const struct firmware **fw_entry,
			struct echoaudio *chip, const short fw_index);
static void free_firmware(const struct firmware *fw_entry,
			  struct echoaudio *chip);
#ifdef ECHOCARD_HAS_MIDI
static int enable_midi_input(struct echoaudio *chip, char enable);
static void snd_echo_midi_output_trigger(
			struct snd_rawmidi_substream *substream, int up);
static int midi_service_irq(struct echoaudio *chip);
static int snd_echo_midi_create(struct snd_card *card,
				struct echoaudio *chip);
#endif
static inline void clear_handshake(struct echoaudio *chip)
{
	chip->comm_page->handshake = 0;
}
static inline u32 get_dsp_register(struct echoaudio *chip, u32 index)
{
	return readl(&chip->dsp_registers[index]);
}
static inline void set_dsp_register(struct echoaudio *chip, u32 index,
				    u32 value)
{
	writel(value, &chip->dsp_registers[index]);
}
static inline int px_digital_out(const struct echoaudio *chip)
{
	return PX_DIGITAL_OUT;
}
static inline int px_analog_in(const struct echoaudio *chip)
{
	return PX_ANALOG_IN;
}
static inline int px_digital_in(const struct echoaudio *chip)
{
	return PX_DIGITAL_IN;
}
static inline int px_num(const struct echoaudio *chip)
{
	return PX_NUM;
}
static inline int bx_digital_out(const struct echoaudio *chip)
{
	return BX_DIGITAL_OUT;
}
static inline int bx_analog_in(const struct echoaudio *chip)
{
	return BX_ANALOG_IN;
}
static inline int bx_digital_in(const struct echoaudio *chip)
{
	return BX_DIGITAL_IN;
}
static inline int bx_num(const struct echoaudio *chip)
{
	return BX_NUM;
}
static inline int num_pipes_out(const struct echoaudio *chip)
{
	return px_analog_in(chip);
}
static inline int num_pipes_in(const struct echoaudio *chip)
{
	return px_num(chip) - px_analog_in(chip);
}
static inline int num_busses_out(const struct echoaudio *chip)
{
	return bx_analog_in(chip);
}
static inline int num_busses_in(const struct echoaudio *chip)
{
	return bx_num(chip) - bx_analog_in(chip);
}
static inline int num_analog_busses_out(const struct echoaudio *chip)
{
	return bx_digital_out(chip);
}
static inline int num_analog_busses_in(const struct echoaudio *chip)
{
	return bx_digital_in(chip) - bx_analog_in(chip);
}
static inline int num_digital_busses_out(const struct echoaudio *chip)
{
	return num_busses_out(chip) - num_analog_busses_out(chip);
}
static inline int num_digital_busses_in(const struct echoaudio *chip)
{
	return num_busses_in(chip) - num_analog_busses_in(chip);
}
static inline int monitor_index(const struct echoaudio *chip, int out, int in)
{
	return out * num_busses_in(chip) + in;
}
#endif  
