#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x)*1000)
#define msleep(x) Sleep(x)
#else
#include <unistd.h>
#define msleep(x) usleep((x)*1000)
#endif
#include "gateway/config.h"
#include "gateway/log.h"
#include "gateway/adapter/znp_adapter.h"
#include "gateway/mqtt_handler.h"
#include "gateway/database.h"
#include "gateway/zcl.h"
#include "cJSON.h"

static volatile sig_atomic_t g_running = 1;

typedef struct {
    coordinator_adapter_t *adapter;
    mqtt_handler_t *mqtt;
} app_context_t;

static app_context_t *g_ctx = NULL;

static void sig_handler(int signum) {
    (void)signum;
    g_running = 0;
}

static void my_log_cb(const char *json_str) {
    if (g_ctx && g_ctx->mqtt) {
        mqtt_publish_log(g_ctx->mqtt, json_str);
    }
}

static void on_zcl_message(uint16_t sender, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data) {
    app_context_t *ctx = (app_context_t *)user_data;
    if (ctx && ctx->mqtt) {
        mqtt_publish_zcl_message(ctx->mqtt, sender, profile_id, cluster_id, src_ep, dst_ep, payload, payload_len);
        
        // Mini-Z2M: If it's a Read Attributes Response for Basic Cluster, extract ModelIdentifier
        if (cluster_id == 0x0000 && payload_len >= 3) {
            uint8_t cmd = (payload[0] & 0x04) ? payload[4] : payload[2]; // Handle mfg specific
            if (cmd == 0x01) { // Read Attributes Response
                cJSON *parsed = zcl_parse_payload(cluster_id, payload, payload_len);
                if (parsed) {
                    cJSON *attrs = cJSON_GetObjectItem(parsed, "attributes");
                    if (attrs) {
                        cJSON *model = cJSON_GetObjectItem(attrs, "attr_0x0005");
                        if (model && cJSON_IsString(model)) {
                            db_set_device_info(sender, NULL, model->valuestring);
                            mqtt_publish_devices(ctx->mqtt, db_get_devices());
                        }
                    }
                    cJSON_Delete(parsed);
                }
            }
        }
    }
}

static void on_device_join(uint16_t node_id, const uint8_t *eui64, uint8_t status, void *user_data) {
    (void)status;
    app_context_t *ctx = (app_context_t *)user_data;
    
    // Save to DB
    db_update_device(node_id, eui64);

    char eui64_hex[17] = {0};
    if (eui64) {
        snprintf(eui64_hex, sizeof(eui64_hex), "%02X%02X%02X%02X%02X%02X%02X%02X",
                 eui64[7], eui64[6], eui64[5], eui64[4], eui64[3], eui64[2], eui64[1], eui64[0]);
    }
    
    if (ctx && ctx->mqtt) {
        mqtt_publish_ha_discovery(ctx->mqtt, node_id, eui64_hex);
    }
    
    // Send ZCL Read Attributes for Basic Cluster (0x0000) -> ModelIdentifier (0x0005) & ManufacturerName (0x0004)
    LOG_INFO("main", "Starting device interview: Requesting Model ID", LOG_INT("node_id", node_id));
    uint8_t payload[7];
    payload[0] = 0x00; // Frame Ctrl: Global cmd, Client to Server
    payload[1] = 0xAA; // Seq num
    payload[2] = 0x00; // Cmd: Read Attributes
    payload[3] = 0x04; // Attr 1: ManufacturerName (0x0004)
    payload[4] = 0x00; 
    payload[5] = 0x05; // Attr 2: ModelIdentifier (0x0005)
    payload[6] = 0x00;
    
    if (ctx && ctx->adapter && ctx->adapter->send_unicast) {
        // Send to Profile 0x0104 (HA), Cluster 0x0000 (Basic), Dst Endpoint 1 (guessing EP 1 is valid)
        ctx->adapter->send_unicast(ctx->adapter, node_id, 0x0104, 0x0000, 1, 1, payload, sizeof(payload));
    }
}

