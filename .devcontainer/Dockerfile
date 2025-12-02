# ThreadXBrowser Development Container
# Includes ARM toolchain for ASR 3605 cross-compilation

FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install development tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gdb \
    gdb-multiarch \
    clang-format \
    cppcheck \
    doxygen \
    graphviz \
    wget \
    curl \
    git \
    unzip \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install ARM GCC toolchain
RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    -O /tmp/arm-toolchain.tar.xz \
    && mkdir -p /opt/arm-toolchain \
    && tar -xf /tmp/arm-toolchain.tar.xz -C /opt/arm-toolchain --strip-components=1 \
    && rm /tmp/arm-toolchain.tar.xz \
    && ln -sf /opt/arm-toolchain/bin/arm-none-eabi-* /usr/local/bin/

# Verify ARM toolchain installation
RUN arm-none-eabi-gcc --version

# Create directories for ThreadX/NetX (stubs for development)
RUN mkdir -p /opt/threadx/common/inc /opt/netx/common/inc

# Create stub header files for IDE support
RUN echo '/* ThreadX API stub for development */\n\
#ifndef TX_API_H\n\
#define TX_API_H\n\
\n\
#include <stdint.h>\n\
\n\
typedef unsigned int UINT;\n\
typedef unsigned long ULONG;\n\
typedef unsigned char UCHAR;\n\
typedef char CHAR;\n\
typedef void VOID;\n\
typedef uint64_t UINT64;\n\
\n\
#define TX_SUCCESS 0\n\
#define TX_NO_MEMORY 1\n\
#define TX_MUTEX_ERROR 2\n\
#define TX_NO_EVENTS 3\n\
#define TX_WAIT_FOREVER 0xFFFFFFFF\n\
#define TX_NO_WAIT 0\n\
#define TX_AND 2\n\
#define TX_AND_CLEAR 3\n\
#define TX_OR 0\n\
#define TX_OR_CLEAR 1\n\
#define TX_AUTO_START 1\n\
#define TX_DONT_START 0\n\
#define TX_NO_INHERIT 0\n\
#define TX_NO_TIME_SLICE 0\n\
#define TX_TIMER_TICKS_PER_SECOND 100\n\
#define TX_READY 0\n\
#define TX_SUSPENDED 1\n\
\n\
typedef struct TX_THREAD_STRUCT {\n\
    CHAR *tx_thread_name;\n\
    void (*tx_thread_entry)(ULONG);\n\
    ULONG tx_thread_entry_input;\n\
    UINT tx_thread_state;\n\
    void *tx_thread_posix;\n\
} TX_THREAD;\n\
\n\
typedef struct TX_MUTEX_STRUCT {\n\
    CHAR *tx_mutex_name;\n\
    void *tx_mutex_posix;\n\
} TX_MUTEX;\n\
\n\
typedef struct TX_EVENT_FLAGS_STRUCT {\n\
    CHAR *tx_event_flags_group_name;\n\
    ULONG tx_event_flags_current;\n\
    void *tx_event_mutex;\n\
    void *tx_event_cond;\n\
} TX_EVENT_FLAGS_GROUP;\n\
\n\
typedef struct TX_BYTE_POOL_STRUCT {\n\
    CHAR *tx_byte_pool_name;\n\
    VOID *tx_byte_pool_start;\n\
    ULONG tx_byte_pool_size;\n\
    ULONG tx_byte_pool_available;\n\
} TX_BYTE_POOL;\n\
\n\
typedef struct TX_BLOCK_POOL_STRUCT {\n\
    CHAR *tx_block_pool_name;\n\
} TX_BLOCK_POOL;\n\
\n\
void tx_kernel_enter(void);\n\
ULONG tx_time_get(void);\n\
UINT tx_thread_sleep(ULONG timer_ticks);\n\
UINT tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr, void (*entry_function)(ULONG), ULONG entry_input, VOID *stack_start, ULONG stack_size, UINT priority, UINT preempt_threshold, ULONG time_slice, UINT auto_start);\n\
UINT tx_thread_resume(TX_THREAD *thread_ptr);\n\
UINT tx_mutex_create(TX_MUTEX *mutex_ptr, CHAR *name_ptr, UINT inherit);\n\
UINT tx_mutex_get(TX_MUTEX *mutex_ptr, ULONG wait_option);\n\
UINT tx_mutex_put(TX_MUTEX *mutex_ptr);\n\
UINT tx_mutex_delete(TX_MUTEX *mutex_ptr);\n\
UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP *group_ptr, CHAR *name_ptr);\n\
UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG flags_to_set, UINT set_option);\n\
UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG requested_flags, UINT get_option, ULONG *actual_flags_ptr, ULONG wait_option);\n\
UINT tx_byte_pool_create(TX_BYTE_POOL *pool_ptr, CHAR *name_ptr, VOID *pool_start, ULONG pool_size);\n\
UINT tx_byte_allocate(TX_BYTE_POOL *pool_ptr, VOID **memory_ptr, ULONG memory_size, ULONG wait_option);\n\
UINT tx_byte_release(VOID *memory_ptr);\n\
\n\
#endif /* TX_API_H */\n' > /opt/threadx/common/inc/tx_api.h

