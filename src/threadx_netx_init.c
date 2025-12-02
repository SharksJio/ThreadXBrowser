/*
 * threadx_netx_init.c
 * ThreadX and NetX initialization for ASR 3605 emulator
 */

#include "threadx_netx_init.h"
#include "websocket_client.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global Resources
 * ============================================================================ */

/* ThreadX resources */
TX_BYTE_POOL        byte_pool;
TX_BLOCK_POOL       block_pool;

/* NetX resources */
NX_IP               ip_instance;
NX_PACKET_POOL      packet_pool;

/* Thread handles */
TX_THREAD           init_thread;
TX_THREAD           ws_client_thread;
TX_THREAD           app_thread;

/* Synchronization */
TX_EVENT_FLAGS_GROUP system_events;
TX_MUTEX            network_mutex;

/* Memory pools */
static UCHAR byte_pool_memory[ASR3605_BYTE_POOL_SIZE];
static UCHAR ip_stack_memory[NETX_IP_STACK_SIZE];
static UCHAR arp_cache_memory[NETX_ARP_CACHE_SIZE];
static UCHAR packet_pool_memory[NETX_PACKET_POOL_SIZE];

/* Thread stacks */
static UCHAR init_thread_stack[MAIN_THREAD_STACK_SIZE];
static UCHAR ws_thread_stack[WS_CLIENT_THREAD_STACK_SIZE];
static UCHAR app_thread_stack[NETWORK_THREAD_STACK_SIZE];

/* WebSocket client instance */
static ws_client_t ws_client;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void ws_on_connect(ws_state_t state, ws_error_t error);
static void ws_on_text(const char *message, UINT length);
static void ws_on_binary(const UCHAR *data, UINT length);
static void ws_on_close(UINT status_code, const char *reason);

/* ============================================================================
 * ThreadX Application Define Entry Point
 * ============================================================================ */

void tx_application_define(void *first_unused_memory)
{
    UINT status;
    
    DEBUG_INFO("ThreadX Application Define");
    
    /* Initialize ThreadX resources */
    status = threadx_init(first_unused_memory);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("ThreadX init failed: %u", status);
        return;
    }
    
    /* Create threads */
    status = threads_create();
    if (status != TX_SUCCESS) {
        DEBUG_ERR("Thread creation failed: %u", status);
        return;
    }
    
    DEBUG_INFO("Application defined successfully");
}

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

UINT threadx_init(VOID *first_unused_memory)
{
    UINT status;
    
    /* Create byte pool for dynamic allocations */
    status = tx_byte_pool_create(&byte_pool, "byte_pool",
                                  byte_pool_memory, ASR3605_BYTE_POOL_SIZE);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("Byte pool create failed: %u", status);
        return status;
    }
    
    /* Create event flags group */
    status = tx_event_flags_create(&system_events, "system_events");
    if (status != TX_SUCCESS) {
        DEBUG_ERR("Event flags create failed: %u", status);
        return status;
    }
    
    /* Create network mutex */
    status = tx_mutex_create(&network_mutex, "network_mutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("Mutex create failed: %u", status);
        return status;
    }
    
    DEBUG_INFO("ThreadX resources initialized");
    return TX_SUCCESS;
}

