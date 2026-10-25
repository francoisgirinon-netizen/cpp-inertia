#include "loopback.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace inertia::test_loopback {

namespace {

#if defined(_WIN32)
using Handle = SOCKET;
constexpr Handle kInvalid = INVALID_SOCKET;

void closeHandle(Handle handle) { ::closesocket(handle); }

// Winsock must be initialised once per process before any other call.
void ensureNetworking() {
    static const bool started = [] {
        WSADATA data{};
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup() failed");
        }
        return true;
    }();
    static_cast<void>(started);
}

void setTimeouts(Handle handle, std::chrono::seconds limit) {
    // Winsock takes a DWORD of milliseconds
    const auto milliseconds = static_cast<DWORD>(std::chrono::milliseconds{limit}.count());
    ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&milliseconds),
                 sizeof(milliseconds));
    ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&milliseconds),
                 sizeof(milliseconds));
}

long long sendSome(Handle handle, const char *data, std::size_t size) {
    return ::send(handle, data, static_cast<int>(size), 0);
}

long long receiveSome(Handle handle, char *data, std::size_t size) {
    return ::recv(handle, data, static_cast<int>(size), 0);
}
#else
using Handle = int;
constexpr Handle kInvalid = -1;

void closeHandle(Handle handle) { ::close(handle); }

void ensureNetworking() {}

void setTimeouts(Handle handle, std::chrono::seconds limit) {
    // Counted in the type of tv_sec, which std::chrono::seconds' representation need not be.
    const timeval value{
        .tv_sec =
            std::chrono::duration_cast<std::chrono::duration<decltype(timeval::tv_sec)>>(limit)
                .count(),
        .tv_usec = 0};
    ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
    ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
}

long long sendSome(Handle handle, const char *data, std::size_t size) {
    return ::send(handle, data, size, 0);
}

long long receiveSome(Handle handle, char *data, std::size_t size) {
    return ::recv(handle, data, size, 0);
}
#endif

// The length of a resolved address as the socket calls want it: socklen_t, which is an int on
// Windows where addrinfo::ai_addrlen is a size_t.
socklen_t addressLength(const addrinfo &target) {
    return static_cast<socklen_t>(static_cast<std::size_t>(target.ai_addrlen));
}

// Owns a socket handle.
class Socket {
  public:
    explicit Socket(const addrinfo &target)
        : handle_(::socket(target.ai_family, target.ai_socktype, target.ai_protocol)) {
        if (handle_ == kInvalid) {
            throw std::runtime_error("socket() failed");
        }
    }
    ~Socket() { closeHandle(handle_); }
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&) = delete;
    Socket &operator=(Socket &&) = delete;

    [[nodiscard]] Handle get() const { return handle_; }

  private:
    Handle handle_;
};

// Resolves the loopback address through getaddrinfo, which hands back a ready sockaddr and so
// needs no cast of a sockaddr_in. `port` 0 asks for any port when binding.
std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> loopback(std::uint16_t port) {
    ensureNetworking();
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    addrinfo *found = nullptr;
    if (::getaddrinfo("127.0.0.1", std::to_string(port).c_str(), &hints, &found) != 0) {
        throw std::runtime_error("getaddrinfo() failed");
    }
    return {found, &::freeaddrinfo};
}

} // namespace

std::uint16_t probeFreePort() {
    const auto target = loopback(0);
    const Socket socket{*target};
    if (::bind(socket.get(), target->ai_addr, addressLength(*target)) != 0) {
        throw std::runtime_error("cannot probe a free port: bind() failed");
    }
    // The address that was resolved is overwritten with the bound one (same family, same size).
    auto length = addressLength(*target);
    if (::getsockname(socket.get(), target->ai_addr, &length) != 0) {
        throw std::runtime_error("cannot probe a free port: getsockname() failed");
    }
    // getnameinfo turns the bound address into its port number without a cast to sockaddr_in.
    std::array<char, 16> service{};
    if (::getnameinfo(target->ai_addr, length, nullptr, 0, service.data(),
                      static_cast<socklen_t>(service.size()), NI_NUMERICSERV) != 0) {
        throw std::runtime_error("cannot probe a free port: getnameinfo() failed");
    }
    const std::string_view text{service.data()};
    unsigned port = 0;
    const auto [parsed, ec] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (ec != std::errc{} || parsed != text.data() + text.size() || port == 0 || port > 65535) {
        throw std::runtime_error("cannot probe a free port: unexpected port number");
    }
    return static_cast<std::uint16_t>(port);
}

bool acceptsConnections(std::uint16_t port) {
    const auto target = loopback(port);
    const Socket socket{*target};
    return ::connect(socket.get(), target->ai_addr, addressLength(*target)) == 0;
}

std::string rawExchange(std::uint16_t port, const std::string &request) {
    const auto target = loopback(port);
    const Socket socket{*target};
    // A hung server must fail a test, not hang it.
    setTimeouts(socket.get(), std::chrono::seconds{15});
    if (::connect(socket.get(), target->ai_addr, addressLength(*target)) != 0) {
        throw std::runtime_error("connect() failed");
    }
    std::size_t sent = 0;
    while (sent < request.size()) {
        const auto wrote = sendSome(socket.get(), request.data() + sent, request.size() - sent);
        if (wrote <= 0) {
            throw std::runtime_error("send() failed");
        }
        sent += static_cast<std::size_t>(wrote);
    }
    std::string out;
    std::array<char, 4096> buffer{};
    while (true) {
        const auto got = receiveSome(socket.get(), buffer.data(), buffer.size());
        if (got <= 0) {
            break;
        }
        out.append(buffer.data(), static_cast<std::size_t>(got));
    }
    return out;
}

} // namespace inertia::test_loopback
