#pragma once

#include "../buffers/ByteStream.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace lumalink::platform::transport
{
    class IClient : public lumalink::platform::buffers::IByteChannel
    {
    public:
        ~IClient() override = default;
        virtual void stop() = 0;
        virtual bool connected() = 0;
        virtual std::string_view remoteAddress() const = 0;
        virtual std::uint16_t remotePort() = 0;
        virtual std::string_view localAddress() const = 0;
        virtual std::uint16_t localPort() = 0;
        virtual void setTimeout(std::uint32_t timeoutMs) = 0;
        virtual std::uint32_t getTimeout() const = 0;
    };

    class IServer
    {
    public:
        using ClientType = IClient;
        virtual ~IServer() = default;
        IServer() = default;
        virtual std::unique_ptr<IClient> accept() = 0;
        virtual void begin() = 0;
        virtual std::uint16_t port() const = 0;
        virtual std::string_view localAddress() const = 0;
        virtual void end() = 0;
    };

    class IPeer
    {
    public:
        virtual ~IPeer() = default;
        virtual bool begin(std::uint16_t port) = 0;
        virtual bool beginMulticast(std::string_view multicastAddress, std::uint16_t port) = 0;
        virtual void stop() = 0;
        virtual bool beginPacket(std::string_view address, std::uint16_t port) = 0;
        virtual bool endPacket() = 0;
        virtual std::size_t write(std::span<const std::uint8_t> buffer) = 0;
        virtual lumalink::platform::buffers::AvailableResult parsePacket() = 0;
        virtual lumalink::platform::buffers::AvailableResult available() = 0;
        virtual std::size_t read(std::span<std::uint8_t> buffer) = 0;
        virtual std::size_t peek(std::span<std::uint8_t> buffer) = 0;
        virtual void flush() = 0;
        virtual std::string_view remoteAddress() const = 0;
        virtual std::uint16_t remotePort() const = 0;
    };
}
