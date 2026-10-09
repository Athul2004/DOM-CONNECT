#include "gateway/zcl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void parse_attribute_value(cJSON *obj, uint16_t attr_id, uint8_t type, const uint8_t *data, size_t data_len) {
    char key[32];
    snprintf(key, sizeof(key), "attr_0x%04X", attr_id);
    
    switch (type) {
        case ZCL_TYPE_BOOLEAN:
            if (data_len >= 1) cJSON_AddBoolToObject(obj, key, data[0]);
            break;
        case ZCL_TYPE_UINT8:
        case ZCL_TYPE_ENUM8:
        case ZCL_TYPE_BITMAP8:
            if (data_len >= 1) cJSON_AddNumberToObject(obj, key, data[0]);
            break;
        case ZCL_TYPE_UINT16:
            if (data_len >= 2) cJSON_AddNumberToObject(obj, key, data[0] | (data[1] << 8));
            break;
        case ZCL_TYPE_INT8:
            if (data_len >= 1) cJSON_AddNumberToObject(obj, key, (int8_t)data[0]);
            break;
        case ZCL_TYPE_INT16:
            if (data_len >= 2) {
                int16_t val = (int16_t)(data[0] | (data[1] << 8));
                cJSON_AddNumberToObject(obj, key, val);
            }
            break;
        case ZCL_TYPE_CHAR_STR:
            if (data_len >= 1) {
                uint8_t len = data[0];
                if (data_len >= (size_t)(1 + len)) {
                    char *str = malloc(len + 1);
                    memcpy(str, &data[1], len);
                    str[len] = '\0';
                    cJSON_AddStringToObject(obj, key, str);
                    free(str);
                }
            }
            break;
        default:
            // For unknown types, just store type as a hint
            cJSON_AddNumberToObject(obj, key, type);
            break;
    }
}

cJSON *zcl_parse_payload(uint16_t cluster_id, const uint8_t *payload, size_t payload_len) {
    if (payload_len < 3) return NULL; // Min: Frame Ctrl, Seq, Cmd
    
    uint8_t frame_ctrl = payload[0];
    bool is_mfg_specific = (frame_ctrl & 0x04) != 0;
    bool is_cluster_specific = (frame_ctrl & 0x01) != 0;
    
    size_t header_len = is_mfg_specific ? 5 : 3;
    if (payload_len < header_len) return NULL;
    
    uint8_t seq = is_mfg_specific ? payload[3] : payload[1];
    uint8_t cmd = is_mfg_specific ? payload[4] : payload[2];
    
    const uint8_t *data = payload + header_len;
    size_t data_len = payload_len - header_len;
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "zcl_seq", seq);
    cJSON_AddNumberToObject(result, "zcl_cmd", cmd);
    
    // We only parse Global Report Attributes (0x0A) and Read Attributes Response (0x01)
    if (!is_cluster_specific) {
        if (cmd == ZCL_CMD_REPORT_ATTRIBUTES || cmd == ZCL_CMD_READ_ATTRIBUTES_RESPONSE) {
            cJSON *attrs = cJSON_CreateObject();
            size_t offset = 0;
            while (offset + 2 < data_len) { // At least attr_id
                uint16_t attr_id = data[offset] | (data[offset+1] << 8);
                offset += 2;
                
                if (cmd == ZCL_CMD_READ_ATTRIBUTES_RESPONSE) {
                    if (offset >= data_len) break;
                    uint8_t status = data[offset++];
                    if (status != 0x00) continue; // Skip failed reads
                }
                
                if (offset >= data_len) break;
                uint8_t type = data[offset++];
                
                // Determine length based on type
                size_t val_len = 0;
                switch (type) {
                    case ZCL_TYPE_BOOLEAN:
                    case ZCL_TYPE_UINT8:
                    case ZCL_TYPE_INT8:
                    case ZCL_TYPE_ENUM8:
                    case ZCL_TYPE_BITMAP8:
                        val_len = 1; break;
                    case ZCL_TYPE_UINT16:
                    case ZCL_TYPE_INT16:
                        val_len = 2; break;
                    case ZCL_TYPE_CHAR_STR:
                        if (offset < data_len) val_len = 1 + data[offset];
                        break;
                    default:
                        val_len = 1; // Fallback, not robust for all types
                        break;
                }
                
                if (offset + val_len <= data_len) {
                    parse_attribute_value(attrs, attr_id, type, &data[offset], val_len);
                }
                offset += val_len;
            }
            cJSON_AddItemToObject(result, "attributes", attrs);
        }
    } else {
        // Cluster specific commands (e.g., On/Off Toggle = 0x02)
        // Just report the command ID
        cJSON_AddStringToObject(result, "type", "cluster_command");
    }
    
    return result;
}
