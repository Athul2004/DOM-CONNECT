#include "gateway/mqtt_handler.h"
#include "gateway/log.h"
#include <mosquitto.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "gateway/zcl.h"
#include "gateway/converters.h"

struct mqtt_handler {
    mqtt_config_t cfg;
    struct mosquitto *mosq;
    bool connected;
    mqtt_command_callback_t cmd_cb;
    void *cmd_user_data;
};

static void on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
    mqtt_handler_t *h = (mqtt_handler_t *)obj;
    if (reason_code == 0) {
        h->connected = true;
        LOG_INFO("mqtt", "Connected to broker", LOG_STR("broker", h->cfg.broker_url));
        
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/+/set", h->cfg.topic_prefix);
        mosquitto_subscribe(h->mosq, NULL, topic, 0);
        LOG_INFO("mqtt", "Subscribed to command topic", LOG_STR("topic", topic));
    } else {
        LOG_WARN("mqtt", "Failed to connect", LOG_INT("reason", reason_code));
    }
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int reason_code) {
    mqtt_handler_t *h = (mqtt_handler_t *)obj;
    h->connected = false;
    LOG_INFO("mqtt", "Disconnected from broker", LOG_INT("reason", reason_code));
}

static uint8_t hex2val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    mqtt_handler_t *h = (mqtt_handler_t *)obj;
    if (!h->cmd_cb) return;

    // Expected topic: <prefix>/0xABCD/set
    char *prefix = h->cfg.topic_prefix;
    size_t prefix_len = strlen(prefix);
    
    if (strncmp(msg->topic, prefix, prefix_len) != 0 || msg->topic[prefix_len] != '/') return;
    
    char *node_str = (char*)&msg->topic[prefix_len + 1];
    if (strncmp(node_str, "0x", 2) != 0) return;
    
    uint16_t node_id = strtol(node_str, NULL, 16);
    
    // Semantic Command Parsing (Phase M) via Converters
    uint16_t out_cluster;
    size_t out_len;
    uint8_t *zcl_payload = converters_mqtt_to_zcl(msg->payload, &out_cluster, &out_len);
    if (zcl_payload) {
        LOG_INFO("mqtt", "Received semantic command", LOG_INT("node", node_id));
        h->cmd_cb(node_id, 0x0104, out_cluster, 1, 1, zcl_payload, out_len, h->cmd_user_data);
        free(zcl_payload);
        return;
    }
    
    // Raw Command Parsing
    cJSON *json = cJSON_Parse(msg->payload);
    if (!json) return;
    
    cJSON *prof = cJSON_GetObjectItem(json, "profile_id");
    cJSON *clus = cJSON_GetObjectItem(json, "cluster_id");
    cJSON *src = cJSON_GetObjectItem(json, "src_ep");
    cJSON *dst = cJSON_GetObjectItem(json, "dst_ep");
    cJSON *pl = cJSON_GetObjectItem(json, "payload");
    
    if (prof && clus && src && dst && pl && cJSON_IsString(pl)) {
        size_t hex_len = strlen(pl->valuestring);
        size_t bin_len = hex_len / 2;
        uint8_t *bin = malloc(bin_len);
        for (size_t i = 0; i < bin_len; i++) {
            bin[i] = (hex2val(pl->valuestring[i*2]) << 4) | hex2val(pl->valuestring[i*2+1]);
        }
        
        LOG_INFO("mqtt", "Received command", LOG_INT("node", node_id), LOG_INT("cluster", clus->valueint));
        h->cmd_cb(node_id, prof->valueint, clus->valueint, src->valueint, dst->valueint, bin, bin_len, h->cmd_user_data);
        free(bin);
    } else {
        LOG_WARN("mqtt", "Invalid command format");
    }
    cJSON_Delete(json);
}

