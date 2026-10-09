#ifndef GATEWAY_TRANSPORT_H
#define GATEWAY_TRANSPORT_H

#include "gateway/types.h"
#include "gateway/error.h"

typedef struct transport transport_t;

struct transport {
    gw_err_t (*open)(transport_t *self);
    gw_err_t (*close)(transport_t *self);
    gw_err_t (*read)(transport_t *self, uint8_t *buf, size_t max_len,
                     size_t *out_len, uint32_t timeout_ms);
    gw_err_t (*write)(transport_t *self, const uint8_t *buf, size_t len);
    gw_err_t (*flush)(transport_t *self);
    bool     (*is_open)(transport_t *self);
    gw_err_t (*reset)(transport_t *self);
    void     (*destroy)(transport_t *self);
    
    void     *impl;   // Implementation specific data
};

#endif // GATEWAY_TRANSPORT_H
