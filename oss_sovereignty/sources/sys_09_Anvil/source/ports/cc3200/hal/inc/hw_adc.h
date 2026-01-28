


































#ifndef __HW_ADC_H__
#define __HW_ADC_H__






#define ADC_O_ADC_CTRL          0x00000000  
#define ADC_O_adc_ch0_gain      0x00000004  
#define ADC_O_adc_ch1_gain      0x00000008  
#define ADC_O_adc_ch2_gain      0x0000000C  
#define ADC_O_adc_ch3_gain      0x00000010  
#define ADC_O_adc_ch4_gain      0x00000014  
#define ADC_O_adc_ch5_gain      0x00000018  
#define ADC_O_adc_ch6_gain      0x0000001C  
#define ADC_O_adc_ch7_gain      0x00000020  
#define ADC_O_adc_ch0_irq_en    0x00000024  
                                            
#define ADC_O_adc_ch1_irq_en    0x00000028  
                                            
#define ADC_O_adc_ch2_irq_en    0x0000002C  
                                            
#define ADC_O_adc_ch3_irq_en    0x00000030  
                                            
#define ADC_O_adc_ch4_irq_en    0x00000034  
                                            
#define ADC_O_adc_ch5_irq_en    0x00000038  
                                            
#define ADC_O_adc_ch6_irq_en    0x0000003C  
                                            
#define ADC_O_adc_ch7_irq_en    0x00000040  
                                            
#define ADC_O_adc_ch0_irq_status \
                                0x00000044  
                                            

#define ADC_O_adc_ch1_irq_status \
                                0x00000048  
                                            

#define ADC_O_adc_ch2_irq_status \
                                0x0000004C

#define ADC_O_adc_ch3_irq_status \
                                0x00000050  
                                            

#define ADC_O_adc_ch4_irq_status \
                                0x00000054  
                                            

#define ADC_O_adc_ch5_irq_status \
                                0x00000058

#define ADC_O_adc_ch6_irq_status \
                                0x0000005C  
                                            

#define ADC_O_adc_ch7_irq_status \
                                0x00000060  
                                            

#define ADC_O_adc_dma_mode_en   0x00000064  
#define ADC_O_adc_timer_configuration \
                                0x00000068  

#define ADC_O_adc_timer_current_count \
                                0x00000070  

#define ADC_O_channel0FIFODATA  0x00000074  
#define ADC_O_channel1FIFODATA  0x00000078  
#define ADC_O_channel2FIFODATA  0x0000007C  
#define ADC_O_channel3FIFODATA  0x00000080  
#define ADC_O_channel4FIFODATA  0x00000084  
#define ADC_O_channel5FIFODATA  0x00000088  
#define ADC_O_channel6FIFODATA  0x0000008C  
#define ADC_O_channel7FIFODATA  0x00000090  
#define ADC_O_adc_ch0_fifo_lvl  0x00000094  
#define ADC_O_adc_ch1_fifo_lvl  0x00000098  
                                            
#define ADC_O_adc_ch2_fifo_lvl  0x0000009C
#define ADC_O_adc_ch3_fifo_lvl  0x000000A0  
                                            
#define ADC_O_adc_ch4_fifo_lvl  0x000000A4  
                                            
#define ADC_O_adc_ch5_fifo_lvl  0x000000A8
#define ADC_O_adc_ch6_fifo_lvl  0x000000AC  
                                            
#define ADC_O_adc_ch7_fifo_lvl  0x000000B0  
                                            

#define ADC_O_ADC_CH_ENABLE     0x000000B8






#define ADC_ADC_CTRL_adc_cap_scale \
                                0x00000020  

#define ADC_ADC_CTRL_adc_buf_bypass \
                                0x00000010  
                                            
                                            

#define ADC_ADC_CTRL_adc_buf_en 0x00000008  
                                            
#define ADC_ADC_CTRL_adc_core_en \
                                0x00000004  
                                            
                                            

#define ADC_ADC_CTRL_adc_soft_reset \
                                0x00000002  
                                            

#define ADC_ADC_CTRL_adc_en     0x00000001  
                                            






#define ADC_adc_ch0_gain_adc_channel0_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch0_gain_adc_channel0_gain_S 0






#define ADC_adc_ch1_gain_adc_channel1_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch1_gain_adc_channel1_gain_S 0






#define ADC_adc_ch2_gain_adc_channel2_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch2_gain_adc_channel2_gain_S 0






#define ADC_adc_ch3_gain_adc_channel3_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch3_gain_adc_channel3_gain_S 0






#define ADC_adc_ch4_gain_adc_channel4_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch4_gain_adc_channel4_gain_S 0






#define ADC_adc_ch5_gain_adc_channel5_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch5_gain_adc_channel5_gain_S 0






