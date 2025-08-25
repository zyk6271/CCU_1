#include "stdint.h"

typedef struct
{
    uint8_t temp;
    uint8_t onoff;
    uint8_t burn;
    uint8_t circle;
}heater_remote_state_t;

typedef struct
{
    uint8_t valid[2];
    uint8_t type[2];
    uint32_t addr[2];
    uint32_t prio_addr;
    heater_remote_state_t remote;
    heater_remote_state_t local;
}heater_remote_t;

uint8_t heater_remote_valid_search(uint32_t addr);
uint8_t heater_remote_device_add(uint32_t addr);
void heater_remote_init(void);
void heater_remote_data_refresh(void);
void heater_remote_info_save(void);
uint8_t heater_remote_burn_read(void);
void heater_remote_temp_control(uint32_t addr, uint8_t value);
uint8_t heater_remote_temp_read(void);
void heater_remote_onoff_control(uint32_t addr, uint8_t value);
uint8_t heater_remote_onoff_read(void);
void heater_remote_circle_control(uint32_t addr, uint8_t value);
uint8_t heater_remote_circle_read(void);
void heater_remote_prio_addr_control(uint32_t addr,uint8_t value);
uint32_t heater_remote_prio_addr_read(void);