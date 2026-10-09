#ifndef GATEWAY_MOCK_TRANSPORT_H
#define GATEWAY_MOCK_TRANSPORT_H

#include "gateway/transport/transport.h"

transport_t *mock_transport_create(void);

// Test helpers
void mock_transport_queue_response(transport_t *t, const uint8_t *data, size_t len);
void mock_transport_get_written(transport_t *t, size_t index, const uint8_t **out_data, size_t *out_len);
size_t mock_transport_get_write_count(transport_t *t);
void mock_transport_set_open_failure(transport_t *t, gw_err_t err_to_return);
void mock_transport_clear(transport_t *t);

#endif // GATEWAY_MOCK_TRANSPORT_H
