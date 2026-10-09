#include "gateway/converters.h"
#include "gateway/log.h"
#include "gateway/zcl.h"
#include <string.h>
#include <stdlib.h>

cJSON *converters_zcl_to_mqtt(uint16_t cluster_id, cJSON *zcl_parsed) {
    if (!zcl_parsed) return NULL;
    
    cJSON *mqtt_state = cJSON_CreateObject();
    bool has_data = false;
    
    cJSON *attrs = cJSON_GetObjectItem(zcl_parsed, "attributes");
    if (attrs) {
        // Basic Cluster (0x0000) -> Device Info
        if (cluster_id == ZCL_CLUSTER_BASIC) {
            cJSON *model = cJSON_GetObjectItem(attrs, "attr_0x0005");
            if (model && cJSON_IsString(model)) {
                cJSON_AddStringToObject(mqtt_state, "model_id", model->valuestring);
                has_data = true;
            }
        }
        // On/Off Cluster (0x0006) -> State
        else if (cluster_id == ZCL_CLUSTER_ON_OFF) {
            cJSON *onoff = cJSON_GetObjectItem(attrs, "attr_0x0000");
            if (onoff) {
                cJSON_AddStringToObject(mqtt_state, "state", onoff->valueint == 1 ? "ON" : "OFF");
                has_data = true;
            }
        }
    }
    
    if (!has_data) {
        cJSON_Delete(mqtt_state);
        return NULL;
    }
    
    return mqtt_state;
}

uint8_t *converters_mqtt_to_zcl(const char *command_json, uint16_t *out_cluster, size_t *out_len) {
    cJSON *json = cJSON_Parse(command_json);
    if (!json) return NULL;
    
    uint8_t *payload = NULL;
    *out_len = 0;
    
    cJSON *state = cJSON_GetObjectItem(json, "state");
    if (state && cJSON_IsString(state)) {
        uint8_t zcl_cmd;
        if (strcmp(state->valuestring, "ON") == 0) zcl_cmd = ZCL_CMD_ON_OFF_ON;
        else if (strcmp(state->valuestring, "OFF") == 0) zcl_cmd = ZCL_CMD_ON_OFF_OFF;
        else if (strcmp(state->valuestring, "TOGGLE") == 0) zcl_cmd = ZCL_CMD_ON_OFF_TOGGLE;
        else {
            cJSON_Delete(json);
            return NULL;
        }
        
        *out_cluster = ZCL_CLUSTER_ON_OFF;
        *out_len = 3;
        payload = malloc(3);
        payload[0] = 0x01; // Frame Ctrl: Cluster Spec, Client to Srv
        payload[1] = 0x00; // Seq Num (dummy)
        payload[2] = zcl_cmd;
    }
    
    cJSON_Delete(json);
    return payload;
}
