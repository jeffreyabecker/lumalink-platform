#pragma once

#include <cstddef>
#include <expected>

namespace lumalink::platform::buffers
{
    enum class AvailabilityKind
    {
        Exhausted,
        TemporarilyUnavailable,
        Error
    };

    struct AvailabilityError
    {
        AvailabilityKind kind = AvailabilityKind::Exhausted;
        int errorCode = 0;
    };

    using ByteAvailability = std::expected<std::size_t, AvailabilityError>;

    constexpr bool HasAvailableBytes(const ByteAvailability& result)
    {
        return result.has_value() && result.value() > 0;
    }

    constexpr bool IsExhausted(const ByteAvailability& result)
    {
        return !result.has_value() && result.error().kind == AvailabilityKind::Exhausted;
    }

    constexpr bool IsTemporarilyUnavailable(const ByteAvailability& result)
    {
        return !result.has_value() && result.error().kind == AvailabilityKind::TemporarilyUnavailable;
    }

    constexpr bool HasAvailabilityError(const ByteAvailability& result)
    {
        return !result.has_value() && result.error().kind == AvailabilityKind::Error;
    }

    constexpr std::size_t AvailableByteCount(const ByteAvailability& result)
    {
        return result.has_value() ? result.value() : 0;
    }

    constexpr int AvailabilityErrorCode(const ByteAvailability& result)
    {
        return result.has_value() ? 0 : result.error().errorCode;
    }

    constexpr ByteAvailability MakeAvailabilityError(AvailabilityKind kind, int errorCode = 0)
    {
        return std::unexpected(AvailabilityError{kind, errorCode});
    }

    constexpr ByteAvailability AvailableBytes(std::size_t count)
    {
        return count > 0 ? ByteAvailability(count) : MakeAvailabilityError(AvailabilityKind::Exhausted);
    }

    constexpr ByteAvailability ExhaustedResult()
    {
        return MakeAvailabilityError(AvailabilityKind::Exhausted);
    }

    constexpr ByteAvailability TemporarilyUnavailableResult()
    {
        return MakeAvailabilityError(AvailabilityKind::TemporarilyUnavailable);
    }

    constexpr ByteAvailability ErrorResult(int errorCode = 0)
    {
        return MakeAvailabilityError(AvailabilityKind::Error, errorCode);
    }
}