#define ADC_adc_ch6_gain_adc_channel6_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch6_gain_adc_channel6_gain_S 0






#define ADC_adc_ch7_gain_adc_channel7_gain_M \
                                0x00000003  
                                            
                                            

#define ADC_adc_ch7_gain_adc_channel7_gain_S 0






#define ADC_adc_ch0_irq_en_adc_channel0_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch0_irq_en_adc_channel0_irq_en_S 0






#define ADC_adc_ch1_irq_en_adc_channel1_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch1_irq_en_adc_channel1_irq_en_S 0






#define ADC_adc_ch2_irq_en_adc_channel2_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch2_irq_en_adc_channel2_irq_en_S 0






#define ADC_adc_ch3_irq_en_adc_channel3_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch3_irq_en_adc_channel3_irq_en_S 0






#define ADC_adc_ch4_irq_en_adc_channel4_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch4_irq_en_adc_channel4_irq_en_S 0






#define ADC_adc_ch5_irq_en_adc_channel5_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch5_irq_en_adc_channel5_irq_en_S 0






#define ADC_adc_ch6_irq_en_adc_channel6_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch6_irq_en_adc_channel6_irq_en_S 0






#define ADC_adc_ch7_irq_en_adc_channel7_irq_en_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch7_irq_en_adc_channel7_irq_en_S 0






#define ADC_adc_ch0_irq_status_adc_channel0_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch0_irq_status_adc_channel0_irq_status_S 0






#define ADC_adc_ch1_irq_status_adc_channel1_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch1_irq_status_adc_channel1_irq_status_S 0






#define ADC_adc_ch2_irq_status_adc_channel2_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch2_irq_status_adc_channel2_irq_status_S 0






#define ADC_adc_ch3_irq_status_adc_channel3_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch3_irq_status_adc_channel3_irq_status_S 0






#define ADC_adc_ch4_irq_status_adc_channel4_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch4_irq_status_adc_channel4_irq_status_S 0






#define ADC_adc_ch5_irq_status_adc_channel5_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch5_irq_status_adc_channel5_irq_status_S 0






#define ADC_adc_ch6_irq_status_adc_channel6_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch6_irq_status_adc_channel6_irq_status_S 0






#define ADC_adc_ch7_irq_status_adc_channel7_irq_status_M \
                                0x0000000F  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_ch7_irq_status_adc_channel7_irq_status_S 0






#define ADC_adc_dma_mode_en_DMA_MODEenable_M \
                                0x000000FF  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            

#define ADC_adc_dma_mode_en_DMA_MODEenable_S 0






#define ADC_adc_timer_configuration_timeren \
                                0x02000000  

#define ADC_adc_timer_configuration_timerreset \
                                0x01000000  

#define ADC_adc_timer_configuration_timercount_M \
                                0x00FFFFFF  
                                            
                                            

#define ADC_adc_timer_configuration_timercount_S 0






#define ADC_adc_timer_current_count_timercurrentcount_M \
                                0x0001FFFF  

#define ADC_adc_timer_current_count_timercurrentcount_S 0






#define ADC_channel0FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel0FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel1FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel1FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel2FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel2FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel3FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel3FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel4FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel4FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel5FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel5FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel6FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel6FIFODATA_FIFO_RD_DATA_S 0






#define ADC_channel7FIFODATA_FIFO_RD_DATA_M \
                                0xFFFFFFFF  
                                            
                                            
                                            
                                            
                                            

#define ADC_channel7FIFODATA_FIFO_RD_DATA_S 0






#define ADC_adc_ch0_fifo_lvl_adc_channel0_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch0_fifo_lvl_adc_channel0_fifo_lvl_S 0






#define ADC_adc_ch1_fifo_lvl_adc_channel1_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch1_fifo_lvl_adc_channel1_fifo_lvl_S 0






#define ADC_adc_ch2_fifo_lvl_adc_channel2_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch2_fifo_lvl_adc_channel2_fifo_lvl_S 0






#define ADC_adc_ch3_fifo_lvl_adc_channel3_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch3_fifo_lvl_adc_channel3_fifo_lvl_S 0






#define ADC_adc_ch4_fifo_lvl_adc_channel4_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch4_fifo_lvl_adc_channel4_fifo_lvl_S 0






#define ADC_adc_ch5_fifo_lvl_adc_channel5_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch5_fifo_lvl_adc_channel5_fifo_lvl_S 0






#define ADC_adc_ch6_fifo_lvl_adc_channel6_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch6_fifo_lvl_adc_channel6_fifo_lvl_S 0






#define ADC_adc_ch7_fifo_lvl_adc_channel7_fifo_lvl_M \
                                0x00000007  
                                            
                                            
                                            

#define ADC_adc_ch7_fifo_lvl_adc_channel7_fifo_lvl_S 0



#endif 
