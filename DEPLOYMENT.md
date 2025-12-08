# ThreadXBrowser Docker Deployment

## ✅ Deployment Status: SUCCESSFUL

Your ThreadXBrowser application has been successfully deployed using Docker!

## 🚀 Services Running

### Gateway Server
- **Container**: `threadxbrowser-gateway-1`
- **Status**: Healthy ✓
- **Browser WebSocket**: http://localhost:8081
- **Device WebSocket**: ws://localhost:9001
- **Web App**: http://localhost:8081

### Emulator
- **Container**: `threadxbrowser-emulator-1`
- **Status**: Running ✓
- **Mode**: Host Simulation

## 📝 Port Mappings

| Service | Host Port | Container Port | Description |
|---------|-----------|----------------|-------------|
| Gateway | 8081 | 8080 | Browser WebSocket + Web App |
| Gateway | 9001 | 9000 | Device WebSocket |

*Note: Ports were changed from 8080→8081 and 9000→9001 to avoid conflicts with other services on your system.*

## 🎯 Access URLs

- **Web Application**: http://localhost:8081
- **Browser WebSocket**: ws://localhost:8081
- **Device WebSocket**: ws://localhost:9001

## 📋 Docker Commands

### View running containers:
```bash
docker-compose ps
```

### View logs:
```bash
docker-compose logs -f          # Follow all logs
docker-compose logs gateway     # Gateway logs only
docker-compose logs emulator    # Emulator logs only
```

### Stop services:
```bash
docker-compose stop
```

### Start services:
```bash
docker-compose start
```

### Restart services:
```bash
docker-compose restart
```

### Stop and remove containers:
```bash
docker-compose down
```

### Rebuild and restart:
```bash
docker-compose up --build -d
```

## 🏗️ Build Information

### Changes Made for Docker Deployment:

1. **Created `.dockerignore` files** to optimize builds
2. **Updated gateway Dockerfile** to use `npm install --omit=dev`
3. **Fixed ThreadX/NetX header includes** to support host simulation
4. **Modified Makefile** to exclude ThreadX-specific files from simulation build
5. **Added simulation stubs** in main.c for host testing
6. **Updated port mappings** to avoid conflicts (8080→8081, 9000→9001)

### Architecture:

- **Gateway**: Node.js 20 on Alpine Linux
- **Emulator**: Ubuntu 22.04 with GCC build tools
- **Network**: Bridge network `threadxbrowser_threadx-net`

## 🔍 Health Checks

The gateway service has automatic health checks:
- Interval: 30 seconds
- Timeout: 3 seconds
- Retries: 3
- Start period: 5 seconds

## ⚠️ Important Notes

1. The emulator runs in **simulation mode** - it doesn't have full ThreadX/NetX functionality
2. For full ThreadX/NetX features, you need the actual libraries installed
3. The current setup is perfect for testing the WebSocket gateway functionality
4. For production deployment with ASR 3605 hardware, additional configuration is required

## 🐛 Troubleshooting

### Port already in use:
If you see port conflicts, update the ports in `docker-compose.yml`:
```yaml
ports:
  - "YOUR_PORT:8080"  # Change YOUR_PORT to an available port
```

### View container status:
```bash
docker ps -a | grep threadxbrowser
```

### Inspect container:
```bash
docker logs threadxbrowser-gateway-1
docker logs threadxbrowser-emulator-1
```

### Clean restart:
```bash
docker-compose down -v
docker-compose up --build -d
```

## 📚 Next Steps

1. Open http://localhost:8081 in your browser to access the web app
2. Test the WebSocket connections
3. Monitor logs with `docker-compose logs -f`
4. For production deployment, review security settings and environment variables

---

**Deployment Date**: December 3, 2025
**Docker Compose Version**: 3.8
**Status**: ✅ Running
