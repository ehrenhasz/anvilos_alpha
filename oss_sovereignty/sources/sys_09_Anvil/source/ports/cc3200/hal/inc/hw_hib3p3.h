


































#ifndef __HW_HIB3P3_H__
#define __HW_HIB3P3_H__






#define HIB3P3_O_MEM_HIB_REQ    0x00000000
#define HIB3P3_O_MEM_HIB_RTC_TIMER_ENABLE \
                                0x00000004

#define HIB3P3_O_MEM_HIB_RTC_TIMER_RESET \
                                0x00000008

#define HIB3P3_O_MEM_HIB_RTC_TIMER_READ \
                                0x0000000C

#define HIB3P3_O_MEM_HIB_RTC_TIMER_LSW \
                                0x00000010

#define HIB3P3_O_MEM_HIB_RTC_TIMER_MSW \
                                0x00000014

#define HIB3P3_O_MEM_HIB_RTC_WAKE_EN \
                                0x00000018

#define HIB3P3_O_MEM_HIB_RTC_WAKE_LSW_CONF \
                                0x0000001C

#define HIB3P3_O_MEM_HIB_RTC_WAKE_MSW_CONF \
                                0x00000020

#define HIB3P3_O_MEM_INT_OSC_CONF \
                                0x0000002C

#define HIB3P3_O_MEM_XTAL_OSC_CONF \
                                0x00000034

#define HIB3P3_O_MEM_BGAP_PARAMETERS0 \
                                0x00000038

#define HIB3P3_O_MEM_BGAP_PARAMETERS1 \
                                0x0000003C

#define HIB3P3_O_MEM_HIB_DETECTION_STATUS \
                                0x00000040

#define HIB3P3_O_MEM_HIB_MISC_CONTROLS \
                                0x00000044

#define HIB3P3_O_MEM_HIB_CONFIG 0x00000050
#define HIB3P3_O_MEM_HIB_RTC_IRQ_ENABLE \
                                0x00000054

#define HIB3P3_O_MEM_HIB_RTC_IRQ_LSW_CONF \
                                0x00000058

#define HIB3P3_O_MEM_HIB_RTC_IRQ_MSW_CONF \
                                0x0000005C

#define HIB3P3_O_MEM_HIB_UART_CONF \
                                0x00000400

#define HIB3P3_O_MEM_GPIO_WAKE_EN \
                                0x00000404

#define HIB3P3_O_MEM_GPIO_WAKE_CONF \
                                0x00000408

#define HIB3P3_O_MEM_PAD_OEN_RET33_CONF \
                                0x0000040C

#define HIB3P3_O_MEM_UART_RTS_OEN_RET33_CONF \
                                0x00000410

#define HIB3P3_O_MEM_JTAG_CONF  0x00000414
#define HIB3P3_O_MEM_HIB_REG0   0x00000418
#define HIB3P3_O_MEM_HIB_REG1   0x0000041C
#define HIB3P3_O_MEM_HIB_REG2   0x00000420
#define HIB3P3_O_MEM_HIB_REG3   0x00000424
#define HIB3P3_O_MEM_HIB_SEQUENCER_CFG0 \
                                0x0000045C

#define HIB3P3_O_MEM_HIB_SEQUENCER_CFG1 \
                                0x00000460

#define HIB3P3_O_MEM_HIB_MISC_CONFIG \
                                0x00000464

#define HIB3P3_O_MEM_HIB_WAKE_STATUS \
                                0x00000468

#define HIB3P3_O_MEM_HIB_LPDS_GPIO_SEL \
                                0x0000046C

#define HIB3P3_O_MEM_HIB_SEQUENCER_CFG2 \
                                0x00000470

#define HIB3P3_O_HIBANA_SPARE_LOWV \
                                0x00000474

#define HIB3P3_O_HIB_TMUX_CTRL  0x00000478
#define HIB3P3_O_HIB_1P2_1P8_LDO_TRIM \
                                0x0000047C

#define HIB3P3_O_HIB_COMP_TRIM  0x00000480
#define HIB3P3_O_HIB_EN_TS      0x00000484
#define HIB3P3_O_HIB_1P8V_DET_EN \
                                0x00000488

