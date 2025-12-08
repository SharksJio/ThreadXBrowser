/*
 * main.c
 * ThreadX WebSocket Browser Client - Application Entry Point
 * Target: ASR 3605
 */

#include "platform_config.h"
#include "threadx_netx_init.h"
#include "websocket_client.h"
#include "stream_handler.h"
#include "ws_client_sim.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Simulation Support
 * ============================================================================ */

#ifdef HOST_SIMULATION

/* For host simulation, we need to provide ThreadX stubs and main entry */

#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

/* Simulation state */
static volatile int sim_running = 1;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig)
{
    printf("\n[SIM] Received signal %d, shutting down...\n", sig);
    sim_running = 0;
}

/* Simulated ThreadX timer tick (100 Hz = 10ms) */
static void *timer_thread(void *arg)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000; /* 10ms */
    
    while (sim_running) {
        nanosleep(&ts, NULL);
        /* In real ThreadX, this would increment tx_time and trigger scheduler */
    }
    return NULL;
}

/* Main entry point for host simulation */
int main(int argc, char *argv[])
{
    pthread_t timer_tid;
    
    printf("========================================\n");
    printf("ThreadX WebSocket Browser Client\n");
    printf("Host Simulation Mode\n");
    printf("========================================\n\n");
    
    /* Install signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Print configuration */
    printf("[CONFIG] Server: %s:%d%s\n", WS_SERVER_HOST, WS_SERVER_PORT, WS_SERVER_PATH);
    printf("[CONFIG] TLS: %s\n", WS_USE_TLS ? "Enabled" : "Disabled");
    printf("[CONFIG] Auto-reconnect: %s\n", WS_RECONNECT_ENABLED ? "Enabled" : "Disabled");
    printf("[CONFIG] Debug level: %d\n\n", DEBUG_LEVEL);
    
    /* Start simulated timer thread */
    if (pthread_create(&timer_tid, NULL, timer_thread, NULL) != 0) {
        printf("[ERR] Failed to create timer thread\n");
        return 1;
    }
    
    /* In a real ThreadX environment, tx_kernel_enter() is called here */
    /* For simulation, we call the application define directly */
    printf("[SIM] Calling tx_application_define...\n");
    tx_application_define(NULL);
    
    /* Connect to WebSocket gateway */
    const char *ws_host = getenv("WS_SERVER_HOST");
    const char *ws_port_str = getenv("WS_SERVER_PORT");
    
    if (!ws_host) ws_host = "gateway";
    int ws_port = ws_port_str ? atoi(ws_port_str) : 9000;
    
    printf("[SIM] Connecting to WebSocket gateway at %s:%d...\n", ws_host, ws_port);
    
    if (ws_connect(ws_host, ws_port) == 0) {
        printf("[SIM] Successfully connected to gateway!\n");
        printf("[SIM] Emulator is now visible in the web interface\n\n");
    } else {
        printf("[SIM] Failed to connect to gateway\n");
        printf("[SIM] Running in standalone mode...\n\n");
    }
    
    /* Simulation main loop */
    printf("[SIM] Entering main loop (Ctrl+C to exit)...\n\n");
    
    while (sim_running) {
        /* Process WebSocket messages (poll 10 times per second) */
        for (int i = 0; i < 10 && ws_is_connected(); i++) {
            ws_receive();
            usleep(100000);  /* 100ms */
        }
        
        /* Check for stream status every 5 seconds */
        static int status_counter = 0;
        if (status_counter++ % 5 == 0 && is_stream_active()) {
            char stats[256];
            get_stream_stats(stats, sizeof(stats));
            printf("[STATUS] %s\n", stats);
        }
    }
    
    /* Cleanup */
    ws_disconnect();
    
    /* Cleanup */
    printf("\n[SIM] Shutting down...\n");
    pthread_join(timer_tid, NULL);
    
    printf("[SIM] Simulation ended\n");
    return 0;
}

#else /* !HOST_SIMULATION */

/* ============================================================================
 * Real ThreadX Entry Point (ASR 3605 Target)
 * ============================================================================ */

/* 
 * For actual ASR 3605 hardware:
 * - The startup code (startup_asr3605.s) calls SystemInit() and then main()
 * - main() calls tx_kernel_enter() which initializes ThreadX
 * - ThreadX calls tx_application_define() to set up the application
 * - tx_application_define() is implemented in threadx_netx_init.c
 */

