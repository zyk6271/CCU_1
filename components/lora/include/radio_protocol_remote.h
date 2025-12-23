#ifndef __RADIO_PROTOCOL_REMOTE_H_
#define __RADIO_PROTOCOL_REMOTE_H_

#include "radio_protocol.h"

#define ACK_RESPONSE_CMD             0x01
#define HEART_UPLOAD_CMD             0x02
#define LEARN_DEVICE_CMD             0x03
#define SYNC_REQUEST_CMD             0x04

void radio_frame_remote_parse(rx_format *rx_frame);
void heater_remote_information_uplaod(uint32_t dest_addr,uint8_t msg_adv);

#endif