#define HIB3P3_O_HIB_VBAT_MON_EN \
                                0x0000048C

#define HIB3P3_O_HIB_NHIB_ENABLE \
                                0x00000490

#define HIB3P3_O_HIB_UART_RTS_SW_ENABLE \
                                0x00000494










#define HIB3P3_MEM_HIB_REQ_reserved_M \
                                0xFFFFFE00

#define HIB3P3_MEM_HIB_REQ_reserved_S 9
#define HIB3P3_MEM_HIB_REQ_NU1_M \
                                0x000001FC

#define HIB3P3_MEM_HIB_REQ_NU1_S 2
#define HIB3P3_MEM_HIB_REQ_mem_hib_clk_disable \
                                0x00000002  
                                            
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_REQ_mem_hib_req \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_HIB_RTC_TIMER_ENABLE_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_MEM_HIB_RTC_TIMER_ENABLE_reserved_S 1
#define HIB3P3_MEM_HIB_RTC_TIMER_ENABLE_mem_hib_rtc_timer_enable \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_HIB_RTC_TIMER_RESET_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_MEM_HIB_RTC_TIMER_RESET_reserved_S 1
#define HIB3P3_MEM_HIB_RTC_TIMER_RESET_mem_hib_rtc_timer_reset \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_HIB_RTC_TIMER_READ_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_MEM_HIB_RTC_TIMER_READ_reserved_S 1
#define HIB3P3_MEM_HIB_RTC_TIMER_READ_mem_hib_rtc_timer_read \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_HIB_RTC_TIMER_LSW_hib_rtc_timer_lsw_M \
                                0xFFFFFFFF  
                                            

#define HIB3P3_MEM_HIB_RTC_TIMER_LSW_hib_rtc_timer_lsw_S 0






#define HIB3P3_MEM_HIB_RTC_TIMER_MSW_reserved_M \
                                0xFFFF0000

#define HIB3P3_MEM_HIB_RTC_TIMER_MSW_reserved_S 16
#define HIB3P3_MEM_HIB_RTC_TIMER_MSW_hib_rtc_timer_msw_M \
                                0x0000FFFF  
                                            

#define HIB3P3_MEM_HIB_RTC_TIMER_MSW_hib_rtc_timer_msw_S 0






#define HIB3P3_MEM_HIB_RTC_WAKE_EN_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_MEM_HIB_RTC_WAKE_EN_reserved_S 1
#define HIB3P3_MEM_HIB_RTC_WAKE_EN_mem_hib_rtc_wake_en \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_HIB_RTC_WAKE_LSW_CONF_mem_hib_rtc_wake_lsw_conf_M \
                                0xFFFFFFFF  
                                            

#define HIB3P3_MEM_HIB_RTC_WAKE_LSW_CONF_mem_hib_rtc_wake_lsw_conf_S 0






#define HIB3P3_MEM_HIB_RTC_WAKE_MSW_CONF_reserved_M \
                                0xFFFF0000

#define HIB3P3_MEM_HIB_RTC_WAKE_MSW_CONF_reserved_S 16
#define HIB3P3_MEM_HIB_RTC_WAKE_MSW_CONF_mem_hib_rtc_wake_msw_conf_M \
                                0x0000FFFF  
                                            

#define HIB3P3_MEM_HIB_RTC_WAKE_MSW_CONF_mem_hib_rtc_wake_msw_conf_S 0






#define HIB3P3_MEM_INT_OSC_CONF_reserved_M \
                                0xFFFF0000

#define HIB3P3_MEM_INT_OSC_CONF_reserved_S 16
#define HIB3P3_MEM_INT_OSC_CONF_cm_clk_good_32k_int \
                                0x00008000  
                                            
                                            

#define HIB3P3_MEM_INT_OSC_CONF_mem_cm_intosc_32k_spare_M \
                                0x00007E00

#define HIB3P3_MEM_INT_OSC_CONF_mem_cm_intosc_32k_spare_S 9
#define HIB3P3_MEM_INT_OSC_CONF_mem_cm_en_intosc_32k_override_ctrl \
                                0x00000100  
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_INT_OSC_CONF_NU1 \
                                0x00000080

#define HIB3P3_MEM_INT_OSC_CONF_mem_cm_intosc_32k_trim_M \
                                0x0000007E

