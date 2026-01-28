
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (44)
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
void iic_master_rxi_isr(void);
void iic_master_txi_isr(void);
void iic_master_tei_isr(void);
void iic_master_eri_isr(void);


#define VECTOR_NUMBER_SCI0_RXI ((IRQn_Type)0)  
#define SCI0_RXI_IRQn          ((IRQn_Type)0)  
#define VECTOR_NUMBER_SCI0_TXI ((IRQn_Type)1)  
#define SCI0_TXI_IRQn          ((IRQn_Type)1)  
#define VECTOR_NUMBER_SCI0_TEI ((IRQn_Type)2)  
#define SCI0_TEI_IRQn          ((IRQn_Type)2)  
#define VECTOR_NUMBER_SCI0_ERI ((IRQn_Type)3)  
#define SCI0_ERI_IRQn          ((IRQn_Type)3)  
#define VECTOR_NUMBER_SCI7_RXI ((IRQn_Type)4)  
#define SCI7_RXI_IRQn          ((IRQn_Type)4)  
#define VECTOR_NUMBER_SCI7_TXI ((IRQn_Type)5)  
#define SCI7_TXI_IRQn          ((IRQn_Type)5)  
#define VECTOR_NUMBER_SCI7_TEI ((IRQn_Type)6)  
#define SCI7_TEI_IRQn          ((IRQn_Type)6)  
#define VECTOR_NUMBER_SCI7_ERI ((IRQn_Type)7)  
#define SCI7_ERI_IRQn          ((IRQn_Type)7)  
#define VECTOR_NUMBER_SCI9_RXI ((IRQn_Type)8)  
#define SCI9_RXI_IRQn          ((IRQn_Type)8)  
#define VECTOR_NUMBER_SCI9_TXI ((IRQn_Type)9)  
#define SCI9_TXI_IRQn          ((IRQn_Type)9)  
#define VECTOR_NUMBER_SCI9_TEI ((IRQn_Type)10)  
#define SCI9_TEI_IRQn          ((IRQn_Type)10)  
#define VECTOR_NUMBER_SCI9_ERI ((IRQn_Type)11)  
#define SCI9_ERI_IRQn          ((IRQn_Type)11)  
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
#define VECTOR_NUMBER_ICU_IRQ1 ((IRQn_Type)17)  
#define ICU_IRQ1_IRQn          ((IRQn_Type)17)  
#define VECTOR_NUMBER_ICU_IRQ2 ((IRQn_Type)18)  
#define ICU_IRQ2_IRQn          ((IRQn_Type)18)  
#define VECTOR_NUMBER_ICU_IRQ3 ((IRQn_Type)19)  
#define ICU_IRQ3_IRQn          ((IRQn_Type)19)  
#define VECTOR_NUMBER_ICU_IRQ4 ((IRQn_Type)20)  
#define ICU_IRQ4_IRQn          ((IRQn_Type)20)  
#define VECTOR_NUMBER_ICU_IRQ5 ((IRQn_Type)21)  
#define ICU_IRQ5_IRQn          ((IRQn_Type)21)  
#define VECTOR_NUMBER_ICU_IRQ6 ((IRQn_Type)22)  
#define ICU_IRQ6_IRQn          ((IRQn_Type)22)  
#define VECTOR_NUMBER_ICU_IRQ7 ((IRQn_Type)23)  
#define ICU_IRQ7_IRQn          ((IRQn_Type)23)  
#define VECTOR_NUMBER_ICU_IRQ8 ((IRQn_Type)24)  
#define ICU_IRQ8_IRQn          ((IRQn_Type)24)  
#define VECTOR_NUMBER_ICU_IRQ9 ((IRQn_Type)25)  
#define ICU_IRQ9_IRQn          ((IRQn_Type)25)  
#define VECTOR_NUMBER_ICU_IRQ10 ((IRQn_Type)26)  
#define ICU_IRQ10_IRQn          ((IRQn_Type)26)  
#define VECTOR_NUMBER_ICU_IRQ11 ((IRQn_Type)27)  
#define ICU_IRQ11_IRQn          ((IRQn_Type)27)  
#define VECTOR_NUMBER_ICU_IRQ12 ((IRQn_Type)28)  
#define ICU_IRQ12_IRQn          ((IRQn_Type)28)  
#define VECTOR_NUMBER_ICU_IRQ13 ((IRQn_Type)29)  
#define ICU_IRQ13_IRQn          ((IRQn_Type)29)  
#define VECTOR_NUMBER_ICU_IRQ14 ((IRQn_Type)30)  
#define ICU_IRQ14_IRQn          ((IRQn_Type)30)  
#define VECTOR_NUMBER_ICU_IRQ15 ((IRQn_Type)31)  
#define ICU_IRQ15_IRQn          ((IRQn_Type)31)  
#define VECTOR_NUMBER_SPI0_RXI ((IRQn_Type)32)  
#define SPI0_RXI_IRQn          ((IRQn_Type)32)  
#define VECTOR_NUMBER_SPI0_TXI ((IRQn_Type)33)  
#define SPI0_TXI_IRQn          ((IRQn_Type)33)  
#define VECTOR_NUMBER_SPI0_TEI ((IRQn_Type)34)  
#define SPI0_TEI_IRQn          ((IRQn_Type)34)  
#define VECTOR_NUMBER_SPI0_ERI ((IRQn_Type)35)  
#define SPI0_ERI_IRQn          ((IRQn_Type)35)  
#define VECTOR_NUMBER_SPI1_RXI ((IRQn_Type)36)  
#define SPI1_RXI_IRQn          ((IRQn_Type)36)  
#define VECTOR_NUMBER_SPI1_TXI ((IRQn_Type)37)  
#define SPI1_TXI_IRQn          ((IRQn_Type)37)  
#define VECTOR_NUMBER_SPI1_TEI ((IRQn_Type)38)  
#define SPI1_TEI_IRQn          ((IRQn_Type)38)  
#define VECTOR_NUMBER_SPI1_ERI ((IRQn_Type)39)  
#define SPI1_ERI_IRQn          ((IRQn_Type)39)  
#define VECTOR_NUMBER_IIC2_RXI ((IRQn_Type)40)  
#define IIC2_RXI_IRQn          ((IRQn_Type)40)  
#define VECTOR_NUMBER_IIC2_TXI ((IRQn_Type)41)  
#define IIC2_TXI_IRQn          ((IRQn_Type)41)  
#define VECTOR_NUMBER_IIC2_TEI ((IRQn_Type)42)  
#define IIC2_TEI_IRQn          ((IRQn_Type)42)  
#define VECTOR_NUMBER_IIC2_ERI ((IRQn_Type)43)  
#define IIC2_ERI_IRQn          ((IRQn_Type)43)  
#ifdef __cplusplus
}
#endif
#endif 