UINT netx_init(void)
{
    UINT status;
    
    /* Create packet pool */
    status = nx_packet_pool_create(&packet_pool, "packet_pool",
                                    NETX_PACKET_SIZE, packet_pool_memory,
                                    NETX_PACKET_POOL_SIZE);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Packet pool create failed: %u", status);
        return status;
    }
    
    /* Create IP instance */
    status = nx_ip_create(&ip_instance, "IP Instance",
                          IP_ADDRESS(0, 0, 0, 0),  /* Will be configured later */
                          IP_ADDRESS(0, 0, 0, 0),  /* Subnet mask */
                          &packet_pool,
                          platform_network_driver,
                          ip_stack_memory, NETX_IP_STACK_SIZE,
                          NETWORK_THREAD_PRIORITY);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("IP create failed: %u", status);
        return status;
    }
    
    /* Enable ARP */
    status = nx_arp_enable(&ip_instance, arp_cache_memory, NETX_ARP_CACHE_SIZE);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("ARP enable failed: %u", status);
        return status;
    }
    
    /* Enable ICMP (for ping) */
    status = nx_icmp_enable(&ip_instance);
    if (status != NX_SUCCESS) {
        DEBUG_WARN("ICMP enable failed: %u", status);
        /* Non-fatal */
    }
    
    /* Enable TCP */
    status = nx_tcp_enable(&ip_instance);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("TCP enable failed: %u", status);
        return status;
    }
    
    /* Enable UDP (optional) */
    status = nx_udp_enable(&ip_instance);
    if (status != NX_SUCCESS) {
        DEBUG_WARN("UDP enable failed: %u", status);
        /* Non-fatal */
    }
    
    DEBUG_INFO("NetX stack initialized");
    return NX_SUCCESS;
}

UINT netx_interface_init(void)
{
    UINT status;
    
    /* The network interface is configured via the driver specified in nx_ip_create */
    /* For hardware, this would initialize the Ethernet MAC/PHY */
    /* For simulation, the driver handles this internally */
    
    /* Wait for link to come up */
    ULONG link_status;
    status = nx_ip_interface_status_check(&ip_instance, 0, NX_IP_LINK_ENABLED,
                                           &link_status, NX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Link not available: %u", status);
        return status;
    }
    
    DEBUG_INFO("Network interface initialized");
    return NX_SUCCESS;
}

UINT netx_ip_configure(void)
{
    UINT status;
    
#if USE_DHCP
    /* Note: DHCP requires nx_dhcp library */
    /* For simulation, use static IP */
    DEBUG_INFO("Using static IP configuration (DHCP not available in simulation)");
#endif
    
    /* Configure static IP */
    status = nx_ip_address_set(&ip_instance,
                                STATIC_IP_ADDRESS,
                                STATIC_SUBNET_MASK);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("IP address set failed: %u", status);
        return status;
    }
    
    /* Set gateway */
    status = nx_ip_gateway_address_set(&ip_instance, STATIC_GATEWAY_ADDRESS);
    if (status != NX_SUCCESS) {
        DEBUG_WARN("Gateway set failed: %u", status);
        /* Non-fatal */
    }
    
    /* Print configured IP */
    ULONG ip_addr, subnet_mask;
    nx_ip_address_get(&ip_instance, &ip_addr, &subnet_mask);
    DEBUG_INFO("IP configured: %lu.%lu.%lu.%lu",
               (ip_addr >> 24) & 0xFF,
               (ip_addr >> 16) & 0xFF,
               (ip_addr >> 8) & 0xFF,
               ip_addr & 0xFF);
    
    return NX_SUCCESS;
}

UINT threads_create(void)
{
    UINT status;
    
    /* Create initialization thread */
    status = tx_thread_create(&init_thread, "init_thread",
                               init_thread_entry, 0,
                               init_thread_stack, MAIN_THREAD_STACK_SIZE,
                               MAIN_THREAD_PRIORITY, MAIN_THREAD_PRIORITY,
                               DEFAULT_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("Init thread create failed: %u", status);
        return status;
    }
    
    /* Create WebSocket client thread (starts suspended) */
    status = tx_thread_create(&ws_client_thread, "ws_client_thread",
                               ws_client_thread_entry, 0,
                               ws_thread_stack, WS_CLIENT_THREAD_STACK_SIZE,
                               WS_CLIENT_THREAD_PRIORITY, WS_CLIENT_THREAD_PRIORITY,
                               DEFAULT_TIME_SLICE, TX_DONT_START);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("WS client thread create failed: %u", status);
        return status;
    }
    
    /* Create application thread (starts suspended) */
    status = tx_thread_create(&app_thread, "app_thread",
                               app_thread_entry, 0,
                               app_thread_stack, NETWORK_THREAD_STACK_SIZE,
                               MAIN_THREAD_PRIORITY + 2, MAIN_THREAD_PRIORITY + 2,
                               DEFAULT_TIME_SLICE, TX_DONT_START);
    if (status != TX_SUCCESS) {
        DEBUG_ERR("App thread create failed: %u", status);
        return status;
    }
    
    DEBUG_INFO("Threads created");
    return TX_SUCCESS;
}

