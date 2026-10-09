#ifndef GATEWAY_DATABASE_H
#define GATEWAY_DATABASE_H

#include <stdint.h>
#include <stdbool.h>
#include "gateway/config.h"

// Initialize the database with configuration. Load existing data if file exists.
void db_init(const db_config_t *cfg);

// Add or update a device in the database.
void db_update_device(uint16_t node_id, const uint8_t *eui64);

// Set the manufacturer and model ID for a device (from Simple Descriptor/Basic Cluster).
void db_set_device_info(uint16_t node_id, const char *manufacturer, const char *model);

// Query if a device exists.
bool db_device_exists(uint16_t node_id);

// Force save the database to disk.
void db_save(void);

// Clean up and save database before exit.
struct cJSON *db_get_devices(void);
void db_destroy(void);

#endif // GATEWAY_DATABASE_H
