#pragma once

#include "../../transport/TransportInterfaces.h"

#include "../../../core/Defines.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace httpadv::v1::platform::posix {

using SocketHandle = int;
constexpr SocketHandle InvalidSocketHandle = -1;

class SocketRuntime {
public:
  bool initialized() const { return true; }
};

inline int lastSocketError() { return errno; }

inline bool isWouldBlockError(int errorCode) {
  return errorCode == EAGAIN || errorCode == EWOULDBLOCK ||
         errorCode == ETIMEDOUT;
}

inline bool isInterruptedError(int errorCode) { return errorCode == EINTR; }

inline int closeSocket(SocketHandle handle) { return close(handle); }

inline bool setNonBlocking(SocketHandle handle, bool enabled) {
  const int flags = fcntl(handle, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }

  const int updatedFlags =
      enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  return fcntl(handle, F_SETFL, updatedFlags) == 0;
}

inline bool setSocketTimeout(SocketHandle handle, std::uint32_t timeoutMs) {
  timeval timeout{};
  timeout.tv_sec = static_cast<long>(timeoutMs / 1000U);
  timeout.tv_usec = static_cast<long>((timeoutMs % 1000U) * 1000U);
  return setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                    sizeof(timeout)) == 0 &&
         setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                    sizeof(timeout)) == 0;
}

inline bool queryPendingByteCount(SocketHandle handle,
                                  std::size_t &pendingBytes) {
  int nativePendingBytes = 0;
  if (ioctl(handle, FIONREAD, &nativePendingBytes) != 0) {
    return false;
  }

  pendingBytes =
      nativePendingBytes > 0 ? static_cast<std::size_t>(nativePendingBytes) : 0;
  return true;
}

inline SocketRuntime &socketRuntime() {
  static SocketRuntime runtime;
  return runtime;
}

inline AvailableResult queryPendingBytes(SocketHandle handle,
                                         bool assumeConnectedWhenNoBytes = true) {
  std::size_t pendingBytes = 0;
  if (!queryPendingByteCount(handle, pendingBytes)) {
    return ErrorResult(lastSocketError());
  }

  if (pendingBytes > 0) {
    return AvailableBytes(pendingBytes);
  }

  return assumeConnectedWhenNoBytes ? TemporarilyUnavailableResult()
                                    : ExhaustedResult();
}

inline bool socketHasExceptionalState(SocketHandle handle) {
  fd_set exceptSet;
  FD_ZERO(&exceptSet);
  FD_SET(handle, &exceptSet);
  timeval timeout{};
  const int ready = select(static_cast<int>(handle) + 1, nullptr, nullptr,
                           &exceptSet, &timeout);
  return ready > 0 && FD_ISSET(handle, &exceptSet);
}

inline bool isSocketConnected(SocketHandle handle) {
  if (handle == InvalidSocketHandle) {
    return false;
  }

  if (socketHasExceptionalState(handle)) {
    return false;
  }

  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(handle, &readSet);
  timeval timeout{};
  const int ready = select(static_cast<int>(handle) + 1, &readSet, nullptr,
                           nullptr, &timeout);
  if (ready < 0) {
    return false;
  }

  if (ready == 0) {
    return true;
  }

  if (!FD_ISSET(handle, &readSet)) {
    return true;
  }

  std::uint8_t byte = 0;
  const int result = recv(handle, reinterpret_cast<char *>(&byte), 1, MSG_PEEK);
  if (result > 0) {
    return true;
  }

  if (result == 0) {
    return false;
  }

  return isWouldBlockError(lastSocketError());
}

inline std::string socketAddressToString(const sockaddr_storage &address) {
  char buffer[INET6_ADDRSTRLEN] = {};
  const void *rawAddress = nullptr;

  if (address.ss_family == AF_INET) {
    rawAddress = &reinterpret_cast<const sockaddr_in &>(address).sin_addr;
  } else if (address.ss_family == AF_INET6) {
    rawAddress = &reinterpret_cast<const sockaddr_in6 &>(address).sin6_addr;
  } else {
    return {};
  }

  return inet_ntop(address.ss_family, rawAddress, buffer, sizeof(buffer)) != nullptr
             ? std::string(buffer)
             : std::string();
}