RUN echo '/* NetX API stub for development */\n\
#ifndef NX_API_H\n\
#define NX_API_H\n\
\n\
#include "tx_api.h"\n\
\n\
#define NX_SUCCESS 0\n\
#define NX_NO_PACKET 1\n\
#define NX_WAIT_FOREVER TX_WAIT_FOREVER\n\
#define NX_IP_NORMAL 0\n\
#define NX_FRAGMENT_OKAY 0\n\
#define NX_IP_TIME_TO_LIVE 128\n\
#define NX_IP_LINK_ENABLED 1\n\
#define NX_TRUE 1\n\
#define NX_FALSE 0\n\
#define NX_ANY_PORT 0\n\
#define NX_UNHANDLED_COMMAND 99\n\
\n\
#define NX_LINK_INITIALIZE 0\n\
#define NX_LINK_ENABLE 1\n\
#define NX_LINK_DISABLE 2\n\
#define NX_LINK_PACKET_SEND 3\n\
#define NX_LINK_PACKET_BROADCAST 4\n\
#define NX_LINK_GET_STATUS 5\n\
#define NX_LINK_ARP_SEND 6\n\
#define NX_LINK_ARP_RESPONSE_SEND 7\n\
#define NX_LINK_RARP_SEND 8\n\
#define NX_LINK_MULTICAST_JOIN 9\n\
#define NX_LINK_MULTICAST_LEAVE 10\n\
#define NX_LINK_GET_SPEED 11\n\
#define NX_LINK_GET_DUPLEX_TYPE 12\n\
#define NX_LINK_GET_ERROR_COUNT 13\n\
#define NX_LINK_GET_RX_COUNT 14\n\
#define NX_LINK_GET_TX_COUNT 15\n\
#define NX_LINK_GET_ALLOC_ERRORS 16\n\
#define NX_LINK_UNINITIALIZE 17\n\
#define NX_LINK_DEFERRED_PROCESSING 18\n\
\n\
#define IP_ADDRESS(a,b,c,d) (((ULONG)(a) << 24) | ((ULONG)(b) << 16) | ((ULONG)(c) << 8) | (ULONG)(d))\n\
\n\
typedef struct NX_IP_STRUCT NX_IP;\n\
typedef struct NX_PACKET_POOL_STRUCT NX_PACKET_POOL;\n\
typedef struct NX_PACKET_STRUCT NX_PACKET;\n\
typedef struct NX_TCP_SOCKET_STRUCT NX_TCP_SOCKET;\n\
typedef struct NX_IP_DRIVER_STRUCT NX_IP_DRIVER;\n\
\n\
typedef struct NX_INTERFACE_STRUCT {\n\
    UINT nx_interface_link_up;\n\
} NX_INTERFACE;\n\
\n\
struct NX_IP_STRUCT {\n\
    CHAR *nx_ip_name;\n\
    NX_INTERFACE nx_ip_interface[4];\n\
};\n\
\n\
struct NX_PACKET_POOL_STRUCT {\n\
    CHAR *nx_packet_pool_name;\n\
};\n\
\n\
struct NX_PACKET_STRUCT {\n\
    ULONG nx_packet_length;\n\
    UCHAR *nx_packet_prepend_ptr;\n\
    UCHAR *nx_packet_append_ptr;\n\
};\n\
\n\
struct NX_TCP_SOCKET_STRUCT {\n\
    CHAR *nx_tcp_socket_name;\n\
};\n\
\n\
struct NX_IP_DRIVER_STRUCT {\n\
    UINT nx_ip_driver_command;\n\
    UINT nx_ip_driver_status;\n\
    NX_IP *nx_ip_driver_ptr;\n\
    NX_PACKET *nx_ip_driver_packet;\n\
    ULONG *nx_ip_driver_return_ptr;\n\
    UINT nx_ip_driver_interface;\n\
};\n\
\n\
UINT nx_packet_pool_create(NX_PACKET_POOL *pool_ptr, CHAR *name_ptr, ULONG payload_size, VOID *pool_start, ULONG pool_size);\n\
UINT nx_packet_allocate(NX_PACKET_POOL *pool_ptr, NX_PACKET **packet_ptr, ULONG packet_type, ULONG wait_option);\n\
UINT nx_packet_release(NX_PACKET *packet_ptr);\n\
UINT nx_packet_data_append(NX_PACKET *packet_ptr, VOID *data_start, ULONG data_size, NX_PACKET_POOL *pool_ptr, ULONG wait_option);\n\
UINT nx_packet_data_retrieve(NX_PACKET *packet_ptr, VOID *buffer_start, ULONG *bytes_copied);\n\
UINT nx_ip_create(NX_IP *ip_ptr, CHAR *name_ptr, ULONG ip_address, ULONG network_mask, NX_PACKET_POOL *default_pool, void (*ip_link_driver)(NX_IP_DRIVER *), VOID *memory_ptr, ULONG memory_size, UINT priority);\n\
UINT nx_ip_address_set(NX_IP *ip_ptr, ULONG ip_address, ULONG network_mask);\n\
UINT nx_ip_address_get(NX_IP *ip_ptr, ULONG *ip_address, ULONG *network_mask);\n\
UINT nx_ip_gateway_address_set(NX_IP *ip_ptr, ULONG ip_address);\n\
UINT nx_ip_interface_status_check(NX_IP *ip_ptr, UINT interface_index, ULONG needed_status, ULONG *actual_status, ULONG wait_option);\n\
UINT nx_arp_enable(NX_IP *ip_ptr, VOID *arp_cache_memory, ULONG arp_cache_size);\n\
UINT nx_icmp_enable(NX_IP *ip_ptr);\n\
UINT nx_tcp_enable(NX_IP *ip_ptr);\n\
UINT nx_udp_enable(NX_IP *ip_ptr);\n\
UINT nx_tcp_socket_create(NX_IP *ip_ptr, NX_TCP_SOCKET *socket_ptr, CHAR *name, ULONG type_of_service, ULONG fragment, UINT time_to_live, ULONG window_size, void (*tcp_urgent_data_callback)(NX_TCP_SOCKET *), void (*tcp_disconnect_callback)(NX_TCP_SOCKET *));\n\
UINT nx_tcp_socket_delete(NX_TCP_SOCKET *socket_ptr);\n\
UINT nx_tcp_client_socket_bind(NX_TCP_SOCKET *socket_ptr, UINT port, ULONG wait_option);\n\
UINT nx_tcp_client_socket_unbind(NX_TCP_SOCKET *socket_ptr);\n\
UINT nx_tcp_client_socket_connect(NX_TCP_SOCKET *socket_ptr, ULONG server_ip, UINT server_port, ULONG wait_option);\n\
UINT nx_tcp_socket_disconnect(NX_TCP_SOCKET *socket_ptr, ULONG wait_option);\n\
UINT nx_tcp_socket_send(NX_TCP_SOCKET *socket_ptr, NX_PACKET *packet_ptr, ULONG wait_option);\n\
UINT nx_tcp_socket_receive(NX_TCP_SOCKET *socket_ptr, NX_PACKET **packet_ptr, ULONG wait_option);\n\
\n\
#endif /* NX_API_H */\n' > /opt/netx/common/inc/nx_api.h

RUN echo '/* NetX TCP header */\n\
#ifndef NX_TCP_H\n\
#define NX_TCP_H\n\
#include "nx_api.h"\n\
#define NX_TCP_PACKET 0\n\
#endif\n' > /opt/netx/common/inc/nx_tcp.h

# Set up user
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && apt-get update \
    && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME \
    && rm -rf /var/lib/apt/lists/*

# Reset frontend
ENV DEBIAN_FRONTEND=dialog

USER $USERNAME

WORKDIR /workspaces/ThreadXBrowser
