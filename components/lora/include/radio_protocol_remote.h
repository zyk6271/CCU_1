#ifndef __RADIO_PROTOCOL_REMOTE_H_
#define __RADIO_PROTOCOL_REMOTE_H_

#include "radio_protocol.h"

#define REQUEST_SYNC_CMD             0x01
#define HEART_UPLOAD_CMD             0x02
#define CONTROL_TEMP_CMD             0x03
#define CONTROL_ONOFF_CMD            0x04
#define CONTROL_CIRCLE_CMD           0x05
#define CONTROL_PRIORITY_CMD         0x06
#define LEARN_DEVICE_CMD             0x07

void radio_frame_remote_parse(rx_format *rx_frame);

#endif

