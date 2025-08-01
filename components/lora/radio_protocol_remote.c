#include "radio.h"
#include "radio_protocol.h"
#include "radio_encoder.h"
#include "radio_protocol_utils.h"
#include "radio_protocol_remote.h"
#include "heater_remote.h"
#include "esp_log.h"

#define TAG "protocol_remote"

uint8_t allow_add_device = 1;
uint32_t radio_local_addr = 0x10001234;

void radio_remote_command_send(tx_format *tx_frame)
{
    unsigned short send_len = 0;
    send_len = set_lora_tx_byte(send_len,0xBF);
    send_len = set_lora_tx_byte(send_len,(NET_REGION_SELECT << 4) | NETWORK_VERSION);
    send_len = set_lora_tx_byte(send_len,(tx_frame->msg_adv << 7) | (DEVICE_TYPE_SELECT << 3) | tx_frame->msg_type);
    send_len = set_lora_tx_word(send_len,tx_frame->dest_addr);
    send_len = set_lora_tx_word(send_len,tx_frame->source_addr);
    send_len = set_lora_tx_byte(send_len,tx_frame->command);
    send_len = set_lora_tx_byte(send_len,tx_frame->tx_len);
    send_len = set_lora_tx_buffer(send_len,tx_frame->tx_data,tx_frame->tx_len);
    send_len = set_lora_tx_crc(send_len);
    lora_tx_enqueue(get_lora_tx_buf(),send_len,tx_frame->parameter,0);
}

static void radio_frame_remote_parse_learn(rx_format *rx_frame)
{
    uint8_t send_value = 0;
    uint8_t sub_command = rx_frame->rx_data[2];
    tx_format tx_frame = {0};

    //ESP_LOGI(pcTaskGetName(NULL), "radio_frame_remote_parse_learn,sub_command %d\r\n",sub_command);
    if(rx_frame->dest_addr == 0xFFFFFFFF && sub_command == 0x01)
    {
        if(allow_add_device)
        {
            send_value = 2;
        }
        else
        {
            send_value = 3;
        }
        tx_frame.msg_adv = 0;
        tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
        tx_frame.dest_addr = rx_frame->source_addr;
        tx_frame.source_addr = radio_local_addr;
        tx_frame.command = LEARN_DEVICE_CMD;
        tx_frame.tx_data = &send_value;
        tx_frame.tx_len = 1;
        radio_remote_command_send(&tx_frame);
    }
    else if(rx_frame->dest_addr == radio_local_addr)
    {
        switch(sub_command)
        {
        case 4://add device
            if(heater_remote_device_add(rx_frame->source_addr) == 1)
            {
                send_value = 5;
            }
            else
            {
                send_value = 6;
            }
            tx_frame.msg_adv = 0;
            tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
            tx_frame.dest_addr = rx_frame->source_addr;
            tx_frame.source_addr = radio_local_addr;
            tx_frame.command = LEARN_DEVICE_CMD;
            tx_frame.tx_data = &send_value;
            tx_frame.tx_len = 1;
            radio_remote_command_send(&tx_frame);
            break;
        default:
            break;
        }
    }
}

static void radio_frame_remote_parse_heart_upload(rx_format *rx_frame)
{
    if(rx_frame->rx_data[5] == 1)
    {
        heater_remote_prio_addr_control(rx_frame->source_addr, 1);
        heater_remote_temp_control(rx_frame->source_addr, rx_frame->rx_data[2]);
        heater_remote_onoff_control(rx_frame->source_addr, rx_frame->rx_data[3]);
        heater_remote_circle_control(rx_frame->source_addr, rx_frame->rx_data[4]);
        if(rx_frame->rx_data[3] == 1)
        {
            heater_remote_prio_addr_control(rx_frame->source_addr, 0);
        }
    }
    else
    {
        heater_remote_prio_addr_control(rx_frame->source_addr, 0);
    }

    uint8_t send_data[8] = {0};
    send_data[0] = heater_remote_temp_read();
    send_data[1] = heater_remote_onoff_read();
    send_data[2] = heater_remote_burn_read();
    send_data[3] = heater_remote_circle_read();
    send_data[4] = (heater_remote_prio_addr_read() >> 24) %  0xFF;
    send_data[5] = (heater_remote_prio_addr_read() >> 16) %  0xFF;
    send_data[6] = (heater_remote_prio_addr_read() >> 8) %  0xFF;
    send_data[7] = heater_remote_prio_addr_read() %  0xFF;

    tx_format tx_frame = {0};
    tx_frame.msg_adv = 0;
    tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
    tx_frame.dest_addr = rx_frame->source_addr;
    tx_frame.source_addr = radio_local_addr;
    tx_frame.command = HEART_UPLOAD_CMD;
    tx_frame.tx_data = send_data;
    tx_frame.tx_len = 8;
    radio_remote_command_send(&tx_frame);
}

