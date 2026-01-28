
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (30)
#endif

void sci_uart_rxi_isr(void);
void sci_uart_txi_isr(void);
void sci_uart_tei_isr(void);
void sci_uart_eri_isr(void);
void rtc_alarm_periodic_isr(void);
void rtc_carry_isr(void);
void r_icu_isr(void);
void usbfs_interrupt_handler(void);
void usbfs_resume_handler(void);
void usbfs_d0fifo_handler(void);
void usbfs_d1fifo_handler(void);
void usbhs_interrupt_handler(void);
void usbhs_d0fifo_handler(void);
void usbhs_d1fifo_handler(void);
void iic_master_rxi_isr(void);
void iic_master_txi_isr(void);
void iic_master_tei_isr(void);
void iic_master_eri_isr(void);
void agt_int_isr(void);
void spi_rxi_isr(void);
void spi_txi_isr(void);
void spi_tei_isr(void);
void spi_eri_isr(void);


#define VECTOR_NUMBER_SCI9_RXI ((IRQn_Type)0)  
#define SCI9_RXI_IRQn          ((IRQn_Type)0)  
#define VECTOR_NUMBER_SCI9_TXI ((IRQn_Type)1)  
#define SCI9_TXI_IRQn          ((IRQn_Type)1)  
#define VECTOR_NUMBER_SCI9_TEI ((IRQn_Type)2)  
#define SCI9_TEI_IRQn          ((IRQn_Type)2)  
#define VECTOR_NUMBER_SCI9_ERI ((IRQn_Type)3)  
#define SCI9_ERI_IRQn          ((IRQn_Type)3)  
#define VECTOR_NUMBER_RTC_ALARM ((IRQn_Type)4)  
#define RTC_ALARM_IRQn          ((IRQn_Type)4)  
#define VECTOR_NUMBER_RTC_PERIOD ((IRQn_Type)5)  
#define RTC_PERIOD_IRQn          ((IRQn_Type)5)  
#define VECTOR_NUMBER_RTC_CARRY ((IRQn_Type)6)  
#define RTC_CARRY_IRQn          ((IRQn_Type)6)  
#define VECTOR_NUMBER_ICU_IRQ0 ((IRQn_Type)7)  
#define ICU_IRQ0_IRQn          ((IRQn_Type)7)  
#define VECTOR_NUMBER_USBFS_INT ((IRQn_Type)8)  
#define USBFS_INT_IRQn          ((IRQn_Type)8)  
#define VECTOR_NUMBER_USBFS_RESUME ((IRQn_Type)9)  
#define USBFS_RESUME_IRQn          ((IRQn_Type)9)  
#define VECTOR_NUMBER_USBFS_FIFO_0 ((IRQn_Type)10)  
#define USBFS_FIFO_0_IRQn          ((IRQn_Type)10)  
#define VECTOR_NUMBER_USBFS_FIFO_1 ((IRQn_Type)11)  
#define USBFS_FIFO_1_IRQn          ((IRQn_Type)11)  
#define VECTOR_NUMBER_USBHS_USB_INT_RESUME ((IRQn_Type)12)  
#define USBHS_USB_INT_RESUME_IRQn          ((IRQn_Type)12)  
#define VECTOR_NUMBER_USBHS_FIFO_0 ((IRQn_Type)13)  
#define USBHS_FIFO_0_IRQn          ((IRQn_Type)13)  
#define VECTOR_NUMBER_USBHS_FIFO_1 ((IRQn_Type)14)  
#define USBHS_FIFO_1_IRQn          ((IRQn_Type)14)  
#define VECTOR_NUMBER_IIC0_RXI ((IRQn_Type)15)  
#define IIC0_RXI_IRQn          ((IRQn_Type)15)  
#define VECTOR_NUMBER_IIC0_TXI ((IRQn_Type)16)  
#define IIC0_TXI_IRQn          ((IRQn_Type)16)  
#define VECTOR_NUMBER_IIC0_TEI ((IRQn_Type)17)  
#define IIC0_TEI_IRQn          ((IRQn_Type)17)  
#define VECTOR_NUMBER_IIC0_ERI ((IRQn_Type)18)  
#define IIC0_ERI_IRQn          ((IRQn_Type)18)  
#define VECTOR_NUMBER_AGT0_INT ((IRQn_Type)19)  
#define AGT0_INT_IRQn          ((IRQn_Type)19)  
#define VECTOR_NUMBER_AGT1_INT ((IRQn_Type)20)  
#define AGT1_INT_IRQn          ((IRQn_Type)20)  
#define VECTOR_NUMBER_SCI8_RXI ((IRQn_Type)21)  
#define SCI8_RXI_IRQn          ((IRQn_Type)21)  
#define VECTOR_NUMBER_SCI8_TXI ((IRQn_Type)22)  
#define SCI8_TXI_IRQn          ((IRQn_Type)22)  
#define VECTOR_NUMBER_SCI8_TEI ((IRQn_Type)23)  
#define SCI8_TEI_IRQn          ((IRQn_Type)23)  
#define VECTOR_NUMBER_SCI8_ERI ((IRQn_Type)24)  
#define SCI8_ERI_IRQn          ((IRQn_Type)24)  
#define VECTOR_NUMBER_SPI1_RXI ((IRQn_Type)25)  
#define SPI1_RXI_IRQn          ((IRQn_Type)25)  
#define VECTOR_NUMBER_SPI1_TXI ((IRQn_Type)26)  
#define SPI1_TXI_IRQn          ((IRQn_Type)26)  
#define VECTOR_NUMBER_SPI1_TEI ((IRQn_Type)27)  
#define SPI1_TEI_IRQn          ((IRQn_Type)27)  
#define VECTOR_NUMBER_SPI1_ERI ((IRQn_Type)28)  
#define SPI1_ERI_IRQn          ((IRQn_Type)28)  
#define VECTOR_NUMBER_ICU_IRQ2 ((IRQn_Type)29)  
#define ICU_IRQ2_IRQn          ((IRQn_Type)29)  
#ifdef __cplusplus
}
#endif
#endif 
