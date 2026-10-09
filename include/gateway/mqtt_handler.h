#ifndef GATEWAY_MQTT_HANDLER_H
#define GATEWAY_MQTT_HANDLER_H

#include "gateway/config.h"
#include "gateway/error.h"

typedef struct mqtt_handler mqtt_handler_t;

typedef void (*mqtt_command_callback_t)(uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data);

mqtt_handler_t *mqtt_handler_create(const mqtt_config_t *cfg);
void mqtt_handler_set_command_callback(mqtt_handler_t *h, mqtt_command_callback_t cb, void *user_data);
gw_err_t mqtt_handler_start(mqtt_handler_t *h);
gw_err_t mqtt_handler_stop(mqtt_handler_t *h);
gw_err_t mqtt_publish_zcl_message(mqtt_handler_t *h, uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len);
gw_err_t mqtt_publish_ha_discovery(mqtt_handler_t *h, uint16_t node_id, const char *eui64);
gw_err_t mqtt_publish_log(mqtt_handler_t *h, const char *json_str);
gw_err_t mqtt_publish_devices(mqtt_handler_t *h, struct cJSON *devices_json);
void mqtt_handler_destroy(mqtt_handler_t *h);

#endif // GATEWAY_MQTT_HANDLER_H