inline std::uint16_t socketAddressToPort(const sockaddr_storage &address) {
  if (address.ss_family == AF_INET) {
    return ntohs(reinterpret_cast<const sockaddr_in &>(address).sin_port);
  }

  if (address.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const sockaddr_in6 &>(address).sin6_port);
  }

  return 0;
}

inline bool resolveSocketAddress(std::string_view host, std::uint16_t port,
                                 int socketType, sockaddr_storage &resolvedAddress,
                                 socklen_t &resolvedLength, bool passive = false) {
  if (!socketRuntime().initialized()) {
    return false;
  }

  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = socketType;
  hints.ai_protocol = socketType == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
  hints.ai_flags = passive ? AI_PASSIVE : 0;

  const std::string hostString(host);
  const std::string portString = std::to_string(port);
  addrinfo *results = nullptr;
  const int result = getaddrinfo(passive ? nullptr : hostString.c_str(),
                                 portString.c_str(), &hints, &results);
  if (result != 0 || results == nullptr) {
    return false;
  }

  const addrinfo *selected = results;
  if (selected->ai_addrlen > static_cast<int>(sizeof(resolvedAddress))) {
    freeaddrinfo(results);
    return false;
  }

  std::memset(&resolvedAddress, 0, sizeof(resolvedAddress));
  std::memcpy(&resolvedAddress, selected->ai_addr, selected->ai_addrlen);
  resolvedLength = static_cast<socklen_t>(selected->ai_addrlen);
  freeaddrinfo(results);
  return true;
}

class SocketHandleOwner {
public:
  SocketHandleOwner() = default;

  explicit SocketHandleOwner(SocketHandle handle) : handle_(handle) {}

  SocketHandleOwner(const SocketHandleOwner &) = delete;
  SocketHandleOwner &operator=(const SocketHandleOwner &) = delete;

  SocketHandleOwner(SocketHandleOwner &&other) noexcept
      : handle_(other.release()) {}

  SocketHandleOwner &operator=(SocketHandleOwner &&other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  ~SocketHandleOwner() { reset(); }

  SocketHandle get() const { return handle_; }

  bool valid() const { return handle_ != InvalidSocketHandle; }

  SocketHandle release() {
    const SocketHandle handle = handle_;
    handle_ = InvalidSocketHandle;
    return handle;
  }

  void reset(SocketHandle handle = InvalidSocketHandle) {
    if (handle_ != InvalidSocketHandle) {
      closeSocket(handle_);
    }

    handle_ = handle;
  }

private:
  SocketHandle handle_ = InvalidSocketHandle;
};

class PosixSocketClient : public IClient {
public:
  explicit PosixSocketClient(SocketHandle socketHandle)
      : socket_(socketHandle) {
    refreshEndpointInfo();
  }

  void stop() override { socket_.reset(); }

  bool connected() override { return isSocketConnected(socket_.get()); }

  std::string_view remoteAddress() const override { return remoteAddress_; }

  std::uint16_t remotePort() override { return remotePort_; }

  std::string_view localAddress() const override { return localAddress_; }

  std::uint16_t localPort() override { return localPort_; }

  void setTimeout(std::uint32_t timeoutMs) override {
    timeoutMs_ = timeoutMs;
    if (socket_.valid()) {
      setSocketTimeout(socket_.get(), timeoutMs_);
    }
  }

  std::uint32_t getTimeout() const override { return timeoutMs_; }

  AvailableResult available() override {
    if (!socket_.valid()) {
      return ExhaustedResult();
    }

    const AvailableResult pending = queryPendingBytes(socket_.get());
    if (pending.hasBytes() || pending.hasError()) {
      return pending;
    }

    return connected() ? TemporarilyUnavailableResult() : ExhaustedResult();
  }

  std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) override {
    if (!socket_.valid() || buffer.empty()) {
      return 0;
    }

    const int bytesRead = recv(socket_.get(), reinterpret_cast<char *>(buffer.data()),
                               static_cast<int>((std::min)(buffer.size(), static_cast<std::size_t>(std::numeric_limits<int>::max()))),
                               0);
    if (bytesRead > 0) {
      return static_cast<std::size_t>(bytesRead);
    }

    if (bytesRead == 0) {
      return 0;
    }

    const int errorCode = lastSocketError();
    if (isWouldBlockError(errorCode) || isInterruptedError(errorCode)) {
      return 0;
    }

    return 0;
  }

