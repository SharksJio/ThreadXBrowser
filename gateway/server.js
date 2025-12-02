/**
 * ThreadXBrowser Gateway Server
 * 
 * WebSocket gateway that bridges browser clients to ASR 3605 devices.
 * 
 * Architecture:
 * - Port 8080: Browser WebSocket connections
 * - Port 9000: Device WebSocket connections (ASR 3605)
 * 
 * Protocol:
 * - Browser <-> Gateway: Standard WebSocket with JSON messages
 * - Device <-> Gateway: Compact binary protocol
 */

const WebSocket = require('ws');
const http = require('http');
const fs = require('fs');
const path = require('path');

// Configuration
const CONFIG = {
    browserPort: process.env.BROWSER_PORT || 8080,
    devicePort: process.env.DEVICE_PORT || 9000,
    heartbeatInterval: 30000,  // 30 seconds
    maxPayloadSize: 64 * 1024, // 64KB
};

// Message types for device protocol
const MSG_TYPE = {
    JSON_TEXT: 0x01,
    BINARY_DATA: 0x02,
    CONTROL: 0x03,
    PING_PONG: 0x04,
};

// Client tracking
const browserClients = new Map();
const deviceClients = new Map();
let clientIdCounter = 0;

// Statistics
const stats = {
    browserConnections: 0,
    deviceConnections: 0,
    messagesRelayed: 0,
    bytesTransferred: 0,
    errors: 0,
};

/**
 * Encode message to device binary protocol
 * Format: [Length (4B BE)] [Type (1B)] [Payload (var)]
 */
function encodeDeviceMessage(type, payload) {
    const payloadBuffer = Buffer.isBuffer(payload) ? payload : Buffer.from(payload);
    const length = payloadBuffer.length + 1; // +1 for type byte
    
    const header = Buffer.alloc(5);
    header.writeUInt32BE(length, 0);
    header.writeUInt8(type, 4);
    
    return Buffer.concat([header, payloadBuffer]);
}

/**
 * Decode device binary protocol message
 */
function decodeDeviceMessage(data) {
    if (data.length < 5) {
        throw new Error('Message too short');
    }
    
    const length = data.readUInt32BE(0);
    const type = data.readUInt8(4);
    const payload = data.slice(5);
    
    if (payload.length !== length - 1) {
        throw new Error(`Payload length mismatch: expected ${length - 1}, got ${payload.length}`);
    }
    
    return { type, payload };
}

/**
 * Create HTTP server for serving static files (webapp)
 */
const httpServer = http.createServer((req, res) => {
    // Serve webapp files
    let filePath = req.url === '/' ? '/index.html' : req.url;
    
    // Prevent directory traversal attacks
    const safePath = path.normalize(filePath).replace(/^(\.\.[\/\\])+/, '');
    const webappPath = path.join(__dirname, '..', 'webapp', safePath);
    
    // Check if file exists
    if (fs.existsSync(webappPath) && fs.statSync(webappPath).isFile()) {
        const ext = path.extname(webappPath).toLowerCase();
        const contentTypes = {
            '.html': 'text/html',
            '.css': 'text/css',
            '.js': 'application/javascript',
            '.json': 'application/json',
            '.png': 'image/png',
            '.jpg': 'image/jpeg',
            '.gif': 'image/gif',
            '.svg': 'image/svg+xml',
            '.ico': 'image/x-icon',
        };
        
        res.writeHead(200, { 'Content-Type': contentTypes[ext] || 'text/plain' });
        fs.createReadStream(webappPath).pipe(res);
    } else {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not Found');
    }
});

/**
 * Browser WebSocket server (Port 8080)
 */
const browserWss = new WebSocket.Server({ 
    server: httpServer,
    maxPayload: CONFIG.maxPayloadSize,
});

browserWss.on('connection', (ws, req) => {
    const clientId = `browser-${++clientIdCounter}`;
    const clientIp = req.socket.remoteAddress;
    
    console.log(`[BROWSER] Client connected: ${clientId} from ${clientIp}`);
    
    browserClients.set(clientId, {
        ws,
        id: clientId,
        ip: clientIp,
        connectedAt: Date.now(),
        messagesReceived: 0,
        messagesSent: 0,
    });
    
    stats.browserConnections++;
    
    // Send welcome message
    ws.send(JSON.stringify({
        type: 'welcome',
        clientId,
        deviceCount: deviceClients.size,
        timestamp: Date.now(),
    }));
    
    // Broadcast device status to new browser
    if (deviceClients.size > 0) {
        ws.send(JSON.stringify({
            type: 'deviceList',
            devices: Array.from(deviceClients.keys()),
        }));
    }
    
    ws.on('message', (data) => {
        const client = browserClients.get(clientId);
        if (client) {
            client.messagesReceived++;
        }
        
        try {
            const message = JSON.parse(data.toString());
            handleBrowserMessage(clientId, message);
        } catch (err) {
            console.error(`[BROWSER] Invalid message from ${clientId}:`, err.message);
            stats.errors++;
        }
    });
    
    ws.on('close', (code, reason) => {
        console.log(`[BROWSER] Client disconnected: ${clientId} (code: ${code})`);
        browserClients.delete(clientId);
    });
    
    ws.on('error', (err) => {
        console.error(`[BROWSER] Error for ${clientId}:`, err.message);
        stats.errors++;
    });
    
    // Setup heartbeat
    ws.isAlive = true;
    ws.on('pong', () => { ws.isAlive = true; });
});

