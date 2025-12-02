/*
 * websocket_client.c
 * RFC6455 Compliant WebSocket Client Implementation for ThreadX/NetX
 * Target: ASR 3605
 */

#include "websocket_client.h"
#include "threadx_netx_init.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef HOST_SIMULATION
#include <time.h>
#endif

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static ws_error_t ws_tcp_connect(ws_client_t *client);
static ws_error_t ws_perform_handshake(ws_client_t *client);
static ws_error_t ws_send_frame(ws_client_t *client, UINT opcode, const UCHAR *data, UINT length, UINT fin);
static ws_error_t ws_receive_frame(ws_client_t *client, ws_frame_t *frame);
static void ws_generate_key(UCHAR *key, UINT len);
static void ws_compute_accept_key(const UCHAR *key, UCHAR *accept);
static void ws_mask_payload(UCHAR *data, UINT length, const UCHAR *mask_key);
static UINT ws_parse_http_response(const char *response, const char *expected_accept);
static void ws_set_state(ws_client_t *client, ws_state_t state);

/* Simple Base64 encoding */
static void base64_encode(const UCHAR *input, UINT input_len, UCHAR *output);

/* Simple SHA1 implementation for WebSocket handshake */
static void sha1_compute(const UCHAR *data, UINT len, UCHAR *hash);

/* ============================================================================
 * Base64 Encoding Table
 * ============================================================================ */

static const char base64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

