/*
 * stream_handler.h
 * Website streaming handler for ASR 3605 emulator
 */

#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#ifdef HOST_SIMULATION

#include <stddef.h>

/**
 * Start streaming a website
 * @param url Website URL to stream
 * @param quality Stream quality: "high", "medium", or "low"
 */
void handle_stream_start(const char *url, const char *quality);

/**
 * Handle stream update (new frame)
 * @param frame Frame number
 */
void handle_stream_update(int frame);

/**
 * Stop the current stream
 */
void handle_stream_stop(void);

/**
 * Check if a stream is currently active
 * @return 1 if active, 0 otherwise
 */
int is_stream_active(void);

/**
 * Get current stream statistics
 * @param buffer Buffer to store stats string
 * @param size Buffer size
 */
void get_stream_stats(char *buffer, size_t size);

/**
 * Process incoming stream command from gateway
 * @param json JSON command string
 */
void process_stream_command(const char *json);

#endif /* HOST_SIMULATION */

#endif /* STREAM_HANDLER_H */