#define HIB3P3_MEM_INT_OSC_CONF_mem_cm_intosc_32k_trim_S 1
#define HIB3P3_MEM_INT_OSC_CONF_mem_cm_en_intosc_32k \
                                0x00000001  
                                            
                                            







#define HIB3P3_MEM_XTAL_OSC_CONF_reserved_M \
                                0xFFF00000

#define HIB3P3_MEM_XTAL_OSC_CONF_reserved_S 20
#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_sli_32k_override_ctrl \
                                0x00080000  
                                            
                                            

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_xtal_32k_override_ctrl \
                                0x00040000  
                                            
                                            

#define HIB3P3_MEM_XTAL_OSC_CONF_cm_clk_good_xtal \
                                0x00020000  
                                            

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_xtal_trim_M \
                                0x0001F800

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_xtal_trim_S 11
#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_sli_32k \
                                0x00000400  
                                            
                                            

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_sli_32k_trim_M \
                                0x00000380

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_sli_32k_trim_S 7
#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_fref_32k_slicer_itrim_M \
                                0x00000070

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_fref_32k_slicer_itrim_S 4
#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_fref_32k_slicer \
                                0x00000008

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_input_sense_M \
                                0x00000006

#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_input_sense_S 1
#define HIB3P3_MEM_XTAL_OSC_CONF_mem_cm_en_xtal_32k \
                                0x00000001  
                                            
                                            







#define HIB3P3_MEM_BGAP_PARAMETERS0_reserved_M \
                                0xFFF80000

#define HIB3P3_MEM_BGAP_PARAMETERS0_reserved_S 19
#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_en_seq \
                                0x00040000

#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_vbok4bg_comp_trim_M \
                                0x0001C000

#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_vbok4bg_comp_trim_S 14
#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_bgap_en_vbat_ok_4bg \
                                0x00001000

#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_bgap_en_vbok4bg_comp \
                                0x00000800

#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_bgap_en_vbok4bg_comp_ref \
                                0x00000400

#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_bgap_spare_M \
                                0x000003FF

#define HIB3P3_MEM_BGAP_PARAMETERS0_mem_bgap_spare_S 0






#define HIB3P3_MEM_BGAP_PARAMETERS1_reserved_M \
                                0xE0000000

#define HIB3P3_MEM_BGAP_PARAMETERS1_reserved_S 29
#define HIB3P3_MEM_BGAP_PARAMETERS1_mem_bgap_act_iref_itrim_M \
                                0x1F000000

#define HIB3P3_MEM_BGAP_PARAMETERS1_mem_bgap_act_iref_itrim_S 24
#define HIB3P3_MEM_BGAP_PARAMETERS1_mem_bgap_en_act_iref \
                                0x00000008

#define HIB3P3_MEM_BGAP_PARAMETERS1_mem_bgap_en_v2i \
                                0x00000004

#define HIB3P3_MEM_BGAP_PARAMETERS1_mem_bgap_en_cap_sw \
                                0x00000002

#define HIB3P3_MEM_BGAP_PARAMETERS1_mem_bgap_en \
                                0x00000001







#define HIB3P3_MEM_HIB_DETECTION_STATUS_reserved_M \
                                0xFFFFFF80

#define HIB3P3_MEM_HIB_DETECTION_STATUS_reserved_S 7
#define HIB3P3_MEM_HIB_DETECTION_STATUS_hib_forced_ana_status \
                                0x00000040  

#define HIB3P3_MEM_HIB_DETECTION_STATUS_hib_forced_flash_status \
                                0x00000004  
                                            

#define HIB3P3_MEM_HIB_DETECTION_STATUS_hib_ext_clk_det_out_status \
                                0x00000002  

#define HIB3P3_MEM_HIB_DETECTION_STATUS_hib_xtal_det_out_status \
                                0x00000001  







#define HIB3P3_MEM_HIB_MISC_CONTROLS_reserved_M \
                                0xFFFFF800

#define HIB3P3_MEM_HIB_MISC_CONTROLS_reserved_S 11
#define HIB3P3_MEM_HIB_MISC_CONTROLS_mem_hib_en_pok_por_comp \
                                0x00000400

