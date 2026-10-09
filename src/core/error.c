#include "gateway/error.h"

const char *gw_err_str(gw_err_t err) {
    switch (err) {
        case GW_OK: return "GW_OK";
        case GW_ERR_TRANSPORT_OPEN: return "GW_ERR_TRANSPORT_OPEN";
        case GW_ERR_TRANSPORT_READ: return "GW_ERR_TRANSPORT_READ";
        case GW_ERR_TRANSPORT_WRITE: return "GW_ERR_TRANSPORT_WRITE";
        case GW_ERR_TRANSPORT_CLOSED: return "GW_ERR_TRANSPORT_CLOSED";
        case GW_ERR_TRANSPORT_TIMEOUT: return "GW_ERR_TRANSPORT_TIMEOUT";
        case GW_ERR_ASH_CRC: return "GW_ERR_ASH_CRC";
        case GW_ERR_ASH_RESET_TIMEOUT: return "GW_ERR_ASH_RESET_TIMEOUT";
        case GW_ERR_ASH_RETRANSMIT: return "GW_ERR_ASH_RETRANSMIT";
        case GW_ERR_ASH_PROTOCOL: return "GW_ERR_ASH_PROTOCOL";
        case GW_ERR_ASH_VERSION: return "GW_ERR_ASH_VERSION";
        case GW_ERR_EZSP_VERSION: return "GW_ERR_EZSP_VERSION";
        case GW_ERR_EZSP_COMMAND: return "GW_ERR_EZSP_COMMAND";
        case GW_ERR_EZSP_TIMEOUT: return "GW_ERR_EZSP_TIMEOUT";
        case GW_ERR_EZSP_COMMUNICATION: return "GW_ERR_EZSP_COMMUNICATION";
        case GW_ERR_COORD_NOT_CONNECTED: return "GW_ERR_COORD_NOT_CONNECTED";
        case GW_ERR_COORD_STARTUP: return "GW_ERR_COORD_STARTUP";
        case GW_ERR_COORD_HEALTH: return "GW_ERR_COORD_HEALTH";
        case GW_ERR_CONFIG: return "GW_ERR_CONFIG";
        case GW_ERR_NOMEM: return "GW_ERR_NOMEM";
        case GW_ERR_INVALID_ARG: return "GW_ERR_INVALID_ARG";
        default: return "UNKNOWN_ERROR";
    }
}
