/*
 * ws_client_sim.h
 * Simple WebSocket client for emulator simulation
 */

#ifndef WS_CLIENT_SIM_H
#define WS_CLIENT_SIM_H

#ifdef HOST_SIMULATION

/**
 * Connect to WebSocket server
 * @param host Server hostname or IP
 * @param port Server port
 * @return 0 on success, -1 on failure
 */
int ws_connect(const char *host, int port);

/**
 * Send text message
 * @param message Text message to send
 * @return 0 on success, -1 on failure
 */
int ws_send_text(const char *message);

/**
 * Receive and process messages (non-blocking)
 * @return Number of bytes received, 0 if no data, -1 on error
 */
int ws_receive();

/**
 * Disconnect from server
 */
void ws_disconnect();

/**
 * Check if connected
 * @return 1 if connected, 0 otherwise
 */
int ws_is_connected();

#endif /* HOST_SIMULATION */

#endif /* WS_CLIENT_SIM_H */
