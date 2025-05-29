#include "esp_err.h"

void dish_washer_info_upload(void);
void dishwasher_poll_status_reset(void);
esp_err_t dish_washer_modbus_poll(void);
void dish_washer_modbus_init(void);