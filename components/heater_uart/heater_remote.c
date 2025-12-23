#include "stdint.h"
#include "storage.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "heater_remote.h"
#include "heater_interface_api.h"
#include "radio_protocol_remote.h"

static const char *TAG = "heater_remote";

heater_remote_t heater_remote = {0};

uint8_t heater_remote_repeat_cnt = 0;

esp_timer_handle_t heater_remote_timeout_timer;
esp_timer_handle_t heater_remote_repeat_timer;

uint8_t heater_remote_valid_search(uint32_t addr)
{
    for(uint8_t i = 0; i< 2; i++)
    {
        if(heater_remote.valid[i])
        {
            if(heater_remote.addr[i] == addr)
            {
                return 1;
            }
        }
    }

    return 0;
}

uint8_t heater_remote_device_add(uint32_t addr)
{
    if(heater_remote_valid_search(addr))
    {
        return 1;
    }
    else
    {
        for(uint8_t i = 0; i< 2; i++)
        {
            if(heater_remote.valid[i] == 0)
            {
                heater_remote.valid[i] = 1;
                heater_remote.addr[i] = addr;
                heater_remote_info_save();
                return 1;
            }
        }
    }

    return 0;
}

void heater_remote_info_save(void)
{
    int result = storage_save_key_blob("remote_info",(uint8_t *)&heater_remote, sizeof(heater_remote_t));
    ESP_LOGI(TAG,"heater_remote_info_save result %d",result);
}

static void heater_remote_timeout_timer_cb(void* arg)
{
    heater_remote_prio_addr_control(heater_remote.prio_addr,0);
}

static void heater_remote_repeat_timer_cb(void* arg)
{
    uint8_t need_retry = 0;
    if(heater_remote.local.temp != heater_remote.remote.temp)
    {
        need_retry = 1;
        heater_interface_temperature_setting(heater_remote.remote.temp);
    }
    if(heater_remote.local.onoff != heater_remote.remote.onoff)
    {
        need_retry = 1;
        heater_interface_power_setting(heater_remote.remote.onoff);
    }
    if(heater_remote.local.circle != heater_remote.remote.circle)
    {
        need_retry = 1;
        heater_interface_circulation_setting(heater_remote.remote.circle);
    }
    if(heater_remote.local.eco != heater_remote.remote.eco)
    {
        need_retry = 1;
        heater_interface_eco_setting(heater_remote.remote.eco);
    }
    if(need_retry)
    {
        if(heater_remote_repeat_cnt++ < 2)
        {
            esp_timer_start_once(heater_remote_repeat_timer, 2000 * 1000);
        }
    }
}

