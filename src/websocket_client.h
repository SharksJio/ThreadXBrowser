/*
 * websocket_client.h
 * RFC6455 Compliant WebSocket Client for ThreadX/NetX
 * Target: ASR 3605
 */

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include "platform_config.h"
#include "tx_api.h"
#include "nx_api.h"
#include "nx_tcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * WebSocket Protocol Constants (RFC6455)
 * ============================================================================ */

/* WebSocket Opcodes */
#define WS_OPCODE_CONTINUATION  0x00
#define WS_OPCODE_TEXT          0x01
#define WS_OPCODE_BINARY        0x02
#define WS_OPCODE_CLOSE         0x08
#define WS_OPCODE_PING          0x09
#define WS_OPCODE_PONG          0x0A

/* WebSocket Frame Bits */
#define WS_FIN_BIT              0x80
#define WS_MASK_BIT             0x80
#define WS_OPCODE_MASK          0x0F
#define WS_PAYLOAD_LEN_MASK     0x7F

/* Extended Payload Length Indicators */
#define WS_PAYLOAD_LEN_16       126
#define WS_PAYLOAD_LEN_64       127

/* WebSocket Close Status Codes */
#define WS_CLOSE_NORMAL         1000
#define WS_CLOSE_GOING_AWAY     1001
#define WS_CLOSE_PROTOCOL_ERROR 1002
#define WS_CLOSE_UNSUPPORTED    1003
#define WS_CLOSE_NO_STATUS      1005
#define WS_CLOSE_ABNORMAL       1006
#define WS_CLOSE_INVALID_DATA   1007
#define WS_CLOSE_POLICY_VIOLATE 1008
#define WS_CLOSE_TOO_BIG        1009
#define WS_CLOSE_EXTENSION_REQ  1010
#define WS_CLOSE_INTERNAL_ERROR 1011

/* WebSocket GUID for handshake */
#define WS_GUID                 "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* ============================================================================
 * WebSocket Client States
 * ============================================================================ */

typedef enum {
    WS_STATE_DISCONNECTED = 0,
    WS_STATE_CONNECTING,
    WS_STATE_HANDSHAKE,
    WS_STATE_CONNECTED,
    WS_STATE_CLOSING,
    WS_STATE_ERROR
} ws_state_t;

/* ============================================================================
 * WebSocket Error Codes
 * ============================================================================ */

typedef enum {
    WS_OK = 0,
    WS_ERROR_INVALID_PARAM,
    WS_ERROR_NO_MEMORY,
    WS_ERROR_SOCKET_CREATE,
    WS_ERROR_CONNECT_FAILED,
    WS_ERROR_HANDSHAKE_FAILED,
    WS_ERROR_HANDSHAKE_TIMEOUT,
    WS_ERROR_INVALID_RESPONSE,
    WS_ERROR_SEND_FAILED,
    WS_ERROR_RECEIVE_FAILED,
    WS_ERROR_RECEIVE_TIMEOUT,
    WS_ERROR_FRAME_TOO_LARGE,
    WS_ERROR_PROTOCOL_ERROR,
    WS_ERROR_CONNECTION_CLOSED,
    WS_ERROR_TLS_INIT_FAILED,
    WS_ERROR_TLS_HANDSHAKE_FAILED
} ws_error_t;

/* ============================================================================
 * WebSocket Frame Structure
 * ============================================================================ */

typedef struct {
    UINT    fin;                                /* Final fragment flag */
    UINT    opcode;                             /* Frame opcode */
    UINT    masked;                             /* Mask flag */
    UINT64  payload_len;                        /* Payload length */
    UCHAR   mask_key[4];                        /* Masking key (client->server) */
    UCHAR   *payload;                           /* Payload data pointer */
} ws_frame_t;

/* ============================================================================
 * WebSocket Message (for reassembly)
 * ============================================================================ */

