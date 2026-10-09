#include "gateway/adapter/znp_adapter.h"
#include "gateway/log.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif
#include "gateway/transport/uart_transport.h"

#define ZNP_SOF 0xFE

typedef struct {
    gateway_config_t cfg;
    transport_t *transport;
    coordinator_info_t info;
    bool connected;
    bool transport_owned;
    
    void (*zcl_cb)(uint16_t, uint16_t, uint16_t, uint8_t, uint8_t, const uint8_t*, size_t, void*);
    void *zcl_user;
    
    void (*join_cb)(uint16_t, const uint8_t*, uint8_t, void*);
    void *join_user;
} znp_adapter_impl_t;

static uint8_t calc_fcs(uint8_t len, uint8_t cmd0, uint8_t cmd1, const uint8_t *data) {
    uint8_t fcs = len ^ cmd0 ^ cmd1;
    for (int i=0; i<len; i++) fcs ^= data[i];
    return fcs;
}

static gw_err_t znp_send(znp_adapter_impl_t *impl, uint8_t cmd0, uint8_t cmd1, const uint8_t *data, uint8_t len) {
    uint8_t buf[256];
    buf[0] = ZNP_SOF;
    buf[1] = len;
    buf[2] = cmd0;
    buf[3] = cmd1;
    if (len > 0 && data) memcpy(&buf[4], data, len);
    buf[4+len] = calc_fcs(len, cmd0, cmd1, data);
    return impl->transport->write(impl->transport, buf, len + 5);
}

static gw_err_t znp_recv(znp_adapter_impl_t *impl, uint8_t *cmd0, uint8_t *cmd1, uint8_t *data, uint8_t *len_out, uint32_t timeout_ms) {
    uint8_t state = 0;
    uint8_t len = 0;
    uint8_t c0 = 0, c1 = 0;
    uint8_t data_idx = 0;
    uint8_t fcs = 0;
    
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        uint8_t b;
        size_t n;
        impl->transport->read(impl->transport, &b, 1, &n, 10);
        if (n == 0) {
            elapsed += 10;
            continue;
        }
        
        if (state == 0) {
            if (b == ZNP_SOF) state = 1;
        } else if (state == 1) {
            len = b;
            fcs ^= b;
            state = 2;
        } else if (state == 2) {
            c0 = b;
            fcs ^= b;
            state = 3;
        } else if (state == 3) {
            c1 = b;
            fcs ^= b;
            if (len == 0) state = 5;
            else state = 4;
        } else if (state == 4) {
            data[data_idx++] = b;
            fcs ^= b;
            if (data_idx == len) state = 5;
        } else if (state == 5) {
            if (b == fcs) {
                *cmd0 = c0;
                *cmd1 = c1;
                *len_out = len;
                return GW_OK;
            } else {
                LOG_WARN("znp", "FCS mismatch");
                state = 0;
            }
        }
    }
    return GW_ERR_TRANSPORT_TIMEOUT;
}

static void process_znp_frame(znp_adapter_impl_t *impl, uint8_t cmd0, uint8_t cmd1, const uint8_t *data, uint8_t len) {
    if (cmd0 == 0x45 && cmd1 == 0xC1) { // ZDO_END_DEVICE_ANNCE_IND
        if (len >= 13) {
            uint16_t nwk = data[1] | (data[2] << 8);
            const uint8_t *ieee = &data[3];
            LOG_INFO("znp", "Device joined (ZDO Annce)", LOG_INT("nwk", nwk));
            if (impl->join_cb) impl->join_cb(nwk, ieee, 0, impl->join_user);
        }
    } else if (cmd0 == 0x44 && cmd1 == 0x81) { // AF_INCOMING_MSG
        if (len >= 17) {
            uint16_t group = data[0] | (data[1] << 8);
            uint16_t cluster = data[2] | (data[3] << 8);
            uint16_t src_addr = data[4] | (data[5] << 8);
            uint8_t src_ep = data[6];
            uint8_t dst_ep = data[7];
            uint16_t payload_len = data[16];
            if (len >= 17 + payload_len) {
                const uint8_t *payload = &data[17];
                if (impl->zcl_cb) impl->zcl_cb(src_addr, 0x0104, cluster, src_ep, dst_ep, payload, payload_len, impl->zcl_user);
            }
        }
    }
}

static gw_err_t znp_adapter_start(coordinator_adapter_t *self) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    gw_err_t err = impl->transport->open(impl->transport);
    if (err != GW_OK) return err;
    
    LOG_INFO("znp", "Sending SYS_RESET_REQ");
    uint8_t rst_data[1] = {0x01};
    znp_send(impl, 0x41, 0x00, rst_data, 1);
    
    uint8_t c0, c1, d[256], len;
    err = znp_recv(impl, &c0, &c1, d, &len, 3000);
    if (err != GW_OK) {
        LOG_WARN("znp", "SYS_RESET_IND timeout");
        return GW_ERR_TRANSPORT_TIMEOUT;
    }
    
    LOG_INFO("znp", "Z-Stack started!");
    impl->connected = true;
    
    // Get info
    znp_send(impl, 0x27, 0x00, NULL, 0); // UTIL_GET_DEVICE_INFO
    err = znp_recv(impl, &c0, &c1, d, &len, 1000);
    if (err == GW_OK && c0 == 0x67 && c1 == 0x00 && len >= 14) {
        memcpy(impl->info.eui64, &d[1], 8);
        impl->info.node_id = d[9] | (d[10] << 8);
        LOG_INFO("znp", "Got device info");
    }
    
    // Start coordinator
    uint8_t startup[2] = {0x00, 0x00};
    znp_send(impl, 0x25, 0x40, startup, 2); // ZDO_STARTUP_FROM_APP
    
    return GW_OK;
}