#define HIB3P3_MEM_HIB_MISC_CONTROLS_mem_hib_en_pok_por_comp_ref \
                                0x00000200

#define HIB3P3_MEM_HIB_MISC_CONTROLS_mem_hib_pok_por_comp_trim_M \
                                0x000001C0

#define HIB3P3_MEM_HIB_MISC_CONTROLS_mem_hib_pok_por_comp_trim_S 6
#define HIB3P3_MEM_HIB_MISC_CONTROLS_NU1 \
                                0x00000020

#define HIB3P3_MEM_HIB_MISC_CONTROLS_mem_hib_flash_det_en \
                                0x00000010

#define HIB3P3_MEM_HIB_MISC_CONTROLS_mem_hib_en_tmux \
                                0x00000001







#define HIB3P3_MEM_HIB_CONFIG_TOP_MUX_CTRL_SOP_SPIO_M \
                                0xFF000000

#define HIB3P3_MEM_HIB_CONFIG_TOP_MUX_CTRL_SOP_SPIO_S 24
#define HIB3P3_MEM_HIB_CONFIG_EN_ANA_DIG_SHARED3 \
                                0x00080000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_CONFIG_EN_ANA_DIG_SHARED2 \
                                0x00040000  
                                            
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_CONFIG_EN_ANA_DIG_SHARED1 \
                                0x00020000  
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_CONFIG_EN_ANA_DIG_SHARED0 \
                                0x00010000  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_CONFIG_mem_hib_xtal_enable \
                                0x00000100  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_HIB_RTC_IRQ_ENABLE_HIB_RTC_IRQ_ENABLE \
                                0x00000001  
                                            







#define HIB3P3_MEM_HIB_RTC_IRQ_LSW_CONF_HIB_RTC_IRQ_LSW_CONF_M \
                                0xFFFFFFFF  
                                            
                                            

#define HIB3P3_MEM_HIB_RTC_IRQ_LSW_CONF_HIB_RTC_IRQ_LSW_CONF_S 0






#define HIB3P3_MEM_HIB_RTC_IRQ_MSW_CONF_HIB_RTC_IRQ_MSW_CONF_M \
                                0x0000FFFF  
                                            
                                            

#define HIB3P3_MEM_HIB_RTC_IRQ_MSW_CONF_HIB_RTC_IRQ_MSW_CONF_S 0






#define HIB3P3_MEM_HIB_UART_CONF_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_MEM_HIB_UART_CONF_reserved_S 1
#define HIB3P3_MEM_HIB_UART_CONF_mem_hib_uart_wake_en \
                                0x00000001  
                                            
                                            
                                            
                                            
                                            
                                            







#define HIB3P3_MEM_GPIO_WAKE_EN_reserved_M \
                                0xFFFFFF00

#define HIB3P3_MEM_GPIO_WAKE_EN_reserved_S 8
#define HIB3P3_MEM_GPIO_WAKE_EN_mem_gpio_wake_en_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_GPIO_WAKE_EN_mem_gpio_wake_en_S 0






#define HIB3P3_MEM_GPIO_WAKE_CONF_reserved_M \
                                0xFFFF0000

#define HIB3P3_MEM_GPIO_WAKE_CONF_reserved_S 16
#define HIB3P3_MEM_GPIO_WAKE_CONF_mem_gpio_wake_conf_M \
                                0x0000FFFF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_GPIO_WAKE_CONF_mem_gpio_wake_conf_S 0






#define HIB3P3_MEM_PAD_OEN_RET33_CONF_mem_pad_oen_ret33_override_ctrl \
                                0x00000004  
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_PAD_OEN_RET33_CONF_PAD_OEN33_CONF \
                                0x00000002

#define HIB3P3_MEM_PAD_OEN_RET33_CONF_PAD_RET33_CONF \
                                0x00000001







#define HIB3P3_MEM_UART_RTS_OEN_RET33_CONF_mem_uart_nrts_oen_ret33_override_ctrl \
                                0x00000004  
                                            
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_UART_RTS_OEN_RET33_CONF_PAD_UART_RTS_OEN33_CONF \
                                0x00000002

#define HIB3P3_MEM_UART_RTS_OEN_RET33_CONF_PAD_UART_RTS_RET33_CONF \
                                0x00000001







