#if defined(LUMALINK_TEST_USE_MODULES)
#if defined(_WIN32)
import lumalink.platform.windows;
#else
import lumalink.platform.posix;
#endif
#endif

#include <unity.h>

#if !defined(LUMALINK_TEST_USE_MODULES) || !defined(_WIN32)
#include <LumaLinkPlatform.h>
#endif

#if defined(LUMALINK_TEST_USE_MODULES)
#include <lumalink/platform/Availability.h>
#include <lumalink/platform/FileSystem.h>
#include <lumalink/platform/TransportInterfaces.h>
#include <lumalink/platform/TransportTraits.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#if !defined(LUMALINK_TEST_USE_MODULES)
#include <windows/WindowsSocketTransport.h>
#endif
#elif !defined(LUMALINK_TEST_USE_MODULES)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <posix/PosixSocketTransport.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>

using lumalink::platform::filesystem::IFileSystem;
using lumalink::platform::buffers::AvailableByteCount;
using lumalink::platform::buffers::ByteAvailability;
using lumalink::platform::buffers::HasAvailableBytes;
using lumalink::platform::buffers::IsExhausted;
using lumalink::platform::buffers::IsTemporarilyUnavailable;
using lumalink::platform::transport::IsStaticTransportFactoryV;
using lumalink::platform::transport::IClient;
using lumalink::platform::transport::IServer;
using lumalink::platform::transport::IPeer;

#if defined(_WIN32)
using TestSocketHandle = SOCKET;
constexpr TestSocketHandle InvalidTestSocketHandle = INVALID_SOCKET;
#else
using TestSocketHandle = int;
constexpr TestSocketHandle InvalidTestSocketHandle = -1;
#endif

#if defined(_WIN32)
static_assert(IsStaticTransportFactoryV<lumalink::platform::windows::WindowsSocketTransportFactory>,
              "Windows socket transport must expose static factory methods");
#else
static_assert(IsStaticTransportFactoryV<lumalink::platform::posix::PosixSocketTransportFactory>,
              "POSIX socket transport must expose static factory methods");
#endif

static void sleepForMilliseconds(int milliseconds)
{
#if defined(_WIN32)
    Sleep(static_cast<DWORD>(milliseconds));
#else
    usleep(static_cast<useconds_t>(milliseconds * 1000));
#endif
}

static void closeTestSocket(TestSocketHandle handle)
{
    if (handle == InvalidTestSocketHandle)
    {
        return;
    }
#if defined(_WIN32)
    closesocket(handle);
#else
    close(handle);
#endif
}

static std::uint16_t allocateEphemeralUdpPort()
{
    TestSocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(InvalidTestSocketHandle, sock);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    TEST_ASSERT_EQUAL_INT(0, bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)));

    sockaddr_in bound{};
    socklen_t boundLen = sizeof(bound);
    TEST_ASSERT_EQUAL_INT(0, getsockname(sock, reinterpret_cast<sockaddr *>(&bound), &boundLen));
    closeTestSocket(sock);
    return ntohs(bound.sin_port);
}

static std::unique_ptr<IClient> acceptWithRetry(IServer &server)
{
    for (int i = 0; i < 50; ++i)
    {
        if (auto accepted = server.accept())
        {
            return accepted;
        }
        sleepForMilliseconds(10);
    }
    return nullptr;
}

static ByteAvailability parsePacketWithRetry(IPeer &peer)
{
    for (int i = 0; i < 50; ++i)
    {
        const ByteAvailability result = peer.parsePacket();
        if (HasAvailableBytes(result))
        {
            return result;
        }
        sleepForMilliseconds(10);
    }
    return peer.parsePacket();
}

void test_native_factory_wrapper_can_be_instantiated()
{
#if defined(LUMALINK_TEST_USE_MODULES) && defined(_WIN32)
    TEST_ASSERT_TRUE(lumalink::platform::windows::WindowsSocketTransportFactory::getHostByName("localhost").size() > 0);
#else
    lumalink::platform::TransportFactory transportFactory;
    TEST_ASSERT_NOT_NULL(&transportFactory);
#endif
}

