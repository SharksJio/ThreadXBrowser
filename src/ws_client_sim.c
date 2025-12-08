/*
 * ws_client_sim.c
 * Simple WebSocket client for emulator simulation
 * Connects to gateway and processes stream commands
 */

#include "platform_config.h"
#include "stream_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#ifdef HOST_SIMULATION

#define BUFFER_SIZE 4096

typedef struct {
    int socket_fd;
    int connected;
    char host[256];
    int port;
} ws_client_t;

static ws_client_t client = {0};

/**
 * Connect to WebSocket server
 */
int ws_connect(const char *host, int port) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    
    /* Create socket */
    client.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client.socket_fd < 0) {
        printf("[WS] Error creating socket\n");
        return -1;
    }
    
    /* Get server address */
    server = gethostbyname(host);
    if (server == NULL) {
        printf("[WS] Error: Host not found: %s\n", host);
        close(client.socket_fd);
        return -1;
    }
    
    /* Setup server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);
    
    /* Connect to server */
    printf("[WS] Connecting to %s:%d...\n", host, port);
    if (connect(client.socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[WS] Connection failed\n");
        close(client.socket_fd);
        return -1;
    }
    
    /* Send WebSocket handshake */
    char handshake[512];
    snprintf(handshake, sizeof(handshake),
             "GET / HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "\r\n",
             host, port);
    
    if (send(client.socket_fd, handshake, strlen(handshake), 0) < 0) {
        printf("[WS] Failed to send handshake\n");
        close(client.socket_fd);
        return -1;
    }
    
    /* Receive handshake response */
    char response[1024];
    int bytes = recv(client.socket_fd, response, sizeof(response) - 1, 0);
    if (bytes <= 0) {
        printf("[WS] Failed to receive handshake response\n");
        close(client.socket_fd);
        return -1;
    }
    response[bytes] = '\0';
    
    /* Check if handshake was accepted */
    if (strstr(response, "101 Switching Protocols") == NULL) {
        printf("[WS] Handshake rejected\n");
        close(client.socket_fd);
        return -1;
    }
    
    strncpy(client.host, host, sizeof(client.host) - 1);
    client.port = port;
    client.connected = 1;
    
    printf("[WS] Connected successfully!\n");
    
    /* Send hello message */
    const char *hello = "{\"type\":\"hello\",\"device\":\"ASR3605-Emulator\",\"version\":\"1.0.0\"}";
    printf("[WS] Sending hello message: %s\n", hello);
    ws_send_text(hello);
    
    return 0;
}

/**
 * Send text message (simplified - just for simulation)
 */
int ws_send_text(const char *message) {
    if (!client.connected) {
        return -1;
    }
    
    /* Simple text frame: FIN=1, opcode=1 (text), MASK=1, length */
    int len = strlen(message);
    unsigned char header[14];  /* Max header size with masking */
    int header_len = 0;
    
    header[0] = 0x81; /* FIN + text opcode */
    header_len = 1;
    
    /* Generate random masking key */
    unsigned char mask_key[4];
    srand(time(NULL));
    for (int i = 0; i < 4; i++) {
        mask_key[i] = rand() % 256;
    }
    
    if (len < 126) {
        header[1] = 0x80 | len;  /* MASK bit set + length */
        memcpy(&header[2], mask_key, 4);
        header_len = 6;
    } else if (len < 65536) {
        header[1] = 0x80 | 126;  /* MASK bit set + extended length */
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        memcpy(&header[4], mask_key, 4);
        header_len = 8;
    }
    
    /* Mask payload */
    unsigned char *frame = malloc(header_len + len);
    memcpy(frame, header, header_len);
    
    for (int i = 0; i < len; i++) {
        frame[header_len + i] = message[i] ^ mask_key[i % 4];
    }
    
    /* Send complete frame in one call */
    int result = send(client.socket_fd, frame, header_len + len, 0);
    free(frame);
    
    return result;
}

/**
 * Receive and process messages
 */
int ws_receive() {
    if (!client.connected) {
        return -1;
    }
    
    unsigned char header[2];
    int bytes = recv(client.socket_fd, header, 2, MSG_DONTWAIT);
    
    if (bytes <= 0) {
        if (bytes == 0) {
            printf("[WS] Connection closed by server\n");
            client.connected = 0;
            return -1;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data available, not an error */
            return 0;
        } else {
            /* Real error */
            printf("[WS] Receive error: %d\n", errno);
            client.connected = 0;
            return -1;
        }
    }
    
    int fin = (header[0] >> 7) & 0x01;
    int opcode = header[0] & 0x0F;
    int masked = (header[1] >> 7) & 0x01;
    int payload_len = header[1] & 0x7F;
    
    /* Handle extended payload length */
    if (payload_len == 126) {
        unsigned char extended[2];
        recv(client.socket_fd, extended, 2, 0);
        payload_len = (extended[0] << 8) | extended[1];
    }
    
    /* Handle control frames first (they have no or small payloads) */
    if (opcode == 0x08) { /* Close frame */
        printf("[WS] Received close frame\n");
        client.connected = 0;
        return -1;
    } else if (opcode == 0x09) { /* Ping frame */
        printf("[WS] Received ping, sending pong\n");
        /* Send pong (opcode 0x0A) with masking */
        unsigned char pong[6];
        pong[0] = 0x8A;  /* FIN + pong opcode */
        pong[1] = 0x80;  /* MASK bit set + length 0 */
        /* Generate masking key */
        srand(time(NULL) ^ getpid());
        for (int i = 0; i < 4; i++) {
            pong[2 + i] = rand() % 256;
        }
        send(client.socket_fd, pong, 6, 0);
        return bytes;
    }
    
    /* Read payload for data frames */
    if (payload_len > 0 && payload_len < BUFFER_SIZE) {
        char payload[BUFFER_SIZE];
        int total = 0;
        while (total < payload_len) {
            int n = recv(client.socket_fd, payload + total, payload_len - total, 0);
            if (n <= 0) break;
            total += n;
        }
        payload[total] = '\0';
        
        /* Process based on opcode */
        if (opcode == 0x01) { /* Text frame */
            printf("[WS] Received: %s\n", payload);
            /* Process stream commands */
            if (strstr(payload, "streamStart") || 
                strstr(payload, "streamUpdate") || 
                strstr(payload, "streamStop")) {
                process_stream_command(payload);
            }
        }
    }
    
    return bytes;
}

/**
 * Disconnect from server
 */
void ws_disconnect() {
    if (client.connected) {
        /* Send close frame */
        unsigned char close_frame[] = {0x88, 0x00};
        send(client.socket_fd, close_frame, 2, 0);
        
        close(client.socket_fd);
        client.connected = 0;
        printf("[WS] Disconnected\n");
    }
}

/**
 * Check if connected
 */
int ws_is_connected() {
    return client.connected;
}

#endif /* HOST_SIMULATION */