ws_error_t ws_client_init(ws_client_t *client, NX_IP *ip_ptr, NX_PACKET_POOL *pool_ptr)
{
    UINT status;
    
    if (client == NULL || ip_ptr == NULL || pool_ptr == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    /* Clear client context */
    memset(client, 0, sizeof(ws_client_t));
    
    /* Store network resources */
    client->ip_ptr = ip_ptr;
    client->packet_pool_ptr = pool_ptr;
    client->state = WS_STATE_DISCONNECTED;
    
    /* Create client mutex */
    status = tx_mutex_create(&client->mutex, "ws_client_mutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("Failed to create mutex: %u", status);
        return WS_ERROR_NO_MEMORY;
    }
    
    /* Set default configuration */
    ws_config_set_defaults(&client->config);
    
    DEBUG_INFO("WebSocket client initialized");
    return WS_OK;
}

void ws_config_set_defaults(ws_config_t *config)
{
    if (config == NULL) return;
    
    config->host = WS_SERVER_HOST;
    config->port = WS_SERVER_PORT;
    config->path = WS_SERVER_PATH;
    config->origin = NULL;
    config->protocols = NULL;
    config->connect_timeout = WS_CONNECT_TIMEOUT;
    config->handshake_timeout = WS_HANDSHAKE_TIMEOUT;
    config->receive_timeout = WS_RECEIVE_TIMEOUT;
    config->ping_interval = WS_PING_INTERVAL;
    config->auto_reconnect = WS_RECONNECT_ENABLED;
    config->reconnect_delay_ms = WS_RECONNECT_DELAY_MS;
    config->max_reconnect = WS_MAX_RECONNECT_ATTEMPTS;
}

ws_error_t ws_client_configure(ws_client_t *client, const ws_config_t *config)
{
    if (client == NULL || config == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    tx_mutex_get(&client->mutex, TX_WAIT_FOREVER);
    memcpy(&client->config, config, sizeof(ws_config_t));
    tx_mutex_put(&client->mutex);
    
    return WS_OK;
}

ws_error_t ws_client_connect(ws_client_t *client)
{
    ws_error_t result;
    UINT status;
    
    if (client == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    tx_mutex_get(&client->mutex, TX_WAIT_FOREVER);
    
    if (client->state != WS_STATE_DISCONNECTED) {
        tx_mutex_put(&client->mutex);
        DEBUG_WARN("Client already connected or connecting");
        return WS_OK;
    }
    
    ws_set_state(client, WS_STATE_CONNECTING);
    client->reconnect_count = 0;
    
    /* Create TCP socket */
    status = nx_tcp_socket_create(client->ip_ptr, &client->tcp_socket,
                                   "WS Client Socket",
                                   NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                   NX_IP_TIME_TO_LIVE, NETX_TCP_WINDOW_SIZE,
                                   NX_NULL, NX_NULL);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Failed to create TCP socket: %u", status);
        ws_set_state(client, WS_STATE_ERROR);
        client->last_error = WS_ERROR_SOCKET_CREATE;
        tx_mutex_put(&client->mutex);
        return WS_ERROR_SOCKET_CREATE;
    }
    
    /* Bind to any available port */
    status = nx_tcp_client_socket_bind(&client->tcp_socket, NX_ANY_PORT, 
                                        client->config.connect_timeout);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Failed to bind socket: %u", status);
        nx_tcp_socket_delete(&client->tcp_socket);
        ws_set_state(client, WS_STATE_ERROR);
        client->last_error = WS_ERROR_SOCKET_CREATE;
        tx_mutex_put(&client->mutex);
        return WS_ERROR_SOCKET_CREATE;
    }
    
    tx_mutex_put(&client->mutex);
    
    /* Connect to server */
    result = ws_tcp_connect(client);
    if (result != WS_OK) {
        return result;
    }
    
    /* Perform WebSocket handshake */
    result = ws_perform_handshake(client);
    if (result != WS_OK) {
        ws_client_disconnect(client, WS_CLOSE_PROTOCOL_ERROR, "Handshake failed");
        return result;
    }
    
    tx_mutex_get(&client->mutex, TX_WAIT_FOREVER);
    ws_set_state(client, WS_STATE_CONNECTED);
    client->running = 1;
    tx_mutex_put(&client->mutex);
    
    DEBUG_INFO("WebSocket connection established");
    return WS_OK;
}

ws_error_t ws_client_disconnect(ws_client_t *client, UINT status_code, const char *reason)
{
    UCHAR close_payload[125];
    UINT payload_len = 0;
    
    if (client == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    tx_mutex_get(&client->mutex, TX_WAIT_FOREVER);
    
    if (client->state == WS_STATE_DISCONNECTED) {
        tx_mutex_put(&client->mutex);
        return WS_OK;
    }
    
    if (client->state == WS_STATE_CONNECTED || client->state == WS_STATE_HANDSHAKE) {
        ws_set_state(client, WS_STATE_CLOSING);
        tx_mutex_put(&client->mutex);
        
        /* Build close payload */
        close_payload[0] = (status_code >> 8) & 0xFF;
        close_payload[1] = status_code & 0xFF;
        payload_len = 2;
        
        if (reason != NULL) {
            UINT reason_len = strlen(reason);
            if (reason_len > 123) reason_len = 123;
            memcpy(&close_payload[2], reason, reason_len);
            payload_len += reason_len;
        }
        
        /* Send close frame */
        ws_send_frame(client, WS_OPCODE_CLOSE, close_payload, payload_len, 1);
        
        tx_mutex_get(&client->mutex, TX_WAIT_FOREVER);
    }
    
    client->running = 0;
    
    /* Close TCP connection */
    nx_tcp_socket_disconnect(&client->tcp_socket, NX_WAIT_FOREVER);
    nx_tcp_client_socket_unbind(&client->tcp_socket);
    nx_tcp_socket_delete(&client->tcp_socket);
    
    ws_set_state(client, WS_STATE_DISCONNECTED);
    tx_mutex_put(&client->mutex);
    
    DEBUG_INFO("WebSocket disconnected");
    return WS_OK;
}

ws_error_t ws_client_send_text(ws_client_t *client, const char *message, UINT length)
{
    if (client == NULL || message == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    if (length == 0) {
        length = strlen(message);
    }
    
    return ws_send_frame(client, WS_OPCODE_TEXT, (const UCHAR *)message, length, 1);
}

ws_error_t ws_client_send_binary(ws_client_t *client, const UCHAR *data, UINT length)
{
    if (client == NULL || data == NULL || length == 0) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    return ws_send_frame(client, WS_OPCODE_BINARY, data, length, 1);
}

ws_error_t ws_client_send_ping(ws_client_t *client, const UCHAR *data, UINT length)
{
    if (client == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    if (length > 125) {
        return WS_ERROR_FRAME_TOO_LARGE;
    }
    
    return ws_send_frame(client, WS_OPCODE_PING, data, length, 1);
}

ws_error_t ws_client_send_pong(ws_client_t *client, const UCHAR *data, UINT length)
{
    if (client == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    if (length > 125) {
        return WS_ERROR_FRAME_TOO_LARGE;
    }
    
    return ws_send_frame(client, WS_OPCODE_PONG, data, length, 1);
}

ws_error_t ws_client_process(ws_client_t *client)
{
    ws_frame_t frame;
    ws_error_t result;
    
    if (client == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    
    if (client->state != WS_STATE_CONNECTED) {
        return WS_ERROR_CONNECTION_CLOSED;
    }
    
    /* Receive and process frame */
    result = ws_receive_frame(client, &frame);
    if (result != WS_OK) {
        return result;
    }
    
    /* Handle frame based on opcode */
    switch (frame.opcode) {
        case WS_OPCODE_TEXT:
            if (client->on_text != NULL) {
                /* Ensure null termination for text */
                if (frame.payload_len < WS_MAX_FRAME_SIZE) {
                    frame.payload[frame.payload_len] = '\0';
                }
                client->on_text((const char *)frame.payload, frame.payload_len);
            }
            DEBUG_DBG("Received text message: %u bytes", (UINT)frame.payload_len);
            break;
            
        case WS_OPCODE_BINARY:
            if (client->on_binary != NULL) {
                client->on_binary(frame.payload, frame.payload_len);
            }
            DEBUG_DBG("Received binary message: %u bytes", (UINT)frame.payload_len);
            break;
            
        case WS_OPCODE_PING:
            DEBUG_DBG("Received ping");
            if (client->on_ping != NULL) {
                client->on_ping(frame.payload, frame.payload_len);
            }
            /* Auto-respond with pong */
            ws_client_send_pong(client, frame.payload, frame.payload_len);
            break;
            
        case WS_OPCODE_PONG:
            DEBUG_DBG("Received pong");
            if (client->on_pong != NULL) {
                client->on_pong(frame.payload, frame.payload_len);
            }
            break;
            
        case WS_OPCODE_CLOSE:
            DEBUG_INFO("Received close frame");
            {
                UINT close_code = WS_CLOSE_NO_STATUS;
                const char *reason = "";
                
                if (frame.payload_len >= 2) {
                    close_code = (frame.payload[0] << 8) | frame.payload[1];
                    if (frame.payload_len > 2) {
                        reason = (const char *)&frame.payload[2];
                    }
                }
                
                if (client->on_close != NULL) {
                    client->on_close(close_code, reason);
                }
                
                /* Send close response if we didn't initiate */
                if (client->state == WS_STATE_CONNECTED) {
                    ws_client_disconnect(client, close_code, NULL);
                }
            }
            return WS_ERROR_CONNECTION_CLOSED;
            
        case WS_OPCODE_CONTINUATION:
            /* Handle fragmented messages */
            DEBUG_DBG("Received continuation frame");
            break;
            
        default:
            DEBUG_WARN("Unknown opcode: 0x%02X", frame.opcode);
            break;
    }
    
    return WS_OK;
}

ws_state_t ws_client_get_state(ws_client_t *client)
{
    if (client == NULL) {
        return WS_STATE_ERROR;
    }
    return client->state;
}

ws_error_t ws_client_get_error(ws_client_t *client)
{
    if (client == NULL) {
        return WS_ERROR_INVALID_PARAM;
    }
    return client->last_error;
}

UINT ws_client_is_connected(ws_client_t *client)
{
    if (client == NULL) {
        return 0;
    }
    return (client->state == WS_STATE_CONNECTED) ? 1 : 0;
}

void ws_client_set_connect_callback(ws_client_t *client, ws_connect_callback_t callback)
{
    if (client != NULL) {
        client->on_connect = callback;
    }
}

void ws_client_set_text_callback(ws_client_t *client, ws_text_callback_t callback)
{
    if (client != NULL) {
        client->on_text = callback;
    }
}

void ws_client_set_binary_callback(ws_client_t *client, ws_binary_callback_t callback)
{
    if (client != NULL) {
        client->on_binary = callback;
    }
}

void ws_client_set_ping_callback(ws_client_t *client, ws_ping_callback_t callback)
{
    if (client != NULL) {
        client->on_ping = callback;
    }
}

void ws_client_set_pong_callback(ws_client_t *client, ws_pong_callback_t callback)
{
    if (client != NULL) {
        client->on_pong = callback;
    }
}

void ws_client_set_close_callback(ws_client_t *client, ws_close_callback_t callback)
{
    if (client != NULL) {
        client->on_close = callback;
    }
}

void ws_client_cleanup(ws_client_t *client)
{
    if (client == NULL) {
        return;
    }
    
    /* Disconnect if connected */
    if (client->state != WS_STATE_DISCONNECTED) {
        ws_client_disconnect(client, WS_CLOSE_GOING_AWAY, "Cleanup");
    }
    
    /* Delete mutex */
    tx_mutex_delete(&client->mutex);
    
    /* Free message buffer if allocated */
    if (client->current_message.data != NULL) {
        /* In a real implementation, use tx_byte_release() */
        client->current_message.data = NULL;
    }
    
    DEBUG_INFO("WebSocket client cleaned up");
}

const char *ws_error_to_string(ws_error_t error)
{
    switch (error) {
        case WS_OK:                         return "Success";
        case WS_ERROR_INVALID_PARAM:        return "Invalid parameter";
        case WS_ERROR_NO_MEMORY:            return "Out of memory";
        case WS_ERROR_SOCKET_CREATE:        return "Socket creation failed";
        case WS_ERROR_CONNECT_FAILED:       return "Connection failed";
        case WS_ERROR_HANDSHAKE_FAILED:     return "Handshake failed";
        case WS_ERROR_HANDSHAKE_TIMEOUT:    return "Handshake timeout";
        case WS_ERROR_INVALID_RESPONSE:     return "Invalid server response";
        case WS_ERROR_SEND_FAILED:          return "Send failed";
        case WS_ERROR_RECEIVE_FAILED:       return "Receive failed";
        case WS_ERROR_RECEIVE_TIMEOUT:      return "Receive timeout";
        case WS_ERROR_FRAME_TOO_LARGE:      return "Frame too large";
        case WS_ERROR_PROTOCOL_ERROR:       return "Protocol error";
        case WS_ERROR_CONNECTION_CLOSED:    return "Connection closed";
        case WS_ERROR_TLS_INIT_FAILED:      return "TLS initialization failed";
        case WS_ERROR_TLS_HANDSHAKE_FAILED: return "TLS handshake failed";
        default:                            return "Unknown error";
    }
}

const char *ws_state_to_string(ws_state_t state)
{
    switch (state) {
        case WS_STATE_DISCONNECTED: return "Disconnected";
        case WS_STATE_CONNECTING:   return "Connecting";
        case WS_STATE_HANDSHAKE:    return "Handshake";
        case WS_STATE_CONNECTED:    return "Connected";
        case WS_STATE_CLOSING:      return "Closing";
        case WS_STATE_ERROR:        return "Error";
        default:                    return "Unknown";
    }
}

/* ============================================================================
 * Private Function Implementations
 * ============================================================================ */

static void ws_set_state(ws_client_t *client, ws_state_t state)
{
    ws_state_t old_state = client->state;
    client->state = state;
    
    DEBUG_DBG("State change: %s -> %s", 
              ws_state_to_string(old_state), 
              ws_state_to_string(state));
    
    if (client->on_connect != NULL && old_state != state) {
        client->on_connect(state, client->last_error);
    }
}

static ws_error_t ws_tcp_connect(ws_client_t *client)
{
    UINT status;
    ULONG server_ip;
    
    /* Parse server IP address */
    /* For simplicity, assuming IP is already in dotted decimal format */
    /* In production, use DNS resolution via nx_dns_host_by_name_get() */
    {
        int a, b, c, d;
        if (sscanf(client->config.host, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
            /* Validate IP address octets are in valid range */
            if (a < 0 || a > 255 || b < 0 || b > 255 || 
                c < 0 || c > 255 || d < 0 || d > 255) {
                DEBUG_ERR("Invalid IP address range: %s", client->config.host);
                return WS_ERROR_INVALID_PARAM;
            }
            server_ip = IP_ADDRESS(a, b, c, d);
        } else {
            DEBUG_ERR("Invalid server IP format: %s", client->config.host);
            return WS_ERROR_INVALID_PARAM;
        }
    }
    
    DEBUG_INFO("Connecting to %s:%u", client->config.host, client->config.port);
    
    /* Connect to server */
    status = nx_tcp_client_socket_connect(&client->tcp_socket, server_ip,
                                           client->config.port,
                                           client->config.connect_timeout);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("TCP connect failed: %u", status);
        client->last_error = WS_ERROR_CONNECT_FAILED;
        return WS_ERROR_CONNECT_FAILED;
    }
    
    DEBUG_INFO("TCP connection established");
    return WS_OK;
}

static ws_error_t ws_perform_handshake(ws_client_t *client)
{
    UINT status;
    NX_PACKET *packet_ptr;
    int len;
    UCHAR expected_accept[28];
    ULONG bytes_received;
    
    tx_mutex_get(&client->mutex, TX_WAIT_FOREVER);
    ws_set_state(client, WS_STATE_HANDSHAKE);
    tx_mutex_put(&client->mutex);
    
    /* Generate random key */
    ws_generate_key(client->sec_key, sizeof(client->sec_key));
    
    /* Compute expected accept key */
    ws_compute_accept_key(client->sec_key, expected_accept);
    
    /* Build HTTP upgrade request */
    len = snprintf((char *)client->handshake_buffer, WS_HANDSHAKE_BUFFER_SIZE,
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n",
        client->config.path,
        client->config.host,
        client->config.port,
        client->sec_key);
    
    /* Add optional headers */
    if (client->config.origin != NULL) {
        len += snprintf((char *)&client->handshake_buffer[len], 
                        WS_HANDSHAKE_BUFFER_SIZE - len,
                        "Origin: %s\r\n", client->config.origin);
    }
    
    if (client->config.protocols != NULL) {
        len += snprintf((char *)&client->handshake_buffer[len],
                        WS_HANDSHAKE_BUFFER_SIZE - len,
                        "Sec-WebSocket-Protocol: %s\r\n", client->config.protocols);
    }
    
    /* End headers */
    len += snprintf((char *)&client->handshake_buffer[len],
                    WS_HANDSHAKE_BUFFER_SIZE - len, "\r\n");
    
    DEBUG_DBG("Handshake request:\n%s", client->handshake_buffer);
    
    /* Allocate packet for sending */
    status = nx_packet_allocate(client->packet_pool_ptr, &packet_ptr,
                                 NX_TCP_PACKET, TX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Failed to allocate packet: %u", status);
        return WS_ERROR_NO_MEMORY;
    }
    
    /* Append data to packet */
    status = nx_packet_data_append(packet_ptr, client->handshake_buffer, len,
                                    client->packet_pool_ptr, TX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        nx_packet_release(packet_ptr);
        DEBUG_ERR("Failed to append data: %u", status);
        return WS_ERROR_SEND_FAILED;
    }
    
    /* Send handshake request */
    status = nx_tcp_socket_send(&client->tcp_socket, packet_ptr,
                                 client->config.handshake_timeout);
    if (status != NX_SUCCESS) {
        nx_packet_release(packet_ptr);
        DEBUG_ERR("Failed to send handshake: %u", status);
        return WS_ERROR_SEND_FAILED;
    }
    
    /* Receive handshake response */
    status = nx_tcp_socket_receive(&client->tcp_socket, &packet_ptr,
                                    client->config.handshake_timeout);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Failed to receive handshake response: %u", status);
        return (status == NX_NO_PACKET) ? WS_ERROR_HANDSHAKE_TIMEOUT : WS_ERROR_RECEIVE_FAILED;
    }
    
    /* Extract response data */
    bytes_received = packet_ptr->nx_packet_length;
    if (bytes_received >= WS_HANDSHAKE_BUFFER_SIZE) {
        bytes_received = WS_HANDSHAKE_BUFFER_SIZE - 1;
    }
    
    status = nx_packet_data_retrieve(packet_ptr, client->handshake_buffer, &bytes_received);
    client->handshake_buffer[bytes_received] = '\0';
    nx_packet_release(packet_ptr);
    
    DEBUG_DBG("Handshake response:\n%s", client->handshake_buffer);
    
    /* Validate response */
    if (!ws_parse_http_response((const char *)client->handshake_buffer, 
                                 (const char *)expected_accept)) {
        DEBUG_ERR("Invalid handshake response");
        return WS_ERROR_HANDSHAKE_FAILED;
    }
    
    DEBUG_INFO("WebSocket handshake completed");
    return WS_OK;
}

static ws_error_t ws_send_frame(ws_client_t *client, UINT opcode, const UCHAR *data, UINT length, UINT fin)
{
    NX_PACKET *packet_ptr;
    UINT status;
    UCHAR header[14];  /* Max header size */
    UINT header_len = 2;
    UCHAR mask_key[4];
    UINT i;
    
    if (client->state != WS_STATE_CONNECTED && client->state != WS_STATE_CLOSING) {
        return WS_ERROR_CONNECTION_CLOSED;
    }
    
    /* Build frame header */
    header[0] = (fin ? WS_FIN_BIT : 0) | (opcode & WS_OPCODE_MASK);
    
    /* Set payload length and mask bit (clients must mask) */
    if (length < 126) {
        header[1] = WS_MASK_BIT | length;
    } else if (length < 65536) {
        header[1] = WS_MASK_BIT | WS_PAYLOAD_LEN_16;
        header[2] = (length >> 8) & 0xFF;
        header[3] = length & 0xFF;
        header_len = 4;
    } else {
        header[1] = WS_MASK_BIT | WS_PAYLOAD_LEN_64;
        header[2] = 0;
        header[3] = 0;
        header[4] = 0;
        header[5] = 0;
        header[6] = (length >> 24) & 0xFF;
        header[7] = (length >> 16) & 0xFF;
        header[8] = (length >> 8) & 0xFF;
        header[9] = length & 0xFF;
        header_len = 10;
    }
    
    /* Generate random mask key */
    ws_generate_key(mask_key, 4);
    memcpy(&header[header_len], mask_key, 4);
    header_len += 4;
    
    /* Allocate packet */
    status = nx_packet_allocate(client->packet_pool_ptr, &packet_ptr,
                                 NX_TCP_PACKET, TX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        return WS_ERROR_NO_MEMORY;
    }
    
    /* Append header */
    status = nx_packet_data_append(packet_ptr, header, header_len,
                                    client->packet_pool_ptr, TX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        nx_packet_release(packet_ptr);
        return WS_ERROR_SEND_FAILED;
    }
    
    /* Mask and append payload */
    if (data != NULL && length > 0) {
        /* Copy and mask payload */
        if (length <= WS_MAX_FRAME_SIZE) {
            memcpy(client->frame_buffer, data, length);
            ws_mask_payload(client->frame_buffer, length, mask_key);
            
            status = nx_packet_data_append(packet_ptr, client->frame_buffer, length,
                                            client->packet_pool_ptr, TX_WAIT_FOREVER);
        } else {
            nx_packet_release(packet_ptr);
            return WS_ERROR_FRAME_TOO_LARGE;
        }
        
        if (status != NX_SUCCESS) {
            nx_packet_release(packet_ptr);
            return WS_ERROR_SEND_FAILED;
        }
    }
    
    /* Send frame */
    status = nx_tcp_socket_send(&client->tcp_socket, packet_ptr, TX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        nx_packet_release(packet_ptr);
        DEBUG_ERR("Send failed: %u", status);
        return WS_ERROR_SEND_FAILED;
    }
    
    DEBUG_DBG("Sent frame: opcode=0x%02X, len=%u", opcode, length);
    return WS_OK;
}

static ws_error_t ws_receive_frame(ws_client_t *client, ws_frame_t *frame)
{
    NX_PACKET *packet_ptr;
    UINT status;
    ULONG bytes_received;
    UCHAR *ptr;
    
    /* Receive packet */
    status = nx_tcp_socket_receive(&client->tcp_socket, &packet_ptr,
                                    client->config.receive_timeout);
    if (status == NX_NO_PACKET) {
        return WS_ERROR_RECEIVE_TIMEOUT;
    }
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Receive failed: %u", status);
        return WS_ERROR_RECEIVE_FAILED;
    }
    
    /* Extract data */
    bytes_received = packet_ptr->nx_packet_length;
    if (bytes_received > WS_RECEIVE_BUFFER_SIZE) {
        bytes_received = WS_RECEIVE_BUFFER_SIZE;
    }
    
    status = nx_packet_data_retrieve(packet_ptr, client->receive_buffer, &bytes_received);
    nx_packet_release(packet_ptr);
    
    if (bytes_received < 2) {
        return WS_ERROR_PROTOCOL_ERROR;
    }
    
    /* Parse frame header */
    ptr = client->receive_buffer;
    frame->fin = (ptr[0] & WS_FIN_BIT) ? 1 : 0;
    frame->opcode = ptr[0] & WS_OPCODE_MASK;
    frame->masked = (ptr[1] & WS_MASK_BIT) ? 1 : 0;
    frame->payload_len = ptr[1] & WS_PAYLOAD_LEN_MASK;
    ptr += 2;
    
    /* Extended payload length */
    if (frame->payload_len == WS_PAYLOAD_LEN_16) {
        if (bytes_received < 4) {
            return WS_ERROR_PROTOCOL_ERROR;
        }
        frame->payload_len = (ptr[0] << 8) | ptr[1];
        ptr += 2;
    } else if (frame->payload_len == WS_PAYLOAD_LEN_64) {
        if (bytes_received < 10) {
            return WS_ERROR_PROTOCOL_ERROR;
        }
        /* For simplicity, only use lower 32 bits */
        frame->payload_len = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
        ptr += 8;
    }
    
    /* Mask key (server->client should not be masked per RFC, but handle it) */
    if (frame->masked) {
        memcpy(frame->mask_key, ptr, 4);
        ptr += 4;
    }
    
    /* Payload */
    frame->payload = ptr;
    
    /* Unmask if needed */
    if (frame->masked && frame->payload_len > 0) {
        ws_mask_payload(frame->payload, frame->payload_len, frame->mask_key);
    }
    
    DEBUG_DBG("Received frame: fin=%u, opcode=0x%02X, len=%llu", 
              frame->fin, frame->opcode, frame->payload_len);
    
    return WS_OK;
}

static void ws_generate_key(UCHAR *key, UINT len)
{
    UINT i;
    
    /*
     * NOTE: This is a simulation/demo implementation using time-based seeding.
     * For production on ASR 3605:
     * - Use hardware RNG if available
     * - Use mbedtls_ctr_drbg_random() from mbedTLS
     * - Use ThreadX secure random if available
     */
#ifdef HOST_SIMULATION
    static UINT seed = 0;
    if (seed == 0) {
        /* Seed from time and process address for simulation */
        seed = (UINT)time(NULL) ^ (UINT)(uintptr_t)key;
    }
#else
    static UINT seed = 0;
    if (seed == 0) {
        /* On real hardware, use hardware RNG or timer-based entropy */
        seed = tx_time_get() ^ 0xDEADBEEF;
    }
#endif
    
    /* LCG-based PRNG - adequate for WebSocket key generation in simulation */
    for (i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        key[i] = (seed >> 16) & 0xFF;
    }
    
    /* Base64 encode if generating WebSocket key */
    if (len == 24) {
        UCHAR raw[16];
        for (i = 0; i < 16; i++) {
            seed = seed * 1103515245 + 12345;
            raw[i] = (seed >> 16) & 0xFF;
        }
        base64_encode(raw, 16, key);
    }
}

static void base64_encode(const UCHAR *input, UINT input_len, UCHAR *output)
{
    UINT i, j;
    UINT val;
    
    for (i = 0, j = 0; i < input_len; i += 3) {
        val = input[i] << 16;
        if (i + 1 < input_len) val |= input[i + 1] << 8;
        if (i + 2 < input_len) val |= input[i + 2];
        
        output[j++] = base64_table[(val >> 18) & 0x3F];
        output[j++] = base64_table[(val >> 12) & 0x3F];
        output[j++] = (i + 1 < input_len) ? base64_table[(val >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < input_len) ? base64_table[val & 0x3F] : '=';
    }
    output[j] = '\0';
}

/*
 * SHA-1 Implementation for WebSocket handshake
 * Based on RFC 3174 - simplified for embedded use
 * NOTE: For production, use mbedtls_sha1() instead
 */

/* SHA-1 circular left shift */
#define SHA1_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_compute(const UCHAR *data, UINT len, UCHAR *hash)
{
    UINT h0 = 0x67452301;
    UINT h1 = 0xEFCDAB89;
    UINT h2 = 0x98BADCFE;
    UINT h3 = 0x10325476;
    UINT h4 = 0xC3D2E1F0;
    
    UINT i, j;
    UINT padded_len;
    UCHAR *padded;
    UINT w[80];
    UINT a, b, c, d, e, f, k, temp;
    
    /* Calculate padded length (multiple of 64 bytes) */
    padded_len = ((len + 8) / 64 + 1) * 64;
    padded = (UCHAR *)malloc(padded_len);
    if (padded == NULL) {
        /* Fallback to simple hash if allocation fails */
        memset(hash, 0, 20);
        return;
    }
    
    /* Copy data and add padding */
    memcpy(padded, data, len);
    padded[len] = 0x80;  /* Append bit '1' */
    memset(padded + len + 1, 0, padded_len - len - 1);
    
    /* Append original length in bits (big-endian, 64-bit) */
    {
        UINT64 bit_len = (UINT64)len * 8;
        padded[padded_len - 8] = (bit_len >> 56) & 0xFF;
        padded[padded_len - 7] = (bit_len >> 48) & 0xFF;
        padded[padded_len - 6] = (bit_len >> 40) & 0xFF;
        padded[padded_len - 5] = (bit_len >> 32) & 0xFF;
        padded[padded_len - 4] = (bit_len >> 24) & 0xFF;
        padded[padded_len - 3] = (bit_len >> 16) & 0xFF;
        padded[padded_len - 2] = (bit_len >> 8) & 0xFF;
        padded[padded_len - 1] = bit_len & 0xFF;
    }
    
    /* Process each 64-byte block */
    for (i = 0; i < padded_len; i += 64) {
        /* Prepare message schedule */
        for (j = 0; j < 16; j++) {
            w[j] = (padded[i + j*4] << 24) |
                   (padded[i + j*4 + 1] << 16) |
                   (padded[i + j*4 + 2] << 8) |
                   (padded[i + j*4 + 3]);
        }
        for (j = 16; j < 80; j++) {
            w[j] = SHA1_ROTL(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
        }
        
        /* Initialize working variables */
        a = h0; b = h1; c = h2; d = h3; e = h4;
        
        /* Main loop */
        for (j = 0; j < 80; j++) {
            if (j < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (j < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (j < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            
            temp = SHA1_ROTL(a, 5) + f + e + k + w[j];
            e = d; d = c; c = SHA1_ROTL(b, 30); b = a; a = temp;
        }
        
        /* Add to hash */
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    
    free(padded);
    
    /* Output hash in big-endian */
    hash[0] = (h0 >> 24) & 0xFF;
    hash[1] = (h0 >> 16) & 0xFF;
    hash[2] = (h0 >> 8) & 0xFF;
    hash[3] = h0 & 0xFF;
    hash[4] = (h1 >> 24) & 0xFF;
    hash[5] = (h1 >> 16) & 0xFF;
    hash[6] = (h1 >> 8) & 0xFF;
    hash[7] = h1 & 0xFF;
    hash[8] = (h2 >> 24) & 0xFF;
    hash[9] = (h2 >> 16) & 0xFF;
    hash[10] = (h2 >> 8) & 0xFF;
    hash[11] = h2 & 0xFF;
    hash[12] = (h3 >> 24) & 0xFF;
    hash[13] = (h3 >> 16) & 0xFF;
    hash[14] = (h3 >> 8) & 0xFF;
    hash[15] = h3 & 0xFF;
    hash[16] = (h4 >> 24) & 0xFF;
    hash[17] = (h4 >> 16) & 0xFF;
    hash[18] = (h4 >> 8) & 0xFF;
    hash[19] = h4 & 0xFF;
}

static void ws_compute_accept_key(const UCHAR *key, UCHAR *accept)
{
    UCHAR combined[256];
    UCHAR hash[20];
    UINT len;
    
    /* Concatenate key + GUID */
    len = snprintf((char *)combined, sizeof(combined), "%s%s", key, WS_GUID);
    
    /* SHA1 hash */
    sha1_compute(combined, len, hash);
    
    /* Base64 encode */
    base64_encode(hash, 20, accept);
}

static void ws_mask_payload(UCHAR *data, UINT length, const UCHAR *mask_key)
{
    UINT i;
    for (i = 0; i < length; i++) {
        data[i] ^= mask_key[i % 4];
    }
}

static UINT ws_parse_http_response(const char *response, const char *expected_accept)
{
    const char *status_line;
    const char *upgrade_header;
    const char *connection_header;
    const char *accept_header;
    const char *accept_value;
    char actual_accept[64];
    UINT i;
    
    /* Check status code 101 */
    status_line = strstr(response, "HTTP/1.1 101");
    if (status_line == NULL) {
        DEBUG_ERR("Expected HTTP 101 Switching Protocols");
        return 0;
    }
    
    /* Check Upgrade header */
    upgrade_header = strstr(response, "Upgrade:");
    if (upgrade_header == NULL || strstr(upgrade_header, "websocket") == NULL) {
        DEBUG_ERR("Missing or invalid Upgrade header");
        return 0;
    }
    
    /* Check Connection header */
    connection_header = strstr(response, "Connection:");
    if (connection_header == NULL || strstr(connection_header, "Upgrade") == NULL) {
        DEBUG_ERR("Missing or invalid Connection header");
        return 0;
    }
    
    /* Check Sec-WebSocket-Accept header and verify value */
    accept_header = strstr(response, "Sec-WebSocket-Accept:");
    if (accept_header == NULL) {
        DEBUG_ERR("Missing Sec-WebSocket-Accept header");
        return 0;
    }
    
    /* Extract the accept value (skip header name and whitespace) */
    accept_value = accept_header + 21;  /* Skip "Sec-WebSocket-Accept:" */
    while (*accept_value == ' ' || *accept_value == '\t') {
        accept_value++;
    }
    
    /* Copy until end of line */
    for (i = 0; i < sizeof(actual_accept) - 1 && accept_value[i] != '\r' && accept_value[i] != '\n' && accept_value[i] != '\0'; i++) {
        actual_accept[i] = accept_value[i];
    }
    actual_accept[i] = '\0';
    
    /* Verify the accept key matches expected value */
    if (strncmp(actual_accept, expected_accept, 28) != 0) {
        DEBUG_ERR("Sec-WebSocket-Accept mismatch: expected '%s', got '%s'", expected_accept, actual_accept);
        return 0;
    }
    
    DEBUG_DBG("WebSocket handshake validated successfully");
    return 1;
}
