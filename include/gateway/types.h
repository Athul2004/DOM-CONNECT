#ifndef GATEWAY_TYPES_H
#define GATEWAY_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t  gw_byte_t;
typedef uint16_t gw_node_id_t;
typedef uint64_t gw_eui64_t;
typedef int      gw_err_t;

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   capacity;
} gw_buffer_t;

typedef struct {
    uint8_t  ezsp_protocol_version;
    uint8_t  stack_type;
    uint16_t stack_version;
    uint8_t  eui64[8];
    uint16_t node_id;
    uint8_t  network_state;
} coordinator_info_t;

#endif // GATEWAY_TYPES_H
