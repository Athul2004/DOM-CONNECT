#include "gateway/ezsp/ezsp_handler.h"
#include "gateway/ezsp/ezsp_protocol.h"
#include "gateway/log.h"
#include <stdlib.h>
#include <string.h>

struct ezsp_handler {
    ash_context_t *ash;
    ezsp_config_t cfg;
    uint8_t seq;
    ezsp_zcl_callback_t zcl_cb;
    void *zcl_user_data;
    ezsp_device_join_callback_t join_cb;
    void *join_user_data;
};

ezsp_handler_t *ezsp_handler_create(ash_context_t *ash, const ezsp_config_t *cfg) {
    ezsp_handler_t *h = calloc(1, sizeof(ezsp_handler_t));
    h->ash = ash;
    memcpy(&h->cfg, cfg, sizeof(ezsp_config_t));
    h->seq = 0;
    return h;
}

void ezsp_handler_set_zcl_callback(ezsp_handler_t *h, ezsp_zcl_callback_t cb, void *user_data) {
    h->zcl_cb = cb;
    h->zcl_user_data = user_data;
}

void ezsp_handler_set_device_join_callback(ezsp_handler_t *h, ezsp_device_join_callback_t cb, void *user_data) {
    h->join_cb = cb;
    h->join_user_data = user_data;
}

void ezsp_handler_destroy(ezsp_handler_t *h) {
    free(h);
}

gw_err_t ezsp_negotiate_version(ezsp_handler_t *h, uint8_t desired_version, coordinator_info_t *info) {
    uint8_t cmd[4];
    size_t cmd_len = ezsp_build_version_cmd(h->seq++, desired_version, cmd, sizeof(cmd));
    
    uint8_t rsp[128];
    size_t rsp_len = 0;
    
    gw_err_t err = ash_send_and_receive(h->ash, cmd, cmd_len, rsp, sizeof(rsp), &rsp_len, (uint32_t)(h->cfg.command_timeout * 1000));
    if (err != GW_OK) return err;
    
    err = ezsp_parse_version_response(rsp, rsp_len, &info->ezsp_protocol_version, &info->stack_type, &info->stack_version);
    if (err == GW_OK) {
        LOG_INFO("coordinator.ezsp", "EZSP version negotiated",
                 LOG_INT("protocol_version", info->ezsp_protocol_version),
                 LOG_INT("stack_type", info->stack_type),
                 LOG_HEX("stack_version", (const uint8_t*)&info->stack_version, 2));
    }
    return err;
}

static void process_async_frame(ezsp_handler_t *h, uint16_t frame_id, const uint8_t *params, size_t params_len) {
    if (frame_id == EZSP_CMD_TRUST_CENTER_JOIN_HANDLER) {
        if (params_len >= 11) { // node_id(2) + eui64(8) + status(1)
            uint16_t node_id = params[0] | (params[1] << 8);
            const uint8_t *eui64 = &params[2];
            uint8_t status = params[10];
            LOG_INFO("coordinator.ezsp", "Device joined network",
                     LOG_HEX("eui64", eui64, 8),
                     LOG_INT("node_id", node_id),
                     LOG_INT("status", status));
            if (h->join_cb) {
                h->join_cb(node_id, eui64, status, h->join_user_data);
            }
        }
    } else if (frame_id == EZSP_CMD_INCOMING_MESSAGE_HANDLER) {
        if (params_len >= 19) {
            uint16_t profile_id = params[1] | (params[2] << 8);
            uint16_t cluster_id = params[3] | (params[4] << 8);
            uint8_t src_ep = params[5];
            uint8_t dst_ep = params[6];
            uint16_t sender = params[14] | (params[15] << 8);
            uint8_t msg_len = params[18];
            const uint8_t *msg = &params[19];
            
            if (profile_id == 0x0000) {
                // ZDO Message
                LOG_INFO("zdo", "Received ZDO message", LOG_INT("cluster", cluster_id), LOG_HEX("payload", msg, msg_len));
                // We could add a zdo_cb here, but for now we just log it as part of the interview process.
                if (cluster_id == 0x8005) { // Active_EP_rsp
                    LOG_INFO("zdo", "Active_EP_rsp received. Endpoints discovered!");
                }
            } else {
                // ZCL Message
                LOG_INFO("zcl", "Incoming message", 
                         LOG_INT("sender", sender),
                         LOG_INT("profile", profile_id),
                         LOG_INT("cluster", cluster_id),
                         LOG_INT("src_ep", src_ep),
                         LOG_INT("dst_ep", dst_ep),
                         LOG_HEX("payload", msg, msg_len));
                         
                if (h->zcl_cb) {
                    h->zcl_cb(sender, profile_id, cluster_id, src_ep, dst_ep, msg, msg_len, h->zcl_user_data);
                }
            }
        }
    } else {
        LOG_DEBUG("coordinator.ezsp", "Received async frame", LOG_INT("frame_id", frame_id));
    }
}