/**
 * Device WebSocket server (Port 9000)
 */
const deviceWss = new WebSocket.Server({ 
    port: CONFIG.devicePort,
    maxPayload: CONFIG.maxPayloadSize,
});

deviceWss.on('connection', (ws, req) => {
    const clientId = `device-${++clientIdCounter}`;
    const clientIp = req.socket.remoteAddress;
    
    console.log(`[DEVICE] Client connected: ${clientId} from ${clientIp}`);
    
    deviceClients.set(clientId, {
        ws,
        id: clientId,
        ip: clientIp,
        connectedAt: Date.now(),
        info: {},
        messagesReceived: 0,
        messagesSent: 0,
    });
    
    stats.deviceConnections++;
    
    // Notify browsers of new device
    broadcastToBrowsers({
        type: 'deviceConnected',
        deviceId: clientId,
        timestamp: Date.now(),
    });
    
    ws.on('message', (data) => {
        const client = deviceClients.get(clientId);
        if (client) {
            client.messagesReceived++;
        }
        
        try {
            // Device messages can be binary protocol or plain text/JSON
            if (Buffer.isBuffer(data) && data.length >= 5) {
                // Try to decode as binary protocol
                try {
                    const decoded = decodeDeviceMessage(data);
                    handleDeviceMessage(clientId, decoded);
                } catch (decodeErr) {
                    // Fallback to treating as raw binary
                    handleDeviceMessage(clientId, { type: MSG_TYPE.BINARY_DATA, payload: data });
                }
            } else {
                // Treat as text message
                const message = data.toString();
                try {
                    const json = JSON.parse(message);
                    handleDeviceMessage(clientId, { type: MSG_TYPE.JSON_TEXT, payload: message, parsed: json });
                } catch (jsonErr) {
                    handleDeviceMessage(clientId, { type: MSG_TYPE.JSON_TEXT, payload: message });
                }
            }
        } catch (err) {
            console.error(`[DEVICE] Error processing message from ${clientId}:`, err.message);
            stats.errors++;
        }
    });
    
    ws.on('close', (code, reason) => {
        console.log(`[DEVICE] Client disconnected: ${clientId} (code: ${code})`);
        deviceClients.delete(clientId);
        
        // Notify browsers
        broadcastToBrowsers({
            type: 'deviceDisconnected',
            deviceId: clientId,
            timestamp: Date.now(),
        });
    });
    
    ws.on('error', (err) => {
        console.error(`[DEVICE] Error for ${clientId}:`, err.message);
        stats.errors++;
    });
    
    // Setup heartbeat
    ws.isAlive = true;
    ws.on('pong', () => { ws.isAlive = true; });
});

/**
 * Handle messages from browser clients
 */
function handleBrowserMessage(browserId, message) {
    console.log(`[BROWSER] Message from ${browserId}:`, message.type || 'unknown');
    stats.messagesRelayed++;
    
    switch (message.type) {
        case 'sendToDevice':
            // Forward message to specific device
            const targetDevice = deviceClients.get(message.deviceId);
            if (targetDevice && targetDevice.ws.readyState === WebSocket.OPEN) {
                const deviceMsg = encodeDeviceMessage(
                    MSG_TYPE.JSON_TEXT,
                    JSON.stringify(message.data)
                );
                targetDevice.ws.send(deviceMsg);
                targetDevice.messagesSent++;
                stats.bytesTransferred += deviceMsg.length;
            } else {
                // Send error back to browser
                const browser = browserClients.get(browserId);
                if (browser && browser.ws.readyState === WebSocket.OPEN) {
                    browser.ws.send(JSON.stringify({
                        type: 'error',
                        message: 'Device not connected',
                        deviceId: message.deviceId,
                    }));
                }
            }
            break;
            
        case 'broadcast':
            // Broadcast to all devices
            const broadcastMsg = encodeDeviceMessage(
                MSG_TYPE.JSON_TEXT,
                JSON.stringify(message.data)
            );
            deviceClients.forEach((device) => {
                if (device.ws.readyState === WebSocket.OPEN) {
                    device.ws.send(broadcastMsg);
                    device.messagesSent++;
                }
            });
            stats.bytesTransferred += broadcastMsg.length * deviceClients.size;
            break;
            
        case 'getStats':
            // Return server statistics
            const browser = browserClients.get(browserId);
            if (browser && browser.ws.readyState === WebSocket.OPEN) {
                browser.ws.send(JSON.stringify({
                    type: 'stats',
                    data: {
                        ...stats,
                        browserClientsCount: browserClients.size,
                        deviceClientsCount: deviceClients.size,
                        uptime: process.uptime(),
                    },
                }));
            }
            break;
            
        case 'ping':
            // Respond with pong
            const browserClient = browserClients.get(browserId);
            if (browserClient && browserClient.ws.readyState === WebSocket.OPEN) {
                browserClient.ws.send(JSON.stringify({
                    type: 'pong',
                    timestamp: Date.now(),
                }));
            }
            break;
            
        default:
            console.log(`[BROWSER] Unknown message type: ${message.type}`);
    }
}