mqtt_handler_t *mqtt_handler_create(const mqtt_config_t *cfg) {
    if (!cfg->enabled) return NULL;
    
    mosquitto_lib_init();
    
    mqtt_handler_t *h = calloc(1, sizeof(mqtt_handler_t));
    memcpy(&h->cfg, cfg, sizeof(mqtt_config_t));
    
    h->mosq = mosquitto_new(h->cfg.client_id, true, h);
    if (!h->mosq) {
        LOG_WARN("mqtt", "Failed to create mosquitto instance");
        free(h);
        return NULL;
    }
    
    mosquitto_connect_callback_set(h->mosq, on_connect);
    mosquitto_disconnect_callback_set(h->mosq, on_disconnect);
    mosquitto_message_callback_set(h->mosq, on_message);
    
    if (strlen(h->cfg.username) > 0) {
        mosquitto_username_pw_set(h->mosq, h->cfg.username, h->cfg.password);
    }
    
    return h;
}

void mqtt_handler_set_command_callback(mqtt_handler_t *h, mqtt_command_callback_t cb, void *user_data) {
    if (h) {
        h->cmd_cb = cb;
        h->cmd_user_data = user_data;
    }
}

gw_err_t mqtt_handler_start(mqtt_handler_t *h) {
    if (!h) return GW_OK;
    
    int rc = mosquitto_connect_async(h->mosq, h->cfg.broker_url, h->cfg.port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_WARN("mqtt", "mosquitto_connect_async failed", LOG_INT("rc", rc));
        return GW_ERR_CONFIG;
    }
    
    mosquitto_loop_start(h->mosq);
    return GW_OK;
}

gw_err_t mqtt_handler_stop(mqtt_handler_t *h) {
    if (!h) return GW_OK;
    mosquitto_disconnect(h->mosq);
    mosquitto_loop_stop(h->mosq, true);
    return GW_OK;
}

void mqtt_handler_destroy(mqtt_handler_t *h) {
    if (!h) return;
    mosquitto_destroy(h->mosq);
    free(h);
    mosquitto_lib_cleanup();
}

static char* hex_string(const uint8_t *data, size_t len) {
    if (len == 0) return strdup("");
    char *out = malloc(len * 2 + 1);
    for (size_t i = 0; i < len; i++) {
        sprintf(&out[i * 2], "%02X", data[i]);
    }
    return out;
}

gw_err_t mqtt_publish_zcl_message(mqtt_handler_t *h, uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len) {
    if (!h || !h->connected) return GW_ERR_COORD_NOT_CONNECTED;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "sender", sender);
    cJSON_AddNumberToObject(json, "profile_id", profile_id);
    cJSON_AddNumberToObject(json, "cluster_id", cluster_id);
    cJSON_AddNumberToObject(json, "src_ep", src_ep);
    cJSON_AddNumberToObject(json, "dst_ep", dst_ep);
    
    char *payload_hex = hex_string(payload, payload_len);
    cJSON_AddStringToObject(json, "payload_raw", payload_hex);
    free(payload_hex);
    
    cJSON *zcl_parsed = zcl_parse_payload(cluster_id, payload, payload_len);
    if (zcl_parsed) {
        cJSON_AddItemToObject(json, "zcl", zcl_parsed);
        
        // Semantic State Translation (Phase M) via Converters
        cJSON *semantic_state = converters_zcl_to_mqtt(cluster_id, zcl_parsed);
        if (semantic_state) {
            // Merge semantic state into the root json
            cJSON *child = semantic_state->child;
            while (child) {
                cJSON *next = child->next;
                cJSON_DetachItemViaPointer(semantic_state, child);
                cJSON_AddItemToObject(json, child->string, child);
                child = next;
            }
            cJSON_Delete(semantic_state);
        }
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/0x%04X", h->cfg.topic_prefix, sender);
    
    int rc = mosquitto_publish(h->mosq, NULL, topic, strlen(json_str), json_str, 0, false);
    free(json_str);
    
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_WARN("mqtt", "Failed to publish message", LOG_INT("rc", rc));
        return GW_ERR_TRANSPORT_WRITE;
    }
    
    return GW_OK;
}

