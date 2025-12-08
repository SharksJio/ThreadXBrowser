# Website Streaming Feature

## Overview

The ThreadXBrowser now supports streaming website content from the web interface to ASR 3605 emulator devices. This feature allows you to:

1. Load any website in the browser
2. Preview it in the web interface
3. Stream the content to connected ASR emulator devices in real-time

## Architecture

```
┌─────────────┐        ┌──────────────┐        ┌────────────────┐
│  Web Browser│◄──────►│   Gateway    │◄──────►│ ASR Emulator   │
│  (Port 8081)│  HTTP  │  Server      │  WS    │  (Simulation)  │
│             │   WS   │  (Node.js)   │        │                │
└─────────────┘        └──────────────┘        └────────────────┘
```

### Components:

1. **Web Interface** (`webapp/index.html`)
   - URL input field
   - Website preview iframe
   - Stream quality selector
   - Stream controls (start/stop)

2. **Gateway Server** (`gateway/server.js`)
   - Relays messages between browser and devices
   - Handles WebSocket connections on ports 8081 and 9001

3. **Emulator** (`src/stream_handler.c`)
   - Receives streaming commands
   - Processes and displays stream information
   - Tracks stream statistics

## Usage

### 1. Start the System

```bash
docker-compose up -d
```

### 2. Access the Web Interface

Open your browser and navigate to:
```
http://localhost:8081
```

### 3. Stream a Website

1. **Load Website**:
   - Enter a URL (e.g., `https://example.com`)
   - Click "Load Website"
   - Preview appears in the interface

2. **Configure Stream**:
   - Select quality: High, Medium, or Low (Device Optimized)
   - Optionally specify a target device ID
   - Leave empty to broadcast to all devices

3. **Start Streaming**:
   - Click "Stream to Device"
   - Watch the emulator logs for stream output

4. **Monitor**:
   ```bash
   docker-compose logs -f emulator
   ```

5. **Stop Streaming**:
   - Click "Stop Stream" when done

## Stream Quality Options

| Quality | Description | Use Case |
|---------|-------------|----------|
| **High** | Full quality, higher data rate | Testing, demos |
| **Medium** | Balanced quality and performance | General use |
| **Low** | Optimized for device constraints | Production, IoT devices |

## Stream Commands

The system uses JSON commands for streaming control:

### Start Stream
```json
{
  "type": "streamStart",
  "url": "https://example.com",
  "quality": "medium",
  "timestamp": 1234567890
}
```

### Stream Update (Frame)
```json
{
  "type": "streamUpdate",
  "url": "https://example.com",
  "frame": 42,
  "quality": "medium",
  "timestamp": 1234567891
}
```

### Stop Stream
```json
{
  "type": "streamStop",
  "timestamp": 1234567892
}
```

## Emulator Output Example

When streaming is active, you'll see output like:

```
========================================
WEBSITE STREAM STARTED
========================================
URL:     https://example.com
Quality: medium
========================================

[STREAM] Frame 10 | Duration: 00:10 | URL: https://example.com
[STREAM] Frame 20 | Duration: 00:20 | URL: https://example.com
[STATUS] Stream: https://example.com | Frames: 25 | Duration: 00:25 | FPS: 1.0

========================================
WEBSITE STREAM STOPPED
========================================
Total Frames: 30
Duration:     00:30
========================================
```

## Testing

### Manual Testing

Use the web interface at http://localhost:8081

### Command-Line Testing

Send commands directly to the emulator:

```bash
# Start a stream
echo '{"type":"streamStart","url":"https://example.com","quality":"medium"}' | \
  docker exec -i threadxbrowser-emulator-1 sh -c 'cat'

# Stop the stream
echo '{"type":"streamStop"}' | \
  docker exec -i threadxbrowser-emulator-1 sh -c 'cat'
```

### Automated Test Script

Run the included test script:

```bash
./test_streaming.sh
```

## Technical Details

### Files Modified/Added

**New Files:**
- `src/stream_handler.c` - Stream processing logic
- `src/stream_handler.h` - Stream handler interface
- `test_streaming.sh` - Testing script

**Modified Files:**
- `webapp/index.html` - Added streaming UI
- `src/main.c` - Integrated stream handler
- `Makefile` - Added stream_handler.c to build

### Stream Statistics

The emulator tracks:
- Current URL
- Stream quality setting
- Total frames received
- Stream duration
- Frames per second (FPS)
- Active/inactive status

### Browser Compatibility

The iframe preview may be blocked by some websites due to:
- `X-Frame-Options` header
- `Content-Security-Policy` restrictions

For full functionality, use websites that allow iframe embedding or consider using a proxy service.

## Troubleshooting

### Website Won't Load in Preview

**Issue**: Some sites block iframe embedding

**Solutions**:
1. Try a different website
2. The stream will still work - preview is optional
3. Use developer tools to check console errors

### Stream Not Reaching Emulator

**Check**:
```bash
# Verify emulator is running
docker-compose ps

# Check emulator logs
docker-compose logs emulator

# Verify gateway is relaying messages
docker-compose logs gateway
```

### Port Conflicts

If ports 8081 or 9001 are in use, modify `docker-compose.yml`:

```yaml
ports:
  - "YOUR_PORT:8080"  # Change YOUR_PORT
```

## Future Enhancements

Potential improvements:

1. **Real WebSocket Connection**: Currently simulated, could use actual WebSocket client in emulator
2. **Screenshot Capture**: Capture actual website screenshots for streaming
3. **Binary Protocol**: More efficient binary streaming protocol
4. **Video Streaming**: Add video codec support
5. **Compression**: Add data compression for bandwidth optimization
6. **Multi-device Sync**: Synchronize playback across multiple devices

## API Reference

### Stream Handler Functions

```c
// Start streaming
void handle_stream_start(const char *url, const char *quality);

// Update stream (new frame)
void handle_stream_update(int frame);

// Stop streaming
void handle_stream_stop(void);

// Check stream status
int is_stream_active(void);

// Get statistics
void get_stream_stats(char *buffer, size_t size);

// Process JSON commands
void process_stream_command(const char *json);
```

## License

This feature is part of ThreadXBrowser and follows the same license as the main project.

## Support

For issues or questions:
1. Check the logs: `docker-compose logs -f`
2. Review this documentation
3. Open an issue on GitHub

---

**Last Updated**: December 3, 2025
