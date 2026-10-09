#ifndef _WIN32

#include "gateway/transport/uart_transport.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "gateway/log.h"

typedef struct {
    int fd;
    transport_config_t cfg;
    bool is_open;
} uart_impl_t;

static speed_t get_baudrate(int baudrate) {
    switch(baudrate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default: return B115200;
    }
}

static gw_err_t uart_open(transport_t *self) {
    uart_impl_t *impl = (uart_impl_t *)self->impl;
    
    if (impl->is_open) return GW_OK;
    
    impl->fd = open(impl->cfg.port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (impl->fd < 0) {
        LOG_ERROR("transport.uart", "Failed to open port", LOG_STR("port", impl->cfg.port));
        return GW_ERR_TRANSPORT_OPEN;
    }
    
    struct termios tty;
    if (tcgetattr(impl->fd, &tty) != 0) {
        close(impl->fd);
        return GW_ERR_TRANSPORT_OPEN;
    }
    
    // Raw mode
    tty.c_cflag &= ~PARENB; // No parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8; // 8 data bits
    tty.c_cflag &= ~CRTSCTS; // No hardware flow control (default)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
    
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    
    if (strcmp(impl->cfg.flow_control, "hardware") == 0) {
        tty.c_cflag |= CRTSCTS;
    } else if (strcmp(impl->cfg.flow_control, "software") == 0) {
        tty.c_iflag |= (IXON | IXOFF | IXANY);
    }
    
    speed_t speed = get_baudrate(impl->cfg.baudrate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    
    if (tcsetattr(impl->fd, TCSANOW, &tty) != 0) {
        close(impl->fd);
        return GW_ERR_TRANSPORT_OPEN;
    }
    
    impl->is_open = true;
    LOG_INFO("transport.uart", "UART opened", 
             LOG_STR("port", impl->cfg.port),
             LOG_INT("baudrate", impl->cfg.baudrate));
             
    return GW_OK;
}

static gw_err_t uart_close(transport_t *self) {
    uart_impl_t *impl = (uart_impl_t *)self->impl;
    if (impl->is_open) {
        close(impl->fd);
        impl->is_open = false;
        LOG_INFO("transport.uart", "UART closed", LOG_STR("port", impl->cfg.port));
    }
    return GW_OK;
}

static gw_err_t uart_read(transport_t *self, uint8_t *buf, size_t max_len,
                          size_t *out_len, uint32_t timeout_ms) {
    uart_impl_t *impl = (uart_impl_t *)self->impl;
    if (!impl->is_open) return GW_ERR_TRANSPORT_CLOSED;
    
    *out_len = 0;
    
    struct pollfd pfd = { .fd = impl->fd, .events = POLLIN };
    int ret = poll(&pfd, 1, timeout_ms);
    
    if (ret < 0) {
        return GW_ERR_TRANSPORT_READ;
    } else if (ret == 0) {
        return GW_OK; // Timeout, no data
    }
    
    if (pfd.revents & POLLIN) {
        ssize_t n = read(impl->fd, buf, max_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return GW_OK;
            return GW_ERR_TRANSPORT_READ;
        }
        *out_len = (size_t)n;
    }
    return GW_OK;
}

static gw_err_t uart_write(transport_t *self, const uint8_t *buf, size_t len) {
    uart_impl_t *impl = (uart_impl_t *)self->impl;
    if (!impl->is_open) return GW_ERR_TRANSPORT_CLOSED;
    
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(impl->fd, buf + total, len - total);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return GW_ERR_TRANSPORT_WRITE;
        }
        total += n;
    }
    return GW_OK;
}

static gw_err_t uart_flush(transport_t *self) {
    uart_impl_t *impl = (uart_impl_t *)self->impl;
    if (!impl->is_open) return GW_ERR_TRANSPORT_CLOSED;
    
    tcdrain(impl->fd);
    return GW_OK;
}

static bool uart_is_open(transport_t *self) {
    return ((uart_impl_t *)self->impl)->is_open;
}

static gw_err_t uart_reset(transport_t *self) {
    uart_close(self);
    usleep(500 * 1000);
    return uart_open(self);
}

static void uart_destroy(transport_t *self) {
    uart_close(self);
    free(self->impl);
    free(self);
}

transport_t *uart_transport_create(const transport_config_t *cfg) {
    transport_t *t = calloc(1, sizeof(transport_t));
    uart_impl_t *impl = calloc(1, sizeof(uart_impl_t));
    
    memcpy(&impl->cfg, cfg, sizeof(transport_config_t));
    
    t->impl = impl;
    t->open = uart_open;
    t->close = uart_close;
    t->read = uart_read;
    t->write = uart_write;
    t->flush = uart_flush;
    t->is_open = uart_is_open;
    t->reset = uart_reset;
    t->destroy = uart_destroy;
    
    return t;
}

#else

// Dummy implementation for Windows compilation
#include "gateway/transport/uart_transport.h"
#include <stdlib.h>

static gw_err_t dummy_err(transport_t *self) { return GW_ERR_TRANSPORT_OPEN; }
static gw_err_t dummy_read(transport_t *self, uint8_t *buf, size_t max_len, size_t *out_len, uint32_t timeout_ms) { return GW_ERR_TRANSPORT_OPEN; }
static gw_err_t dummy_write(transport_t *self, const uint8_t *buf, size_t len) { return GW_ERR_TRANSPORT_OPEN; }
static bool dummy_bool(transport_t *self) { return false; }
static void dummy_void(transport_t *self) { free(self); }

transport_t *uart_transport_create(const transport_config_t *cfg) {
    transport_t *t = calloc(1, sizeof(transport_t));
    t->open = dummy_err;
    t->close = dummy_err;
    t->read = dummy_read;
    t->write = dummy_write;
    t->flush = dummy_err;
    t->is_open = dummy_bool;
    t->reset = dummy_err;
    t->destroy = dummy_void;
    return t;
}

#endif