  std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) override {
    if (!socket_.valid() || buffer.empty()) {
      return 0;
    }

    const int bytesRead = recv(socket_.get(), reinterpret_cast<char *>(buffer.data()),
                               static_cast<int>((std::min)(buffer.size(), static_cast<std::size_t>(std::numeric_limits<int>::max()))),
                               MSG_PEEK);
    if (bytesRead > 0) {
      return static_cast<std::size_t>(bytesRead);
    }

    if (bytesRead == 0) {
      return 0;
    }

    const int errorCode = lastSocketError();
    if (isWouldBlockError(errorCode) || isInterruptedError(errorCode)) {
      return 0;
    }

    return 0;
  }

  std::size_t write(httpadv::v1::util::span<const std::uint8_t> buffer) override {
    if (!socket_.valid() || buffer.empty()) {
      return 0;
    }

    std::size_t totalSent = 0;
    while (totalSent < buffer.size()) {
      const int sent = send(socket_.get(), reinterpret_cast<const char *>(buffer.data() + totalSent),
                            static_cast<int>((std::min)(buffer.size() - totalSent, static_cast<std::size_t>(std::numeric_limits<int>::max()))),
                            0);
      if (sent > 0) {
        totalSent += static_cast<std::size_t>(sent);
        continue;
      }

      if (sent == 0) {
        break;
      }

      const int errorCode = lastSocketError();
      if (isWouldBlockError(errorCode) || isInterruptedError(errorCode)) {
        break;
      }

      break;
    }

    return totalSent;
  }

  void flush() override {}

private:
  void refreshEndpointInfo() {
    if (!socket_.valid()) {
      remoteAddress_.clear();
      remotePort_ = 0;
      localAddress_.clear();
      localPort_ = 0;
      return;
    }

    sockaddr_storage address{};
    socklen_t addressLength = sizeof(address);
    if (getpeername(socket_.get(), reinterpret_cast<sockaddr *>(&address), &addressLength) == 0) {
      remoteAddress_ = socketAddressToString(address);
      remotePort_ = socketAddressToPort(address);
    }

    address = {};
    addressLength = sizeof(address);
    if (getsockname(socket_.get(), reinterpret_cast<sockaddr *>(&address), &addressLength) == 0) {
      localAddress_ = socketAddressToString(address);
      localPort_ = socketAddressToPort(address);
    }
  }

  SocketHandleOwner socket_{};
  std::string remoteAddress_{};
  std::uint16_t remotePort_ = 0;
  std::string localAddress_{};
  std::uint16_t localPort_ = 0;
  std::uint32_t timeoutMs_ = 0;
};

class PosixSocketServer : public IServer {
public:
  explicit PosixSocketServer(std::uint16_t port)
      : configuredPort_(port), localPort_(port) {}

  std::unique_ptr<IClient> accept() override {
    if (!listener_.valid()) {
      return nullptr;
    }

    sockaddr_storage address{};
    socklen_t addressLength = sizeof(address);
    const SocketHandle accepted = ::accept(listener_.get(), reinterpret_cast<sockaddr *>(&address), &addressLength);
    if (accepted == InvalidSocketHandle) {
      const int errorCode = lastSocketError();
      return isWouldBlockError(errorCode) || isInterruptedError(errorCode) ? nullptr : nullptr;
    }

    return std::make_unique<PosixSocketClient>(accepted);
  }

  void begin() override {
    if (listener_.valid() || !socketRuntime().initialized()) {
      return;
    }

    sockaddr_storage bindAddress{};
    socklen_t bindLength = 0;
    if (!resolveSocketAddress({}, configuredPort_, SOCK_STREAM, bindAddress, bindLength, true)) {
      return;
    }

    SocketHandleOwner listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!listener.valid()) {
      return;
    }

    int reuseAddress = 1;
    setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&reuseAddress), sizeof(reuseAddress));
    if (bind(listener.get(), reinterpret_cast<const sockaddr *>(&bindAddress), bindLength) != 0) {
      return;
    }

    if (listen(listener.get(), static_cast<int>(MAX_CONCURRENT_CONNECTIONS)) != 0) {
      return;
    }

    if (!setNonBlocking(listener.get(), true)) {
      return;
    }

    listener_ = std::move(listener);
    refreshLocalEndpointInfo();
  }

  std::uint16_t port() const override { return localPort_; }

  std::string_view localAddress() const override { return localAddress_; }

  void end() override { listener_.reset(); }

