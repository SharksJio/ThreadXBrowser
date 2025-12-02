# NetX Secure (WSS) Integration Guide

This guide explains how to enable TLS/SSL support for secure WebSocket connections (WSS) in the ThreadXBrowser client.

## Overview

The ThreadXBrowser client supports both plain WebSocket (WS) and secure WebSocket (WSS) connections. WSS uses TLS encryption to secure the communication between the device and the gateway server.

## Requirements

- NetX Secure library (part of Azure RTOS)
- mbedTLS or hardware cryptographic accelerator
- X.509 certificate for server verification (optional but recommended)

## Configuration

### Enable TLS

Set `WS_USE_TLS` to 1 in `src/platform_config.h`:

```c
#define WS_USE_TLS              1    /* Enable WSS (secure WebSocket) */
```

### TLS Buffer Configuration

Configure the TLS buffer sizes based on your memory constraints:

```c
#define TLS_METADATA_BUFFER_SIZE    8192    /* TLS session metadata */
#define TLS_PACKET_BUFFER_SIZE      4096    /* TLS packet reassembly */
#define TLS_CERT_BUFFER_SIZE        4096    /* Server certificate storage */
```

### Certificate Verification

For production deployments, enable certificate verification:

```c
#define TLS_VERIFY_CERTIFICATE      1    /* Verify server certificate */
#define TLS_VERIFY_HOSTNAME         1    /* Verify hostname in certificate */
```

## Implementation

### 1. Initialize NetX Secure

Before creating the WebSocket connection, initialize the NetX Secure module:

```c
#include "nx_secure_tls_api.h"

/* TLS session and buffers */
NX_SECURE_TLS_SESSION tls_session;
UCHAR tls_metadata_buffer[TLS_METADATA_BUFFER_SIZE];
UCHAR tls_packet_buffer[TLS_PACKET_BUFFER_SIZE];
UCHAR tls_certificate_buffer[TLS_CERT_BUFFER_SIZE];

/* Cryptographic methods */
extern const NX_SECURE_TLS_CRYPTO nx_crypto_tls_ciphers;

UINT init_tls(void)
{
    UINT status;
    
    /* Initialize TLS */
    status = nx_secure_tls_initialize();
    if (status != NX_SUCCESS)
        return status;
    
    /* Create TLS session */
    status = nx_secure_tls_session_create(&tls_session,
                                          &nx_crypto_tls_ciphers,
                                          tls_metadata_buffer,
                                          TLS_METADATA_BUFFER_SIZE);
    if (status != NX_SUCCESS)
        return status;
    
    /* Configure packet buffer for reassembly */
    status = nx_secure_tls_session_packet_buffer_set(&tls_session,
                                                      tls_packet_buffer,
                                                      TLS_PACKET_BUFFER_SIZE);
    if (status != NX_SUCCESS)
        return status;
    
    return NX_SUCCESS;
}
```

### 2. Add Trusted CA Certificates

For certificate verification, add the CA certificate chain:

```c
/* Root CA certificate (PEM or DER format) */
extern const UCHAR root_ca_cert[];
extern const UINT root_ca_cert_len;

UINT add_trusted_certificates(void)
{
    UINT status;
    NX_SECURE_X509_CERT trusted_cert;
    
    /* Initialize certificate structure */
    status = nx_secure_x509_certificate_initialize(&trusted_cert,
                                                    (UCHAR *)root_ca_cert,
                                                    root_ca_cert_len,
                                                    NX_NULL, 0,
                                                    NX_NULL, 0,
                                                    NX_SECURE_X509_KEY_TYPE_NONE);
    if (status != NX_SUCCESS)
        return status;
    
    /* Add to trusted store */
    status = nx_secure_tls_trusted_certificate_add(&tls_session, &trusted_cert);
    if (status != NX_SUCCESS)
        return status;
    
    return NX_SUCCESS;
}
```

### 3. Establish TLS Connection

After TCP connection, perform TLS handshake:

```c
UINT connect_wss(NX_TCP_SOCKET *tcp_socket, const char *hostname)
{
    UINT status;
    
    /* TCP connect (already done) */
    
    /* Set up TLS over TCP socket */
    status = nx_secure_tls_session_start(&tls_session, tcp_socket,
                                          NX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
        DEBUG_ERR("TLS handshake failed: %u", status);
        return status;
    }
    
#if TLS_VERIFY_HOSTNAME
    /* Verify server hostname */
    status = nx_secure_tls_session_sni_extension_set(&tls_session,
                                                      (UCHAR *)hostname,
                                                      strlen(hostname));
    if (status != NX_SUCCESS) {
        DEBUG_WARN("SNI extension failed: %u", status);
        /* Non-fatal on some servers */
    }
#endif
    
    DEBUG_INFO("TLS connection established");
    return NX_SUCCESS;
}
```

