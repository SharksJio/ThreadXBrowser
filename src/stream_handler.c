/*
 * stream_handler.c
 * Website streaming handler for ASR 3605 emulator
 * Processes streaming commands from the gateway
 */

#include "platform_config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HOST_SIMULATION

/* Stream state */
typedef struct {
    int active;
    char url[512];
    char quality[16];
    int frame_count;
    time_t start_time;
} stream_state_t;

static stream_state_t current_stream = {0};

/**
 * Start streaming a website
 */
void handle_stream_start(const char *url, const char *quality)
{
    if (current_stream.active) {
        printf("[STREAM] Stopping existing stream...\n");
        handle_stream_stop();
    }
    
    printf("\n");
    printf("========================================\n");
    printf("WEBSITE STREAM STARTED\n");
    printf("========================================\n");
    printf("URL:     %s\n", url);
    printf("Quality: %s\n", quality);
    printf("========================================\n\n");
    
    current_stream.active = 1;
    strncpy(current_stream.url, url, sizeof(current_stream.url) - 1);
    strncpy(current_stream.quality, quality, sizeof(current_stream.quality) - 1);
    current_stream.frame_count = 0;
    current_stream.start_time = time(NULL);
}

/**
 * Handle stream update (new frame)
 */
void handle_stream_update(int frame)
{
    if (!current_stream.active) {
        printf("[STREAM] Warning: Received update but no active stream\n");
        return;
    }
    
    current_stream.frame_count = frame;
    
    /* Print periodic status */
    if (frame % 10 == 0) {
        int duration = (int)(time(NULL) - current_stream.start_time);
        printf("[STREAM] Frame %d | Duration: %02d:%02d | URL: %s\n",
               frame, duration / 60, duration % 60, current_stream.url);
    }
    
    /* Simulate frame processing based on quality */
    if (strcmp(current_stream.quality, "high") == 0) {
        /* High quality: more detailed simulation */
        if (frame % 5 == 0) {
            printf("[STREAM] Processing high-quality frame %d...\n", frame);
        }
    } else if (strcmp(current_stream.quality, "low") == 0) {
        /* Low quality: optimized for device */
        if (frame % 20 == 0) {
            printf("[STREAM] Processing optimized frame %d...\n", frame);
        }
    }
}

/**
 * Stop streaming
 */
void handle_stream_stop(void)
{
    if (!current_stream.active) {
        return;
    }
    
    int duration = (int)(time(NULL) - current_stream.start_time);
    
    printf("\n");
    printf("========================================\n");
    printf("WEBSITE STREAM STOPPED\n");
    printf("========================================\n");
    printf("Total Frames: %d\n", current_stream.frame_count);
    printf("Duration:     %02d:%02d\n", duration / 60, duration % 60);
    printf("========================================\n\n");
    
    memset(&current_stream, 0, sizeof(current_stream));
}

/**
 * Get current stream status
 */
int is_stream_active(void)
{
    return current_stream.active;
}

/**
 * Get stream statistics
 */
void get_stream_stats(char *buffer, size_t size)
{
    if (!current_stream.active) {
        snprintf(buffer, size, "No active stream");
        return;
    }
    
    int duration = (int)(time(NULL) - current_stream.start_time);
    float fps = current_stream.frame_count / (float)(duration > 0 ? duration : 1);
    
    snprintf(buffer, size,
             "Stream: %s | Frames: %d | Duration: %02d:%02d | FPS: %.1f",
             current_stream.url, current_stream.frame_count,
             duration / 60, duration % 60, fps);
}

/**
 * Process incoming stream command (JSON format)
 * Example: {"type":"streamStart","url":"https://example.com","quality":"medium"}
 */
void process_stream_command(const char *json)
{
    /* Simple JSON parsing for simulation */
    char type[32] = {0};
    char url[512] = {0};
    char quality[16] = "medium";
    int frame = 0;
    
    /* Extract type */
    const char *type_ptr = strstr(json, "\"type\"");
    if (type_ptr) {
        const char *value_start = strchr(type_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '\"');
            if (value_start) {
                value_start++;
                const char *value_end = strchr(value_start, '\"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(type)) {
                        strncpy(type, value_start, len);
                    }
                }
            }
        }
    }
    
    /* Extract URL if present */
    const char *url_ptr = strstr(json, "\"url\"");
    if (url_ptr) {
        const char *value_start = strchr(url_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '\"');
            if (value_start) {
                value_start++;
                const char *value_end = strchr(value_start, '\"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(url)) {
                        strncpy(url, value_start, len);
                    }
                }
            }
        }
    }
    
    /* Extract quality if present */
    const char *quality_ptr = strstr(json, "\"quality\"");
    if (quality_ptr) {
        const char *value_start = strchr(quality_ptr, ':');
        if (value_start) {
            value_start = strchr(value_start, '\"');
            if (value_start) {
                value_start++;
                const char *value_end = strchr(value_start, '\"');
                if (value_end) {
                    size_t len = value_end - value_start;
                    if (len < sizeof(quality)) {
                        strncpy(quality, value_start, len);
                    }
                }
            }
        }
    }
    
    /* Extract frame number if present */
    const char *frame_ptr = strstr(json, "\"frame\"");
    if (frame_ptr) {
        sscanf(frame_ptr + 7, ":%d", &frame);
    }
    
    /* Process command based on type */
    if (strcmp(type, "streamStart") == 0 && strlen(url) > 0) {
        handle_stream_start(url, quality);
    } else if (strcmp(type, "streamUpdate") == 0) {
        handle_stream_update(frame);
    } else if (strcmp(type, "streamStop") == 0) {
        handle_stream_stop();
    } else {
        printf("[STREAM] Unknown command type: %s\n", type);
    }
}

#endif /* HOST_SIMULATION */