static void on_mqtt_command(uint16_t node_id, uint16_t profile_id, uint16_t cluster_id, uint8_t src_ep, uint8_t dst_ep, const uint8_t *payload, size_t payload_len, void *user_data) {
    app_context_t *ctx = (app_context_t *)user_data;
    if (ctx && ctx->adapter && ctx->adapter->send_unicast) {
        gw_err_t err = ctx->adapter->send_unicast(ctx->adapter, node_id, profile_id, cluster_id, src_ep, dst_ep, payload, payload_len);
        if (err != GW_OK) {
            LOG_WARN("main", "Failed to send unicast command");
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    gateway_config_t cfg;
    config_load("config/gateway.yaml", &cfg);
    
    db_init(&cfg.db);
    
    log_level_t level = LOG_LVL_INFO;
    if (strcmp(cfg.logging.level, "DEBUG") == 0) level = LOG_LVL_DEBUG;
    
    log_init(level, cfg.logging.format);
    
    LOG_INFO("main", "DOM Connect Gateway starting", 
             LOG_STR("version", cfg.gateway.version));
             
    mqtt_handler_t *mqtt = mqtt_handler_create(&cfg.mqtt);
    coordinator_adapter_t *adapter = znp_adapter_create(&cfg, NULL);
    
    app_context_t ctx;
    ctx.mqtt = mqtt;
    ctx.adapter = adapter;
    
    g_ctx = &ctx;
    log_set_mqtt_callback(my_log_cb);
    
    if (mqtt) {
        mqtt_handler_set_command_callback(mqtt, on_mqtt_command, &ctx);
        mqtt_handler_start(mqtt);
    }
    
    gw_err_t err = adapter->start(adapter);
    if (err != GW_OK) {
        LOG_FATAL("main", "Failed to start coordinator", LOG_STR("error", gw_err_str(err)));
        adapter->destroy(adapter);
        return 1;
    }
    
    coordinator_info_t info;
    adapter->get_info(adapter, &info);
    
    LOG_INFO("main", "Coordinator identity", 
             LOG_HEX("eui64", info.eui64, 8),
             LOG_INT("node_id", info.node_id));
             
    LOG_INFO("main", "Network state", LOG_INT("state", info.network_state));
    
    // Phase I: Network Formation
    if (info.network_state == 0) { // EMBER_NO_NETWORK
        LOG_INFO("main", "No network found. Forming a new centralized network...");
        gw_err_t net_err = adapter->form_network(adapter);
        if (net_err != GW_OK) {
            LOG_FATAL("main", "Failed to form Zigbee network");
        } else {
            LOG_INFO("main", "Zigbee network formed successfully.");
        }
    }
    
    adapter->set_zcl_callback(adapter, on_zcl_message, &ctx);
    adapter->set_device_join_callback(adapter, on_device_join, &ctx);
    
    LOG_INFO("main", "Enabling permit join for 60 seconds");
    gw_err_t pj_err = adapter->permit_join(adapter, 60);
    if (pj_err != GW_OK) {
        LOG_WARN("main", "Failed to enable permit join");
    }
    
    if (mqtt) {
        mqtt_publish_devices(mqtt, db_get_devices());
    }
    
    // Main loop
    int health_timer = 0;
    int health_interval_ms = cfg.coordinator.health.interval * 1000;
    
    while (g_running) {
        adapter->poll(adapter);
        msleep(50);
        health_timer += 50;
        
        if (health_timer >= health_interval_ms) {
            health_timer = 0;
            gw_err_t herr = adapter->health_check(adapter);
            if (herr != GW_OK && cfg.coordinator.health.auto_reconnect) {
                LOG_WARN("main", "Connection lost, attempting reconnect");
                adapter->reset(adapter);
            }
        }
    }
    
    LOG_INFO("main", "Shutting down");
    adapter->stop(adapter);
    adapter->destroy(adapter);
    
    if (mqtt) {
        mqtt_handler_stop(mqtt);
        mqtt_handler_destroy(mqtt);
    }
    
    db_destroy();
    
    return 0;
}