gw_err_t mqtt_publish_ha_discovery(mqtt_handler_t *h, uint16_t node_id, const char *eui64) {
    if (!h || !h->connected) return GW_ERR_COORD_NOT_CONNECTED;
    
    // We'll publish a generic switch discovery for MVP
    cJSON *json = cJSON_CreateObject();
    
    char name[64];
    snprintf(name, sizeof(name), "Zigbee Switch 0x%04X", node_id);
    cJSON_AddStringToObject(json, "name", name);
    
    char cmd_topic[128];
    snprintf(cmd_topic, sizeof(cmd_topic), "%s/0x%04X/set", h->cfg.topic_prefix, node_id);
    cJSON_AddStringToObject(json, "command_topic", cmd_topic);
    
    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), "%s/0x%04X", h->cfg.topic_prefix, node_id);
    cJSON_AddStringToObject(json, "state_topic", state_topic);
    
    // ZCL OnOff is boolean (1=ON, 0=OFF) or Enum. We'll map the state template.
    // ZCL parser outputs attr_0x0000 with 1 or 0 for OnOff cluster (0x0006).
    cJSON_AddStringToObject(json, "value_template", "{{ 'ON' if value_json.zcl.attributes.attr_0x0000 == 1 else 'OFF' }}");
    cJSON_AddStringToObject(json, "payload_on", "ON");
    cJSON_AddStringToObject(json, "payload_off", "OFF");
    cJSON_AddStringToObject(json, "command_template", "{\"state\": \"{{ value }}\"}");
    
    char unique_id[64];
    snprintf(unique_id, sizeof(unique_id), "0x%04X_switch", node_id);
    cJSON_AddStringToObject(json, "unique_id", unique_id);
    
    cJSON *device = cJSON_CreateObject();
    cJSON *ids = cJSON_CreateArray();
    char dev_id[32];
    snprintf(dev_id, sizeof(dev_id), "0x%04X", node_id);
    cJSON_AddItemToArray(ids, cJSON_CreateString(dev_id));
    
    if (eui64) {
        cJSON_AddItemToArray(ids, cJSON_CreateString(eui64));
    }
    
    cJSON_AddItemToObject(device, "identifiers", ids);
    cJSON_AddStringToObject(device, "name", name);
    cJSON_AddStringToObject(device, "manufacturer", "DOM Connect Gateway");
    cJSON_AddStringToObject(device, "model", "Generic Zigbee Device");
    cJSON_AddItemToObject(json, "device", device);
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    char topic[256];
    // Home Assistant discovery topic: homeassistant/<component>/<node_id>/config
    snprintf(topic, sizeof(topic), "homeassistant/switch/0x%04X/config", node_id);
    
    LOG_INFO("mqtt", "Publishing HA Discovery", LOG_STR("topic", topic));
    int rc = mosquitto_publish(h->mosq, NULL, topic, strlen(json_str), json_str, 0, true); // retain=true
    free(json_str);
    
    char joined_topic[256];
    snprintf(joined_topic, sizeof(joined_topic), "%s/0x%04X", h->cfg.topic_prefix, node_id);
    const char *joined_payload = "{\"status\":\"joined\"}";
    mosquitto_publish(h->mosq, NULL, joined_topic, strlen(joined_payload), joined_payload, 0, true);
    
    return (rc == MOSQ_ERR_SUCCESS) ? GW_OK : GW_ERR_TRANSPORT_WRITE;
}

gw_err_t mqtt_publish_log(mqtt_handler_t *h, const char *json_str) {
    if (!h || !h->connected) return GW_ERR_COORD_NOT_CONNECTED;
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/bridge/log", h->cfg.topic_prefix);
    int rc = mosquitto_publish(h->mosq, NULL, topic, strlen(json_str), json_str, 0, false);
    return (rc == MOSQ_ERR_SUCCESS) ? GW_OK : GW_ERR_TRANSPORT_WRITE;
}

gw_err_t mqtt_publish_devices(mqtt_handler_t *h, struct cJSON *devices_json) {
    if (!h || !h->connected || !devices_json) return GW_ERR_COORD_NOT_CONNECTED;
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/bridge/devices", h->cfg.topic_prefix);
    char *json_str = cJSON_PrintUnformatted(devices_json);
    if (!json_str) return GW_ERR_UNKNOWN;
    int rc = mosquitto_publish(h->mosq, NULL, topic, strlen(json_str), json_str, 0, true);
    free(json_str);
    return (rc == MOSQ_ERR_SUCCESS) ? GW_OK : GW_ERR_TRANSPORT_WRITE;
}
