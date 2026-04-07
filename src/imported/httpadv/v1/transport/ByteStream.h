#pragma once

#include "Availability.h"
#include "../util/Span.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace httpadv::v1::transport
{
    inline int LegacyAvailableFromResult(const AvailableResult &result)
    {
        switch (result.state)
        {
        case AvailabilityState::HasBytes:
            return static_cast<int>(std::min<std::size_t>(result.count, static_cast<std::size_t>(std::numeric_limits<int>::max())));

        case AvailabilityState::Exhausted:
            return 0;

        case AvailabilityState::TemporarilyUnavailable:
        case AvailabilityState::Error:
        default:
            return -1;
        }
    }

    class IByteSource
    {
    public:
        virtual ~IByteSource() = default;

        virtual AvailableResult available() = 0;
        virtual size_t read(httpadv::v1::util::span<uint8_t> buffer) = 0;
        virtual int read(){
            uint8_t byte = 0;
            return read(httpadv::v1::util::span<uint8_t>(&byte, 1)) == 0 ? -1 : byte;
        }
        virtual size_t readBytes(uint8_t *buffer, size_t size)
        {
            return read(httpadv::v1::util::span<uint8_t>(buffer, size));
        }
        virtual size_t peek(uint8_t *buffer, size_t size)
        {
            return peek(httpadv::v1::util::span<uint8_t>(buffer, size));
        }

        virtual size_t peek(httpadv::v1::util::span<uint8_t> buffer) = 0;
    };

    class IByteSink
    {
    public:
        virtual ~IByteSink() = default;

        virtual std::size_t write(std::uint8_t byte)
        {
            return write(httpadv::v1::util::span<const uint8_t>(&byte, 1));
        }
        virtual std::size_t write(std::uint8_t* buffer, std::size_t size)
        {
            return write(httpadv::v1::util::span<const uint8_t>(buffer, size));
        }
        virtual std::size_t write(httpadv::v1::util::span<const uint8_t> buffer) = 0;

        std::size_t write(std::string_view buffer)
        {
            return write(httpadv::v1::util::span<const uint8_t>(reinterpret_cast<const std::uint8_t *>(buffer.data()), buffer.size()));
        }

        virtual void flush() = 0;
    };

    class IByteChannel : public IByteSource, public IByteSink
    {
    public:
        ~IByteChannel() override = default;
    };

    inline int ReadByte(IByteSource &source)
    {
        uint8_t byte = 0;
        return source.read(httpadv::v1::util::span<uint8_t>(&byte, 1)) == 0 ? -1 : byte;
    }

    inline int PeekByte(IByteSource &source)
    {
        uint8_t byte = 0;
        return source.peek(httpadv::v1::util::span<uint8_t>(&byte, 1)) == 0 ? -1 : byte;
    }

    inline std::vector<uint8_t> ReadAsVector(IByteSource &source, size_t maxLength = SIZE_MAX)
    {
        std::vector<uint8_t> result;
        const AvailableResult available = source.available();
        if (available.hasBytes() && available.count < maxLength)
        {
            result.reserve(available.count);
        }
        else if (maxLength != SIZE_MAX)
        {
            result.reserve(maxLength);
        }

        uint8_t buffer[256] = {};
        size_t totalRead = 0;
        while (totalRead < maxLength)
        {
            const size_t chunkSize = (std::min)(sizeof(buffer), maxLength - totalRead);
            const size_t bytesRead = source.read(httpadv::v1::util::span<uint8_t>(buffer, chunkSize));
            if (bytesRead == 0)
            {
                break;
            }

            result.insert(result.end(), buffer, buffer + bytesRead);
            totalRead += bytesRead;
        }

        return result;
    }

    inline std::string ReadAsStdString(IByteSource &source, size_t maxLength = SIZE_MAX)
    {
        const std::vector<uint8_t> data = ReadAsVector(source, maxLength);
        return std::string(data.begin(), data.end());
    }

    class SingleByteSource : public IByteSource
    {
    public:
        size_t read(httpadv::v1::util::span<uint8_t> buffer) override
        {
            size_t totalRead = 0;
            while (totalRead < buffer.size())
            {
                const int value = readSingleByte();
                if (value < 0)
                {
                    break;
                }

                buffer[totalRead++] = static_cast<uint8_t>(value);
            }

            return totalRead;
        }

        size_t peek(httpadv::v1::util::span<uint8_t> buffer) override
        {
            if (buffer.empty())
            {
                return 0;
            }

            const int value = peekSingleByte();
            if (value < 0)
            {
                return 0;
            }

            buffer[0] = static_cast<uint8_t>(value);
            return 1;
        }

    protected:
        virtual int readSingleByte() = 0;
        virtual int peekSingleByte() = 0;
    };

    class SpanByteSource : public IByteSource
    {
    public:
        SpanByteSource(const uint8_t *data, size_t size)
            : data_(data), size_(size)
        {
        }

        explicit SpanByteSource(std::string_view data)
            : SpanByteSource(reinterpret_cast<const uint8_t *>(data.data()), data.size())
        {
        }

        AvailableResult available() override
        {
            if (position_ >= size_)
            {
                return ExhaustedResult();
            }

            return AvailableBytes(size_ - position_);
        }

        size_t read(httpadv::v1::util::span<uint8_t> buffer) override
        {
            const size_t copied = peek(buffer);
            position_ += copied;
            return copied;
        }

        size_t peek(httpadv::v1::util::span<uint8_t> buffer) override
        {
            if (buffer.empty() || position_ >= size_ || data_ == nullptr)
            {
                return 0;
            }

            const size_t copied = std::min<size_t>(static_cast<size_t>(buffer.size()), size_ - position_);
            std::copy_n(data_ + position_, copied, buffer.data());
            return copied;
        }

    protected:
        const uint8_t *data_ = nullptr;
        size_t size_ = 0;
        size_t position_ = 0;
    };

    class StdStringByteSource : public SpanByteSource
    {
    public:
        explicit StdStringByteSource(std::string data)
            : SpanByteSource(nullptr, 0), storage_(std::move(data))
        {
            data_ = reinterpret_cast<const uint8_t *>(storage_.data());
            size_ = storage_.size();
        }

    private:
        std::string storage_;
    };

    class VectorByteSource : public SpanByteSource
    {
    public:
        explicit VectorByteSource(std::vector<uint8_t> data)
            : SpanByteSource(nullptr, 0), storage_(std::move(data))
        {
            data_ = storage_.data();
            size_ = storage_.size();
        }

    private:
        std::vector<uint8_t> storage_;
    };

    class ConcatByteSource : public IByteSource
    {
    public:
        explicit ConcatByteSource(std::vector<std::unique_ptr<IByteSource>> sources)
            : sources_(std::move(sources))
        {
        }

        AvailableResult available() override
        {
            return currentAvailability();
        }

        size_t read(httpadv::v1::util::span<uint8_t> buffer) override
        {
            size_t totalRead = 0;
            while (totalRead < buffer.size())
            {
                const AvailableResult availableResult = currentAvailability();
                if (!availableResult.hasBytes())
                {
                    break;
                }

                httpadv::v1::util::span<uint8_t> destination(buffer.data() + totalRead, buffer.size() - totalRead);
                const size_t bytesRead = sources_[currentIndex_]->read(destination);
                if (bytesRead == 0)
                {
                    break;
                }

                totalRead += bytesRead;
            }

            return totalRead;
        }

        size_t peek(httpadv::v1::util::span<uint8_t> buffer) override
        {
            const AvailableResult availableResult = currentAvailability();
            if (!availableResult.hasBytes() || buffer.empty())
            {
                return 0;
            }

            return sources_[currentIndex_]->peek(buffer);
        }

    private:
        AvailableResult currentAvailability()
        {
            while (currentIndex_ < sources_.size())
            {
                AvailableResult result = sources_[currentIndex_]->available();
                if (result.isExhausted())
                {
                    ++currentIndex_;
                    continue;
                }

                return result;
            }

            return ExhaustedResult();
        }

        std::vector<std::unique_ptr<IByteSource>> sources_;
        size_t currentIndex_ = 0;
    };
}