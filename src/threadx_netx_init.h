/*
 * threadx_netx_init.h
 * ThreadX and NetX initialization for ASR 3605 emulator
 */

#ifndef THREADX_NETX_INIT_H
#define THREADX_NETX_INIT_H

#include "platform_config.h"

/* Include ThreadX/NetX headers only when not in simulation mode */
#ifndef HOST_SIMULATION
#include "tx_api.h"
#include "nx_api.h"
#else
/* Simulation stub types - minimal definitions for host build */
#include <stdint.h>
#include <pthread.h>

typedef uint8_t UCHAR;
typedef uint16_t USHORT;
typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef uint64_t UINT64;
typedef void VOID;
typedef char CHAR;

/* ThreadX stub types */
typedef struct TX_THREAD_STRUCT {
    CHAR *tx_thread_name;
    void (*tx_thread_entry)(ULONG);
    ULONG tx_thread_entry_input;
    int tx_thread_state;
    pthread_t tx_thread_posix;
} TX_THREAD;

typedef struct TX_BYTE_POOL_STRUCT {
    CHAR *tx_byte_pool_name;
    VOID *tx_byte_pool_start;
    ULONG tx_byte_pool_size;
    ULONG tx_byte_pool_available;
} TX_BYTE_POOL;

typedef struct TX_BLOCK_POOL_STRUCT {
    CHAR *tx_block_pool_name;
} TX_BLOCK_POOL;

typedef struct TX_MUTEX_STRUCT {
    CHAR *tx_mutex_name;
    pthread_mutex_t tx_mutex_posix;
} TX_MUTEX;

typedef struct TX_EVENT_FLAGS_GROUP_STRUCT {
    CHAR *tx_event_flags_group_name;
    ULONG tx_event_flags_current;
    pthread_mutex_t tx_event_mutex;
    pthread_cond_t tx_event_cond;
} TX_EVENT_FLAGS_GROUP;

/* NetX stub types */
typedef struct NX_IP_STRUCT {
    CHAR *nx_ip_name;
} NX_IP;

typedef struct NX_PACKET_POOL_STRUCT {
    CHAR *nx_packet_pool_name;
} NX_PACKET_POOL;

typedef struct NX_IP_DRIVER_STRUCT {
    UINT nx_ip_driver_command;
    UINT nx_ip_driver_status;
} NX_IP_DRIVER;

/* ThreadX constants */
#define TX_SUCCESS              0
#define TX_NO_EVENTS            1
#define TX_NO_MEMORY            2
#define TX_MUTEX_ERROR          3
#define TX_WAIT_FOREVER         0xFFFFFFFF
#define TX_NO_WAIT              0
#define TX_AUTO_START           1
#define TX_DONT_START           0
#define TX_NO_TIME_SLICE        0
#define TX_OR                   2
#define TX_AND                  3
#define TX_OR_CLEAR             4
#define TX_AND_CLEAR            5
#define TX_READY                1
#define TX_SUSPENDED            2
#define TX_TIMER_TICKS_PER_SECOND 100

/* NetX constants */
#define NX_SUCCESS              0
#define NX_NOT_SUCCESSFUL       1
#define NX_UNHANDLED_COMMAND    0xFF
#define NX_LINK_INITIALIZE      0x00
#define NX_LINK_ENABLE          0x01
#define NX_LINK_DISABLE         0x02
#define NX_LINK_PACKET_SEND     0x03
#define NX_LINK_PACKET_BROADCAST 0x04
#define NX_LINK_ARP_SEND        0x05
#define NX_LINK_ARP_RESPONSE_SEND 0x06
#define NX_LINK_RARP_SEND       0x07
#define NX_LINK_MULTICAST_JOIN  0x08
#define NX_LINK_MULTICAST_LEAVE 0x09
#define NX_LINK_GET_STATUS      0x0A
#define NX_LINK_GET_SPEED       0x0B
#define NX_LINK_GET_DUPLEX_TYPE 0x0C
#define NX_LINK_GET_ERROR_COUNT 0x0D
#define NX_LINK_GET_RX_COUNT    0x0E
#define NX_LINK_GET_TX_COUNT    0x0F
#define NX_LINK_GET_ALLOC_ERRORS 0x10
#define NX_LINK_UNINITIALIZE    0x11
#define NX_LINK_DEFERRED_PROCESSING 0x12
#define IP_ADDRESS(a,b,c,d)     ((a << 24) | (b << 16) | (c << 8) | d)

