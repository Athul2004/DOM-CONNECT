#ifndef GATEWAY_LOG_H
#define GATEWAY_LOG_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO,
    LOG_LVL_WARN,
    LOG_LVL_ERROR,
    LOG_LVL_FATAL
} log_level_t;

typedef enum {
    LOG_KV_STR = 1,
    LOG_KV_INT,
    LOG_KV_HEX
} log_kv_type_t;

void log_init(log_level_t level, const char *format);
void log_set_mqtt_callback(void (*cb)(const char *json_str));
void log_write(log_level_t lvl, const char *comp, const char *msg, ...);

#define LOG_DEBUG(comp, msg, ...)  log_write(LOG_LVL_DEBUG, comp, msg, ##__VA_ARGS__, 0)
#define LOG_INFO(comp, msg, ...)   log_write(LOG_LVL_INFO,  comp, msg, ##__VA_ARGS__, 0)
#define LOG_WARN(comp, msg, ...)   log_write(LOG_LVL_WARN,  comp, msg, ##__VA_ARGS__, 0)
#define LOG_ERROR(comp, msg, ...)  log_write(LOG_LVL_ERROR, comp, msg, ##__VA_ARGS__, 0)
#define LOG_FATAL(comp, msg, ...)  log_write(LOG_LVL_FATAL, comp, msg, ##__VA_ARGS__, 0)

#define LOG_STR(k, v)      LOG_KV_STR, (k), (v)
#define LOG_INT(k, v)      LOG_KV_INT, (k), ((long)(v))
#define LOG_HEX(k, v, len) LOG_KV_HEX, (k), (v), ((size_t)(len))

#endif // GATEWAY_LOG_H
