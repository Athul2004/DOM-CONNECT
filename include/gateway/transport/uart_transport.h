#ifndef GATEWAY_UART_TRANSPORT_H
#define GATEWAY_UART_TRANSPORT_H

#include "gateway/transport/transport.h"
#include "gateway/config.h"

transport_t *uart_transport_create(const transport_config_t *cfg);

#endif // GATEWAY_UART_TRANSPORT_H