/* ============================================================================
 * Accessor Functions
 * ============================================================================ */

NX_IP *get_ip_instance(void)
{
    return &ip_instance;
}

NX_PACKET_POOL *get_packet_pool(void)
{
    return &packet_pool;
}

UINT wait_for_network(ULONG timeout)
{
    ULONG actual_flags;
    return tx_event_flags_get(&system_events, EVENT_NETWORK_READY,
                               TX_AND, &actual_flags, timeout);
}

void signal_network_ready(void)
{
    tx_event_flags_set(&system_events, EVENT_NETWORK_READY, TX_OR);
}

/* ============================================================================
 * Thread Entry Points
 * ============================================================================ */

void init_thread_entry(ULONG thread_input)
{
    UINT status;
    
    (void)thread_input;  /* Unused */
    
    DEBUG_INFO("Initialization thread started");
    
    /* Initialize NetX stack */
    status = netx_init();
    if (status != NX_SUCCESS) {
        DEBUG_ERR("NetX init failed, halting");
        return;
    }
    
    /* Initialize network interface */
    status = netx_interface_init();
    if (status != NX_SUCCESS) {
        DEBUG_ERR("Interface init failed, halting");
        return;
    }
    
    /* Configure IP address */
    status = netx_ip_configure();
    if (status != NX_SUCCESS) {
        DEBUG_ERR("IP config failed, halting");
        return;
    }
    
    /* Signal network ready */
    signal_network_ready();
    
    /* Start WebSocket client thread */
    tx_thread_resume(&ws_client_thread);
    
    /* Start application thread */
    tx_thread_resume(&app_thread);
    
    DEBUG_INFO("Initialization complete");
    
    /* Init thread can now terminate or go idle */
    while (1) {
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND * 60);
    }
}

void ws_client_thread_entry(ULONG thread_input)
{
    ws_error_t result;
    ws_config_t config;
    ULONG actual_flags;
    
    (void)thread_input;  /* Unused */
    
    DEBUG_INFO("WebSocket client thread started");
    
    /* Wait for network to be ready */
    tx_event_flags_get(&system_events, EVENT_NETWORK_READY,
                        TX_AND, &actual_flags, TX_WAIT_FOREVER);
    
    /* Initialize WebSocket client */
    result = ws_client_init(&ws_client, &ip_instance, &packet_pool);
    if (result != WS_OK) {
        DEBUG_ERR("WebSocket client init failed: %s", ws_error_to_string(result));
        return;
    }
    
    /* Configure client */
    ws_config_set_defaults(&config);
    config.host = WS_SERVER_HOST;
    config.port = WS_SERVER_PORT;
    config.path = WS_SERVER_PATH;
    config.auto_reconnect = WS_RECONNECT_ENABLED;
    
    result = ws_client_configure(&ws_client, &config);
    if (result != WS_OK) {
        DEBUG_ERR("WebSocket client configure failed: %s", ws_error_to_string(result));
        return;
    }
    
    /* Set callbacks */
    ws_client_set_connect_callback(&ws_client, ws_on_connect);
    ws_client_set_text_callback(&ws_client, ws_on_text);
    ws_client_set_binary_callback(&ws_client, ws_on_binary);
    ws_client_set_close_callback(&ws_client, ws_on_close);
    
    /* Connect to server */
    DEBUG_INFO("Connecting to WebSocket server...");
    result = ws_client_connect(&ws_client);
    if (result != WS_OK) {
        DEBUG_ERR("WebSocket connect failed: %s", ws_error_to_string(result));
        /* Will retry if auto_reconnect is enabled */
    }
    
    /* Main receive loop */
    while (1) {
        if (ws_client_is_connected(&ws_client)) {
            result = ws_client_process(&ws_client);
            if (result == WS_ERROR_CONNECTION_CLOSED) {
                DEBUG_INFO("Connection closed");
                tx_event_flags_set(&system_events, EVENT_WS_DISCONNECTED, TX_OR);
                
                /* Auto-reconnect logic */
                if (config.auto_reconnect) {
                    DEBUG_INFO("Reconnecting in %u ms...", config.reconnect_delay_ms);
                    tx_thread_sleep(config.reconnect_delay_ms * TX_TIMER_TICKS_PER_SECOND / 1000);
                    ws_client_connect(&ws_client);
                }
            } else if (result != WS_OK && result != WS_ERROR_RECEIVE_TIMEOUT) {
                DEBUG_ERR("Process error: %s", ws_error_to_string(result));
            }
        } else {
            /* Not connected, wait and retry */
            tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
            if (config.auto_reconnect) {
                ws_client_connect(&ws_client);
            }
        }
    }
}