static void radio_frame_remote_parse_request_sync(rx_format *rx_frame)
{
    uint8_t send_data[8] = {0};
    send_data[0] = heater_remote_temp_read();
    send_data[1] = heater_remote_onoff_read();
    send_data[2] = heater_remote_burn_read();
    send_data[3] = heater_remote_circle_read();
    send_data[4] = (heater_remote_prio_addr_read() >> 24) %  0xFF;
    send_data[5] = (heater_remote_prio_addr_read() >> 16) %  0xFF;
    send_data[6] = (heater_remote_prio_addr_read() >> 8) %  0xFF;
    send_data[7] = heater_remote_prio_addr_read() %  0xFF;

    tx_format tx_frame = {0};
    tx_frame.msg_adv = 1;
    tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
    tx_frame.dest_addr = rx_frame->source_addr;
    tx_frame.source_addr = radio_local_addr;
    tx_frame.command = REQUEST_SYNC_CMD;
    tx_frame.tx_data = send_data;
    tx_frame.tx_len = 8;
    radio_remote_command_send(&tx_frame);
}

static void radio_frame_remote_parse_control_temp(rx_format *rx_frame)
{
    heater_remote_temp_control(rx_frame->source_addr,rx_frame->rx_data[2]);
    uint8_t real_value = heater_remote_temp_read();

    tx_format tx_frame = {0};
    tx_frame.msg_adv = 0;
    tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
    tx_frame.dest_addr = rx_frame->source_addr;
    tx_frame.source_addr = radio_local_addr;
    tx_frame.command = CONTROL_TEMP_CMD;
    tx_frame.tx_data = &real_value;
    tx_frame.tx_len = 1;
    radio_remote_command_send(&tx_frame);
}

static void radio_frame_remote_parse_control_onoff(rx_format *rx_frame)
{
    heater_remote_onoff_control(rx_frame->source_addr,rx_frame->rx_data[2]);
    uint8_t real_value = heater_remote_onoff_read();

    tx_format tx_frame = {0};
    tx_frame.msg_adv = 0;
    tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
    tx_frame.dest_addr = rx_frame->source_addr;
    tx_frame.source_addr = radio_local_addr;
    tx_frame.command = CONTROL_ONOFF_CMD;
    tx_frame.tx_data = &real_value;
    tx_frame.tx_len = 1;
    radio_remote_command_send(&tx_frame);
}

static void radio_frame_remote_parse_control_circle(rx_format *rx_frame)
{
    heater_remote_circle_control(rx_frame->source_addr,rx_frame->rx_data[2]);
    uint8_t real_value = heater_remote_circle_read();

    tx_format tx_frame = {0};
    tx_frame.msg_adv = 0;
    tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
    tx_frame.dest_addr = rx_frame->source_addr;
    tx_frame.source_addr = radio_local_addr;
    tx_frame.command = CONTROL_CIRCLE_CMD;
    tx_frame.tx_data = &real_value;
    tx_frame.tx_len = 1;
    radio_remote_command_send(&tx_frame);
}

static void radio_frame_remote_parse_control_priority(rx_format *rx_frame)
{
    heater_remote_prio_addr_control(rx_frame->source_addr,rx_frame->rx_data[2]);
    uint8_t real_value = heater_remote_prio_addr_read() == rx_frame->source_addr ? 1 : 0;

    tx_format tx_frame = {0};
    tx_frame.msg_adv = 0;
    tx_frame.msg_type = MSG_UNCONFIRMED_DOWNLINK;
    tx_frame.dest_addr = rx_frame->source_addr;
    tx_frame.source_addr = radio_local_addr;
    tx_frame.command = CONTROL_CIRCLE_CMD;
    tx_frame.tx_data = &real_value;
    tx_frame.tx_len = 1;
    radio_remote_command_send(&tx_frame);
}

void radio_frame_remote_parse(rx_format *rx_frame)
{
    if((rx_frame->rx_data[0] == LEARN_DEVICE_CMD))//learn device ignore source address check
    {
        radio_frame_remote_parse_learn(rx_frame);
        return;
    }

    if((rx_frame->dest_addr != radio_local_addr) || (heater_remote_valid_search(rx_frame->source_addr) == 0))
    {
        //ESP_LOGE(TAG,"radio_frame_remote_parse fail,dest 0x%08X,src 0x%08X",rx_frame->dest_addr,rx_frame->source_addr);
        return;
    }

    uint8_t command = rx_frame->rx_data[0];
    switch(command)
    {
    case REQUEST_SYNC_CMD:
        radio_frame_remote_parse_request_sync(rx_frame);
        break;
    case HEART_UPLOAD_CMD:
        radio_frame_remote_parse_heart_upload(rx_frame);
        break;
    case CONTROL_TEMP_CMD:
        radio_frame_remote_parse_control_temp(rx_frame);
        break;
    case CONTROL_ONOFF_CMD:
        radio_frame_remote_parse_control_onoff(rx_frame);
        break;
    case CONTROL_CIRCLE_CMD:
        radio_frame_remote_parse_control_circle(rx_frame);
        break;
    case CONTROL_PRIORITY_CMD:
        radio_frame_remote_parse_control_priority(rx_frame);
        break;
    default:
        break;
    }
}