


#ifndef _TXGBE_HW_H_
#define _TXGBE_HW_H_

int txgbe_disable_sec_tx_path(struct wx *wx);
void txgbe_enable_sec_tx_path(struct wx *wx);
int txgbe_read_pba_string(struct wx *wx, u8 *pba_num, u32 pba_num_size);
int txgbe_validate_eeprom_checksum(struct wx *wx, u16 *checksum_val);
int txgbe_reset_hw(struct wx *wx);

#endif 
