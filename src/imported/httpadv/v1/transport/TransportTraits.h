#pragma once

#include "TransportInterfaces.h"

#include <string>
#include <type_traits>
#include <utility>

namespace httpadv::v1::transport
{
    namespace detail
    {
        template <typename TFactory>
        using CreateServerResult = decltype(TFactory::createServer(std::declval<std::uint16_t>()));

        template <typename TFactory>
        using CreateClientResult = decltype(TFactory::createClient(std::declval<std::string_view>(), std::declval<std::uint16_t>()));

        template <typename TFactory>
        using CreatePeerResult = decltype(TFactory::createPeer());

        template <typename TFactory>
        using GetHostByNameResult = decltype(TFactory::getHostByName(std::declval<std::string_view>()));
    }

    template <typename TFactory, typename = void>
    struct HasStaticCreateServer : std::false_type
    {
    };

    template <typename TFactory>
    struct HasStaticCreateServer<TFactory, std::void_t<detail::CreateServerResult<TFactory>>>
        : std::bool_constant<std::is_convertible_v<detail::CreateServerResult<TFactory>, std::unique_ptr<IServer>>>
    {
    };

    template <typename TFactory, typename = void>
    struct HasStaticCreateClient : std::false_type
    {
    };

    template <typename TFactory>
    struct HasStaticCreateClient<TFactory, std::void_t<detail::CreateClientResult<TFactory>>>
        : std::bool_constant<std::is_convertible_v<detail::CreateClientResult<TFactory>, std::unique_ptr<IClient>>>
    {
    };

    template <typename TFactory, typename = void>
    struct HasStaticCreatePeer : std::false_type
    {
    };

    template <typename TFactory>
    struct HasStaticCreatePeer<TFactory, std::void_t<detail::CreatePeerResult<TFactory>>>
        : std::bool_constant<std::is_convertible_v<detail::CreatePeerResult<TFactory>, std::unique_ptr<IPeer>>>
    {
    };

    template <typename TFactory, typename = void>
    struct HasStaticGetHostByName : std::false_type
    {
    };

    template <typename TFactory>
    struct HasStaticGetHostByName<TFactory, std::void_t<detail::GetHostByNameResult<TFactory>>>
        : std::bool_constant<std::is_convertible_v<detail::GetHostByNameResult<TFactory>, std::string>>
    {
    };

    template <typename TFactory>
    struct IsStaticTransportFactory
        : std::bool_constant<HasStaticCreateServer<TFactory>::value && HasStaticCreateClient<TFactory>::value &&
                             HasStaticCreatePeer<TFactory>::value && HasStaticGetHostByName<TFactory>::value>
    {
    };

    template <typename TFactory>
    inline constexpr bool IsStaticTransportFactoryV = IsStaticTransportFactory<TFactory>::value;

    class ITransportFactory
    {
    public:
        virtual ~ITransportFactory() = default;

        virtual std::unique_ptr<IServer> createServer(std::uint16_t port) = 0;
        virtual std::unique_ptr<IClient> createClient(std::string_view address, std::uint16_t port) = 0;
        virtual std::unique_ptr<IPeer> createPeer() = 0;
        virtual std::string getHostByName(std::string_view hostName) = 0;
    };
}