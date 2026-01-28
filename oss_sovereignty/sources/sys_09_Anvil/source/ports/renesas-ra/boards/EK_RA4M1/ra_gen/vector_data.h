
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (21)
#endif

void sci_uart_rxi_isr(void);
void sci_uart_txi_isr(void);
void sci_uart_tei_isr(void);
void sci_uart_eri_isr(void);
void rtc_alarm_periodic_isr(void);
void rtc_carry_isr(void);
void agt_int_isr(void);
void r_icu_isr(void);
void spi_rxi_isr(void);
void spi_txi_isr(void);
void spi_tei_isr(void);
void spi_eri_isr(void);


#define VECTOR_NUMBER_SCI0_RXI ((IRQn_Type)0)  
#define SCI0_RXI_IRQn          ((IRQn_Type)0)  
#define VECTOR_NUMBER_SCI0_TXI ((IRQn_Type)1)  
#define SCI0_TXI_IRQn          ((IRQn_Type)1)  
#define VECTOR_NUMBER_SCI0_TEI ((IRQn_Type)2)  
#define SCI0_TEI_IRQn          ((IRQn_Type)2)  
#define VECTOR_NUMBER_SCI0_ERI ((IRQn_Type)3)  
#define SCI0_ERI_IRQn          ((IRQn_Type)3)  
#define VECTOR_NUMBER_SCI1_RXI ((IRQn_Type)4)  
#define SCI1_RXI_IRQn          ((IRQn_Type)4)  
#define VECTOR_NUMBER_SCI1_TXI ((IRQn_Type)5)  
#define SCI1_TXI_IRQn          ((IRQn_Type)5)  
#define VECTOR_NUMBER_SCI1_TEI ((IRQn_Type)6)  
#define SCI1_TEI_IRQn          ((IRQn_Type)6)  
#define VECTOR_NUMBER_SCI1_ERI ((IRQn_Type)7)  
#define SCI1_ERI_IRQn          ((IRQn_Type)7)  
#define VECTOR_NUMBER_SCI2_RXI ((IRQn_Type)8)  
#define SCI2_RXI_IRQn          ((IRQn_Type)8)  
#define VECTOR_NUMBER_SCI2_TXI ((IRQn_Type)9)  
#define SCI2_TXI_IRQn          ((IRQn_Type)9)  
#define VECTOR_NUMBER_SCI2_TEI ((IRQn_Type)10)  
#define SCI2_TEI_IRQn          ((IRQn_Type)10)  
#define VECTOR_NUMBER_SCI2_ERI ((IRQn_Type)11)  
#define SCI2_ERI_IRQn          ((IRQn_Type)11)  
#define VECTOR_NUMBER_RTC_ALARM ((IRQn_Type)12)  
#define RTC_ALARM_IRQn          ((IRQn_Type)12)  
#define VECTOR_NUMBER_RTC_PERIOD ((IRQn_Type)13)  
#define RTC_PERIOD_IRQn          ((IRQn_Type)13)  
#define VECTOR_NUMBER_RTC_CARRY ((IRQn_Type)14)  
#define RTC_CARRY_IRQn          ((IRQn_Type)14)  
#define VECTOR_NUMBER_AGT0_INT ((IRQn_Type)15)  
#define AGT0_INT_IRQn          ((IRQn_Type)15)  
#define VECTOR_NUMBER_ICU_IRQ0 ((IRQn_Type)16)  
#define ICU_IRQ0_IRQn          ((IRQn_Type)16)  
#define VECTOR_NUMBER_SPI0_RXI ((IRQn_Type)17)  
#define SPI0_RXI_IRQn          ((IRQn_Type)17)  
#define VECTOR_NUMBER_SPI0_TXI ((IRQn_Type)18)  
#define SPI0_TXI_IRQn          ((IRQn_Type)18)  
#define VECTOR_NUMBER_SPI0_TEI ((IRQn_Type)19)  
#define SPI0_TEI_IRQn          ((IRQn_Type)19)  
#define VECTOR_NUMBER_SPI0_ERI ((IRQn_Type)20)  
#define SPI0_ERI_IRQn          ((IRQn_Type)20)  
#ifdef __cplusplus
}
#endif
#endif 