private:
  void refreshLocalEndpointInfo() {
    if (!listener_.valid()) {
      localAddress_ = "0.0.0.0";
      localPort_ = configuredPort_;
      return;
    }

    sockaddr_storage address{};
    socklen_t addressLength = sizeof(address);
    if (getsockname(listener_.get(), reinterpret_cast<sockaddr *>(&address), &addressLength) == 0) {
      localAddress_ = socketAddressToString(address);
      localPort_ = socketAddressToPort(address);
    }
  }

  SocketHandleOwner listener_{};
  std::uint16_t configuredPort_ = 0;
  std::string localAddress_ = "0.0.0.0";
  std::uint16_t localPort_ = 0;
};

class PosixSocketPeer : public IPeer {
public:
  bool begin(std::uint16_t port) override { return openSocket(port); }

  bool beginMulticast(std::string_view multicastAddress, std::uint16_t port) override {
    if (!openSocket(port)) {
      return false;
    }

    sockaddr_storage multicastSocketAddress{};
    socklen_t multicastAddressLength = 0;
    if (!resolveSocketAddress(multicastAddress, port, SOCK_DGRAM, multicastSocketAddress,
                              multicastAddressLength, false) ||
        multicastSocketAddress.ss_family != AF_INET) {
      return false;
    }

    ip_mreq membership{};
    membership.imr_multiaddr = reinterpret_cast<const sockaddr_in &>(multicastSocketAddress).sin_addr;
    membership.imr_interface.s_addr = htonl(INADDR_ANY);
    return setsockopt(socket_.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      reinterpret_cast<const char *>(&membership), sizeof(membership)) == 0;
  }

  void stop() override {
    socket_.reset();
    inboundPacket_.clear();
    inboundOffset_ = 0;
    outboundPacket_.clear();
    outboundDestinationLength_ = 0;
    remoteAddress_.clear();
    remotePort_ = 0;
  }

  bool beginPacket(std::string_view address, std::uint16_t port) override {
    outboundPacket_.clear();
    outboundDestination_ = {};
    outboundDestinationLength_ = 0;
    if (!socket_.valid()) {
      return false;
    }

    return resolveSocketAddress(address, port, SOCK_DGRAM, outboundDestination_,
                                outboundDestinationLength_, false);
  }

  bool endPacket() override {
    if (!socket_.valid() || outboundDestinationLength_ == 0) {
      return false;
    }

    const int sent = sendto(socket_.get(), reinterpret_cast<const char *>(outboundPacket_.data()),
                            static_cast<int>((std::min)(outboundPacket_.size(), static_cast<std::size_t>(std::numeric_limits<int>::max()))),
                            0, reinterpret_cast<const sockaddr *>(&outboundDestination_), outboundDestinationLength_);
    outboundPacket_.clear();
    outboundDestinationLength_ = 0;
    return sent >= 0;
  }

  std::size_t write(httpadv::v1::util::span<const std::uint8_t> buffer) override {
    outboundPacket_.insert(outboundPacket_.end(), buffer.begin(), buffer.end());
    return buffer.size();
  }

  AvailableResult parsePacket() override {
    if (!socket_.valid()) {
      return ExhaustedResult();
    }

    if (inboundOffset_ < inboundPacket_.size()) {
      return AvailableBytes(inboundPacket_.size() - inboundOffset_);
    }

    const AvailableResult pendingBytes = queryPendingBytes(socket_.get());
    if (pendingBytes.hasError()) {
      return pendingBytes;
    }

    if (!pendingBytes.hasBytes()) {
      return TemporarilyUnavailableResult();
    }

    std::vector<std::uint8_t> packetBuffer(pendingBytes.count);
    sockaddr_storage sourceAddress{};
    socklen_t sourceAddressLength = sizeof(sourceAddress);
    const int received = recvfrom(socket_.get(), reinterpret_cast<char *>(packetBuffer.data()),
                                  static_cast<int>((std::min)(packetBuffer.size(), static_cast<std::size_t>(std::numeric_limits<int>::max()))),
                                  0, reinterpret_cast<sockaddr *>(&sourceAddress), &sourceAddressLength);
    if (received <= 0) {
      const int errorCode = lastSocketError();
      return isWouldBlockError(errorCode) || isInterruptedError(errorCode)
                 ? TemporarilyUnavailableResult()
                 : ErrorResult(errorCode);
    }

    packetBuffer.resize(static_cast<std::size_t>(received));
    inboundPacket_ = std::move(packetBuffer);
    inboundOffset_ = 0;
    remoteAddress_ = socketAddressToString(sourceAddress);
    remotePort_ = socketAddressToPort(sourceAddress);
    return AvailableBytes(inboundPacket_.size());
  }

