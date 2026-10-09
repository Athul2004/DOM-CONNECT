#ifndef GATEWAY_ASH_H
#define GATEWAY_ASH_H

#include "gateway/transport/transport.h"
#include "gateway/config.h"

typedef struct ash_context ash_context_t;

ash_context_t *ash_create(transport_t *transport, const ash_config_t *cfg);
void           ash_destroy(ash_context_t *ctx);

gw_err_t ash_reset(ash_context_t *ctx);
gw_err_t ash_send_data(ash_context_t *ctx, const uint8_t *data, size_t len);
gw_err_t ash_receive_data(ash_context_t *ctx, uint8_t *buf, size_t max_len, size_t *out_len, uint32_t timeout_ms);
gw_err_t ash_send_and_receive(ash_context_t *ctx, const uint8_t *cmd, size_t cmd_len, uint8_t *rsp, size_t rsp_max, size_t *rsp_len, uint32_t timeout_ms);
bool     ash_is_connected(ash_context_t *ctx);

#endif // GATEWAY_ASH_H
