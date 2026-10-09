#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include "gateway/types.h"

typedef struct {
    char port[128];
    int baudrate;
    char flow_control[16];
    float read_timeout;
    float write_timeout;
} transport_config_t;

typedef struct {
    int protocol_version;
    float command_timeout;
    int max_retries;
    float retry_delay;
} ezsp_config_t;

typedef struct {
    int max_frame_size;
    float ack_timeout;
    int max_retransmissions;
    float reset_timeout;
} ash_config_t;

typedef struct {
    float interval;
    int failure_threshold;
    bool auto_reconnect;
    float reconnect_delay;
} health_config_t;

typedef struct {
    char type[16];
    transport_config_t transport;
    ezsp_config_t ezsp;
    ash_config_t ash;
    health_config_t health;
} coordinator_config_t;

typedef struct {
    char level[16];
    char format[8];
} logging_config_t;

typedef struct {
    bool enabled;
    char broker_url[128];
    int port;
    char client_id[64];
    char topic_prefix[64];
    char username[64];
    char password[64];
} mqtt_config_t;

typedef struct {
    char filepath[256];
    bool auto_save;
} db_config_t;

typedef struct {
    char id[64];
    char name[128];
    char version[32];
} gateway_info_t;

typedef struct {
    gateway_info_t gateway;
    coordinator_config_t coordinator;
    logging_config_t logging;
    mqtt_config_t mqtt;
    db_config_t db;
} gateway_config_t;

gw_err_t config_load(const char *path, gateway_config_t *cfg);
void config_defaults(gateway_config_t *cfg);

#endif // GATEWAY_CONFIG_H
