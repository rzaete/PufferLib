/* invictus_transport.h
 *
 * Raw UDP transport for the Invictus / Robocup Soccer server.
 *
 * - UDP sockets (non-blocking)
 * - connect / send / recv / has_incoming_data
 * - Records last message sent and last message received
 *
 * The higher-level protocol types (sensors, playmodes, effectors, parsers)
 * live in invictus_player_protocol.h (or invictus_trainer_protocol.h).
 *
 * Include via invictus_player.h for normal use.
 * You can also reach the raw transport directly via: &p->comm
 */

#ifndef INVICTUS_TRANSPORT_H
#define INVICTUS_TRANSPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Constants for the transport layer                                  */
/* ------------------------------------------------------------------ */

#define INVICTUS_MAX_MESG     8192
#define INVICTUS_DEFAULT_PORT 6000

/* ------------------------------------------------------------------ */
/* Raw transport client state                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    /* network */
    int                sock;
    struct sockaddr_in server_addr;

    /* last messages sent / received */
    char last_msg_sent[INVICTUS_MAX_MESG];
    int  last_msg_sent_len;
    char last_msg_received[INVICTUS_MAX_MESG];
    int  last_msg_received_len;

    /* comms-level state */
    bool connected;
} InvictusTransport;

/* ------------------------------------------------------------------ */
/* Client transport API (communication only)                          */
/* ------------------------------------------------------------------ */

/* All functions return int error/result codes:
 *   0     = success
 *   < 0   = error (typically -1)
 *
 * Data functions retain their length-based semantics:
 *   send: bytes sent or <=0 on failure
 *   recv: bytes received, 0 on would-block, -1 on error
 */

/* Initialize a bare transport client */
int invictus_transport_init(InvictusTransport* c);

/* Connect to server (opens socket, sets peer). Does NOT send soccer init. */
int invictus_transport_connect(InvictusTransport* c, const char* host, int port);

int invictus_transport_disconnect(InvictusTransport* c);

/* Send a null-terminated or exact message (adds the +1 like classic clients).
 * Records into last_msg_sent on success. */
int invictus_transport_send(InvictusTransport* c, const char* msg);

/* Receive one datagram into the transport's last_msg_received buffer. Returns len or <=0 */
int invictus_transport_recv(InvictusTransport* c);

/* Check for incoming data with a select using the provided timeout (msec).
 * Returns >0 if data ready, 0 on timeout, -1 on error or invalid. */
int invictus_transport_has_incoming_data(InvictusTransport* c, int timeout_msec);

/* ------------------------------------------------------------------ */
/* Implementation                                                     */
/* ------------------------------------------------------------------ */

static void invictus_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static int invictus_open_socket(InvictusTransport* c)
{
    if (!c) return -1;
    c->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (c->sock < 0) {
        invictus_log("invictus: socket() failed: %s", strerror(errno));
        return -1;
    }
    /* bind to ephemeral port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    if (bind(c->sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        invictus_log("invictus: bind() failed: %s", strerror(errno));
        close(c->sock);
        c->sock = -1;
        return -1;
    }
    /* non-blocking */
    int flags = fcntl(c->sock, F_GETFL, 0);
    if (flags >= 0) fcntl(c->sock, F_SETFL, flags | O_NONBLOCK);
    return 0;
}

static int invictus_set_peer(InvictusTransport* c, const char* host, int port)
{
    if (!c || !host) return -1;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        memset(&c->server_addr, 0, sizeof(c->server_addr));
        c->server_addr.sin_family = AF_INET;
        c->server_addr.sin_port   = htons(port);
        if (inet_aton(host, &c->server_addr.sin_addr) == 0) {
            invictus_log("invictus: cannot resolve host %s", host);
            if (res) freeaddrinfo(res);
            return -1;
        }
    } else {
        memcpy(&c->server_addr, res->ai_addr, res->ai_addrlen);
        c->server_addr.sin_port = htons(port);
        freeaddrinfo(res);
    }
    return 0;
}

