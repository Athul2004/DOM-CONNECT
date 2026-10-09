#ifndef GATEWAY_EZSP_HANDLER_H
#define GATEWAY_EZSP_HANDLER_H

#include "gateway/ezsp/ash.h"
#include "gateway/config.h"

typedef struct ezsp_handler ezsp_handler_t;

typedef void (*ezsp_zcl_callback_t)(uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data);
typedef void (*ezsp_device_join_callback_t)(uint16_t node_id, const uint8_t *eui64, uint8_t status, void *user_data);

ezsp_handler_t *ezsp_handler_create(ash_context_t *ash, const ezsp_config_t *cfg);
void ezsp_handler_set_zcl_callback(ezsp_handler_t *h, ezsp_zcl_callback_t cb, void *user_data);
void ezsp_handler_set_device_join_callback(ezsp_handler_t *h, ezsp_device_join_callback_t cb, void *user_data);
void ezsp_handler_destroy(ezsp_handler_t *h);

gw_err_t ezsp_negotiate_version(ezsp_handler_t *h, uint8_t desired_version, coordinator_info_t *info);
gw_err_t ezsp_get_eui64(ezsp_handler_t *h, uint8_t eui64[8]);
gw_err_t ezsp_get_node_id(ezsp_handler_t *h, uint16_t *node_id);
gw_err_t ezsp_get_network_state(ezsp_handler_t *h, uint8_t *state);
gw_err_t ezsp_form_network(ezsp_handler_t *h);
gw_err_t ezsp_nop(ezsp_handler_t *h);
gw_err_t ezsp_set_permit_joining(ezsp_handler_t *h, uint8_t duration_sec);
gw_err_t ezsp_add_endpoint(ezsp_handler_t *h);
gw_err_t ezsp_send_unicast(ezsp_handler_t *h, uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len);
gw_err_t ezsp_poll(ezsp_handler_t *h);

#endif // GATEWAY_EZSP_HANDLER_H