typedef struct {
    UINT    opcode;                             /* Original opcode */
    UCHAR   *data;                              /* Complete message data */
    UINT    length;                             /* Total message length */
    UINT    capacity;                           /* Buffer capacity */
} ws_message_t;

/* ============================================================================
 * Callback Function Types
 * ============================================================================ */

/* Called when connection state changes */
typedef void (*ws_connect_callback_t)(ws_state_t state, ws_error_t error);

/* Called when a text message is received */
typedef void (*ws_text_callback_t)(const char *message, UINT length);

/* Called when a binary message is received */
typedef void (*ws_binary_callback_t)(const UCHAR *data, UINT length);

/* Called when a ping is received */
typedef void (*ws_ping_callback_t)(const UCHAR *data, UINT length);

/* Called when a pong is received */
typedef void (*ws_pong_callback_t)(const UCHAR *data, UINT length);

/* Called when the connection is closed */
typedef void (*ws_close_callback_t)(UINT status_code, const char *reason);

/* ============================================================================
 * WebSocket Client Configuration
 * ============================================================================ */

typedef struct {
    const char              *host;              /* Server hostname/IP */
    UINT                    port;               /* Server port */
    const char              *path;              /* WebSocket path (default "/") */
    const char              *origin;            /* Origin header (optional) */
    const char              *protocols;         /* Subprotocols (optional) */
    ULONG                   connect_timeout;    /* Connection timeout (ticks) */
    ULONG                   handshake_timeout;  /* Handshake timeout (ticks) */
    ULONG                   receive_timeout;    /* Receive timeout (ticks) */
    ULONG                   ping_interval;      /* Ping interval (ticks, 0=disable) */
    UINT                    auto_reconnect;     /* Enable auto-reconnect */
    UINT                    reconnect_delay_ms; /* Reconnect delay */
    UINT                    max_reconnect;      /* Max reconnect attempts */
} ws_config_t;

/* ============================================================================
 * WebSocket Client Context
 * ============================================================================ */

typedef struct {
    /* Configuration */
    ws_config_t             config;
    
    /* Network Resources */
    NX_IP                   *ip_ptr;            /* NetX IP instance */
    NX_PACKET_POOL          *packet_pool_ptr;   /* NetX packet pool */
    NX_TCP_SOCKET           tcp_socket;         /* TCP socket */
    
    /* TLS Context (if enabled) */
#if WS_USE_TLS
    NX_SECURE_TLS_SESSION   tls_session;
    UCHAR                   tls_metadata[TLS_METADATA_BUFFER_SIZE];
    UCHAR                   tls_packet_buffer[TLS_PACKET_BUFFER_SIZE];
    UCHAR                   tls_cert_buffer[TLS_CERT_BUFFER_SIZE];
#endif
    
    /* State */
    ws_state_t              state;
    ws_error_t              last_error;
    UINT                    reconnect_count;
    
    /* Handshake */
    UCHAR                   sec_key[24];        /* Base64 encoded key */
    UCHAR                   sec_accept[28];     /* Expected Sec-WebSocket-Accept */
    
    /* Buffers */
    UCHAR                   handshake_buffer[WS_HANDSHAKE_BUFFER_SIZE];
    UCHAR                   frame_buffer[WS_MAX_FRAME_SIZE + 14]; /* +14 for frame header */
    UCHAR                   receive_buffer[WS_RECEIVE_BUFFER_SIZE];
    UINT                    receive_len;
    
    /* Message Reassembly */
    ws_message_t            current_message;
    
    /* Callbacks */
    ws_connect_callback_t   on_connect;
    ws_text_callback_t      on_text;
    ws_binary_callback_t    on_binary;
    ws_ping_callback_t      on_ping;
    ws_pong_callback_t      on_pong;
    ws_close_callback_t     on_close;
    
    /* Threading */
    TX_MUTEX                mutex;
    TX_EVENT_FLAGS_GROUP    *events;
    UINT                    running;
} ws_client_t;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * Initialize WebSocket client
 * @param client    Pointer to client context
 * @param ip_ptr    NetX IP instance
 * @param pool_ptr  NetX packet pool
 * @return WS_OK on success
 */