int invictus_transport_init(InvictusTransport* c)
{
    if (!c) return -1;
    memset(c, 0, sizeof(*c));
    c->sock = -1;
    c->last_msg_sent[0] = '\0';
    c->last_msg_sent_len = 0;
    c->last_msg_received[0] = '\0';
    c->last_msg_received_len = 0;
    c->connected = false;
    return 0;
}

int invictus_transport_connect(InvictusTransport* c, const char* host, int port)
{
    if (!c || !host) {
        invictus_log("invictus: transport_connect: invalid args (c=%p host=%s)", (void*)c, host ? host : "(null)");
        return -1;
    }
    if (c->sock >= 0) invictus_transport_disconnect(c);

    if (invictus_open_socket(c) != 0) return -1;
    if (invictus_set_peer(c, host, port) != 0) {
        invictus_log("invictus: connect: failed to set peer for %s:%d", host, port);
        close(c->sock);
        c->sock = -1;
        return -1;
    }
    c->connected = true;
    return 0;
}

int invictus_transport_disconnect(InvictusTransport* c)
{
    if (!c) {
        invictus_log("invictus: transport_disconnect: null transport");
        return -1;
    }
    if (c->sock >= 3 && c->sock < 4096) {
        close(c->sock);
        c->sock = -1;
    } else {
        c->sock = -1;
    }
    c->connected = false;
    return 0;
}

int invictus_transport_send(InvictusTransport* c, const char* msg)
{
    if (!c || c->sock < 0 || !msg) return 0;
    size_t len = strlen(msg);
    for (int r = 0; r < 4; r++) {
        ssize_t n = sendto(c->sock, msg, len + 1, 0,
                           (struct sockaddr*)&c->server_addr, sizeof(c->server_addr));
        if (n > 0) {
            /* record last sent */
            c->last_msg_sent_len = (len > sizeof(c->last_msg_sent) - 1) ? (int)sizeof(c->last_msg_sent) - 1 : (int)len;
            memcpy(c->last_msg_sent, msg, c->last_msg_sent_len);
            c->last_msg_sent[c->last_msg_sent_len] = '\0';
            // invictus_log("[SENT] %s", c->last_msg_sent);
            return (int)n;
        }
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                invictus_log("invictus: sendto failed: %s", strerror(errno));
                return 0;
            }
            // transient, small backoff
            struct timespec ts = {0, 15000000L}; // 15ms
            nanosleep(&ts, NULL);
            continue;
        }
        // n==0, try again
    }
    return 0;
}

int invictus_transport_recv(InvictusTransport* c)
{
    if (!c || c->sock < 0) return -1;
    char buf[INVICTUS_MAX_MESG];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(c->sock, buf, sizeof(buf)-1, 0,
                         (struct sockaddr*)&from, &fromlen);
    if (n <= 0) {
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            invictus_log("invictus: recvfrom: %s", strerror(errno));
        return (int)n;
    }
    buf[n] = '\0';
    c->last_msg_received_len = (n > (int)sizeof(c->last_msg_received)-1) ? (int)sizeof(c->last_msg_received)-1 : (int)n;
    memcpy(c->last_msg_received, buf, c->last_msg_received_len);
    c->last_msg_received[c->last_msg_received_len] = '\0';
    // invictus_log("[RECV] %s", c->last_msg_received);
    /* Update destination to server's actual send address (switches from init port
       6000 to the dedicated player port for v>=8 clients after first response). */
    c->server_addr.sin_addr = from.sin_addr;
    c->server_addr.sin_port = from.sin_port;
    return (int)n;
}

int invictus_transport_has_incoming_data(InvictusTransport* c, int timeout_msec)
{
    if (!c || c->sock < 0) return -1;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(c->sock, &rfds);

    struct timeval tv;
    if (timeout_msec < 0) timeout_msec = 0;
    tv.tv_sec  = timeout_msec / 1000;
    tv.tv_usec = (timeout_msec % 1000) * 1000;

    int ret = select(c->sock + 1, &rfds, NULL, NULL, &tv);
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* INVICTUS_TRANSPORT_H */
