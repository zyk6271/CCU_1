#ifndef __RADIO_PROTOCOL_UTILS_H_
#define __RADIO_PROTOCOL_UTILS_H_

unsigned char get_check_crc(unsigned char *pack, unsigned short pack_len);
unsigned char *get_lora_tx_buf(void);
unsigned short set_lora_tx_byte(unsigned short dest, unsigned char byte);
unsigned short set_lora_tx_buffer(unsigned short dest, const unsigned char *src, unsigned short len);
unsigned short set_lora_tx_word(unsigned short dest, unsigned int word);
unsigned short set_lora_tx_crc(unsigned short dest);

#endif

