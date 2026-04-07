#pragma once

#include "ByteStreamFixtures.h"

#include "../../../src/httpadv/v1/core/IHttpContextHandlerFactory.h"
#include "../../../src/httpadv/v1/pipeline/IPipelineHandler.h"
#include "../../../src/httpadv/v1/transport/TransportInterfaces.h"
#include "../../../src/httpadv/v1/response/StringResponse.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace httpadv
{
    inline namespace v1
    {
        namespace TestSupport
        {
            using namespace httpadv::v1::core;
            using namespace httpadv::v1::handlers;
            using namespace httpadv::v1::pipeline;
            using namespace httpadv::v1::response;
            using namespace httpadv::v1::transport;
            using namespace httpadv::v1::util;
            struct CapturedResponse
            {
                HttpStatus status;
                std::vector<std::pair<std::string, std::string>> headers;
                std::string body;
            };

    inline CapturedResponse CaptureResponse(std::unique_ptr<IHttpResponse> response)
    {
        if (!response)
        {
            throw std::invalid_argument("CaptureResponse requires a non-null response");
        }

        CapturedResponse captured{};
        captured.status = response->status();

        HttpHeaderCollection &headers = response->headers();
        captured.headers.reserve(headers.size());
        for (const auto &header : headers)
        {
            captured.headers.emplace_back(std::string(header.nameView()), std::string(header.valueView()));
        }

        if (std::unique_ptr<IByteSource> body = response->getBody())
        {
            captured.body = ReadByteSourceAsStdString(*body);
        }

        return captured;
    }

    inline CapturedResponse CaptureResponse(HandlerResult result)
    {
        if (!result.isResponse())
        {
            throw std::invalid_argument("CaptureResponse requires a response result");
        }

        return CaptureResponse(std::move(result.response));
    }

    inline std::optional<std::string> FindCapturedHeader(const CapturedResponse &response, std::string_view name)
    {
        for (const auto &header : response.headers)
        {
            if (HttpHeaderCollectionDetail::EqualsIgnoreCase(header.first, name))
            {
                return header.second;
            }
        }

        return std::nullopt;
    }

    class RecordingRequestHandlerFactory : public IHttpContextHandlerFactory
    {
    public:
        using HandlerFactoryCallback = std::function<std::unique_ptr<IHttpHandler>(HttpContext &)>;
        using ResponseFactoryCallback = std::function<std::unique_ptr<IHttpResponse>(HttpStatus, std::string)>;

        explicit RecordingRequestHandlerFactory(
            HandlerFactoryCallback handlerFactory = nullptr,
            ResponseFactoryCallback responseFactory = nullptr)
            : handlerFactory_(std::move(handlerFactory)),
              responseFactory_(std::move(responseFactory))
        {
        }

        std::unique_ptr<IHttpHandler> create(HttpContext &context) override
        {
            ++createCount_;
            lastCreateContext_ = &context;
            if (handlerFactory_)
            {
                return handlerFactory_(context);
            }

            return nullptr;
        }

        std::unique_ptr<IHttpResponse> createResponse(HttpStatus status, std::string body)
        {
            ++responseCreateCount_;
            responseStatuses_.push_back(status);
            responseBodies_.push_back(body);
            if (responseFactory_)
            {
                return responseFactory_(status, std::move(body));
            }

            return StringResponse::create(status, std::move(body), {});
        }

        std::unique_ptr<IHttpResponse> createResponse(HttpStatus status, std::string_view body)
        {
            return createResponse(status, std::string(body));
        }

        std::unique_ptr<IHttpResponse> createResponse(HttpStatus status, const char *body)
        {
            return createResponse(status, std::string(body != nullptr ? body : ""));
        }

        std::size_t createCount() const
        {
            return createCount_;
        }

        HttpContext *lastCreateContext() const
        {
            return lastCreateContext_;
        }

        std::size_t responseCreateCount() const
        {
            return responseCreateCount_;
        }

        const std::vector<HttpStatus> &responseStatuses() const
        {
            return responseStatuses_;
        }

        const std::vector<std::string> &responseBodies() const
        {
            return responseBodies_;
        }

        const std::string &lastResponseBody() const
        {
            return responseBodies_.back();
        }

    private:
        HandlerFactoryCallback handlerFactory_;
        ResponseFactoryCallback responseFactory_;
        std::size_t createCount_ = 0;
        HttpContext *lastCreateContext_ = nullptr;
        std::size_t responseCreateCount_ = 0;
        std::vector<HttpStatus> responseStatuses_;
        std::vector<std::string> responseBodies_;
    };

    enum class PipelineEventKind
    {
        MessageBegin,
        AddressesSet,
        Header,
        HeadersComplete,
        BodyChunk,
        MessageComplete,
        Error,
        ResponseStarted,
        ResponseCompleted,
        ClientDisconnected,
        RequestResultDelivered
    };

    struct RecordedPipelineEvent
    {
        PipelineEventKind kind = PipelineEventKind::MessageBegin;
        std::string firstText;
        std::string secondText;
        std::uint16_t firstNumber = 0;
        std::uint16_t secondNumber = 0;
        PipelineErrorCode errorCode = PipelineErrorCode::None;
    };

    class PipelineEventRecorder : public IPipelineHandler
    {
    public:
        int onMessageBegin(const char *method, std::uint16_t versionMajor, std::uint16_t versionMinor, std::string_view url) override
        {
            method_ = method != nullptr ? method : "";
            versionMajor_ = versionMajor;
            versionMinor_ = versionMinor;
            url_ = std::string(url);
            events_.push_back({PipelineEventKind::MessageBegin, method_, url_, versionMajor, versionMinor, PipelineErrorCode::None});
            return 0;
        }

        void setAddresses(std::string_view remoteAddress, std::uint16_t remotePort, std::string_view localAddress, std::uint16_t localPort) override
        {
            remoteAddress_ = std::string(remoteAddress);
            remotePort_ = remotePort;
            localAddress_ = std::string(localAddress);
            localPort_ = localPort;
            events_.push_back({PipelineEventKind::AddressesSet, remoteAddress_, localAddress_, remotePort, localPort, PipelineErrorCode::None});
        }

        int onHeader(std::string_view field, std::string_view value) override
        {
            headers_.push_back({std::string(field), std::string(value)});
            events_.push_back({PipelineEventKind::Header, std::string(field), std::string(value), 0, 0, PipelineErrorCode::None});
            return 0;
        }

        int onHeadersComplete() override
        {
            ++headersCompleteCount_;
            events_.push_back({PipelineEventKind::HeadersComplete, {}, {}, 0, 0, PipelineErrorCode::None});
            return 0;
        }

        int onBody(const std::uint8_t *at, std::size_t length) override
        {
            bodyChunks_.emplace_back(at, at + length);
            events_.push_back({PipelineEventKind::BodyChunk, std::string(reinterpret_cast<const char *>(at), length), {}, static_cast<std::uint16_t>(length), 0, PipelineErrorCode::None});
            return 0;
        }

        int onMessageComplete() override
        {
            ++messageCompleteCount_;
            events_.push_back({PipelineEventKind::MessageComplete, {}, {}, 0, 0, PipelineErrorCode::None});
            return 0;
        }

        void onError(PipelineError error) override
        {
            errors_.push_back(error.code());
            events_.push_back({PipelineEventKind::Error, error.name(), error.message(), 0, 0, error.code()});
        }

        void onResponseStarted() override
        {
            ++responseStartedCount_;
            events_.push_back({PipelineEventKind::ResponseStarted, {}, {}, 0, 0, PipelineErrorCode::None});
        }

        void onResponseCompleted() override
        {
            ++responseCompletedCount_;
            events_.push_back({PipelineEventKind::ResponseCompleted, {}, {}, 0, 0, PipelineErrorCode::None});
        }

        void onClientDisconnected() override
        {
            ++clientDisconnectedCount_;
            events_.push_back({PipelineEventKind::ClientDisconnected, {}, {}, 0, 0, PipelineErrorCode::None});
        }

        bool hasPendingResult() const override
        {
            return pendingResult_.hasValue();
        }

        RequestHandlingResult takeResult() override
        {
            RequestHandlingResult result = std::move(pendingResult_);
            pendingResult_ = RequestHandlingResult();
            return result;
        }

        void emitResponseResult(std::unique_ptr<IByteSource> responseStream)
        {
            pendingResult_ = RequestHandlingResult::response(std::move(responseStream));
            events_.push_back({PipelineEventKind::RequestResultDelivered, {}, {}, 0, 0, PipelineErrorCode::None});
        }

        const std::vector<RecordedPipelineEvent> &events() const
        {
            return events_;
        }

        const std::string &method() const
        {
            return method_;
        }

        const std::string &url() const
        {
            return url_;
        }

        std::uint16_t versionMajor() const
        {
            return versionMajor_;
        }

        std::uint16_t versionMinor() const
        {
            return versionMinor_;
        }

        const std::vector<std::pair<std::string, std::string>> &headers() const
        {
            return headers_;
        }

        std::string bodyText() const
        {
            std::string result;
            for (const auto &chunk : bodyChunks_)
            {
                result.append(reinterpret_cast<const char *>(chunk.data()), chunk.size());
            }

            return result;
        }

        const std::vector<PipelineErrorCode> &errors() const
        {
            return errors_;
        }

        const std::string &remoteAddress() const
        {
            return remoteAddress_;
        }

        std::uint16_t remotePort() const
        {
            return remotePort_;
        }

        const std::string &localAddress() const
        {
            return localAddress_;
        }

        std::uint16_t localPort() const
        {
            return localPort_;
        }

        std::size_t headersCompleteCount() const
        {
            return headersCompleteCount_;
        }

        std::size_t messageCompleteCount() const
        {
            return messageCompleteCount_;
        }

        std::size_t responseStartedCount() const
        {
            return responseStartedCount_;
        }

        std::size_t responseCompletedCount() const
        {
            return responseCompletedCount_;
        }

        std::size_t clientDisconnectedCount() const
        {
            return clientDisconnectedCount_;
        }

    private:
        std::vector<RecordedPipelineEvent> events_;
        std::string method_;
        std::string url_;
        std::uint16_t versionMajor_ = 0;
        std::uint16_t versionMinor_ = 0;
        std::string remoteAddress_;
        std::uint16_t remotePort_ = 0;
        std::string localAddress_;
        std::uint16_t localPort_ = 0;
        std::vector<std::pair<std::string, std::string>> headers_;
        std::vector<std::vector<std::uint8_t>> bodyChunks_;
        std::vector<PipelineErrorCode> errors_;
        RequestHandlingResult pendingResult_;
        std::size_t headersCompleteCount_ = 0;
        std::size_t messageCompleteCount_ = 0;
        std::size_t responseStartedCount_ = 0;
        std::size_t responseCompletedCount_ = 0;
        std::size_t clientDisconnectedCount_ = 0;
    };

    class FakeClient : public IClient
    {
    public:
        using IByteSink::write;

        explicit FakeClient(
            std::initializer_list<std::pair<const char *, bool>> readSteps = {},
            std::string remoteAddress = "127.0.0.1",
            std::uint16_t remotePort = 12345,
            std::string localAddress = "127.0.0.1",
            std::uint16_t localPort = 80)
            : readable_(readSteps), remoteAddress_(std::move(remoteAddress)), remotePort_(remotePort), localAddress_(std::move(localAddress)), localPort_(localPort)
        {
        }

        AvailableResult available() override
        {
            return readable_.available();
        }

        std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) override
        {
            return readable_.read(buffer);
        }

        std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) override
        {
            return readable_.peek(buffer);
        }

        std::size_t write(httpadv::v1::util::span<const std::uint8_t> buffer) override
        {
            if (!connected())
            {
                return 0;
            }

            std::size_t accepted = buffer.size();
            if (writeScriptIndex_ < writeScript_.size())
            {
                accepted = (std::min)(accepted, writeScript_[writeScriptIndex_++]);
            }

            written_.insert(written_.end(), buffer.begin(), buffer.begin() + accepted);
            writeSizes_.push_back(accepted);
            return accepted;
        }

        void flush() override
        {
            ++flushCount_;
        }

        void stop() override
        {
            ++stopCount_;
            connected_ = false;
            stopped_ = true;
        }

        bool connected() override
        {
            ++connectedCheckCount_;
            if (connected_)
            {
                return true;
            }

            std::uint8_t byte = 0;
            return readable_.peek(httpadv::v1::util::span<std::uint8_t>(&byte, 1)) > 0;
        }

        std::string_view remoteAddress() const override
        {
            return remoteAddress_;
        }

        std::uint16_t remotePort() override
        {
            return remotePort_;
        }

        std::string_view localAddress() const override
        {
            return localAddress_;
        }

        std::uint16_t localPort() override
        {
            return localPort_;
        }

        void setTimeout(std::uint32_t timeoutMs) override
        {
            timeoutMs_ = timeoutMs;
            timeoutHistory_.push_back(timeoutMs);
        }

        std::uint32_t getTimeout() const override
        {
            return timeoutMs_;
        }

        std::size_t connectedCheckCount() const
        {
            return connectedCheckCount_;
        }

        const std::vector<std::uint32_t> &timeoutHistory() const
        {
            return timeoutHistory_;
        }

        void disconnect()
        {
            connected_ = false;
        }

        bool stopped() const
        {
            return stopped_;
        }

        std::size_t stopCount() const
        {
            return stopCount_;
        }

        void setWriteScript(std::vector<std::size_t> writeScript)
        {
            writeScript_ = std::move(writeScript);
            writeScriptIndex_ = 0;
        }

        const std::vector<std::size_t> &writeSizes() const
        {
            return writeSizes_;
        }

        std::size_t flushCount() const
        {
            return flushCount_;
        }

        std::string writtenText() const
        {
            return std::string(written_.begin(), written_.end());
        }

    private:
        ScriptedByteSource readable_;
        std::string remoteAddress_;
        std::uint16_t remotePort_ = 0;
        std::string localAddress_;
        std::uint16_t localPort_ = 0;
        bool connected_ = true;
        bool stopped_ = false;
        std::uint32_t timeoutMs_ = 0;
        std::vector<std::uint32_t> timeoutHistory_;
        std::vector<std::size_t> writeScript_;
        std::size_t writeScriptIndex_ = 0;
        std::vector<std::uint8_t> written_;
        std::vector<std::size_t> writeSizes_;
        std::size_t flushCount_ = 0;
        std::size_t stopCount_ = 0;
        std::size_t connectedCheckCount_ = 0;
    };

    class FakeServer : public IServer
    {
    public:
        explicit FakeServer(std::string localAddress = "127.0.0.1", std::uint16_t port = 80)
            : localAddress_(std::move(localAddress)), port_(port)
        {
        }

        std::unique_ptr<IClient> accept() override
        {
            ++acceptCount_;
            if (pendingClients_.empty())
            {
                return nullptr;
            }

            std::unique_ptr<IClient> accepted = std::move(pendingClients_.front());
            pendingClients_.pop_front();
            return accepted;
        }

        void begin() override
        {
            ++beginCount_;
            began_ = true;
        }

        std::uint16_t port() const override
        {
            return port_;
        }

        std::string_view localAddress() const override
        {
            return localAddress_;
        }

        void end() override
        {
            ++endCount_;
            ended_ = true;
        }

        void enqueue(std::unique_ptr<IClient> client)
        {
            pendingClients_.push_back(std::move(client));
        }

        std::size_t pendingCount() const
        {
            return pendingClients_.size();
        }

        std::size_t beginCount() const
        {
            return beginCount_;
        }

        std::size_t endCount() const
        {
            return endCount_;
        }

        std::size_t acceptCount() const
        {
            return acceptCount_;
        }

        bool began() const
        {
            return began_;
        }

        bool ended() const
        {
            return ended_;
        }

    private:
        std::deque<std::unique_ptr<IClient>> pendingClients_;
        std::string localAddress_;
        std::uint16_t port_ = 0;
        std::size_t beginCount_ = 0;
        std::size_t endCount_ = 0;
        std::size_t acceptCount_ = 0;
        bool began_ = false;
        bool ended_ = false;
    };
        }
    }
}
