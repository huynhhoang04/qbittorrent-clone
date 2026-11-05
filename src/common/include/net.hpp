#pragma once

// Thin portability layer for the POSIX socket API used by the peer and
// tracker components. Linux stays on the native BSD sockets; Windows builds
// map the same operations onto Winsock so the code can be compiled and
// exercised there without changing the wire protocol.

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

using socket_t = SOCKET;
inline constexpr socket_t invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using socket_t = int;
inline constexpr socket_t invalid_socket = -1;
#endif

inline bool net_valid(socket_t s) { return s != invalid_socket; }

// Must be called once per process before any socket operation on Windows.
// On POSIX it is a no-op.
bool net_init();

int net_close(socket_t s);
int net_shutdown(socket_t s);

bool net_set_timeouts(socket_t s, int milliseconds);
bool net_set_nonblocking(socket_t s, bool enabled);

// Blocking connect with an explicit timeout.
bool net_connect_timeout(socket_t s, const struct sockaddr* addr, int addr_len,
                         int timeout_ms);

bool net_send_all(socket_t s, const void* data, std::size_t length);
bool net_recv_exact(socket_t s, void* data, std::size_t length);

// Reads up to `length` bytes and returns the byte count, 0 on orderly
// shutdown, or -1 on error. Unlike net_recv_exact() this is suitable for
// HTTP-style "read until the peer closes" responses.
int net_recv_some(socket_t s, void* data, std::size_t length);

std::string net_last_error();
