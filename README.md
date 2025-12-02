# ThreadXBrowser

A ThreadX-based WebSocket browser client for ASR 3605 that connects to cloud apps using WebSocket and streaming protocols. Designed for embedded RTOS environments with support for GitHub Codespaces and local Docker development.

## Architecture

```
┌─────────────────┐     WebSocket      ┌─────────────────┐     WebSocket     ┌─────────────────┐
│   Web Browser   │◄──────────────────►│  Node.js        │◄─────────────────►│  ASR 3605       │
│   (webapp/)     │    Port 8080       │  Gateway        │    Port 9000      │  ThreadX Client │
└─────────────────┘                    │  (gateway/)     │                   │  (src/)         │
                                       └─────────────────┘                   └─────────────────┘
```

## Features

- **RFC6455 Compliant WebSocket Client** - Full handshake validation, frame masking, text/binary support
- **ThreadX + NetX Integration** - Optimized for RTOS with minimal memory footprint
- **TLS Support (WSS)** - Optional NetX Secure + mbedTLS integration
- **Node.js Gateway** - Translates browser traffic to compact device protocol
- **GitHub Codespaces Ready** - Full dev environment with ARM toolchain
- **Docker Support** - Local development with docker-compose

## Quick Start

### GitHub Codespaces (Recommended)

1. Click **Code** → **Codespaces** → **Create codespace on main**
2. Wait for container to build (includes ARM toolchain + Node.js)
3. Start the gateway:
   ```bash
   cd gateway && npm install && npm start
   ```
4. In a new terminal, build and run the emulator:
   ```bash
   make host-sim
   ./build/threadx_browser_sim
   ```
5. Open `webapp/index.html` in the browser (use Codespaces port forward for 8080)

### Local Docker Development

1. Clone the repository:
   ```bash
   git clone https://github.com/SharksJio/ThreadXBrowser.git
   cd ThreadXBrowser
   ```

2. Start the full stack:
   ```bash
   docker-compose up --build
   ```

3. Open http://localhost:8080 for the web app

## Project Structure

```
ThreadXBrowser/
├── .devcontainer/          # GitHub Codespaces configuration
│   ├── devcontainer.json
│   └── Dockerfile
├── src/                    # ThreadX WebSocket client source
│   ├── main.c              # Application entry point
│   ├── websocket_client.c  # WebSocket implementation
│   ├── websocket_client.h
│   ├── threadx_netx_init.c # ThreadX/NetX initialization
│   ├── threadx_netx_init.h
│   └── platform_config.h   # Platform-specific configuration
├── gateway/                # Node.js gateway server
│   ├── server.js
│   ├── package.json
│   └── Dockerfile
├── emulator/               # ThreadX emulator setup
│   ├── Dockerfile
│   └── run_emulator.sh
├── webapp/                 # Browser test client
│   └── index.html
├── docs/                   # Documentation
│   └── netx_wss_integration.md
├── docker-compose.yml      # Local Docker stack
├── Makefile                # Build system
└── README.md
```

## Building

### Host Simulation (for testing without hardware)

```bash
make host-sim
./build/threadx_browser_sim
```

### Cross-compile for ASR 3605

```bash
make asr3605
```

### Clean build

```bash
make clean
```

## Configuration

Edit `src/platform_config.h` to configure:

- `WS_SERVER_HOST` - Gateway server IP/hostname
- `WS_SERVER_PORT` - Gateway device port (default 9000)
- `WS_USE_TLS` - Enable/disable TLS (WSS)
- `THREADX_STACK_SIZE` - Thread stack sizes
- `NETX_PACKET_POOL_SIZE` - Network buffer pool size

## Protocol

The gateway uses a compact binary protocol for device communication:

```
Device Frame Format:
┌──────────────┬──────────┬─────────────────┐
│ Length (4B)  │ Type (1B)│ Payload (var)   │
│ Big-endian   │          │                 │
└──────────────┴──────────┴─────────────────┘

Types:
  0x01 - JSON/Text message
  0x02 - Binary data (audio/video chunks)
  0x03 - Control command
  0x04 - Ping/Pong
```

## Testing

### Test with wscat

```bash
# Connect as browser client
npx wscat -c ws://localhost:8080

# Connect as device client (in another terminal)
npx wscat -c ws://localhost:9000
```

### Run unit tests

```bash
make test
```

## Documentation

- [NetX Secure (WSS) Integration Guide](docs/netx_wss_integration.md)
- [Protocol Specification](docs/protocol.md)
- [Troubleshooting](docs/troubleshooting.md)

## Requirements

- **For Codespaces/Docker**: Just a browser or Docker installed
- **For local native build**:
  - ARM GCC toolchain (arm-none-eabi-gcc)
  - ThreadX and NetX libraries
  - mbedTLS (for TLS support)

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## Acknowledgments

- Azure RTOS (ThreadX, NetX, NetX Secure)
- mbedTLS for cryptographic functions
- ASR Microelectronics for ASR 3605 platform