static gw_err_t znp_adapter_stop(coordinator_adapter_t *self) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    if (impl->transport) impl->transport->close(impl->transport);
    impl->connected = false;
    return GW_OK;
}

static gw_err_t znp_adapter_reset(coordinator_adapter_t *self) {
    znp_adapter_stop(self);
    usleep(500 * 1000);
    return znp_adapter_start(self);
}

static gw_err_t znp_adapter_get_info(coordinator_adapter_t *self, coordinator_info_t *info) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    memcpy(info, &impl->info, sizeof(coordinator_info_t));
    return GW_OK;
}

static gw_err_t znp_adapter_health_check(coordinator_adapter_t *self) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    znp_send(impl, 0x21, 0x01, NULL, 0); // SYS_PING
    return GW_OK;
}

static gw_err_t znp_adapter_form_network(coordinator_adapter_t *self) {
    return GW_OK; // ZDO_STARTUP handles this
}

static gw_err_t znp_adapter_permit_join(coordinator_adapter_t *self, uint8_t duration_sec) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    uint8_t data[5] = {0x0F, 0xFC, 0xFF, duration_sec, 0x00}; // AddrMode 15, Dst 0xFFFC, Duration, TCSignificance
    return znp_send(impl, 0x25, 0x36, data, 5); // ZDO_MGMT_PERMIT_JOIN_REQ
}

static gw_err_t znp_adapter_send_unicast(coordinator_adapter_t *self, uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    uint8_t buf[256];
    buf[0] = node_id & 0xFF; buf[1] = (node_id >> 8) & 0xFF; // DstAddr
    buf[2] = dst_ep; buf[3] = src_ep;
    buf[4] = cluster_id & 0xFF; buf[5] = (cluster_id >> 8) & 0xFF;
    buf[6] = 0x00; // TransID
    buf[7] = 0x00; // Options
    buf[8] = 0x0F; // Radius
    buf[9] = payload_len;
    memcpy(&buf[10], payload, payload_len);
    return znp_send(impl, 0x24, 0x01, buf, 10 + payload_len); // AF_DATA_REQUEST
}

static gw_err_t znp_adapter_poll(coordinator_adapter_t *self) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    uint8_t c0, c1, d[256], len;
    while (znp_recv(impl, &c0, &c1, d, &len, 10) == GW_OK) {
        process_znp_frame(impl, c0, c1, d, len);
    }
    return GW_OK;
}

static void znp_adapter_set_zcl_callback(coordinator_adapter_t *self, void (*cb)(uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data), void *user_data) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    impl->zcl_cb = cb;
    impl->zcl_user = user_data;
}

static void znp_adapter_set_device_join_callback(coordinator_adapter_t *self, void (*cb)(uint16_t node_id, const uint8_t *eui64, uint8_t status, void *user_data), void *user_data) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    impl->join_cb = cb;
    impl->join_user = user_data;
}

static bool znp_adapter_is_connected(coordinator_adapter_t *self) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    return impl->connected;
}

static void znp_adapter_destroy(coordinator_adapter_t *self) {
    znp_adapter_impl_t *impl = (znp_adapter_impl_t *)self->impl;
    znp_adapter_stop(self);
    if (impl->transport_owned && impl->transport) {
        impl->transport->destroy(impl->transport);
    }
    free(impl);
    free(self);
}

coordinator_adapter_t *znp_adapter_create(const gateway_config_t *cfg, transport_t *transport) {
    coordinator_adapter_t *a = calloc(1, sizeof(coordinator_adapter_t));
    znp_adapter_impl_t *impl = calloc(1, sizeof(znp_adapter_impl_t));
    memcpy(&impl->cfg, cfg, sizeof(gateway_config_t));
    
    if (transport) {
        impl->transport = transport;
        impl->transport_owned = false;
    } else {
        impl->transport = uart_transport_create(&cfg->coordinator.transport);
        impl->transport_owned = true;
    }
    
    a->impl = impl;
    a->start = znp_adapter_start;
    a->stop = znp_adapter_stop;
    a->reset = znp_adapter_reset;
    a->get_info = znp_adapter_get_info;
    a->health_check = znp_adapter_health_check;
    a->form_network = znp_adapter_form_network;
    a->permit_join = znp_adapter_permit_join;
    a->send_unicast = znp_adapter_send_unicast;
    a->poll = znp_adapter_poll;
    a->set_zcl_callback = znp_adapter_set_zcl_callback;
    a->set_device_join_callback = znp_adapter_set_device_join_callback;
    a->is_connected = znp_adapter_is_connected;
    a->destroy = znp_adapter_destroy;
    return a;
}
