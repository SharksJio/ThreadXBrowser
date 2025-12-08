#!/bin/bash
# run_emulator.sh
# Script to run the ThreadX WebSocket Browser Client emulator

set -e

# Default configuration
WS_HOST="${WS_SERVER_HOST:-localhost}"
WS_PORT="${WS_SERVER_PORT:-9001}"

echo "=========================================="
echo "ThreadXBrowser Emulator"
echo "=========================================="
echo "WebSocket Server: ${WS_HOST}:${WS_PORT}"
echo "=========================================="

# Check if binary exists
BINARY="./build/threadx_browser_sim"

if [ ! -f "$BINARY" ]; then
    echo "Building emulator..."
    make host-sim
fi

# Wait for gateway to be ready (in Docker)
if [ -n "$WAIT_FOR_GATEWAY" ]; then
    echo "Waiting for gateway server..."
    MAX_RETRIES=30
    RETRY=0
    while ! nc -z "$WS_HOST" "$WS_PORT" 2>/dev/null; do
        RETRY=$((RETRY + 1))
        if [ $RETRY -ge $MAX_RETRIES ]; then
            echo "Timeout waiting for gateway server"
            exit 1
        fi
        echo "  Waiting... ($RETRY/$MAX_RETRIES)"
        sleep 1
    done
    echo "Gateway is ready!"
fi

# Run the emulator
echo ""
echo "Starting emulator..."
echo "Press Ctrl+C to exit"
echo ""

exec "$BINARY"
