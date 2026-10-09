#ifndef GATEWAY_CONVERTERS_H
#define GATEWAY_CONVERTERS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cJSON.h"

/**
 * @brief zigbee-herdsman-converters equivalent for DOM_CONNECT
 * 
 * This module abstracts the translation between raw Zigbee Cluster Library (ZCL) 
 * payloads and human-readable semantic JSON objects used by MQTT.
 */

// Convert parsed ZCL data into semantic MQTT state (e.g., {"state": "ON"})
cJSON *converters_zcl_to_mqtt(uint16_t cluster_id, cJSON *zcl_parsed);

// Convert semantic MQTT command (e.g., {"state": "TOGGLE"}) to ZCL binary payload
// Caller must free() the returned pointer.
uint8_t *converters_mqtt_to_zcl(const char *command_json, uint16_t *out_cluster, size_t *out_len);

#endif // GATEWAY_CONVERTERS_H
