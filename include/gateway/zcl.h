#ifndef GATEWAY_ZCL_H
#define GATEWAY_ZCL_H

#include <stdint.h>
#include <stddef.h>
#include "cJSON.h"

// ZCL Global Commands
#define ZCL_CMD_READ_ATTRIBUTES               0x00
#define ZCL_CMD_READ_ATTRIBUTES_RESPONSE      0x01
#define ZCL_CMD_WRITE_ATTRIBUTES              0x02
#define ZCL_CMD_WRITE_ATTRIBUTES_RESPONSE     0x04
#define ZCL_CMD_REPORT_ATTRIBUTES             0x0A
#define ZCL_CMD_DEFAULT_RESPONSE              0x0B
#define ZCL_CMD_DISCOVER_ATTRIBUTES           0x0C

// Common ZCL Data Types
#define ZCL_TYPE_BOOLEAN                      0x10
#define ZCL_TYPE_BITMAP8                      0x18
#define ZCL_TYPE_UINT8                        0x20
#define ZCL_TYPE_UINT16                       0x21
#define ZCL_TYPE_INT8                         0x28
#define ZCL_TYPE_INT16                        0x29
#define ZCL_TYPE_ENUM8                        0x30
#define ZCL_TYPE_CHAR_STR                     0x42

// Common Cluster IDs
#define ZCL_CLUSTER_BASIC                     0x0000
#define ZCL_CLUSTER_ON_OFF                    0x0006
#define ZCL_CLUSTER_LEVEL_CONTROL             0x0008
#define ZCL_CLUSTER_TEMPERATURE_MEASUREMENT   0x0402
#define ZCL_CLUSTER_ILLUMINANCE_MEASUREMENT   0x0400

// On/Off Cluster Commands
#define ZCL_CMD_ON_OFF_OFF                    0x00
#define ZCL_CMD_ON_OFF_ON                     0x01
#define ZCL_CMD_ON_OFF_TOGGLE                 0x02

// Parse a ZCL payload into a JSON object.
// Returns NULL if parsing fails or memory cannot be allocated.
cJSON *zcl_parse_payload(uint16_t cluster_id, const uint8_t *payload, size_t payload_len);

#endif // GATEWAY_ZCL_H
