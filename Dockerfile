# ==============================================================================
#  DOM ECOSYSTEM - ENTERPRISE ZIGBEE GATEWAY DOCKERFILE
#  Optimized for Orange Pi (ARM64 / aarch64) & x86_64 Edge Platforms
#  Proprietary & Confidential - (c) 2026 DOM ECOSYSTEM Inc.
# ==============================================================================

# ------------------------------------------------------------------------------
# Stage 1: Build Environment & Workspace Compilation
# ------------------------------------------------------------------------------
FROM node:20-alpine AS builder

WORKDIR /build

# Install build dependencies for native Node.js modules (@serialport/bindings-cpp)
RUN apk add --no-cache python3 make gcc g++ linux-headers git udev

# Install PNPM globally
RUN corepack enable && corepack prepare pnpm@latest --activate

# Copy monorepo configuration
COPY package.json pnpm-lock.yaml pnpm-workspace.yaml ./

# Copy all package manifests first for efficient layer caching
COPY zigbee-herdsman/package.json ./zigbee-herdsman/
COPY zigbee-herdsman-converters/package.json ./zigbee-herdsman-converters/
COPY zigbee2mqtt/package.json ./zigbee2mqtt/
COPY zigbee2mqtt-frontend/package.json ./zigbee2mqtt-frontend/
COPY zigbee2mqtt-windfront/package.json ./zigbee2mqtt-windfront/

# Install workspace dependencies
RUN pnpm install --frozen-lockfile

# Copy all package sources
COPY zigbee-herdsman/ ./zigbee-herdsman/
COPY zigbee-herdsman-converters/ ./zigbee-herdsman-converters/
COPY zigbee2mqtt/ ./zigbee2mqtt/
COPY zigbee2mqtt-frontend/ ./zigbee2mqtt-frontend/
COPY zigbee2mqtt-windfront/ ./zigbee2mqtt-windfront/

# Rebuild native serialport bindings for the target architecture
RUN pnpm --filter zigbee-herdsman rebuild @serialport/bindings-cpp

# Build all workspace packages and custom DOM Windfront UI
RUN pnpm run build:all

# ------------------------------------------------------------------------------
# Stage 2: Production Hardened Runtime
# ------------------------------------------------------------------------------
FROM node:20-alpine AS runner

LABEL maintainer="DOM ECOSYSTEM DevOps <engineering@domecosystem.io>"
LABEL description="DOM Connect Gateway - Enterprise Zigbee Gateway Runtime"

ENV NODE_ENV=production
ENV ZIGBEE2MQTT_DATA=/app/data

WORKDIR /app

# Install runtime utilities (tini for PID 1 signal forwarding, eudev for serial auto-detection)
RUN apk add --no-cache tzdata eudev tini

# Copy built artifacts and production dependencies from builder
COPY --from=builder /build/node_modules ./node_modules
COPY --from=builder /build/package.json ./package.json
COPY --from=builder /build/pnpm-workspace.yaml ./pnpm-workspace.yaml
COPY --from=builder /build/zigbee-herdsman ./zigbee-herdsman
COPY --from=builder /build/zigbee-herdsman-converters ./zigbee-herdsman-converters
COPY --from=builder /build/zigbee2mqtt ./zigbee2mqtt
COPY --from=builder /build/zigbee2mqtt-frontend ./zigbee2mqtt-frontend
COPY --from=builder /build/zigbee2mqtt-windfront ./zigbee2mqtt-windfront

# Create persistent storage directory
RUN mkdir -p /app/data

# Default Web UI port
EXPOSE 8080

ENTRYPOINT ["/sbin/tini", "--"]
CMD ["node", "zigbee2mqtt/index.js"]
