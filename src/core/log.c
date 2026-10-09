#include "gateway/log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include "cJSON.h"

static log_level_t g_level = LOG_LVL_INFO;
static bool g_json_format = true;

void log_init(log_level_t level, const char *format) {
    g_level = level;
    if (format && strcmp(format, "text") == 0) {
        g_json_format = false;
    } else {
        g_json_format = true;
    }
}

static void (*g_mqtt_log_cb)(const char *json_str) = NULL;

void log_set_mqtt_callback(void (*cb)(const char *json_str)) {
    g_mqtt_log_cb = cb;
}

static const char *level_to_str(log_level_t lvl) {
    switch (lvl) {
        case LOG_LVL_DEBUG: return "DEBUG";
        case LOG_LVL_INFO:  return "INFO";
        case LOG_LVL_WARN:  return "WARN";
        case LOG_LVL_ERROR: return "ERROR";
        case LOG_LVL_FATAL: return "FATAL";
        default:            return "UNKNOWN";
    }
}

static bool is_sensitive(const char *key) {
    if (!key) return false;
    if (strstr(key, "key") || strstr(key, "password") || 
        strstr(key, "secret") || strstr(key, "credential")) {
        return true;
    }
    return false;
}

void log_write(log_level_t lvl, const char *comp, const char *msg, ...) {
    if (lvl < g_level) return;

    va_list args;
    va_start(args, msg);

    // Get current time
    time_t now;
    time(&now);
    struct tm *tm_info = gmtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    if (g_json_format) {
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddStringToObject(root, "timestamp", time_buf);
            cJSON_AddStringToObject(root, "level", level_to_str(lvl));
            cJSON_AddStringToObject(root, "component", comp ? comp : "unknown");
            cJSON_AddStringToObject(root, "message", msg ? msg : "");

            while (1) {
                int type = va_arg(args, int);
                if (type == 0) break; // Sentinel

                const char *key = va_arg(args, const char *);
                if (!key) break;
                
                bool redact = is_sensitive(key);

                if (type == LOG_KV_STR) {
                    const char *val = va_arg(args, const char *);
                    cJSON_AddStringToObject(root, key, redact ? "***REDACTED***" : (val ? val : "null"));
                } else if (type == LOG_KV_INT) {
                    long val = va_arg(args, long);
                    if (redact) {
                        cJSON_AddStringToObject(root, key, "***REDACTED***");
                    } else {
                        cJSON_AddNumberToObject(root, key, (double)val);
                    }
                } else if (type == LOG_KV_HEX) {
                    const uint8_t *data = va_arg(args, const uint8_t *);
                    size_t len = va_arg(args, size_t);
                    if (redact) {
                        cJSON_AddStringToObject(root, key, "***REDACTED***");
                    } else {
                        if (data && len > 0) {
                            char *hex = malloc(len * 2 + 1);
                            if (hex) {
                                for (size_t i = 0; i < len; i++) {
                                    sprintf(&hex[i * 2], "%02x", data[i]);
                                }
                                cJSON_AddStringToObject(root, key, hex);
                                free(hex);
                            }
                        } else {
                            cJSON_AddStringToObject(root, key, "");
                        }
                    }
                }
            }

            char *json_str = cJSON_PrintUnformatted(root);
            if (json_str) {
                printf("%s\n", json_str);
                if (g_mqtt_log_cb) {
                    g_mqtt_log_cb(json_str);
                }
                free(json_str);
            }
            cJSON_Delete(root);
        }
    } else {
        printf("%s [%-5s] %-20s | %s", time_buf, level_to_str(lvl), comp ? comp : "unknown", msg ? msg : "");
        while (1) {
            int type = va_arg(args, int);
            if (type == 0) break;

            const char *key = va_arg(args, const char *);
            if (!key) break;

            bool redact = is_sensitive(key);
            
            if (type == LOG_KV_STR) {
                const char *val = va_arg(args, const char *);
                printf(" %s=%s", key, redact ? "***REDACTED***" : (val ? val : "null"));
            } else if (type == LOG_KV_INT) {
                long val = va_arg(args, long);
                if (redact) {
                    printf(" %s=***REDACTED***", key);
                } else {
                    printf(" %s=%ld", key, val);
                }
            } else if (type == LOG_KV_HEX) {
                const uint8_t *data = va_arg(args, const uint8_t *);
                size_t len = va_arg(args, size_t);
                if (redact) {
                    printf(" %s=***REDACTED***", key);
                } else {
                    printf(" %s=", key);
                    if (data && len > 0) {
                        for (size_t i = 0; i < len; i++) {
                            printf("%02x", data[i]);
                        }
                    }
                }
            }
        }
        printf("\n");
    }

    va_end(args);
    fflush(stdout);
}