#endif /* HOST_SIMULATION */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Global Resources (defined in threadx_netx_init.c)
 * ============================================================================ */

/* ThreadX resources */
extern TX_BYTE_POOL        byte_pool;
extern TX_BLOCK_POOL       block_pool;

/* NetX resources */
extern NX_IP               ip_instance;
extern NX_PACKET_POOL      packet_pool;

/* Thread handles */
extern TX_THREAD           init_thread;
extern TX_THREAD           ws_client_thread;
extern TX_THREAD           app_thread;

/* Synchronization */
extern TX_EVENT_FLAGS_GROUP system_events;
extern TX_MUTEX            network_mutex;

/* ============================================================================
 * Event Flags
 * ============================================================================ */

#define EVENT_NETWORK_READY     (1 << 0)
#define EVENT_WS_CONNECTED      (1 << 1)
#define EVENT_WS_DISCONNECTED   (1 << 2)
#define EVENT_WS_MESSAGE        (1 << 3)
#define EVENT_WS_ERROR          (1 << 4)
#define EVENT_APP_START         (1 << 5)
#define EVENT_SHUTDOWN          (1 << 6)

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

/**
 * Initialize ThreadX kernel resources (byte pool, block pool)
 * Called from tx_application_define()
 * @param first_unused_memory   Pointer to first available memory
 * @return TX_SUCCESS on success
 */
UINT threadx_init(VOID *first_unused_memory);

/**
 * Initialize NetX networking stack
 * Creates IP instance, packet pool, enables protocols
 * @return NX_SUCCESS on success
 */
UINT netx_init(void);

/**
 * Initialize network interface (Ethernet driver for ASR 3605 or simulator)
 * @return NX_SUCCESS on success
 */
UINT netx_interface_init(void);

/**
 * Configure IP address (static or DHCP)
 * @return NX_SUCCESS on success
 */
UINT netx_ip_configure(void);

/**
 * Create application threads
 * @return TX_SUCCESS on success
 */
UINT threads_create(void);

/**
 * Get pointer to the IP instance
 * @return Pointer to NX_IP instance
 */
NX_IP *get_ip_instance(void);

/**
 * Get pointer to the packet pool
 * @return Pointer to NX_PACKET_POOL
 */
NX_PACKET_POOL *get_packet_pool(void);

/**
 * Wait for network to be ready
 * @param timeout   Timeout in ThreadX ticks
 * @return TX_SUCCESS if ready, TX_NO_EVENTS on timeout
 */
UINT wait_for_network(ULONG timeout);

/**
 * Signal that network is ready
 */
void signal_network_ready(void);

/* ============================================================================
 * Thread Entry Points (implemented in respective modules)
 * ============================================================================ */

/**
 * Initialization thread entry
 * Initializes NetX and starts other threads
 */
void init_thread_entry(ULONG thread_input);

/**
 * WebSocket client thread entry
 * Manages WebSocket connection and message processing
 */
void ws_client_thread_entry(ULONG thread_input);

/**
 * Application thread entry
 * Main application logic
 */
void app_thread_entry(ULONG thread_input);

/* ============================================================================
 * Platform-Specific Network Driver
 * ============================================================================ */

/**
 * Network driver entry point (platform-specific)
 * For ASR 3605: hardware Ethernet driver
 * For simulator: RAM driver or loopback
 */
VOID platform_network_driver(NX_IP_DRIVER *driver_req_ptr);

#ifdef __cplusplus
}
#endif

#endif /* THREADX_NETX_INIT_H */