#ifndef GATEWAY_EZSP_PROTOCOL_H
#define GATEWAY_EZSP_PROTOCOL_H

#include "gateway/types.h"
#include "gateway/error.h"

#define EZSP_CMD_VERSION        0x0000
#define EZSP_CMD_ADD_ENDPOINT   0x0002
#define EZSP_CMD_NOP            0x0005
#define EZSP_CMD_NETWORK_STATE  0x0018
#define EZSP_CMD_TRUST_CENTER_JOIN_HANDLER 0x0024
#define EZSP_CMD_GET_EUI64      0x0026
#define EZSP_CMD_GET_NODE_ID    0x0027
#define EZSP_CMD_FORM_NETWORK   0x001E
#define EZSP_CMD_SET_INITIAL_SECURITY_STATE 0x0068
#define EZSP_CMD_SET_PERMIT_JOINING 0x0022
#define EZSP_CMD_SEND_UNICAST   0x0034
#define EZSP_CMD_INCOMING_MESSAGE_HANDLER 0x0045

size_t ezsp_build_version_cmd(uint8_t seq, uint8_t desired_ver, uint8_t *buf, size_t max);
size_t ezsp_build_command(uint8_t seq, uint16_t frame_id, const uint8_t *params, size_t params_len, uint8_t *buf, size_t max);

gw_err_t ezsp_parse_version_response(const uint8_t *data, size_t len, uint8_t *proto_ver, uint8_t *stack_type, uint16_t *stack_ver);
gw_err_t ezsp_parse_response_header(const uint8_t *data, size_t len, uint8_t *seq, uint16_t *frame_id, const uint8_t **params, size_t *params_len);

#endif // GATEWAY_EZSP_PROTOCOL_H