#define HIB3P3_MEM_JTAG_CONF_mem_jtag1_oen_ret33_override_ctrl \
                                0x00000200

#define HIB3P3_MEM_JTAG_CONF_mem_jtag0_oen_ret33_override_ctrl \
                                0x00000100

#define HIB3P3_MEM_JTAG_CONF_PAD_JTAG1_RTS_OEN33_CONF \
                                0x00000008

#define HIB3P3_MEM_JTAG_CONF_PAD_JTAG1_RTS_RET33_CONF \
                                0x00000004

#define HIB3P3_MEM_JTAG_CONF_PAD_JTAG0_RTS_OEN33_CONF \
                                0x00000002

#define HIB3P3_MEM_JTAG_CONF_PAD_JTAG0_RTS_RET33_CONF \
                                0x00000001







#define HIB3P3_MEM_HIB_REG0_mem_hib_reg0_M \
                                0xFFFFFFFF

#define HIB3P3_MEM_HIB_REG0_mem_hib_reg0_S 0






#define HIB3P3_MEM_HIB_REG1_mem_hib_reg1_M \
                                0xFFFFFFFF

#define HIB3P3_MEM_HIB_REG1_mem_hib_reg1_S 0






#define HIB3P3_MEM_HIB_REG2_mem_hib_reg2_M \
                                0xFFFFFFFF

#define HIB3P3_MEM_HIB_REG2_mem_hib_reg2_S 0






#define HIB3P3_MEM_HIB_REG3_mem_hib_reg3_M \
                                0xFFFFFFFF

#define HIB3P3_MEM_HIB_REG3_mem_hib_reg3_S 0






#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev0_to_ev1_time_M \
                                0xFFFF0000  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev0_to_ev1_time_S 16
#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_NU1 \
                                0x00008000

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev3_to_ev4_time_M \
                                0x00006000  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev3_to_ev4_time_S 13
#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev2_to_ev3_time_M \
                                0x00001800  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev2_to_ev3_time_S 11
#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev1_to_ev2_time_M \
                                0x00000600  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bdc_ev1_to_ev2_time_S 9
#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_en_crude_ref_comp \
                                0x00000100

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_en_vbok4bg_ref_override_ctrl \
                                0x00000080  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_en_vbok4bg_comp_override_ctrl \
                                0x00000040  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_en_v2i_override_ctrl \
                                0x00000020  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_por_comp_ref_override_ctrl \
                                0x00000010  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_en_por_comp_override_ctrl \
                                0x00000008  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_cap_sw_override_ctrl \
                                0x00000004  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_bg_override_ctrl \
                                0x00000002  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG0_mem_act_iref_override_ctrl \
                                0x00000001







#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_reserved_M \
                                0xFFFF0000

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_reserved_S 16
#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_ev5_to_ev6_time_M \
                                0x0000C000  
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_ev5_to_ev6_time_S 14
#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_to_active_ev1_to_ev2_time_M \
                                0x00003000  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_to_active_ev1_to_ev2_time_S 12
#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_to_active_ev0_to_ev1_time_M \
                                0x00000C00  

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_to_active_ev0_to_ev1_time_S 10
#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_to_active_ev0_to_active_M \
                                0x00000300  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_bdc_to_active_ev0_to_active_S 8
#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_active_to_bdc_ev1_to_bdc_ev0_time_M \
                                0x000000C0  
                                            
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_mem_active_to_bdc_ev1_to_bdc_ev0_time_S 6
#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_NU1_M \
                                0x0000003F

#define HIB3P3_MEM_HIB_SEQUENCER_CFG1_NU1_S 0






#define HIB3P3_MEM_HIB_MISC_CONFIG_mem_en_pll_untrim_current \
                                0x00000001







#define HIB3P3_MEM_HIB_WAKE_STATUS_hib_wake_src_M \
                                0x0000001E  
                                            

#define HIB3P3_MEM_HIB_WAKE_STATUS_hib_wake_src_S 1
#define HIB3P3_MEM_HIB_WAKE_STATUS_hib_wake_status \
                                0x00000001  
                                            







#define HIB3P3_MEM_HIB_LPDS_GPIO_SEL_HIB_LPDS_GPIO_SEL_M \
                                0x00000007

