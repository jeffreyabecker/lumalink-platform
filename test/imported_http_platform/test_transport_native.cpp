#include "../support/include/ConsolidatedNativeSuite.h"

#include "../../src/httpadv/v1/HttpServerAdvanced.h"
#include "../../src/httpadv/v1/transport/TransportTraits.h"

#include <unity.h>

#if defined(_WIN32)
#include "../../src/httpadv/v1/platform/windows/WindowsSocketTransport.h"
#else
#include "../../src/httpadv/v1/platform/posix/PosixSocketTransport.h"
#endif

#include "../../src/httpadv/v1/platform/TransportFactory.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace httpadv::v1::core;
using namespace httpadv::v1::handlers;
using namespace httpadv::v1::pipeline;
using namespace httpadv::v1::response;
using namespace httpadv::v1::routing;
using namespace httpadv::v1::server;
using namespace httpadv::v1::staticfiles;
using namespace httpadv::v1::transport;
using namespace httpadv::v1::util;
using namespace httpadv::v1::websocket;
using namespace httpadv::v1::platform;

namespace {
#ifdef _WIN32
using NativeTransportFactory = httpadv::v1::platform::windows::WindowsSocketTransportFactory;
#else
using NativeTransportFactory = platform::posix::PosixSocketTransportFactory;
#endif

static_assert(IsStaticTransportFactoryV<NativeTransportFactory>,
              "Native socket transport must expose static factory methods");

#ifdef _WIN32
using TestSocketHandle = SOCKET;
constexpr TestSocketHandle InvalidTestSocketHandle = INVALID_SOCKET;
#else
using TestSocketHandle = int;
constexpr TestSocketHandle InvalidTestSocketHandle = -1;
#endif

void sleepForMilliseconds(int milliseconds) {
#ifdef _WIN32
  Sleep(static_cast<DWORD>(milliseconds));
#else
  usleep(static_cast<useconds_t>(milliseconds * 1000));
#endif
}

void closeTestSocket(TestSocketHandle socketHandle) {
  if (socketHandle == InvalidTestSocketHandle) {
    return;
  }

#ifdef _WIN32
  closesocket(socketHandle);
#else
  close(socketHandle);
#endif
}

std::uint16_t allocateEphemeralUdpPort() {
  TestSocketHandle socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  TEST_ASSERT_NOT_EQUAL(InvalidTestSocketHandle, socketHandle);

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  TEST_ASSERT_EQUAL_INT(0, bind(socketHandle,
                                reinterpret_cast<const sockaddr *>(&address),
                                sizeof(address)));

  sockaddr_in boundAddress{};
  socklen_t boundLength = sizeof(boundAddress);
  TEST_ASSERT_EQUAL_INT(
      0, getsockname(socketHandle, reinterpret_cast<sockaddr *>(&boundAddress),
                     &boundLength));
  closeTestSocket(socketHandle);
  return ntohs(boundAddress.sin_port);
}

std::unique_ptr<IClient> acceptWithRetry(IServer &server) {
  for (int attempt = 0; attempt < 50; ++attempt) {
    if (std::unique_ptr<IClient> accepted = server.accept()) {
      return accepted;
    }

    sleepForMilliseconds(10);
  }

  return nullptr;
}

AvailableResult parsePacketWithRetry(IPeer &peer) {
  for (int attempt = 0; attempt < 50; ++attempt) {
    const AvailableResult result = peer.parsePacket();
    if (result.hasBytes()) {
      return result;
    }

    sleepForMilliseconds(10);
  }

  return peer.parsePacket();
}

void localSetUp() {}

void localTearDown() {}

void test_native_factory_creates_tcp_server_and_client_loopback() {
  std::unique_ptr<IServer> server = NativeTransportFactory::createServer(0);
  TEST_ASSERT_NOT_NULL(server.get());
  server->begin();

  TEST_ASSERT_GREATER_THAN_UINT16(0, server->port());

  std::unique_ptr<IClient> client =
      NativeTransportFactory::createClient("127.0.0.1", server->port());
  TEST_ASSERT_NOT_NULL(client.get());
  client->setTimeout(250);
  TEST_ASSERT_EQUAL_UINT32(250, client->getTimeout());
  TEST_ASSERT_TRUE(client->connected());

  std::unique_ptr<IClient> accepted = acceptWithRetry(*server);
  TEST_ASSERT_NOT_NULL(accepted.get());
  accepted->setTimeout(250);
  TEST_ASSERT_TRUE(accepted->connected());
  TEST_ASSERT_GREATER_THAN_UINT16(0, accepted->remotePort());
  TEST_ASSERT_GREATER_THAN_UINT16(0, accepted->localPort());

  TEST_ASSERT_EQUAL_UINT64(5, client->write(std::string_view("hello")));

  AvailableResult available = accepted->available();
  for (int attempt = 0; attempt < 50 && !available.hasBytes(); ++attempt) {
    sleepForMilliseconds(10);
    available = accepted->available();
  }

  TEST_ASSERT_TRUE(available.hasBytes());
  std::uint8_t buffer[8] = {};
  TEST_ASSERT_EQUAL_UINT64(
      5, accepted->read(httpadv::v1::util::span<std::uint8_t>(buffer, 5)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const std::uint8_t *>("hello"),
                                buffer, 5);

  TEST_ASSERT_EQUAL_UINT64(5, accepted->write(std::string_view("world")));
  std::memset(buffer, 0, sizeof(buffer));
  available = client->available();
  for (int attempt = 0; attempt < 50 && !available.hasBytes(); ++attempt) {
    sleepForMilliseconds(10);
    available = client->available();
  }

  TEST_ASSERT_TRUE(available.hasBytes());
  TEST_ASSERT_EQUAL_UINT64(
      5, client->peek(httpadv::v1::util::span<std::uint8_t>(buffer, 5)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const std::uint8_t *>("world"),
                                buffer, 5);
  std::memset(buffer, 0, sizeof(buffer));
  TEST_ASSERT_EQUAL_UINT64(
      5, client->read(httpadv::v1::util::span<std::uint8_t>(buffer, 5)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const std::uint8_t *>("world"),
                                buffer, 5);

  client->stop();
  accepted->stop();
  TEST_ASSERT_FALSE(client->connected());
  TEST_ASSERT_FALSE(accepted->connected());
  server->end();
}
void test_native_factory_wrapper_can_be_instantiated() {
  httpadv::v1::platform::TransportFactory transportFactory;
  TEST_ASSERT_NOT_NULL(&transportFactory);
}
void test_native_factory_creates_udp_peers_for_loopback_packets() {


  const std::uint16_t senderPort = allocateEphemeralUdpPort();
  const std::uint16_t receiverPort = allocateEphemeralUdpPort();

  std::unique_ptr<IPeer> sender = NativeTransportFactory::createPeer();
  std::unique_ptr<IPeer> receiver = NativeTransportFactory::createPeer();
  TEST_ASSERT_NOT_NULL(sender.get());
  TEST_ASSERT_NOT_NULL(receiver.get());

  TEST_ASSERT_TRUE(sender->begin(senderPort));
  TEST_ASSERT_TRUE(receiver->begin(receiverPort));

  TEST_ASSERT_TRUE(sender->beginPacket("127.0.0.1", receiverPort));
  const std::uint8_t pingBytes[] = {'p', 'i', 'n', 'g'};
  TEST_ASSERT_EQUAL_UINT64(
      4, sender->write(httpadv::v1::util::span<const std::uint8_t>(
             pingBytes, sizeof(pingBytes))));
  TEST_ASSERT_TRUE(sender->endPacket());

  const AvailableResult packet = parsePacketWithRetry(*receiver);
  TEST_ASSERT_TRUE(packet.hasBytes());
  TEST_ASSERT_EQUAL_UINT64(4, packet.count);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1",
                           std::string(receiver->remoteAddress()).c_str());
  TEST_ASSERT_EQUAL_UINT16(senderPort, receiver->remotePort());

  std::uint8_t buffer[8] = {};
  TEST_ASSERT_EQUAL_UINT64(
      4, receiver->peek(httpadv::v1::util::span<std::uint8_t>(buffer, 4)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const std::uint8_t *>("ping"),
                                buffer, 4);
  std::memset(buffer, 0, sizeof(buffer));
  TEST_ASSERT_EQUAL_UINT64(
      4, receiver->read(httpadv::v1::util::span<std::uint8_t>(buffer, 4)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const std::uint8_t *>("ping"),
                                buffer, 4);
  TEST_ASSERT_TRUE(receiver->available().isTemporarilyUnavailable());

  sender->stop();
  receiver->stop();
  TEST_ASSERT_TRUE(sender->available().isExhausted());
  TEST_ASSERT_TRUE(receiver->available().isExhausted());
}

int runUnitySuite() {
  UNITY_BEGIN();
  RUN_TEST(test_native_factory_wrapper_can_be_instantiated);
  RUN_TEST(test_native_factory_creates_tcp_server_and_client_loopback);
  RUN_TEST(test_native_factory_creates_udp_peers_for_loopback_packets);
  return UNITY_END();
}
} // namespace

int run_test_transport_native() {
  return httpadv::v1::TestSupport::RunConsolidatedSuite(
      "native transport", runUnitySuite, localSetUp, localTearDown);
}
