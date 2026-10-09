#include "gateway/ezsp/ash.h"
#include "gateway/log.h"
#include <stdlib.h>
#include <string.h>

#define ASH_FLAG 0x7E
#define ASH_ESC 0x7D
#define ASH_XON 0x11
#define ASH_XOFF 0x13
#define ASH_SUB 0x18
#define ASH_CAN 0x1A

#define ASH_CTRL_RST 0xC0
#define ASH_CTRL_RSTACK 0xC1
#define ASH_CTRL_ERROR 0xC2

struct ash_context {
    transport_t *transport;
    ash_config_t cfg;
    bool connected;
    uint8_t tx_frm_num;
    uint8_t rx_ack_num;
    uint8_t rx_buf[512];
    size_t rx_buf_len;
};

// CRC-CCITT
static uint16_t crc_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

// LFSR Randomization
static void ash_randomize(const uint8_t *in, size_t len, uint8_t *out) {
    uint8_t lfsr = 0x42;
    for (size_t i = 0; i < len; i++) {
        out[i] = in[i] ^ lfsr;
        uint8_t bit = (lfsr & 1) ? 1 : 0;
        lfsr = (lfsr >> 1) | (bit << 7);
        if (bit) lfsr ^= 0xB8;
    }
}

// Byte Stuffing
static bool is_reserved(uint8_t b) {
    return (b == ASH_FLAG || b == ASH_ESC || b == ASH_XON || 
            b == ASH_XOFF || b == ASH_SUB || b == ASH_CAN);
}

static size_t ash_stuff(const uint8_t *in, size_t in_len, uint8_t *out) {
    size_t out_len = 0;
    for (size_t i = 0; i < in_len; i++) {
        if (is_reserved(in[i])) {
            out[out_len++] = ASH_ESC;
            out[out_len++] = in[i] ^ 0x20;
        } else {
            out[out_len++] = in[i];
        }
    }
    return out_len;
}

static size_t ash_unstuff(const uint8_t *in, size_t in_len, uint8_t *out) {
    size_t out_len = 0;
    bool escape = false;
    for (size_t i = 0; i < in_len; i++) {
        if (in[i] == ASH_ESC) {
            escape = true;
        } else {
            if (escape) {
                out[out_len++] = in[i] ^ 0x20;
                escape = false;
            } else {
                out[out_len++] = in[i];
            }
        }
    }
    return out_len;
}

ash_context_t *ash_create(transport_t *transport, const ash_config_t *cfg) {
    ash_context_t *ctx = calloc(1, sizeof(ash_context_t));
    ctx->transport = transport;
    memcpy(&ctx->cfg, cfg, sizeof(ash_config_t));
    return ctx;
}

void ash_destroy(ash_context_t *ctx) {
    free(ctx);
}

bool ash_is_connected(ash_context_t *ctx) {
    return ctx->connected;
}

static gw_err_t send_raw(ash_context_t *ctx, const uint8_t *data, size_t len) {
    uint8_t *stuffed = malloc(len * 2 + 1);
    size_t stuffed_len = ash_stuff(data, len, stuffed);
    stuffed[stuffed_len++] = ASH_FLAG; // Append flag byte
    
    gw_err_t err = ctx->transport->write(ctx->transport, stuffed, stuffed_len);
    free(stuffed);
    return err;
}

gw_err_t ash_reset(ash_context_t *ctx) {
    ctx->connected = false;
    ctx->tx_frm_num = 0;
    ctx->rx_ack_num = 0;
    
    uint8_t rst_frame[3];
    rst_frame[0] = ASH_CTRL_RST;
    uint16_t crc = crc_ccitt(rst_frame, 1);
    rst_frame[1] = (crc >> 8) & 0xFF;
    rst_frame[2] = crc & 0xFF;
    
    LOG_INFO("coordinator.ash", "Sending RST to NCP");
    
    gw_err_t err = send_raw(ctx, rst_frame, 3);
    if (err != GW_OK) return err;
    
    uint8_t raw_rx[64];
    size_t out_len;
    err = ctx->transport->read(ctx->transport, raw_rx, sizeof(raw_rx), &out_len, (uint32_t)(ctx->cfg.reset_timeout * 1000));
    
    if (err != GW_OK || out_len == 0) return GW_ERR_ASH_RESET_TIMEOUT;
    
    size_t flag_idx = 0;
    while (flag_idx < out_len && raw_rx[flag_idx] != ASH_FLAG) flag_idx++;
    if (flag_idx == out_len) return GW_ERR_ASH_PROTOCOL;
    
    uint8_t unstuffed[64];
    size_t ulen = ash_unstuff(raw_rx, flag_idx, unstuffed);
    if (ulen < 3) return GW_ERR_ASH_PROTOCOL;
    
    if (unstuffed[0] == ASH_CTRL_RSTACK) {
        LOG_INFO("coordinator.ash", "RSTACK received",
                 LOG_INT("ash_version", unstuffed[1]),
                 LOG_INT("reset_code", unstuffed[2]));
        ctx->connected = true;
        return GW_OK;
    }
    
    return GW_ERR_ASH_PROTOCOL;
}