void app_thread_entry(ULONG thread_input)
{
    ULONG actual_flags;
    UINT counter = 0;
    char message[128];
    
    (void)thread_input;  /* Unused */
    
    DEBUG_INFO("Application thread started");
    
    /* Wait for WebSocket connection */
    tx_event_flags_get(&system_events, EVENT_WS_CONNECTED,
                        TX_AND, &actual_flags, TX_WAIT_FOREVER);
    
    DEBUG_INFO("Application ready");
    
    /* Main application loop */
    while (1) {
        /* Wait for next event or timeout */
        UINT status = tx_event_flags_get(&system_events, 
                                          EVENT_WS_MESSAGE | EVENT_SHUTDOWN,
                                          TX_OR_CLEAR, &actual_flags,
                                          TX_TIMER_TICKS_PER_SECOND * 5);
        
        if (actual_flags & EVENT_SHUTDOWN) {
            DEBUG_INFO("Shutdown requested");
            break;
        }
        
        /* Periodic status message */
        if (ws_client_is_connected(&ws_client)) {
            snprintf(message, sizeof(message), 
                     "{\"type\":\"status\",\"counter\":%u,\"uptime\":%lu}",
                     counter++, tx_time_get());
            
            ws_client_send_text(&ws_client, message, 0);
            DEBUG_DBG("Sent status message #%u", counter);
        }
    }
    
    DEBUG_INFO("Application thread exiting");
}

/* ============================================================================
 * WebSocket Callbacks
 * ============================================================================ */

static void ws_on_connect(ws_state_t state, ws_error_t error)
{
    DEBUG_INFO("Connection state: %s", ws_state_to_string(state));
    
    if (state == WS_STATE_CONNECTED) {
        tx_event_flags_set(&system_events, EVENT_WS_CONNECTED, TX_OR);
        
        /* Send initial hello message */
        ws_client_send_text(&ws_client, 
                            "{\"type\":\"hello\",\"device\":\"ASR3605\",\"version\":\"1.0\"}",
                            0);
    } else if (state == WS_STATE_DISCONNECTED || state == WS_STATE_ERROR) {
        tx_event_flags_set(&system_events, EVENT_WS_DISCONNECTED, TX_OR);
        if (error != WS_OK) {
            DEBUG_ERR("Connection error: %s", ws_error_to_string(error));
        }
    }
}

static void ws_on_text(const char *message, UINT length)
{
    DEBUG_INFO("Received text (%u bytes): %s", length, message);
    tx_event_flags_set(&system_events, EVENT_WS_MESSAGE, TX_OR);
    
    /* Echo back with acknowledgment */
    char response[256];
    snprintf(response, sizeof(response), "{\"type\":\"ack\",\"received\":%u}", length);
    ws_client_send_text(&ws_client, response, 0);
}