static gw_err_t do_command(ezsp_handler_t *h, uint16_t frame_id, const uint8_t *params, size_t params_len, const uint8_t **out_params, size_t *out_params_len) {
    static uint8_t rsp_buf[256];
    uint8_t cmd_buf[128];
    uint8_t current_seq = h->seq++;
    size_t cmd_len = ezsp_build_command(current_seq, frame_id, params, params_len, cmd_buf, sizeof(cmd_buf));
    
    gw_err_t err = ash_send_data(h->ash, cmd_buf, cmd_len);
    if (err != GW_OK) return err;
    
    uint32_t remaining_ms = (uint32_t)(h->cfg.command_timeout * 1000);
    while (remaining_ms > 0) {
        size_t rsp_len = 0;
        err = ash_receive_data(h->ash, rsp_buf, sizeof(rsp_buf), &rsp_len, 50);
        
        if (err == GW_OK && rsp_len > 0) {
            uint8_t rx_seq;
            uint16_t rx_frame_id;
            err = ezsp_parse_response_header(rsp_buf, rsp_len, &rx_seq, &rx_frame_id, out_params, out_params_len);
            if (err == GW_OK) {
                if (rx_seq == current_seq) {
                    return GW_OK; // Got the response
                } else {
                    process_async_frame(h, rx_frame_id, *out_params, *out_params_len);
                }
            }
        } else if (err != GW_ERR_TRANSPORT_TIMEOUT && err != GW_OK) {
            return err;
        }
        
        if (remaining_ms >= 50) remaining_ms -= 50; else remaining_ms = 0;
    }
    
    return GW_ERR_EZSP_TIMEOUT;
}

gw_err_t ezsp_get_eui64(ezsp_handler_t *h, uint8_t eui64[8]) {
    const uint8_t *params;
    size_t params_len;
    gw_err_t err = do_command(h, EZSP_CMD_GET_EUI64, NULL, 0, &params, &params_len);
    if (err == GW_OK && params_len >= 8) {
        memcpy(eui64, params, 8);
    }
    return err;
}

gw_err_t ezsp_get_node_id(ezsp_handler_t *h, uint16_t *node_id) {
    const uint8_t *params;
    size_t params_len;
    gw_err_t err = do_command(h, EZSP_CMD_GET_NODE_ID, NULL, 0, &params, &params_len);
    if (err == GW_OK && params_len >= 2) {
        *node_id = params[0] | (params[1] << 8);
    }
    return err;
}

gw_err_t ezsp_get_network_state(ezsp_handler_t *h, uint8_t *state) {
    const uint8_t *params;
    size_t params_len;
    gw_err_t err = do_command(h, EZSP_CMD_NETWORK_STATE, NULL, 0, &params, &params_len);
    if (err == GW_OK && params_len >= 1) {
        *state = params[0];
    }
    return err;
}

gw_err_t ezsp_form_network(ezsp_handler_t *h) {
    uint8_t sec_params[43] = {0};
    // EMBER_HAVE_PRECONFIGURED_KEY (0x01) | EMBER_TRUST_CENTER_GLOBAL_LINK_KEY (0x04) = 0x0005
    sec_params[0] = 0x05;
    sec_params[1] = 0x00;
    // Preconfigured Key: ZigBeeAlliance09
    const uint8_t tc_link_key[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};
    memcpy(&sec_params[2], tc_link_key, 16);
    
    const uint8_t *out;
    size_t out_len;
    gw_err_t err = do_command(h, EZSP_CMD_SET_INITIAL_SECURITY_STATE, sec_params, sizeof(sec_params), &out, &out_len);
    if (err != GW_OK || out_len == 0 || out[0] != 0x00) {
        LOG_WARN("coordinator.ezsp", "Failed to set security state");
        return GW_ERR_EZSP_COMMAND;
    }
    
    uint8_t nwk_params[20] = {0};
    nwk_params[8] = 0xFF; // panId
    nwk_params[9] = 0xFF;
    nwk_params[10] = 8;   // radioTxPower
    nwk_params[11] = 11;  // radioChannel (11)
    nwk_params[16] = 0x00; // channels (little-endian 1<<11 = 0x0800)
    nwk_params[17] = 0x08;
    nwk_params[18] = 0x00;
    nwk_params[19] = 0x00;
    
    err = do_command(h, EZSP_CMD_FORM_NETWORK, nwk_params, sizeof(nwk_params), &out, &out_len);
    if (err != GW_OK || out_len == 0 || out[0] != 0x00) {
        LOG_WARN("coordinator.ezsp", "Failed to form network");
        return GW_ERR_EZSP_COMMAND;
    }
    
    LOG_INFO("coordinator.ezsp", "Network formed successfully");
    return GW_OK;
}