### 4. Secure Send/Receive

Replace TCP send/receive with TLS versions:

```c
/* Send data over TLS */
UINT tls_send(NX_PACKET *packet_ptr)
{
    return nx_secure_tls_session_send(&tls_session, packet_ptr,
                                       NX_WAIT_FOREVER);
}

/* Receive data over TLS */
UINT tls_receive(NX_PACKET **packet_ptr, ULONG timeout)
{
    return nx_secure_tls_session_receive(&tls_session, packet_ptr,
                                          timeout);
}
```

### 5. Cleanup

On disconnect, properly cleanup TLS session:

```c
void cleanup_tls(void)
{
    /* End TLS session */
    nx_secure_tls_session_end(&tls_session, NX_WAIT_FOREVER);
    
    /* Delete session */
    nx_secure_tls_session_delete(&tls_session);
}
```

## Hardware Cryptographic Acceleration

For better performance on ASR 3605, use hardware crypto if available:

```c
/* Example: Configure hardware crypto */
#ifdef ASR3605_HW_CRYPTO
    extern NX_CRYPTO_METHOD crypto_method_aes_cbc_hw;
    extern NX_CRYPTO_METHOD crypto_method_sha256_hw;
    
    /* Replace software methods with hardware */
    nx_secure_tls_session_crypto_methods_override(&tls_session,
                                                   &crypto_method_aes_cbc_hw,
                                                   &crypto_method_sha256_hw);
#endif
```

## Memory Optimization

For memory-constrained devices:

1. **Reduce buffer sizes** - Use smaller TLS buffers if possible
2. **Disable unused cipher suites** - Only enable required ciphers
3. **Use session resumption** - Cache TLS sessions to avoid full handshakes

```c
/* Minimal cipher suite configuration */
#define NX_SECURE_TLS_CIPHERSUITE_LIST \
    TLS_RSA_WITH_AES_128_CBC_SHA256, \
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256

/* Enable session tickets */
nx_secure_tls_session_ticket_enable(&tls_session,
                                     session_ticket_buffer,
                                     SESSION_TICKET_SIZE);
```

## Troubleshooting

### Common Issues

1. **Certificate verification failure**
   - Ensure correct CA certificate is loaded
   - Check certificate expiration date
   - Verify system time is correct

2. **Handshake timeout**
   - Increase `TLS_HANDSHAKE_TIMEOUT`
   - Check network connectivity
   - Verify server TLS configuration

3. **Out of memory during handshake**
   - Increase `TLS_METADATA_BUFFER_SIZE`
   - Reduce maximum certificate chain length

### Debug Output

Enable detailed TLS debugging:

```c
#define NX_SECURE_TLS_DEBUG_ENABLE
#define NX_SECURE_TLS_DEBUG_LEVEL 4  /* Max verbosity */
```

## Example: Complete WSS Connection

```c
UINT ws_connect_secure(ws_client_t *client)
{
    UINT status;
    
    /* 1. Initialize TLS */
    status = init_tls();
    if (status != NX_SUCCESS) return status;
    
    /* 2. Add trusted certificates */
    status = add_trusted_certificates();
    if (status != NX_SUCCESS) return status;
    
    /* 3. TCP connect */
    status = nx_tcp_client_socket_connect(&client->tcp_socket,
                                           server_ip, server_port,
                                           client->config.connect_timeout);
    if (status != NX_SUCCESS) return status;
    
    /* 4. TLS handshake */
    status = connect_wss(&client->tcp_socket, client->config.host);
    if (status != NX_SUCCESS) {
        nx_tcp_socket_disconnect(&client->tcp_socket, NX_NO_WAIT);
        return status;
    }
    
    /* 5. WebSocket handshake over TLS */
    status = ws_perform_handshake_tls(client);
    if (status != NX_SUCCESS) {
        cleanup_tls();
        nx_tcp_socket_disconnect(&client->tcp_socket, NX_NO_WAIT);
        return status;
    }
    
    return NX_SUCCESS;
}
```

## References

- [Azure RTOS NetX Secure Documentation](https://docs.microsoft.com/en-us/azure/rtos/netx-duo/netx-secure-tls/chapter1)
- [RFC 6455 - The WebSocket Protocol](https://tools.ietf.org/html/rfc6455)
- [RFC 5246 - TLS 1.2](https://tools.ietf.org/html/rfc5246)
- [mbedTLS Documentation](https://tls.mbed.org/api/)
