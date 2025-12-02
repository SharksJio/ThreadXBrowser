/*
 * threadx_netx_init.h
 * ThreadX and NetX initialization for ASR 3605 emulator
 */

#ifndef THREADX_NETX_INIT_H
#define THREADX_NETX_INIT_H

#include "platform_config.h"
#include "tx_api.h"
#include "nx_api.h"

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