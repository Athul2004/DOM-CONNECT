#ifndef GATEWAY_COORDINATOR_ADAPTER_H
#define GATEWAY_COORDINATOR_ADAPTER_H

#include "gateway/types.h"
#include "gateway/error.h"

typedef struct coordinator_adapter coordinator_adapter_t;

struct coordinator_adapter {
    gw_err_t (*start)(coordinator_adapter_t *self);
    gw_err_t (*stop)(coordinator_adapter_t *self);
    gw_err_t (*reset)(coordinator_adapter_t *self);
    gw_err_t (*get_info)(coordinator_adapter_t *self, coordinator_info_t *info);
    gw_err_t (*health_check)(coordinator_adapter_t *self);
    gw_err_t (*form_network)(coordinator_adapter_t *self);
    gw_err_t (*permit_join)(coordinator_adapter_t *self, uint8_t duration_sec);
    gw_err_t (*send_unicast)(coordinator_adapter_t *self, uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len);
    gw_err_t (*poll)(coordinator_adapter_t *self);
    void     (*set_zcl_callback)(coordinator_adapter_t *self, void (*cb)(uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data), void *user_data);
    void     (*set_device_join_callback)(coordinator_adapter_t *self, void (*cb)(uint16_t node_id, const uint8_t *eui64, uint8_t status, void *user_data), void *user_data);
    bool     (*is_connected)(coordinator_adapter_t *self);
    void     (*destroy)(coordinator_adapter_t *self);
    
    void     *impl;
};

#endif // GATEWAY_COORDINATOR_ADAPTER_H
