#include "gateway/transport/mock_transport.h"
#include <stdlib.h>
#include <string.h>

#define MAX_WRITES 100
#define MAX_QUEUE 100

typedef struct {
    uint8_t *data;
    size_t len;
} mock_buffer_t;

typedef struct {
    bool is_open;
    gw_err_t open_err;
    
    mock_buffer_t writes[MAX_WRITES];
    size_t write_count;
    
    mock_buffer_t rx_queue[MAX_QUEUE];
    size_t rx_queue_head;
    size_t rx_queue_tail;
} mock_impl_t;

static gw_err_t mock_open(transport_t *self) {
    mock_impl_t *impl = (mock_impl_t *)self->impl;
    if (impl->open_err != GW_OK) return impl->open_err;
    impl->is_open = true;
    return GW_OK;
}

static gw_err_t mock_close(transport_t *self) {
    mock_impl_t *impl = (mock_impl_t *)self->impl;
    impl->is_open = false;
    return GW_OK;
}

static gw_err_t mock_read(transport_t *self, uint8_t *buf, size_t max_len,
                          size_t *out_len, uint32_t timeout_ms) {
    mock_impl_t *impl = (mock_impl_t *)self->impl;
    (void)timeout_ms;
    
    if (!impl->is_open) return GW_ERR_TRANSPORT_CLOSED;
    
    if (impl->rx_queue_head == impl->rx_queue_tail) {
        *out_len = 0;
        return GW_OK; // simulated timeout / empty
    }
    
    mock_buffer_t *qbuf = &impl->rx_queue[impl->rx_queue_head];
    size_t to_copy = qbuf->len;
    if (to_copy > max_len) to_copy = max_len;
    
    memcpy(buf, qbuf->data, to_copy);
    *out_len = to_copy;
    
    free(qbuf->data);
    qbuf->data = NULL;
    impl->rx_queue_head = (impl->rx_queue_head + 1) % MAX_QUEUE;
    
    return GW_OK;
}

static gw_err_t mock_write(transport_t *self, const uint8_t *buf, size_t len) {
    mock_impl_t *impl = (mock_impl_t *)self->impl;
    if (!impl->is_open) return GW_ERR_TRANSPORT_CLOSED;
    
    if (impl->write_count < MAX_WRITES) {
        impl->writes[impl->write_count].data = malloc(len);
        memcpy(impl->writes[impl->write_count].data, buf, len);
        impl->writes[impl->write_count].len = len;
        impl->write_count++;
    }
    return GW_OK;
}

static gw_err_t mock_flush(transport_t *self) {
    (void)self;
    return GW_OK;
}

static bool mock_is_open(transport_t *self) {
    return ((mock_impl_t *)self->impl)->is_open;
}

static gw_err_t mock_reset(transport_t *self) {
    mock_close(self);
    return mock_open(self);
}

static void mock_destroy(transport_t *self) {
    mock_transport_clear(self);
    free(self->impl);
    free(self);
}

transport_t *mock_transport_create(void) {
    transport_t *t = calloc(1, sizeof(transport_t));
    mock_impl_t *impl = calloc(1, sizeof(mock_impl_t));
    
    t->impl = impl;
    t->open = mock_open;
    t->close = mock_close;
    t->read = mock_read;
    t->write = mock_write;
    t->flush = mock_flush;
    t->is_open = mock_is_open;
    t->reset = mock_reset;
    t->destroy = mock_destroy;
    
    return t;
}

void mock_transport_queue_response(transport_t *t, const uint8_t *data, size_t len) {
    mock_impl_t *impl = (mock_impl_t *)t->impl;
    size_t next = (impl->rx_queue_tail + 1) % MAX_QUEUE;
    if (next != impl->rx_queue_head) {
        impl->rx_queue[impl->rx_queue_tail].data = malloc(len);
        memcpy(impl->rx_queue[impl->rx_queue_tail].data, data, len);
        impl->rx_queue[impl->rx_queue_tail].len = len;
        impl->rx_queue_tail = next;
    }
}

void mock_transport_get_written(transport_t *t, size_t index, const uint8_t **out_data, size_t *out_len) {
    mock_impl_t *impl = (mock_impl_t *)t->impl;
    if (index < impl->write_count) {
        *out_data = impl->writes[index].data;
        *out_len = impl->writes[index].len;
    } else {
        *out_data = NULL;
        *out_len = 0;
    }
}

size_t mock_transport_get_write_count(transport_t *t) {
    return ((mock_impl_t *)t->impl)->write_count;
}

void mock_transport_set_open_failure(transport_t *t, gw_err_t err_to_return) {
    ((mock_impl_t *)t->impl)->open_err = err_to_return;
}

void mock_transport_clear(transport_t *t) {
    mock_impl_t *impl = (mock_impl_t *)t->impl;
    
    for (size_t i = 0; i < impl->write_count; i++) {
        free(impl->writes[i].data);
    }
    impl->write_count = 0;
    
    while (impl->rx_queue_head != impl->rx_queue_tail) {
        free(impl->rx_queue[impl->rx_queue_head].data);
        impl->rx_queue_head = (impl->rx_queue_head + 1) % MAX_QUEUE;
    }
}
