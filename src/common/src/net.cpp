#include "net.hpp"

#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <mstcpip.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <sys/select.h>
#endif

namespace {

#ifdef _WIN32
struct WinsockInitializer {
    WinsockInitializer() { net_init(); }
};
const WinsockInitializer g_winsock_initializer;
#endif

} // namespace

bool net_init() {
#ifdef _WIN32
    static const int result = []() {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0 ? 0 : -1;
    }();
    return result == 0;
#else
    return true;
#endif
}

int net_close(socket_t s) {
    if (!net_valid(s)) {
        return 0;
    }
#ifdef _WIN32
    return ::closesocket(s);
#else
    return ::close(s);
#endif
}

int net_shutdown(socket_t s) {
    if (!net_valid(s)) {
        return 0;
    }
#ifdef _WIN32
    return ::shutdown(s, SD_BOTH);
#else
    return ::shutdown(s, SHUT_RDWR);
#endif
}

bool net_set_timeouts(socket_t s, int milliseconds) {
    if (!net_valid(s) || milliseconds <= 0) {
        return false;
    }
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(milliseconds);
    const char* value = reinterpret_cast<const char*>(&timeout);
    return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, value, sizeof(timeout)) == 0 &&
           ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, value, sizeof(timeout)) == 0;
#else
    struct timeval timeout {};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
           ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

bool net_set_nonblocking(socket_t s, bool enabled) {
    if (!net_valid(s)) {
        return false;
    }
#ifdef _WIN32
    u_long mode = enabled ? 1UL : 0UL;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return ::fcntl(s, F_SETFL, flags) == 0;
#endif
}

bool net_connect_timeout(socket_t s, const struct sockaddr* addr, int addr_len,
                         int timeout_ms) {
    if (!net_valid(s) || addr == nullptr || addr_len <= 0) {
        return false;
    }

    if (!net_set_nonblocking(s, true)) {
        return false;
    }

    int rc = ::connect(s, addr, static_cast<socklen_t>(addr_len));
    if (rc != 0) {
#ifdef _WIN32
        const int error = WSAGetLastError();
        const bool in_progress = error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
        const bool in_progress = errno == EINPROGRESS || errno == EWOULDBLOCK;
#endif
        if (!in_progress) {
            net_set_nonblocking(s, false);
            return false;
        }

        fd_set writable;
        fd_set exceptfds;
        FD_ZERO(&writable);
        FD_ZERO(&exceptfds);
        FD_SET(s, &writable);
        FD_SET(s, &exceptfds);

        struct timeval timeout {};
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

#ifdef _WIN32
        const int selected = ::select(0, nullptr, &writable, &exceptfds, &timeout);
#else
        const int selected = ::select(s + 1, nullptr, &writable, &exceptfds, &timeout);
#endif
        if (selected <= 0) {
            net_set_nonblocking(s, false);
            return false;
        }

        int socket_error = 0;
        socklen_t error_length = sizeof(socket_error);
        if (::getsockopt(s, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&socket_error), &error_length) != 0) {
            net_set_nonblocking(s, false);
            return false;
        }
        if (socket_error != 0) {
            net_set_nonblocking(s, false);
            return false;
        }
    }

    return net_set_nonblocking(s, false);
}

bool net_send_all(socket_t s, const void* data, std::size_t length) {
    if (!net_valid(s) || (data == nullptr && length > 0)) {
        return false;
    }

    const char* cursor = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < length) {
        const std::size_t remaining = length - sent;
        const int chunk = static_cast<int>(
            remaining > static_cast<std::size_t>(1 << 20) ? (1 << 20) : remaining);
#ifdef _WIN32
        const int rc = ::send(s, cursor + sent, chunk, 0);
#else
        const ssize_t rc = ::send(s, cursor + sent, static_cast<size_t>(chunk),
                                  MSG_NOSIGNAL);
#endif
        if (rc <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(rc);
    }
    return true;
}

bool net_recv_exact(socket_t s, void* data, std::size_t length) {
    if (!net_valid(s) || (data == nullptr && length > 0)) {
        return false;
    }

    char* cursor = static_cast<char*>(data);
    std::size_t received = 0;
    while (received < length) {
        const std::size_t remaining = length - received;
        const int chunk = static_cast<int>(
            remaining > static_cast<std::size_t>(1 << 20) ? (1 << 20) : remaining);
#ifdef _WIN32
        const int rc = ::recv(s, cursor + received, chunk, 0);
#else
        const ssize_t rc = ::recv(s, cursor + received, static_cast<size_t>(chunk), 0);
#endif
        if (rc <= 0) {
            return false;
        }
        received += static_cast<std::size_t>(rc);
    }
    return true;
}

int net_recv_some(socket_t s, void* data, std::size_t length) {
    if (!net_valid(s) || (data == nullptr && length > 0)) {
        return -1;
    }
    const std::size_t request = length > static_cast<std::size_t>(1 << 20)
                                    ? static_cast<std::size_t>(1 << 20)
                                    : length;
#ifdef _WIN32
    return ::recv(s, static_cast<char*>(data), static_cast<int>(request), 0);
#else
    return static_cast<int>(::recv(s, data, request, 0));
#endif
}

std::string net_last_error() {
#ifdef _WIN32
    const int code = WSAGetLastError();
    char* text = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageA(
        flags, nullptr, static_cast<DWORD>(code), 0,
        reinterpret_cast<char*>(&text), 0, nullptr);
    std::string message = length > 0 && text != nullptr ? std::string(text, length)
                                                        : std::string("winsock error");
    if (text != nullptr) {
        LocalFree(text);
    }
    while (!message.empty() &&
           (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) {
        message.pop_back();
    }
    return message + " (" + std::to_string(code) + ")";
#else
    return std::string(std::strerror(errno));
#endif
}
