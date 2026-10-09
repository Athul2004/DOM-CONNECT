# DOM Connect Zigbee Gateway

A proprietary, production-grade Zigbee Gateway for Orange Pi Zero 3 written in C11.

## Architecture
- **Language**: C (C11)
- **Target**: Orange Pi Zero 3 (Allwinner H618)
- **Coordinator**: Silicon Labs EFR32MG NCP (via UART)
- **Dependencies**: Minimal (cJSON)

## Building
```bash
mkdir build && cd build
cmake ..
make -j4
```

## Running
```bash
./zigbee-gateway
```