gw_err_t ezsp_nop(ezsp_handler_t *h) {
    const uint8_t *params;
    size_t params_len;
    return do_command(h, EZSP_CMD_NOP, NULL, 0, &params, &params_len);
}

gw_err_t ezsp_set_permit_joining(ezsp_handler_t *h, uint8_t duration_sec) {
    const uint8_t *params_out;
    size_t params_out_len;
    gw_err_t err = do_command(h, EZSP_CMD_SET_PERMIT_JOINING, &duration_sec, 1, &params_out, &params_out_len);
    if (err == GW_OK) {
        if (params_out_len >= 1 && params_out[0] == 0x00) { // EMBER_SUCCESS is 0x00
            LOG_INFO("coordinator.ezsp", "Permit joining updated", LOG_INT("duration", duration_sec));
            return GW_OK;
        } else {
            LOG_WARN("coordinator.ezsp", "Permit joining failed", LOG_INT("status", params_out_len > 0 ? params_out[0] : -1));
            return GW_ERR_EZSP_COMMAND;
        }
    }
    return err;
}

gw_err_t ezsp_add_endpoint(ezsp_handler_t *h) {
    uint8_t params_in[8];
    params_in[0] = 1; // endpoint
    params_in[1] = 0x04; // profile ID low (0x0104 HA)
    params_in[2] = 0x01; // profile ID high
    params_in[3] = 0x05; // device ID low
    params_in[4] = 0x00; // device ID high
    params_in[5] = 0;    // app flags
    params_in[6] = 0;    // in cluster count
    params_in[7] = 0;    // out cluster count
    
    const uint8_t *params_out;
    size_t params_out_len;
    
    gw_err_t err = do_command(h, EZSP_CMD_ADD_ENDPOINT, params_in, sizeof(params_in), &params_out, &params_out_len);
    if (err == GW_OK) {
        if (params_out_len > 0 && params_out[0] == 0x00) {
            LOG_INFO("coordinator.ezsp", "Endpoint 1 added");
        } else {
            LOG_WARN("coordinator.ezsp", "Failed to add endpoint", LOG_INT("status", params_out_len > 0 ? params_out[0] : -1));
        }
    }
    return err;
}

gw_err_t ezsp_send_unicast(ezsp_handler_t *h, uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len) {
    if (payload_len > 128) return GW_ERR_INVALID_ARG;
    
    uint8_t params_in[256];
    params_in[0] = 0x00; // EMBER_OUTGOING_DIRECT
    params_in[1] = node_id & 0xFF;
    params_in[2] = (node_id >> 8) & 0xFF;
    
    // APS Frame
    params_in[3] = profile_id & 0xFF;
    params_in[4] = (profile_id >> 8) & 0xFF;
    params_in[5] = cluster_id & 0xFF;
    params_in[6] = (cluster_id >> 8) & 0xFF;
    params_in[7] = src_ep;
    params_in[8] = dst_ep;
    params_in[9] = 0x40; // options low (0x0140 = retry + enable route disc)
    params_in[10] = 0x01; // options high
    params_in[11] = 0x00; // group ID low
    params_in[12] = 0x00; // group ID high
    params_in[13] = 0x00; // sequence
    
    params_in[14] = 0x00; // message tag
    params_in[15] = (uint8_t)payload_len; // message length
    
    if (payload_len > 0) {
        memcpy(&params_in[16], payload, payload_len);
    }
    
    const uint8_t *params_out;
    size_t params_out_len;
    
    gw_err_t err = do_command(h, EZSP_CMD_SEND_UNICAST, params_in, 16 + payload_len, &params_out, &params_out_len);
    if (err == GW_OK) {
        if (params_out_len >= 1 && params_out[0] == 0x00) { // EMBER_SUCCESS
            LOG_INFO("coordinator.ezsp", "Send unicast successful", LOG_INT("seq", params_out_len > 1 ? params_out[1] : -1));
        } else {
            LOG_WARN("coordinator.ezsp", "Send unicast failed", LOG_INT("status", params_out_len > 0 ? params_out[0] : -1));
            return GW_ERR_EZSP_COMMAND;
        }
    }
    return err;
}

gw_err_t ezsp_poll(ezsp_handler_t *h) {
    uint8_t buf[256];
    size_t len = 0;
    
    gw_err_t err = ash_receive_data(h->ash, buf, sizeof(buf), &len, 10);
    if (err == GW_ERR_TRANSPORT_TIMEOUT) {
        return GW_OK; // No data
    }
    if (err != GW_OK) return err;
    
    if (len > 0) {
        uint8_t seq;
        uint16_t frame_id;
        const uint8_t *params;
        size_t params_len;
        
        err = ezsp_parse_response_header(buf, len, &seq, &frame_id, &params, &params_len);
        if (err == GW_OK) {
            process_async_frame(h, frame_id, params, params_len);
        }
    }
    
    return GW_OK;
}
