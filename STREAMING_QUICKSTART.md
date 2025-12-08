# 🌐 Quick Start: Website Streaming to ASR Emulator

## ✅ System Status

Your ThreadXBrowser is now deployed with **website streaming capability**!

## 🚀 Access the Application

**Web Interface**: http://localhost:8081

## 📖 How to Stream a Website

### Step 1: Load a Website

1. Open http://localhost:8081 in your browser
2. Find the **"Website Streaming"** section
3. Enter a URL (try these examples):
   - `https://example.com`
   - `https://www.wikipedia.org`
   - `https://httpbin.org`
4. Click **"Load Website"**
5. Preview appears below

### Step 2: Configure Stream

Choose stream quality:
- **High Quality**: Maximum detail
- **Medium Quality**: Balanced (recommended)
- **Low Quality**: Optimized for device

### Step 3: Start Streaming

1. Click **"Stream to Device"** button
2. Streaming begins immediately
3. Monitor the emulator output

### Step 4: Monitor Stream

Open a new terminal and watch the emulator:

```bash
cd /Users/sharath.ks/Project/Module/lowBudget/ThreadXBrowser
docker-compose logs -f emulator
```

You'll see output like:

```
========================================
WEBSITE STREAM STARTED
========================================
URL:     https://example.com
Quality: medium
========================================

[STREAM] Frame 10 | Duration: 00:10 | URL: https://example.com
[STREAM] Frame 20 | Duration: 00:20 | URL: https://example.com
```

### Step 5: Stop Streaming

Click **"Stop Stream"** when done.

## 🎯 Example Streaming Session

```bash
# Terminal 1: Watch logs
docker-compose logs -f emulator

# Browser: Load and stream https://example.com

# You'll see real-time output in Terminal 1
```

## 🔍 Check System Status

```bash
# View all services
docker-compose ps

# View logs
docker-compose logs gateway     # Gateway server
docker-compose logs emulator    # Emulator output
docker-compose logs -f          # Follow all logs

# Restart services
docker-compose restart

# Stop services
docker-compose down

# Rebuild and restart
docker-compose up --build -d
```

## 📊 Web Interface Features

### 🌐 Website Streaming
- URL input with validation
- Live preview iframe
- Quality selector (High/Medium/Low)
- Stream controls

### 📱 Device Management
- View connected devices
- Device status indicators
- Click device to select target

### 📨 Message Log
- Real-time message history
- Color-coded log levels
- Auto-scroll to latest

### 🎮 Manual Controls
- Send custom JSON commands
- Broadcast or target specific device
- Ping/Stats commands

## 🛠️ Troubleshooting

### Problem: Website preview is blank

**Cause**: Website blocks iframe embedding

**Solution**: The stream will still work! The preview is optional. Try a different URL or check browser console for errors.

### Problem: Stream not visible in emulator

**Check**:
```bash
# 1. Verify emulator is running
docker-compose ps

# 2. Check logs for errors
docker-compose logs emulator

# 3. Verify gateway is connected
docker-compose logs gateway | grep -i device
```

### Problem: Port already in use

**Solution**: Stop other services or change ports in `docker-compose.yml`

## 📚 Documentation

- **Streaming Guide**: `docs/STREAMING.md`
- **Deployment Info**: `DEPLOYMENT.md`
- **Architecture**: `README.md`

## 🧪 Testing

Run automated test:

```bash
./test_streaming.sh
```

Send manual commands:

```bash
# Start stream
echo '{"type":"streamStart","url":"https://example.com","quality":"medium"}' | \
  docker exec -i threadxbrowser-emulator-1 sh -c 'cat'

# Stop stream
echo '{"type":"streamStop"}' | \
  docker exec -i threadxbrowser-emulator-1 sh -c 'cat'
```

## 🎉 What's Working

✅ Web interface with streaming controls
✅ Gateway server relaying messages
✅ Emulator receiving and displaying streams
✅ Real-time log monitoring
✅ Multiple quality settings
✅ Device selection (broadcast or target)

## 🔮 Architecture

```
┌──────────────────┐
│   Your Browser   │
│  localhost:8081  │
└────────┬─────────┘
         │ HTTP + WebSocket
         ▼
┌──────────────────┐
│  Gateway Server  │
│   (Node.js)      │
│  Ports: 8081     │
│         9001     │
└────────┬─────────┘
         │ WebSocket
         ▼
┌──────────────────┐
│  ASR Emulator    │
│  (C Simulation)  │
│  Stream Handler  │
└──────────────────┘
```

## 🎓 Next Steps

1. **Try streaming different websites**
2. **Monitor the emulator logs in real-time**
3. **Experiment with different quality settings**
4. **Try the manual controls for custom commands**
5. **Check out the full documentation in `docs/STREAMING.md`**

---

**Status**: ✅ Running
**Access**: http://localhost:8081
**Updated**: December 3, 2025