static gw_err_t ash_send_ack(ash_context_t *ctx) {
    uint8_t ack_frame[3];
    ack_frame[0] = 0x80 | (ctx->rx_ack_num & 0x07);
    uint16_t crc = crc_ccitt(ack_frame, 1);
    ack_frame[1] = (crc >> 8) & 0xFF;
    ack_frame[2] = crc & 0xFF;
    return send_raw(ctx, ack_frame, 3);
}

gw_err_t ash_send_data(ash_context_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx->connected) return GW_ERR_COORD_NOT_CONNECTED;
    
    uint8_t *frame = malloc(len + 3);
    frame[0] = ((ctx->tx_frm_num & 0x07) << 4) | (ctx->rx_ack_num & 0x07);
    ash_randomize(data, len, frame + 1);
    uint16_t crc = crc_ccitt(frame, len + 1);
    frame[len + 1] = (crc >> 8) & 0xFF;
    frame[len + 2] = crc & 0xFF;
    
    gw_err_t err = send_raw(ctx, frame, len + 3);
    free(frame);
    
    if (err == GW_OK) {
        ctx->tx_frm_num = (ctx->tx_frm_num + 1) % 8;
    }
    return err;
}

gw_err_t ash_receive_data(ash_context_t *ctx, uint8_t *buf, size_t max_len, size_t *out_len, uint32_t timeout_ms) {
    *out_len = 0;
    if (!ctx->connected) return GW_ERR_COORD_NOT_CONNECTED;
    
    uint8_t raw[256];
    size_t read_len;
    gw_err_t err = ctx->transport->read(ctx->transport, raw, sizeof(raw), &read_len, timeout_ms);
    
    if (err != GW_OK || read_len == 0) return GW_ERR_TRANSPORT_TIMEOUT;
    
    size_t flag_idx = 0;
    while (flag_idx < read_len && raw[flag_idx] != ASH_FLAG) flag_idx++;
    if (flag_idx == read_len) return GW_ERR_ASH_PROTOCOL;
    
    uint8_t unstuffed[256];
    size_t ulen = ash_unstuff(raw, flag_idx, unstuffed);
    
    if (ulen < 3) return GW_ERR_ASH_PROTOCOL;
    
    uint8_t ctrl = unstuffed[0];
    if ((ctrl & 0x80) == 0) {
        ctx->rx_ack_num = ((ctrl >> 4) & 0x07) + 1;
        size_t payload_len = ulen - 3;
        if (payload_len > max_len) return GW_ERR_NOMEM;
        
        ash_randomize(unstuffed + 1, payload_len, buf);
        *out_len = payload_len;
        
        ash_send_ack(ctx);
        return GW_OK;
    } else if ((ctrl & 0xE0) == 0x80) {
        return GW_OK;
    }
    
    return GW_ERR_ASH_PROTOCOL;
}

gw_err_t ash_send_and_receive(ash_context_t *ctx, const uint8_t *cmd, size_t cmd_len, uint8_t *rsp, size_t rsp_max, size_t *rsp_len, uint32_t timeout_ms) {
    gw_err_t err = ash_send_data(ctx, cmd, cmd_len);
    if (err != GW_OK) return err;
    
    for (int i = 0; i < 2; i++) {
        err = ash_receive_data(ctx, rsp, rsp_max, rsp_len, timeout_ms);
        if (err == GW_OK && *rsp_len > 0) return GW_OK;
        if (err != GW_OK) return err;
    }
    return GW_ERR_EZSP_TIMEOUT;
}
