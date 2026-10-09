#include "gateway/config.h"
#include "gateway/error.h"
#include <string.h>

void config_defaults(gateway_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    // Gateway Info
    strncpy(cfg->gateway.id, "gw-001", sizeof(cfg->gateway.id)-1);
    strncpy(cfg->gateway.name, "DOM Connect Gateway", sizeof(cfg->gateway.name)-1);
    strncpy(cfg->gateway.version, "0.1.0", sizeof(cfg->gateway.version)-1);

    // Logging
    strncpy(cfg->logging.level, "INFO", sizeof(cfg->logging.level)-1);
    strncpy(cfg->logging.format, "json", sizeof(cfg->logging.format)-1);

    // Coordinator
    strncpy(cfg->coordinator.type, "ezsp", sizeof(cfg->coordinator.type)-1);

    // Transport
    strncpy(cfg->coordinator.transport.port, "/dev/ttyUSB0", sizeof(cfg->coordinator.transport.port)-1);
    cfg->coordinator.transport.baudrate = 115200;
    strncpy(cfg->coordinator.transport.flow_control, "software", sizeof(cfg->coordinator.transport.flow_control)-1);
    cfg->coordinator.transport.read_timeout = 3.0f;
    cfg->coordinator.transport.write_timeout = 3.0f;

    // EZSP
    cfg->coordinator.ezsp.protocol_version = 13;
    cfg->coordinator.ezsp.command_timeout = 5.0f;
    cfg->coordinator.ezsp.max_retries = 3;
    cfg->coordinator.ezsp.retry_delay = 0.5f;

    // ASH
    cfg->coordinator.ash.max_frame_size = 128;
    cfg->coordinator.ash.ack_timeout = 1.6f;
    cfg->coordinator.ash.max_retransmissions = 4;
    cfg->coordinator.ash.reset_timeout = 5.0f;

    // Health
    cfg->coordinator.health.interval = 30.0f;
    cfg->coordinator.health.failure_threshold = 3;
    cfg->coordinator.health.auto_reconnect = true;
    cfg->coordinator.health.reconnect_delay = 5.0f;
    
    // MQTT
    cfg->mqtt.enabled = true;
    strncpy(cfg->mqtt.broker_url, "localhost", sizeof(cfg->mqtt.broker_url)-1);
    cfg->mqtt.port = 1883;
    strncpy(cfg->mqtt.client_id, "zigbee-gateway-01", sizeof(cfg->mqtt.client_id)-1);
    strncpy(cfg->mqtt.topic_prefix, "zigbee2mqtt", sizeof(cfg->mqtt.topic_prefix)-1);
    
    // Database
    strncpy(cfg->db.filepath, "database.json", sizeof(cfg->db.filepath)-1);
    cfg->db.auto_save = true;
}

gw_err_t config_load(const char *path, gateway_config_t *cfg) {
    (void)path;
    config_defaults(cfg);
    // (YAML parsing via libyaml can be added here)
    return GW_OK;
}
