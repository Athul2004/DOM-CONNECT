#ifndef GATEWAY_ZNP_ADAPTER_H
#define GATEWAY_ZNP_ADAPTER_H

#include "gateway/adapter/coordinator_adapter.h"
#include "gateway/config.h"
#include "gateway/transport/transport.h"

coordinator_adapter_t *znp_adapter_create(const gateway_config_t *cfg, transport_t *transport);

#endif // GATEWAY_ZNP_ADAPTER_H