ws_error_t ws_client_init(ws_client_t *client, NX_IP *ip_ptr, NX_PACKET_POOL *pool_ptr);

/**
 * Configure WebSocket client
 * @param client    Pointer to client context
 * @param config    Configuration structure
 * @return WS_OK on success
 */
ws_error_t ws_client_configure(ws_client_t *client, const ws_config_t *config);

/**
 * Set default configuration values
 * @param config    Configuration structure to fill
 */
void ws_config_set_defaults(ws_config_t *config);

/**
 * Connect to WebSocket server
 * @param client    Pointer to client context
 * @return WS_OK on success
 */
ws_error_t ws_client_connect(ws_client_t *client);

/**
 * Disconnect from WebSocket server
 * @param client        Pointer to client context
 * @param status_code   Close status code
 * @param reason        Close reason (optional, can be NULL)
 * @return WS_OK on success
 */
ws_error_t ws_client_disconnect(ws_client_t *client, UINT status_code, const char *reason);

/**
 * Send text message
 * @param client    Pointer to client context
 * @param message   UTF-8 text message
 * @param length    Message length (0 = use strlen)
 * @return WS_OK on success
 */
ws_error_t ws_client_send_text(ws_client_t *client, const char *message, UINT length);

/**
 * Send binary data
 * @param client    Pointer to client context
 * @param data      Binary data
 * @param length    Data length
 * @return WS_OK on success
 */
ws_error_t ws_client_send_binary(ws_client_t *client, const UCHAR *data, UINT length);

/**
 * Send ping frame
 * @param client    Pointer to client context
 * @param data      Optional ping data (can be NULL)
 * @param length    Data length (max 125)
 * @return WS_OK on success
 */
ws_error_t ws_client_send_ping(ws_client_t *client, const UCHAR *data, UINT length);

/**
 * Send pong frame (response to ping)
 * @param client    Pointer to client context
 * @param data      Pong data (should match ping data)
 * @param length    Data length
 * @return WS_OK on success
 */
ws_error_t ws_client_send_pong(ws_client_t *client, const UCHAR *data, UINT length);

/**
 * Process incoming data (call from receive thread)
 * @param client    Pointer to client context
 * @return WS_OK on success, error code on failure
 */
ws_error_t ws_client_process(ws_client_t *client);

/**
 * Get current connection state
 * @param client    Pointer to client context
 * @return Current state
 */
ws_state_t ws_client_get_state(ws_client_t *client);

/**
 * Get last error code
 * @param client    Pointer to client context
 * @return Last error code
 */
ws_error_t ws_client_get_error(ws_client_t *client);

/**
 * Check if connected
 * @param client    Pointer to client context
 * @return 1 if connected, 0 otherwise
 */
UINT ws_client_is_connected(ws_client_t *client);

/**
 * Set callback functions
 */
void ws_client_set_connect_callback(ws_client_t *client, ws_connect_callback_t callback);
void ws_client_set_text_callback(ws_client_t *client, ws_text_callback_t callback);
void ws_client_set_binary_callback(ws_client_t *client, ws_binary_callback_t callback);
void ws_client_set_ping_callback(ws_client_t *client, ws_ping_callback_t callback);
void ws_client_set_pong_callback(ws_client_t *client, ws_pong_callback_t callback);
void ws_client_set_close_callback(ws_client_t *client, ws_close_callback_t callback);

/**
 * Cleanup and release resources
 * @param client    Pointer to client context
 */
void ws_client_cleanup(ws_client_t *client);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get error message string
 * @param error     Error code
 * @return Error message string
 */
const char *ws_error_to_string(ws_error_t error);

/**
 * Get state name string
 * @param state     State value
 * @return State name string
 */
const char *ws_state_to_string(ws_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_CLIENT_H */
