#include "esp_err.h"

void dish_washer_info_upload(void);
void ccu_poll_status_reset(void);
void ccu_modbus_poll(void);
void ccu_modbus_init(void);
uint8_t modbus_detect_result_read(void);