void heater_remote_init(void)
{
    uint32_t length = 0;
    storage_read_key_blob("remote_info", (uint8_t *)&heater_remote, &length);
    const esp_timer_create_args_t heater_remote_timeout_timer_args = 
    {
        .callback = &heater_remote_timeout_timer_cb,
        .name = "heater_remote_timeout_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&heater_remote_timeout_timer_args, &heater_remote_timeout_timer));

    const esp_timer_create_args_t heater_remote_repeat_timer_args = 
    {
        .callback = &heater_remote_repeat_timer_cb,
        .name = "heater_remote_repeat_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&heater_remote_repeat_timer_args, &heater_remote_repeat_timer));
}

void heater_remote_data_refresh(void)
{
    uint8_t need_upload = 0;
    if(heater_remote.local.temp != heater_interface_temperature_value_read())
    {
        need_upload = 1;
        heater_remote.local.temp = heater_interface_temperature_value_read();
    }
    if(heater_remote.local.circle != heater_interface_circulation_value_read())
    {
        need_upload = 1;
        heater_remote.local.circle = heater_interface_circulation_value_read();
    }
    if(heater_remote.local.onoff != heater_interface_power_value_read())
    {
        need_upload = 1;
        heater_remote.local.onoff = heater_interface_power_value_read();
    }
    if(heater_remote.local.burn != heater_interface_burn_status_read())
    {
        need_upload = 1;
        heater_remote.local.burn = heater_interface_burn_status_read();
    }
    if(heater_remote.local.eco != heater_interface_eco_read())
    {
        need_upload = 1;
        heater_remote.local.eco = heater_interface_eco_read();
    }
    if(need_upload)
    {
        heater_remote_information_uplaod(0xFFFFFFFF,1);
    }
}

void heater_remote_repeat_start(void)
{
    heater_remote_repeat_cnt = 0;
    // esp_timer_stop(heater_remote_repeat_timer);
    // esp_timer_start_once(heater_remote_repeat_timer, 2000 * 1000);
}

void heater_remote_timeout_start(void)
{
    esp_timer_start_once(heater_remote_timeout_timer, 10 * 60 * 1000 * 1000);
}

void heater_remote_timeout_stop(void)
{
    esp_timer_stop(heater_remote_timeout_timer);
}

void heater_remote_timeout_refresh(void)
{
    esp_timer_restart(heater_remote_timeout_timer, 10 * 60 * 1000 * 1000);
}

uint8_t heater_remote_burn_read(void)
{
    return heater_remote.local.burn;
}

void heater_remote_temp_control(uint32_t addr, uint8_t value)
{
    if(addr == heater_remote.prio_addr)
    {
        if(heater_remote.local.temp != value)
        {
            heater_remote.remote.temp = value;
            heater_interface_temperature_setting(value);
        }
    }
}

uint8_t heater_remote_temp_read(void)
{
    return heater_remote.local.temp;
}

void heater_remote_onoff_control(uint32_t addr, uint8_t value)
{
    if(addr == heater_remote.prio_addr)
    {
        if(heater_remote.local.onoff != value)
        {
            heater_remote.remote.onoff = value;
            heater_interface_power_setting(value);
        }
    }
}

uint8_t heater_remote_onoff_read(void)
{
    return heater_remote.local.onoff;
}

void heater_remote_circle_control(uint32_t addr, uint8_t value)
{
    if(addr == heater_remote.prio_addr)
    {
        if(heater_remote.local.circle == 0 || heater_remote.local.circle == 1 )
        {
            if(heater_remote.local.circle != value)
            {
                heater_remote.remote.circle = value;
                heater_interface_circulation_setting(value);
            }   
        }
    }
}

uint8_t heater_remote_circle_read(void)
{
    return heater_remote.local.circle;
}

uint8_t heater_remote_prio_valid_search(uint32_t addr)
{
    return heater_remote.prio_addr == addr;
}

void heater_remote_prio_addr_control(uint32_t addr,uint8_t value)
{
    if(value)
    {
        if(heater_remote.prio_addr == 0 || heater_remote.prio_addr != addr)
        {
            if(heater_interface_priority_read() == 0x01)
            {
                ESP_LOGE(TAG, "Heater priority can't set t because of bathroom RC");
                return;
            }
            heater_remote.prio_addr = addr;
            heater_remote_timeout_start();
        }
    }
    else
    {
        if(heater_remote.prio_addr == addr)
        {
            if(heater_remote_temp_read() >= 50)
            {
                heater_remote_temp_control(addr, 48);
            }
            heater_remote.prio_addr = 0;
            heater_remote_timeout_stop();
        }
    }
}

uint8_t heater_remote_prio_valid_check(uint32_t addr)
{
    return heater_remote.prio_addr == addr;
}

uint32_t heater_remote_prio_addr_read(void)
{
    return heater_remote.prio_addr;
}

void heater_remote_eco_control(uint32_t addr, uint8_t value)
{
    if(addr == heater_remote.prio_addr)
    {
        if(heater_remote.local.eco == 0 || heater_remote.local.eco == 1 )
        {
            if(heater_remote.local.eco != value)
            {
                heater_remote.remote.eco = value;
                heater_interface_eco_setting(value);
            }   
        }
    }
}

uint8_t heater_remote_eco_read(void)
{
    return heater_remote.local.eco;
}

uint8_t heater_remote_priority_type_read(void)
{
    uint8_t result = 0;
    uint8_t heater_priority = heater_interface_priority_read();
    uint8_t remote_priority = heater_remote.prio_addr;
    if(heater_priority == 0)
    {
        if(remote_priority)
        {
            result = 3;
        }
        else
        {
            result = 0;
        }
    }
    else if(heater_priority == 1)
    {
        result = 1;
    }
    else if(heater_priority == 2)
    {
        if(remote_priority)
        {
            result = 3;
        }
        else
        {
            result = 2;
        }
    }

    return result;
}