  AvailableResult available() override {
    if (inboundOffset_ < inboundPacket_.size()) {
      return AvailableBytes(inboundPacket_.size() - inboundOffset_);
    }

    return socket_.valid() ? TemporarilyUnavailableResult() : ExhaustedResult();
  }

  std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) override {
    const std::size_t copied = peek(buffer);
    inboundOffset_ += copied;
    if (inboundOffset_ >= inboundPacket_.size()) {
      inboundPacket_.clear();
      inboundOffset_ = 0;
    }

    return copied;
  }

  std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) override {
    if (buffer.empty() || inboundOffset_ >= inboundPacket_.size()) {
      return 0;
    }

    const std::size_t copied = (std::min)(buffer.size(), inboundPacket_.size() - inboundOffset_);
    std::copy_n(inboundPacket_.data() + inboundOffset_, copied, buffer.data());
    return copied;
  }

  void flush() override {}

  std::string_view remoteAddress() const override { return remoteAddress_; }

  std::uint16_t remotePort() const override { return remotePort_; }

private:
  bool openSocket(std::uint16_t port) {
    stop();
    if (!socketRuntime().initialized()) {
      return false;
    }

    sockaddr_storage bindAddress{};
    socklen_t bindLength = 0;
    if (!resolveSocketAddress({}, port, SOCK_DGRAM, bindAddress, bindLength, true)) {
      return false;
    }

    SocketHandleOwner socketHandle(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (!socketHandle.valid()) {
      return false;
    }

    int reuseAddress = 1;
    setsockopt(socketHandle.get(), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&reuseAddress), sizeof(reuseAddress));
    if (bind(socketHandle.get(), reinterpret_cast<const sockaddr *>(&bindAddress), bindLength) != 0) {
      return false;
    }

    if (!setNonBlocking(socketHandle.get(), true)) {
      return false;
    }

    socket_ = std::move(socketHandle);
    return true;
  }

  SocketHandleOwner socket_{};
  std::vector<std::uint8_t> inboundPacket_{};
  std::size_t inboundOffset_ = 0;
  std::vector<std::uint8_t> outboundPacket_{};
  sockaddr_storage outboundDestination_{};
  socklen_t outboundDestinationLength_ = 0;
  std::string remoteAddress_{};
  std::uint16_t remotePort_ = 0;
};

class PosixSocketTransportFactory {
public:
  static std::unique_ptr<IServer> createServer(std::uint16_t port) {
    return std::make_unique<PosixSocketServer>(port);
  }

  static std::unique_ptr<IClient> createClient(std::string_view address,
                                               std::uint16_t port) {
    if (!socketRuntime().initialized()) {
      return nullptr;
    }

    sockaddr_storage remoteAddress{};
    socklen_t remoteAddressLength = 0;
    if (!resolveSocketAddress(address, port, SOCK_STREAM, remoteAddress,
                              remoteAddressLength, false)) {
      return nullptr;
    }

    SocketHandleOwner socketHandle(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!socketHandle.valid()) {
      return nullptr;
    }

    if (connect(socketHandle.get(), reinterpret_cast<const sockaddr *>(&remoteAddress),
                remoteAddressLength) != 0) {
      return nullptr;
    }

    return std::make_unique<PosixSocketClient>(socketHandle.release());
  }

  static std::unique_ptr<IPeer> createPeer() {
    return std::make_unique<PosixSocketPeer>();
  }

  static std::string getHostByName(std::string_view hostName) {
    if (hostName.empty()) {
      return {};
    }

    sockaddr_storage resolvedAddress{};
    socklen_t resolvedAddressLength = 0;
    if (!resolveSocketAddress(hostName, 0, SOCK_STREAM, resolvedAddress,
                              resolvedAddressLength, false)) {
      return {};
    }

    return socketAddressToString(resolvedAddress);
  }
};

} // namespace httpadv::v1::platform::posix