static void ws_on_binary(const UCHAR *data, UINT length)
{
    DEBUG_INFO("Received binary data: %u bytes", length);
    tx_event_flags_set(&system_events, EVENT_WS_MESSAGE, TX_OR);
    
    /* Process binary protocol message */
    if (length >= 5) {
        UINT msg_len = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        UCHAR msg_type = data[4];
        
        DEBUG_DBG("Binary message: type=0x%02X, len=%u", msg_type, msg_len);
        
        /* Handle message types per protocol spec */
        switch (msg_type) {
            case 0x01:  /* JSON/Text */
                DEBUG_DBG("JSON payload");
                break;
            case 0x02:  /* Binary data (audio/video) */
                DEBUG_DBG("Binary payload");
                break;
            case 0x03:  /* Control command */
                DEBUG_DBG("Control command");
                break;
            case 0x04:  /* Ping/Pong */
                DEBUG_DBG("Ping/Pong");
                break;
            default:
                DEBUG_WARN("Unknown message type: 0x%02X", msg_type);
                break;
        }
    }
}

static void ws_on_close(UINT status_code, const char *reason)
{
    DEBUG_INFO("Connection closed: code=%u, reason=%s", status_code, 
               reason ? reason : "(none)");
    tx_event_flags_set(&system_events, EVENT_WS_DISCONNECTED, TX_OR);
}

/* ============================================================================
 * Platform Network Driver (Simulation)
 * ============================================================================ */

/* This is a minimal simulation driver for testing without hardware */
/* In production, replace with actual ASR 3605 Ethernet driver */

#ifndef NX_DRIVER_DEFERRED_PROCESSING
static UCHAR driver_initialized = 0;
static UCHAR driver_link_up = 1;  /* Assume link up for simulation */
#endif

VOID platform_network_driver(NX_IP_DRIVER *driver_req_ptr)
{
    NX_IP *ip_ptr;
    NX_PACKET *packet_ptr;
    UINT status;
    
    ip_ptr = driver_req_ptr->nx_ip_driver_ptr;
    
    switch (driver_req_ptr->nx_ip_driver_command) {
        case NX_LINK_INITIALIZE:
            DEBUG_DBG("Driver: Initialize");
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_ENABLE:
            DEBUG_DBG("Driver: Enable");
            /* Set interface link state */
            ip_ptr->nx_ip_interface[0].nx_interface_link_up = NX_TRUE;
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_DISABLE:
            DEBUG_DBG("Driver: Disable");
            ip_ptr->nx_ip_interface[0].nx_interface_link_up = NX_FALSE;
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_PACKET_SEND:
        case NX_LINK_PACKET_BROADCAST:
            /* For simulation, just release the packet */
            packet_ptr = driver_req_ptr->nx_ip_driver_packet;
            if (packet_ptr) {
                nx_packet_release(packet_ptr);
            }
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_GET_STATUS:
            /* Return link status */
            *(driver_req_ptr->nx_ip_driver_return_ptr) = NX_TRUE;
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_ARP_SEND:
        case NX_LINK_ARP_RESPONSE_SEND:
        case NX_LINK_RARP_SEND:
            /* ARP packets - release for simulation */
            packet_ptr = driver_req_ptr->nx_ip_driver_packet;
            if (packet_ptr) {
                nx_packet_release(packet_ptr);
            }
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_MULTICAST_JOIN:
        case NX_LINK_MULTICAST_LEAVE:
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_GET_SPEED:
            *(driver_req_ptr->nx_ip_driver_return_ptr) = 100000000; /* 100 Mbps */
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_GET_DUPLEX_TYPE:
            *(driver_req_ptr->nx_ip_driver_return_ptr) = NX_TRUE; /* Full duplex */
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_GET_ERROR_COUNT:
            *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_GET_RX_COUNT:
        case NX_LINK_GET_TX_COUNT:
            *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_GET_ALLOC_ERRORS:
            *(driver_req_ptr->nx_ip_driver_return_ptr) = 0;
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_UNINITIALIZE:
            DEBUG_DBG("Driver: Uninitialize");
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        case NX_LINK_DEFERRED_PROCESSING:
            driver_req_ptr->nx_ip_driver_status = NX_SUCCESS;
            break;
            
        default:
            DEBUG_WARN("Driver: Unknown command %u", driver_req_ptr->nx_ip_driver_command);
            driver_req_ptr->nx_ip_driver_status = NX_UNHANDLED_COMMAND;
            break;
    }
}
