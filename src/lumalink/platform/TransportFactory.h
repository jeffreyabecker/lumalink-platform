#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "TransportInterfaces.h"
#include "TransportTraits.h"


namespace lumalink::platform
{
    template <typename TTransportFactory ,
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

}
