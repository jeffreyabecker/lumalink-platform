#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "../transport/TransportInterfaces.h"
#ifdef ARDUINO
#include "./arduino/ArduinoWiFiTransport.h"
#elif defined(_WIN32)
#include "./windows/WindowsSocketTransport.h"
#else
#include "./posix/PosixSocketTransport.h"
#endif
namespace httpadv::v1::platform
{
    namespace detail
    {
#ifdef ARDUINO
        using NativeTransportFactory = platform::arduino::ArduinoWiFiTransportFactory;
#elif defined(_WIN32)
        using NativeTransportFactory = httpadv::v1::platform::windows::WindowsSocketTransportFactory;
#else
        using NativeTransportFactory = platform::posix::PosixSocketTransportFactory;
#endif

    }
    template <typename TTransportFactory = detail::NativeTransportFactory,
              typename = std::enable_if_t<httpadv::v1::transport::IsStaticTransportFactoryV<TTransportFactory>>>
    class TransportFactoryWrapper : public httpadv::v1::transport::ITransportFactory
    {
    public:
        std::unique_ptr<httpadv::v1::transport::IServer> createServer(std::uint16_t port) override
        {
            return TTransportFactory::createServer(port);
        }

        std::unique_ptr<httpadv::v1::transport::IClient> createClient(std::string_view address, std::uint16_t port) override
        {
            return TTransportFactory::createClient(address, port);
        }

        std::unique_ptr<httpadv::v1::transport::IPeer> createPeer() override
        {
            return TTransportFactory::createPeer();
        }

        std::string getHostByName(std::string_view hostName) override
        {
            return TTransportFactory::getHostByName(hostName);
        }
    };
    using TransportFactory = TransportFactoryWrapper<>;

}