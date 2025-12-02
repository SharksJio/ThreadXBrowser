/*
 * platform_config.h
 * Platform-specific configuration for ThreadX WebSocket Browser Client
 * Target: ASR 3605
 */

#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

/* ============================================================================
 * Network Configuration
 * ============================================================================ */

/* WebSocket Gateway Server Configuration */
#define WS_SERVER_HOST          "192.168.1.100"    /* Gateway IP address */
#define WS_SERVER_PORT          9000               /* Device-side WebSocket port */
#define WS_SERVER_PATH          "/"                /* WebSocket endpoint path */

/* Enable TLS (WSS) - requires NetX Secure + mbedTLS */
#define WS_USE_TLS              0                  /* 0 = ws://, 1 = wss:// */

/* Network Interface Configuration */
#define USE_DHCP                1                  /* 1 = DHCP, 0 = Static IP */

/* Static IP Configuration (used if USE_DHCP = 0) */
#define STATIC_IP_ADDRESS       IP_ADDRESS(192, 168, 1, 50)
#define STATIC_SUBNET_MASK      IP_ADDRESS(255, 255, 255, 0)
#define STATIC_GATEWAY_ADDRESS  IP_ADDRESS(192, 168, 1, 1)
#define STATIC_DNS_SERVER       IP_ADDRESS(8, 8, 8, 8)

/* ============================================================================
 * ThreadX Configuration
 * ============================================================================ */

/* Thread Stack Sizes */
#define MAIN_THREAD_STACK_SIZE          4096
#define WS_CLIENT_THREAD_STACK_SIZE     8192
#define NETWORK_THREAD_STACK_SIZE       4096

/* Thread Priorities (lower number = higher priority) */
#define MAIN_THREAD_PRIORITY            10
#define WS_CLIENT_THREAD_PRIORITY       12
#define NETWORK_THREAD_PRIORITY         8

/* Thread Time Slices */
#define DEFAULT_TIME_SLICE              TX_NO_TIME_SLICE

/* ============================================================================
 * NetX Configuration
 * ============================================================================ */

/* Packet Pool Configuration */
#define NETX_PACKET_POOL_SIZE           (32 * 1536)    /* 32 packets * 1536 bytes */
#define NETX_PACKET_SIZE                1536           /* Standard MTU + headers */
#define NETX_PACKET_COUNT               32

/* IP Instance Configuration */
#define NETX_IP_STACK_SIZE              2048
#define NETX_ARP_CACHE_SIZE             1024

/* TCP Configuration */
#define NETX_TCP_WINDOW_SIZE            (8 * 1536)     /* 8 packets */
#define NETX_TCP_QUEUE_DEPTH            5

/* ============================================================================
 * WebSocket Configuration
 * ============================================================================ */

/* Buffer Sizes */
#define WS_MAX_FRAME_SIZE               4096           /* Max WebSocket frame payload */
#define WS_HANDSHAKE_BUFFER_SIZE        1024           /* HTTP handshake buffer */
#define WS_RECEIVE_BUFFER_SIZE          8192           /* Receive ring buffer */

/* Timeouts (in ThreadX ticks, typically 10ms each) */
#define WS_CONNECT_TIMEOUT              (30 * TX_TIMER_TICKS_PER_SECOND)
#define WS_HANDSHAKE_TIMEOUT            (10 * TX_TIMER_TICKS_PER_SECOND)
#define WS_RECEIVE_TIMEOUT              (60 * TX_TIMER_TICKS_PER_SECOND)
#define WS_PING_INTERVAL                (30 * TX_TIMER_TICKS_PER_SECOND)

/* Reconnection Settings */
#define WS_RECONNECT_ENABLED            1
#define WS_RECONNECT_DELAY_MS           5000
#define WS_MAX_RECONNECT_ATTEMPTS       10

/* ============================================================================
 * TLS/Security Configuration (if WS_USE_TLS = 1)
 * ============================================================================ */

#if WS_USE_TLS

/* TLS Session Configuration */
#define TLS_METADATA_BUFFER_SIZE        8192
#define TLS_PACKET_BUFFER_SIZE          4096
#define TLS_CERT_BUFFER_SIZE            4096

/* Certificate Verification */
#define TLS_VERIFY_CERTIFICATE          1              /* 1 = verify server cert */
#define TLS_VERIFY_HOSTNAME             1              /* 1 = verify hostname in cert */

#endif /* WS_USE_TLS */

/* ============================================================================
 * ASR 3605 Specific Configuration
 * ============================================================================ */

/* Memory Configuration */
#define ASR3605_HEAP_SIZE               (64 * 1024)    /* 64KB heap */
#define ASR3605_BYTE_POOL_SIZE          (32 * 1024)    /* 32KB byte pool */

/* Hardware Interfaces */
#define ASR3605_USE_UART_DEBUG          1              /* Enable UART debug output */
#define ASR3605_UART_BAUD_RATE          115200

/* Network Hardware */
#define ASR3605_ETH_DRIVER              _nx_driver_sim /* Simulated for emulator */

/* ============================================================================
 * Debug Configuration
 * ============================================================================ */

#define DEBUG_ENABLED                   1
#define DEBUG_LEVEL                     3              /* 0=OFF, 1=ERR, 2=WARN, 3=INFO, 4=DEBUG */

#if DEBUG_ENABLED
    #include <stdio.h>
    #define DEBUG_PRINT(level, fmt, ...) \
        do { if (level <= DEBUG_LEVEL) printf("[%%s] " fmt "\n", \
             (level==1)?"ERR":(level==2)?"WRN":(level==3)?"INF":"DBG", ##__VA_ARGS__); } while(0)
    #define DEBUG_ERR(fmt, ...)   DEBUG_PRINT(1, fmt, ##__VA_ARGS__)
    #define DEBUG_WARN(fmt, ...)  DEBUG_PRINT(2, fmt, ##__VA_ARGS__)
    #define DEBUG_INFO(fmt, ...)  DEBUG_PRINT(3, fmt, ##__VA_ARGS__)
    #define DEBUG_DBG(fmt, ...)   DEBUG_PRINT(4, fmt, ##__VA_ARGS__)
#else
    #define DEBUG_ERR(fmt, ...)
    #define DEBUG_WARN(fmt, ...)
    #define DEBUG_INFO(fmt, ...)
    #define DEBUG_DBG(fmt, ...)
#endif

/* ============================================================================
 * Feature Flags
 * ============================================================================ */

#define FEATURE_AUDIO_STREAMING         0              /* Enable audio streaming */
#define FEATURE_VIDEO_STREAMING         0              /* Enable video streaming */
#define FEATURE_COMPRESSION             0              /* Enable payload compression */
#define FEATURE_BINARY_PROTOCOL         1              /* Use compact binary protocol */

#endif /* PLATFORM_CONFIG_H */