#define HIB3P3_MEM_HIB_LPDS_GPIO_SEL_HIB_LPDS_GPIO_SEL_S 0






#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_reserved_M \
                                0xFFFFF800

#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_reserved_S 11
#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_active_to_bdc_ev0_to_active_to_bdc_ev1_time_M \
                                0x00000600  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_active_to_bdc_ev0_to_active_to_bdc_ev1_time_S 9
#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_bdc_ev4_to_ev5_time_M \
                                0x000001C0  
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_bdc_ev4_to_ev5_time_S 6
#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_bdc_ev6_to_ev7_time_M \
                                0x00000030  
                                            
                                            
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_bdc_ev6_to_ev7_time_S 4
#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_bdc_to_active_ev1_to_ev2_time_M \
                                0x0000000C  
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_bdc_to_active_ev1_to_ev2_time_S 2
#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_hib_to_active_ev2_to_ev3_time_M \
                                0x00000003  
                                            

#define HIB3P3_MEM_HIB_SEQUENCER_CFG2_mem_hib_to_active_ev2_to_ev3_time_S 0






#define HIB3P3_HIBANA_SPARE_LOWV_mem_hibana_spare1_M \
                                0xFFC00000

#define HIB3P3_HIBANA_SPARE_LOWV_mem_hibana_spare1_S 22
#define HIB3P3_HIBANA_SPARE_LOWV_mem_hibana_spare0_M \
                                0x0001FFFF

#define HIB3P3_HIBANA_SPARE_LOWV_mem_hibana_spare0_S 0






#define HIB3P3_HIB_TMUX_CTRL_reserved_M \
                                0xFFFFFC00

#define HIB3P3_HIB_TMUX_CTRL_reserved_S 10
#define HIB3P3_HIB_TMUX_CTRL_mem_hd_tmux_cntrl_M \
                                0x000003FF

#define HIB3P3_HIB_TMUX_CTRL_mem_hd_tmux_cntrl_S 0






#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_reserved_M \
                                0xFFFFF000

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_reserved_S 12
#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p2_ldo_en_override_ctrl \
                                0x00000800

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p8_ldo_en_override_ctrl \
                                0x00000400

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p2_ldo_en_override \
                                0x00000200

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p8_ldo_en_override \
                                0x00000100

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p2_ldo_vtrim_M \
                                0x000000F0

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p2_ldo_vtrim_S 4
#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p8_ldo_vtrim_M \
                                0x0000000F

#define HIB3P3_HIB_1P2_1P8_LDO_TRIM_mem_hd_1p8_ldo_vtrim_S 0






#define HIB3P3_HIB_COMP_TRIM_reserved_M \
                                0xFFFFFFF8

#define HIB3P3_HIB_COMP_TRIM_reserved_S 3
#define HIB3P3_HIB_COMP_TRIM_mem_hd_comp_trim_M \
                                0x00000007

#define HIB3P3_HIB_COMP_TRIM_mem_hd_comp_trim_S 0






#define HIB3P3_HIB_EN_TS_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_HIB_EN_TS_reserved_S 1
#define HIB3P3_HIB_EN_TS_mem_hd_en_ts \
                                0x00000001







#define HIB3P3_HIB_1P8V_DET_EN_reserved_M \
                                0xFFFFFFFE

#define HIB3P3_HIB_1P8V_DET_EN_reserved_S 1
#define HIB3P3_HIB_1P8V_DET_EN_mem_hib_1p8v_det_en \
                                0x00000001







#define HIB3P3_HIB_VBAT_MON_EN_reserved_M \
                                0xFFFFFFFC

#define HIB3P3_HIB_VBAT_MON_EN_reserved_S 2
#define HIB3P3_HIB_VBAT_MON_EN_mem_hib_vbat_mon_del_en \
                                0x00000002

#define HIB3P3_HIB_VBAT_MON_EN_mem_hib_vbat_mon_en \
                                0x00000001







#define HIB3P3_HIB_NHIB_ENABLE_mem_hib_nhib_enable \
                                0x00000001







#define HIB3P3_HIB_UART_RTS_SW_ENABLE_mem_hib_uart_rts_sw_enable \
                                0x00000001




#endif 
