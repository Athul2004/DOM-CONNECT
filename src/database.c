#include "gateway/database.h"
#include "gateway/log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static db_config_t g_cfg;
static cJSON *g_db = NULL;

static void load_database(void) {
    FILE *f = fopen(g_cfg.filepath, "rb");
    if (!f) {
        LOG_INFO("db", "No existing database found. Creating new.", LOG_STR("file", g_cfg.filepath));
        g_db = cJSON_CreateObject();
        cJSON_AddObjectToObject(g_db, "devices");
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (length <= 0) {
        fclose(f);
        g_db = cJSON_CreateObject();
        cJSON_AddObjectToObject(g_db, "devices");
        return;
    }
    
    char *data = malloc(length + 1);
    if (fread(data, 1, length, f) != (size_t)length) {
        LOG_WARN("db", "Failed to read database file entirely");
    }
    data[length] = '\0';
    fclose(f);
    
    g_db = cJSON_Parse(data);
    free(data);
    
    if (!g_db) {
        LOG_WARN("db", "Failed to parse database file. Creating new.");
        g_db = cJSON_CreateObject();
        cJSON_AddObjectToObject(g_db, "devices");
    } else if (!cJSON_GetObjectItem(g_db, "devices")) {
        cJSON_AddObjectToObject(g_db, "devices");
    }
    
    LOG_INFO("db", "Database loaded", LOG_STR("file", g_cfg.filepath));
}

void db_save(void) {
    if (!g_db) return;
    
    char *json_str = cJSON_Print(g_db);
    if (!json_str) return;
    
    FILE *f = fopen(g_cfg.filepath, "wb");
    if (f) {
        fwrite(json_str, 1, strlen(json_str), f);
        fclose(f);
        LOG_DEBUG("db", "Database saved");
    } else {
        LOG_WARN("db", "Failed to save database", LOG_STR("file", g_cfg.filepath));
    }
    free(json_str);
}

void db_init(const db_config_t *cfg) {
    memcpy(&g_cfg, cfg, sizeof(db_config_t));
    load_database();
}

static cJSON *get_or_create_device(uint16_t node_id) {
    if (!g_db) return NULL;
    cJSON *devices = cJSON_GetObjectItem(g_db, "devices");
    if (!devices) return NULL;
    
    char node_key[16];
    snprintf(node_key, sizeof(node_key), "0x%04X", node_id);
    
    cJSON *dev = cJSON_GetObjectItem(devices, node_key);
    if (!dev) {
        dev = cJSON_CreateObject();
        cJSON_AddItemToObject(devices, node_key, dev);
    }
    return dev;
}

void db_update_device(uint16_t node_id, const uint8_t *eui64) {
    cJSON *dev = get_or_create_device(node_id);
    if (!dev) return;
    
    if (eui64) {
        char hex[17];
        snprintf(hex, sizeof(hex), "%02X%02X%02X%02X%02X%02X%02X%02X",
                 eui64[7], eui64[6], eui64[5], eui64[4], eui64[3], eui64[2], eui64[1], eui64[0]);
                 
        cJSON *existing = cJSON_GetObjectItem(dev, "eui64");
        if (existing) {
            cJSON_SetValuestring(existing, hex);
        } else {
            cJSON_AddStringToObject(dev, "eui64", hex);
        }
    }
    
    if (g_cfg.auto_save) {
        db_save();
    }
}

void db_set_device_info(uint16_t node_id, const char *manufacturer, const char *model) {
    cJSON *dev = get_or_create_device(node_id);
    if (!dev) return;
    
    if (manufacturer) {
        cJSON *existing = cJSON_GetObjectItem(dev, "manufacturer");
        if (existing) cJSON_SetValuestring(existing, manufacturer);
        else cJSON_AddStringToObject(dev, "manufacturer", manufacturer);
    }
    
    if (model) {
        cJSON *existing = cJSON_GetObjectItem(dev, "model");
        if (existing) cJSON_SetValuestring(existing, model);
        else cJSON_AddStringToObject(dev, "model", model);
    }
    
    if (g_cfg.auto_save) {
        db_save();
    }
}

bool db_device_exists(uint16_t node_id) {
    if (!g_db) return false;
    cJSON *devices = cJSON_GetObjectItem(g_db, "devices");
    if (!devices) return false;
    
    char node_key[16];
    snprintf(node_key, sizeof(node_key), "0x%04X", node_id);
    return cJSON_GetObjectItem(devices, node_key) != NULL;
}

cJSON *db_get_devices(void) {
    if (!g_db) return NULL;
    return cJSON_GetObjectItem(g_db, "devices");
}

void db_destroy(void) {
    if (g_cfg.auto_save) db_save();
    if (g_db) {
        cJSON_Delete(g_db);
        g_db = NULL;
    }
}
