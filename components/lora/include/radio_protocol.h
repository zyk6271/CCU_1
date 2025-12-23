#ifndef RADIO_PROTOCOL_H_
#define RADIO_PROTOCOL_H_

#include "stdint.h"

typedef struct
{
    int16_t rssi;
    int8_t snr;
    uint8_t msg_type;
    uint8_t calc_crc;
    uint8_t src_crc;
    uint8_t device_type;
    uint32_t dest_addr;
    uint32_t source_addr;
    uint8_t *rx_data;
    uint8_t rx_len;
}rx_format;

typedef struct
{
    uint8_t msg_adv;
    uint8_t msg_type;
    uint32_t dest_addr;
    uint32_t source_addr;
    uint8_t command;
    uint8_t need_ack;
    uint8_t parameter;
    uint8_t *tx_data;
    uint8_t tx_len;
}tx_format;


#define NETWORK_VERSION               0x01

#define FRAME_START                   0xBF

#define MSG_UNCONFIRMED_UPLINK        0x00
#define MSG_CONFIRMED_UPLINK          0x01
#define MSG_UNCONFIRMED_DOWNLINK      0x02
#define MSG_CONFIRMED_DOWNLINK        0x03

#define DEVICE_TYPE_CCU_TYPE_A        0x01
#define DEVICE_TYPE_CCU_TYPE_B        0x02
#define DEVICE_TYPE_CCU_TYPE_C        0x03
#define DEVICE_TYPE_CCU_TYPE_D        0x04
#define DEVICE_TYPE_REMOTE_TYPE_A     0x05
#define DEVICE_TYPE_REMOTE_TYPE_B     0x06
#define DEVICE_TYPE_REMOTE_TYPE_C     0x07
#define DEVICE_TYPE_REMOTE_TYPE_D     0x08

#define DEVICE_TYPE_SELECT            DEVICE_TYPE_CCU_TYPE_A
#define NETID_REGION_HONGKONG         0x01
#define NET_REGION_SELECT             NETID_REGION_HONGKONG


void radio_protocol_parse(int rssi,int snr,uint8_t* data, uint8_t len);

#endif /* RADIO_PROTOCOL_RADIO_PROTOCOL_H_ */
