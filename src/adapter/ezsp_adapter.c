#include "gateway/adapter/ezsp_adapter.h"
#include "gateway/ezsp/ash.h"
#include "gateway/ezsp/ezsp_handler.h"
#include "gateway/log.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif

typedef struct {
    gateway_config_t cfg;
    transport_t *transport;
    ash_context_t *ash;
    ezsp_handler_t *ezsp;
    coordinator_info_t info;
    bool connected;
    bool transport_owned;
} ezsp_adapter_impl_t;

static gw_err_t ezsp_adapter_start(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    gw_err_t err;
    
    err = impl->transport->open(impl->transport);
    if (err != GW_OK) return err;
    
    impl->ash = ash_create(impl->transport, &impl->cfg.coordinator.ash);
    err = ash_reset(impl->ash);
    if (err != GW_OK) {
        impl->transport->close(impl->transport);
        return err;
    }
    
    impl->ezsp = ezsp_handler_create(impl->ash, &impl->cfg.coordinator.ezsp);
    
    err = ezsp_negotiate_version(impl->ezsp, impl->cfg.coordinator.ezsp.protocol_version, &impl->info);
    if (err != GW_OK) return err;
    
    err = ezsp_get_eui64(impl->ezsp, impl->info.eui64);
    if (err != GW_OK) return err;
    
    err = ezsp_get_node_id(impl->ezsp, &impl->info.node_id);
    if (err != GW_OK) return err;
    
    err = ezsp_get_network_state(impl->ezsp, &impl->info.network_state);
    if (err != GW_OK) return err;
    
    // Add endpoint to receive ZCL messages
    ezsp_add_endpoint(impl->ezsp);
    
    impl->connected = true;
    LOG_INFO("coordinator.adapter", "Coordinator ready", LOG_STR("adapter", "ezsp"));
    return GW_OK;
}

static gw_err_t ezsp_adapter_stop(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    
    if (impl->ezsp) {
        ezsp_handler_destroy(impl->ezsp);
        impl->ezsp = NULL;
    }
    if (impl->ash) {
        ash_destroy(impl->ash);
        impl->ash = NULL;
    }
    if (impl->transport) {
        impl->transport->close(impl->transport);
    }
    impl->connected = false;
    return GW_OK;
}

static gw_err_t ezsp_adapter_reset(coordinator_adapter_t *self) {
    ezsp_adapter_stop(self);
    usleep(500 * 1000);
    return ezsp_adapter_start(self);
}

static gw_err_t ezsp_adapter_get_info(coordinator_adapter_t *self, coordinator_info_t *info) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    if (!impl->connected) return GW_ERR_COORD_NOT_CONNECTED;
    memcpy(info, &impl->info, sizeof(coordinator_info_t));
    return GW_OK;
}

static gw_err_t ezsp_adapter_health_check(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    if (!impl->connected) return GW_ERR_COORD_NOT_CONNECTED;
    
    gw_err_t err = ezsp_nop(impl->ezsp);
    if (err == GW_OK) {
        LOG_DEBUG("coordinator.health", "Health check passed");
    } else {
        LOG_WARN("coordinator.health", "Health check failed", LOG_STR("error", gw_err_str(err)));
    }
    return err;
}

static bool ezsp_adapter_is_connected(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    return impl->connected;
}

static gw_err_t ezsp_adapter_form_network(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    if (!impl->connected) return GW_ERR_COORD_NOT_CONNECTED;
    return ezsp_form_network(impl->ezsp);
}

static gw_err_t ezsp_adapter_permit_join(coordinator_adapter_t *self, uint8_t duration_sec) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    if (!impl->connected) return GW_ERR_COORD_NOT_CONNECTED;
    return ezsp_set_permit_joining(impl->ezsp, duration_sec);
}

static gw_err_t ezsp_adapter_send_unicast(coordinator_adapter_t *self, uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    if (!impl->connected) return GW_ERR_COORD_NOT_CONNECTED;
    return ezsp_send_unicast(impl->ezsp, node_id, profile_id, cluster_id, src_ep, dst_ep, payload, payload_len);
}

static gw_err_t ezsp_adapter_poll(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    if (!impl->connected) return GW_ERR_COORD_NOT_CONNECTED;
    return ezsp_poll(impl->ezsp);
}

static void ezsp_adapter_set_zcl_callback(coordinator_adapter_t *self, void (*cb)(uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data), void *user_data) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    ezsp_handler_set_zcl_callback(impl->ezsp, cb, user_data);
}

static void ezsp_adapter_set_device_join_callback(coordinator_adapter_t *self, void (*cb)(uint16_t node_id, const uint8_t *eui64, uint8_t status, void *user_data), void *user_data) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    ezsp_handler_set_device_join_callback(impl->ezsp, cb, user_data);
}

static void ezsp_adapter_destroy(coordinator_adapter_t *self) {
    ezsp_adapter_impl_t *impl = (ezsp_adapter_impl_t *)self->impl;
    ezsp_adapter_stop(self);
    if (impl->transport_owned && impl->transport) {
        impl->transport->destroy(impl->transport);
    }
    free(impl);
    free(self);
}

#include "gateway/transport/uart_transport.h"

coordinator_adapter_t *ezsp_adapter_create(const gateway_config_t *cfg, transport_t *transport) {
    coordinator_adapter_t *a = calloc(1, sizeof(coordinator_adapter_t));
    ezsp_adapter_impl_t *impl = calloc(1, sizeof(ezsp_adapter_impl_t));
    
    memcpy(&impl->cfg, cfg, sizeof(gateway_config_t));
    
    if (transport) {
        impl->transport = transport;
        impl->transport_owned = false;
    } else {
        impl->transport = uart_transport_create(&cfg->coordinator.transport);
        impl->transport_owned = true;
    }
    
    a->impl = impl;
    a->start = ezsp_adapter_start;
    a->stop = ezsp_adapter_stop;
    a->reset = ezsp_adapter_reset;
    a->get_info = ezsp_adapter_get_info;
    a->health_check = ezsp_adapter_health_check;
    a->form_network = ezsp_adapter_form_network;
    a->permit_join = ezsp_adapter_permit_join;
    a->send_unicast = ezsp_adapter_send_unicast;
    a->poll = ezsp_adapter_poll;
    a->set_zcl_callback = ezsp_adapter_set_zcl_callback;
    a->set_device_join_callback = ezsp_adapter_set_device_join_callback;
    a->is_connected = ezsp_adapter_is_connected;
    a->destroy = ezsp_adapter_destroy;
    
    return a;
}
