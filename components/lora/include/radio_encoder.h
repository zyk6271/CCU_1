#include "stdint.h"

void lora_tx_enqueue(unsigned char *data,uint8_t length,uint8_t need_ack,uint8_t parameter);
void radio_send_queue_init(void);