int main(void)
{
    /* Initialize hardware */
    /* SystemInit() is typically called by startup code */
    
#if ASR3605_USE_UART_DEBUG
    /* Initialize UART for debug output */
    /* uart_init(ASR3605_UART_BAUD_RATE); */
#endif
    
    DEBUG_INFO("ThreadX WebSocket Browser Client starting...");
    DEBUG_INFO("Target: ASR 3605");
    DEBUG_INFO("Server: %s:%d%s", WS_SERVER_HOST, WS_SERVER_PORT, WS_SERVER_PATH);
    
    /* Enter ThreadX kernel */
    /* This function never returns */
    tx_kernel_enter();
    
    /* Should never reach here */
    return 0;
}

#endif /* HOST_SIMULATION */

/* ============================================================================
 * ThreadX Kernel Hooks (Optional)
 * ============================================================================ */

/* Called when ThreadX kernel is about to start */
void tx_kernel_enter_callback(void)
{
    DEBUG_DBG("Kernel entering");
}

/* Called periodically by ThreadX timer interrupt */
void tx_timer_interrupt_callback(void)
{
    /* Can be used for watchdog feeding, LED blinking, etc. */
}

/* Called when a thread is created */
void tx_thread_create_callback(TX_THREAD *thread_ptr)
{
    DEBUG_DBG("Thread created: %s", thread_ptr->tx_thread_name);
}

/* Called when a thread terminates */
void tx_thread_terminate_callback(TX_THREAD *thread_ptr)
{
    DEBUG_DBG("Thread terminated: %s", thread_ptr->tx_thread_name);
}

/* ============================================================================
 * Memory Management Hooks
 * ============================================================================ */

/* Custom memory allocation failure handler */
void tx_application_memory_failure_notification(TX_BYTE_POOL *pool_ptr)
{
    DEBUG_ERR("Memory allocation failed in pool: %s", pool_ptr->tx_byte_pool_name);
    /* Could trigger recovery or restart here */
}

/* Stack overflow hook */
void tx_thread_stack_error_notification(TX_THREAD *thread_ptr)
{
    DEBUG_ERR("Stack overflow in thread: %s", thread_ptr->tx_thread_name);
    /* Critical error - should reset system */
}

/* ============================================================================
 * ASR 3605 Hardware Abstraction (Stubs for Simulation)
 * ============================================================================ */

#ifdef HOST_SIMULATION

/* These are simulation stubs for functions that would be implemented
 * by the ASR 3605 BSP/HAL in the real firmware */

/* System tick time (simulated) */
static ULONG sim_time_ticks = 0;

ULONG tx_time_get(void)
{
    return sim_time_ticks++;
}

/* Thread sleep */
UINT tx_thread_sleep(ULONG timer_ticks)
{
    /* Convert ticks to microseconds (assuming 100 Hz tick rate) */
    usleep(timer_ticks * 10000);
    sim_time_ticks += timer_ticks;
    return TX_SUCCESS;
}

/* Mutex operations */
UINT tx_mutex_create(TX_MUTEX *mutex_ptr, CHAR *name_ptr, UINT inherit)
{
    if (pthread_mutex_init(&mutex_ptr->tx_mutex_posix, NULL) != 0) {
        return TX_MUTEX_ERROR;
    }
    mutex_ptr->tx_mutex_name = name_ptr;
    return TX_SUCCESS;
}

UINT tx_mutex_get(TX_MUTEX *mutex_ptr, ULONG wait_option)
{
    pthread_mutex_lock(&mutex_ptr->tx_mutex_posix);
    return TX_SUCCESS;
}

UINT tx_mutex_put(TX_MUTEX *mutex_ptr)
{
    pthread_mutex_unlock(&mutex_ptr->tx_mutex_posix);
    return TX_SUCCESS;
}

UINT tx_mutex_delete(TX_MUTEX *mutex_ptr)
{
    pthread_mutex_destroy(&mutex_ptr->tx_mutex_posix);
    return TX_SUCCESS;
}

/* Event flags operations */
UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP *group_ptr, CHAR *name_ptr)
{
    pthread_mutex_init(&group_ptr->tx_event_mutex, NULL);
    pthread_cond_init(&group_ptr->tx_event_cond, NULL);
    group_ptr->tx_event_flags_current = 0;
    group_ptr->tx_event_flags_group_name = name_ptr;
    return TX_SUCCESS;
}

UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG flags_to_set, UINT set_option)
{
    pthread_mutex_lock(&group_ptr->tx_event_mutex);
    if (set_option == TX_OR) {
        group_ptr->tx_event_flags_current |= flags_to_set;
    } else {
        group_ptr->tx_event_flags_current &= flags_to_set;
    }
    pthread_cond_broadcast(&group_ptr->tx_event_cond);
    pthread_mutex_unlock(&group_ptr->tx_event_mutex);
    return TX_SUCCESS;
}

UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG requested_flags,
                        UINT get_option, ULONG *actual_flags_ptr, ULONG wait_option)
{
    struct timespec ts;
    int result;
    
    pthread_mutex_lock(&group_ptr->tx_event_mutex);
    
    while (1) {
        ULONG current = group_ptr->tx_event_flags_current;
        UINT match = 0;
        
        if (get_option & TX_AND) {
            match = ((current & requested_flags) == requested_flags);
        } else {
            match = ((current & requested_flags) != 0);
        }
        
        if (match) {
            *actual_flags_ptr = current;
            if (get_option & TX_OR_CLEAR) {
                group_ptr->tx_event_flags_current &= ~requested_flags;
            }
            pthread_mutex_unlock(&group_ptr->tx_event_mutex);
            return TX_SUCCESS;
        }
        
        if (wait_option == TX_NO_WAIT) {
            pthread_mutex_unlock(&group_ptr->tx_event_mutex);
            return TX_NO_EVENTS;
        }
        
        if (wait_option == TX_WAIT_FOREVER) {
            pthread_cond_wait(&group_ptr->tx_event_cond, &group_ptr->tx_event_mutex);
        } else {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (wait_option * 10000000);
            while (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            result = pthread_cond_timedwait(&group_ptr->tx_event_cond, 
                                             &group_ptr->tx_event_mutex, &ts);
            if (result != 0) {
                pthread_mutex_unlock(&group_ptr->tx_event_mutex);
                return TX_NO_EVENTS;
            }
        }
    }
}

/* Byte pool operations */
UINT tx_byte_pool_create(TX_BYTE_POOL *pool_ptr, CHAR *name_ptr, 
                          VOID *pool_start, ULONG pool_size)
{
    pool_ptr->tx_byte_pool_name = name_ptr;
    pool_ptr->tx_byte_pool_start = pool_start;
    pool_ptr->tx_byte_pool_size = pool_size;
    pool_ptr->tx_byte_pool_available = pool_size;
    return TX_SUCCESS;
}

UINT tx_byte_allocate(TX_BYTE_POOL *pool_ptr, VOID **memory_ptr, 
                       ULONG memory_size, ULONG wait_option)
{
    /* Simplified - just use malloc in simulation */
    *memory_ptr = malloc(memory_size);
    if (*memory_ptr == NULL) {
        return TX_NO_MEMORY;
    }
    return TX_SUCCESS;
}

UINT tx_byte_release(VOID *memory_ptr)
{
    free(memory_ptr);
    return TX_SUCCESS;
}

/* Thread operations */
UINT tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr,
                       VOID (*entry_function)(ULONG), ULONG entry_input,
                       VOID *stack_start, ULONG stack_size,
                       UINT priority, UINT preempt_threshold,
                       ULONG time_slice, UINT auto_start)
{
    thread_ptr->tx_thread_name = name_ptr;
    thread_ptr->tx_thread_entry = entry_function;
    thread_ptr->tx_thread_entry_input = entry_input;
    thread_ptr->tx_thread_state = auto_start ? TX_READY : TX_SUSPENDED;
    
    if (auto_start) {
        pthread_create(&thread_ptr->tx_thread_posix, NULL, 
                       (void*(*)(void*))entry_function, (void*)entry_input);
    }
    
    return TX_SUCCESS;
}

UINT tx_thread_resume(TX_THREAD *thread_ptr)
{
    if (thread_ptr->tx_thread_state == TX_SUSPENDED) {
        thread_ptr->tx_thread_state = TX_READY;
        pthread_create(&thread_ptr->tx_thread_posix, NULL,
                       (void*(*)(void*))thread_ptr->tx_thread_entry,
                       (void*)thread_ptr->tx_thread_entry_input);
    }
    return TX_SUCCESS;
}

/* Stub for tx_application_define - would be implemented in threadx_netx_init.c for real hardware */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    printf("[SIM] tx_application_define called\n");
    printf("[SIM] This is a minimal simulation - full ThreadX/NetX functionality requires actual libraries\n");
    printf("[SIM] The emulator is now ready to receive streaming commands from the web interface\n");
    printf("[SIM] \n");
    printf("[SIM] Instructions:\n");
    printf("[SIM] 1. Open http://localhost:8080 in your browser\n");
    printf("[SIM] 2. Enter a website URL in the 'Website Streaming' section\n");
    printf("[SIM] 3. Click 'Load Website' to preview\n");
    printf("[SIM] 4. Click 'Stream to Device' to start streaming here\n");
    printf("[SIM] \n");
}

#endif /* HOST_SIMULATION */
