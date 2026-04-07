#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "transport/TransportInterfaces.h"
#include "transport/TransportTraits.h"
#ifdef ARDUINO
#include "./arduino/ArduinoWiFiTransport.h"
#elif defined(_WIN32)
#include "./windows/WindowsSocketTransport.h"
#else
#include "./posix/PosixSocketTransport.h"
#endif

namespace lumalink::platform
{
    namespace detail
    {
#ifdef ARDUINO
        using NativeTransportFactory = platform::arduino::ArduinoWiFiTransportFactory;
#elif defined(_WIN32)
        using NativeTransportFactory = lumalink::platform::windows::WindowsSocketTransportFactory;
#else
        using NativeTransportFactory = platform::posix::PosixSocketTransportFactory;
#endif

    }

    template <typename TTransportFactory = detail::NativeTransportFactory,
              typename = std::enable_if_t<lumalink::platform::transport::IsStaticTransportFactoryV<TTransportFactory>>>
    class TransportFactoryWrapper : public lumalink::platform::transport::ITransportFactory
    {
    public:
        std::unique_ptr<lumalink::platform::transport::IServer> createServer(std::uint16_t port) override
        {
            return TTransportFactory::createServer(port);
        }

        std::unique_ptr<lumalink::platform::transport::IClient> createClient(std::string_view address, std::uint16_t port) override
        {
            return TTransportFactory::createClient(address, port);
        }

        std::unique_ptr<lumalink::platform::transport::IPeer> createPeer() override
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
