#include "gateway/ezsp/ezsp_protocol.h"

size_t ezsp_build_version_cmd(uint8_t seq, uint8_t desired_ver, uint8_t *buf, size_t max) {
    if (max < 4) return 0;
    buf[0] = seq;
    buf[1] = 0x00; // frame control (legacy)
    buf[2] = 0x00; // frame id
    buf[3] = desired_ver;
    return 4;
}

size_t ezsp_build_command(uint8_t seq, uint16_t frame_id, const uint8_t *params, size_t params_len, uint8_t *buf, size_t max) {
    if (max < 5 + params_len) return 0;
    buf[0] = seq;
    buf[1] = 0x00; // control low
    buf[2] = 0x00; // control high
    buf[3] = frame_id & 0xFF;
    buf[4] = (frame_id >> 8) & 0xFF;
    for (size_t i = 0; i < params_len; i++) {
        buf[5 + i] = params[i];
    }
    return 5 + params_len;
}

gw_err_t ezsp_parse_version_response(const uint8_t *data, size_t len, uint8_t *proto_ver, uint8_t *stack_type, uint16_t *stack_ver) {
    if (len < 7) return GW_ERR_EZSP_COMMUNICATION;
    *proto_ver = data[3];
    *stack_type = data[4];
    *stack_ver = (data[5] << 8) | data[6];
    return GW_OK;
}

gw_err_t ezsp_parse_response_header(const uint8_t *data, size_t len, uint8_t *seq, uint16_t *frame_id, const uint8_t **params, size_t *params_len) {
    if (len < 5) return GW_ERR_EZSP_COMMUNICATION;
    *seq = data[0];
    *frame_id = data[3] | (data[4] << 8);
    *params = data + 5;
    *params_len = len - 5;
    return GW_OK;
}