void test_native_factory_creates_tcp_server_and_client_loopback()
{
#if defined(_WIN32)
    std::unique_ptr<IServer> server = lumalink::platform::windows::WindowsSocketTransportFactory::createServer(0);
#else
    std::unique_ptr<IServer> server = lumalink::platform::posix::PosixSocketTransportFactory::createServer(0);
#endif
    TEST_ASSERT_NOT_NULL(server.get());
    server->begin();
    TEST_ASSERT_GREATER_THAN_UINT16(0, server->port());

#if defined(_WIN32)
    std::unique_ptr<IClient> client =
        lumalink::platform::windows::WindowsSocketTransportFactory::createClient("127.0.0.1", server->port());
#else
    std::unique_ptr<IClient> client =
        lumalink::platform::posix::PosixSocketTransportFactory::createClient("127.0.0.1", server->port());
#endif
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

    ByteAvailability available = accepted->available();
    for (int i = 0; i < 50 && !HasAvailableBytes(available); ++i)
    {
        sleepForMilliseconds(10);
        available = accepted->available();
    }

    TEST_ASSERT_TRUE(HasAvailableBytes(available));
    std::uint8_t buffer[8] = {};
    TEST_ASSERT_EQUAL_UINT64(5, accepted->read(std::span<std::uint8_t>(buffer, 5)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        reinterpret_cast<const std::uint8_t *>("hello"), buffer, 5);

    TEST_ASSERT_EQUAL_UINT64(5, accepted->write(std::string_view("world")));
    std::memset(buffer, 0, sizeof(buffer));
    available = client->available();
    for (int i = 0; i < 50 && !HasAvailableBytes(available); ++i)
    {
        sleepForMilliseconds(10);
        available = client->available();
    }

    TEST_ASSERT_TRUE(HasAvailableBytes(available));
    TEST_ASSERT_EQUAL_UINT64(5, client->peek(std::span<std::uint8_t>(buffer, 5)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        reinterpret_cast<const std::uint8_t *>("world"), buffer, 5);
    std::memset(buffer, 0, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT64(5, client->read(std::span<std::uint8_t>(buffer, 5)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        reinterpret_cast<const std::uint8_t *>("world"), buffer, 5);

    client->stop();
    accepted->stop();
    TEST_ASSERT_FALSE(client->connected());
    TEST_ASSERT_FALSE(accepted->connected());
    server->end();
}

void test_native_factory_creates_udp_peers_for_loopback_packets()
{
    const std::uint16_t senderPort = allocateEphemeralUdpPort();
    const std::uint16_t receiverPort = allocateEphemeralUdpPort();

#if defined(_WIN32)
    std::unique_ptr<IPeer> sender = lumalink::platform::windows::WindowsSocketTransportFactory::createPeer();
    std::unique_ptr<IPeer> receiver = lumalink::platform::windows::WindowsSocketTransportFactory::createPeer();
#else
    std::unique_ptr<IPeer> sender = lumalink::platform::posix::PosixSocketTransportFactory::createPeer();
    std::unique_ptr<IPeer> receiver = lumalink::platform::posix::PosixSocketTransportFactory::createPeer();
#endif
    TEST_ASSERT_NOT_NULL(sender.get());
    TEST_ASSERT_NOT_NULL(receiver.get());

    TEST_ASSERT_TRUE(sender->begin(senderPort));
    TEST_ASSERT_TRUE(receiver->begin(receiverPort));

    TEST_ASSERT_TRUE(sender->beginPacket("127.0.0.1", receiverPort));
    const std::uint8_t pingBytes[] = {'p', 'i', 'n', 'g'};
    TEST_ASSERT_EQUAL_UINT64(4,
        sender->write(std::span<const std::uint8_t>(pingBytes, sizeof(pingBytes))));
    TEST_ASSERT_TRUE(sender->endPacket());

    const ByteAvailability packet = parsePacketWithRetry(*receiver);
    TEST_ASSERT_TRUE(HasAvailableBytes(packet));
    TEST_ASSERT_EQUAL_UINT64(4, AvailableByteCount(packet));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", std::string(receiver->remoteAddress()).c_str());
    TEST_ASSERT_EQUAL_UINT16(senderPort, receiver->remotePort());

    std::uint8_t buffer[8] = {};
    TEST_ASSERT_EQUAL_UINT64(4, receiver->peek(std::span<std::uint8_t>(buffer, 4)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        reinterpret_cast<const std::uint8_t *>("ping"), buffer, 4);
    std::memset(buffer, 0, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT64(4, receiver->read(std::span<std::uint8_t>(buffer, 4)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        reinterpret_cast<const std::uint8_t *>("ping"), buffer, 4);
    TEST_ASSERT_TRUE(IsTemporarilyUnavailable(receiver->available()));

    sender->stop();
    receiver->stop();
    TEST_ASSERT_TRUE(IsExhausted(sender->available()));
    TEST_ASSERT_TRUE(IsExhausted(receiver->available()));
}
