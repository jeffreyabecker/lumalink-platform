#pragma once

#include "ByteStream.h"
#include "../core/Defines.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace httpadv::v1::transport
{
    /**
     * @brief Abstract byte-oriented client connection.
     *
     * Extends @ref IByteChannel with connection lifecycle, endpoint metadata,
     * and timeout configuration used by stream-oriented transports such as TCP.
     */
    class IClient : public IByteChannel
    {
    public:
        ~IClient() override = default;

        /**
         * @brief Close the client connection and release any underlying transport resources.
         */
        virtual void stop() = 0;

        /**
         * @brief Report whether the underlying client connection is still active.
         * @return `true` when the client remains connected; otherwise `false`.
         */
        virtual bool connected() = 0;

        /**
         * @brief Get the remote endpoint address for this client connection.
         * @return A string view describing the peer address.
         */
        virtual std::string_view remoteAddress() const = 0;

        /**
         * @brief Get the remote endpoint port for this client connection.
         * @return The peer port number.
         */
        virtual std::uint16_t remotePort() = 0;

        /**
         * @brief Get the local interface address that accepted or owns this connection.
         * @return A string view describing the local address.
         */
        virtual std::string_view localAddress() const = 0;

        /**
         * @brief Get the local port bound for this connection.
         * @return The local port number.
         */
        virtual std::uint16_t localPort() = 0;

        /**
         * @brief Configure the transport read/write timeout.
         * @param timeoutMs Timeout duration in milliseconds.
         */
        virtual void setTimeout(std::uint32_t timeoutMs) = 0;

        /**
         * @brief Get the currently configured transport timeout.
         * @return Timeout duration in milliseconds.
         */
        virtual std::uint32_t getTimeout() const = 0;
    };

    /**
     * @brief Abstract server that accepts stream-oriented client connections.
     *
     * Implementations usually wrap a listening socket or platform server object
     * and expose accepted connections as @ref IClient instances.
     */
    class IServer
    {
    public:
        using ClientType = IClient;

        virtual ~IServer() = default;

        IServer() = default;

        /**
         * @brief Accept the next available client connection.
         * @return A newly accepted client, or `nullptr` when no client is available.
         */
        virtual std::unique_ptr<IClient> accept() = 0;

        /**
         * @brief Start listening for incoming client connections.
         */
        virtual void begin() = 0;

        /**
         * @brief Get the listening port exposed by the server.
         * @return The local listening port.
         */
        virtual std::uint16_t port() const = 0;

        /**
         * @brief Get the local address currently bound by the server.
         * @return A string view describing the server bind address.
         */
        virtual std::string_view localAddress() const = 0;

        /**
         * @brief Stop the server and release any listening resources.
         */
        virtual void end() = 0;
    };

    /**
     * @brief Abstract packet-oriented peer transport.
     *
     * Represents a datagram-style endpoint used for connectionless messaging,
     * including packet transmission, packet parsing, and peer endpoint queries.
     */
    class IPeer
    {
    public:
        virtual ~IPeer() = default;

        /**
         * @brief Start the peer on the specified local port.
         * @param port Local port to bind.
         * @return `true` when binding succeeds; otherwise `false`.
         */
        virtual bool begin(std::uint16_t port) = 0;

        /**
         * @brief Start the peer and join a multicast group.
         * @param multicastAddress Multicast group address to join.
         * @param port Local port to bind.
         * @return `true` when initialization succeeds; otherwise `false`.
         */
        virtual bool beginMulticast(std::string_view multicastAddress, std::uint16_t port) = 0;

        /**
         * @brief Stop the peer and release any underlying transport resources.
         */
        virtual void stop() = 0;

        /**
         * @brief Begin composing an outgoing packet for the specified destination.
         * @param address Destination address.
         * @param port Destination port.
         * @return `true` when packet composition can begin; otherwise `false`.
         */
        virtual bool beginPacket(std::string_view address, std::uint16_t port) = 0;

        /**
         * @brief Finalize and send the current outgoing packet.
         * @return `true` when the packet is transmitted successfully; otherwise `false`.
         */
        virtual bool endPacket() = 0;

        /**
         * @brief Write bytes into the current outgoing packet.
         * @param buffer Source bytes to append.
         * @return Number of bytes written.
         */
        virtual std::size_t write(httpadv::v1::util::span<const std::uint8_t> buffer) = 0;

        /**
         * @brief Parse the next received packet and prepare it for reading.
         * @return Availability information for the parsed packet payload.
         */
        virtual AvailableResult parsePacket() = 0;

        /**
         * @brief Query the number of readable bytes remaining in the current packet.
         * @return Availability information for the current packet payload.
         */
        virtual AvailableResult available() = 0;

        /**
         * @brief Read bytes from the current packet into the provided buffer.
         * @param buffer Destination buffer to fill.
         * @return Number of bytes read.
         */
        virtual std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) = 0;

        /**
         * @brief Copy bytes from the current packet without consuming them.
         * @param buffer Destination buffer to receive the peeked bytes.
         * @return Number of bytes copied.
         */
        virtual std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) = 0;

        /**
         * @brief Flush any buffered transport state as defined by the implementation.
         */
        virtual void flush() = 0;

        /**
         * @brief Get the source address for the current or most recently parsed packet.
         * @return A string view describing the remote peer address.
         */
        virtual std::string_view remoteAddress() const = 0;

        /**
         * @brief Get the source port for the current or most recently parsed packet.
         * @return The remote peer port number.
         */
        virtual std::uint16_t remotePort() const = 0;
    };


}