/**
 * Handle messages from device clients
 */
function handleDeviceMessage(deviceId, message) {
    console.log(`[DEVICE] Message from ${deviceId}: type=${message.type}`);
    stats.messagesRelayed++;
    
    const device = deviceClients.get(deviceId);
    
    switch (message.type) {
        case MSG_TYPE.JSON_TEXT:
            // Forward JSON to all browsers
            const jsonPayload = message.parsed || JSON.parse(message.payload.toString());
            
            // Update device info if it's a hello message
            if (jsonPayload.type === 'hello' && device) {
                device.info = {
                    deviceType: jsonPayload.device,
                    version: jsonPayload.version,
                };
            }
            
            broadcastToBrowsers({
                type: 'deviceMessage',
                deviceId,
                data: jsonPayload,
                timestamp: Date.now(),
            });
            break;
            
        case MSG_TYPE.BINARY_DATA:
            // Forward binary data to browsers (as base64 for JSON transport)
            broadcastToBrowsers({
                type: 'deviceBinaryData',
                deviceId,
                data: message.payload.toString('base64'),
                size: message.payload.length,
                timestamp: Date.now(),
            });
            break;
            
        case MSG_TYPE.CONTROL:
            // Handle control messages
            console.log(`[DEVICE] Control message from ${deviceId}`);
            break;
            
        case MSG_TYPE.PING_PONG:
            // Handle ping/pong at protocol level
            if (device && device.ws.readyState === WebSocket.OPEN) {
                device.ws.send(encodeDeviceMessage(MSG_TYPE.PING_PONG, Buffer.alloc(0)));
            }
            break;
            
        default:
            console.log(`[DEVICE] Unknown message type: ${message.type}`);
    }
}

/**
 * Broadcast message to all browser clients
 */
function broadcastToBrowsers(message) {
    const jsonMsg = JSON.stringify(message);
    browserClients.forEach((client) => {
        if (client.ws.readyState === WebSocket.OPEN) {
            client.ws.send(jsonMsg);
            client.messagesSent++;
        }
    });
    stats.bytesTransferred += jsonMsg.length * browserClients.size;
}

/**
 * Heartbeat interval to detect stale connections
 */
const heartbeatInterval = setInterval(() => {
    // Check browser clients
    browserWss.clients.forEach((ws) => {
        if (ws.isAlive === false) {
            return ws.terminate();
        }
        ws.isAlive = false;
        ws.ping();
    });
    
    // Check device clients
    deviceWss.clients.forEach((ws) => {
        if (ws.isAlive === false) {
            return ws.terminate();
        }
        ws.isAlive = false;
        ws.ping();
    });
}, CONFIG.heartbeatInterval);

/**
 * Cleanup on server close
 */
browserWss.on('close', () => {
    clearInterval(heartbeatInterval);
});

deviceWss.on('close', () => {
    clearInterval(heartbeatInterval);
});

/**
 * Start servers
 */
httpServer.listen(CONFIG.browserPort, () => {
    console.log('========================================');
    console.log('ThreadXBrowser Gateway Server');
    console.log('========================================');
    console.log(`Browser WebSocket: ws://localhost:${CONFIG.browserPort}`);
    console.log(`Device WebSocket:  ws://localhost:${CONFIG.devicePort}`);
    console.log(`Web App:           http://localhost:${CONFIG.browserPort}`);
    console.log('========================================');
});

deviceWss.on('listening', () => {
    console.log(`Device server listening on port ${CONFIG.devicePort}`);
});

/**
 * Graceful shutdown
 */
process.on('SIGINT', () => {
    console.log('\nShutting down gateway server...');
    
    // Close all connections
    browserClients.forEach((client) => {
        client.ws.close(1001, 'Server shutting down');
    });
    
    deviceClients.forEach((client) => {
        client.ws.close(1001, 'Server shutting down');
    });
    
    browserWss.close();
    deviceWss.close();
    httpServer.close();
    
    console.log('Gateway server stopped');
    process.exit(0);
});

process.on('SIGTERM', () => {
    process.emit('SIGINT');
});
