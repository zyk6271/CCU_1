#ifndef __TCP_CLIENT_H_
#define __TCP_CLIENT_H_

#include <stdint.h>

void tcp_client_init(void);
uint8_t tcp_client_send(uint8_t *send_buf,size_t len);

#endif