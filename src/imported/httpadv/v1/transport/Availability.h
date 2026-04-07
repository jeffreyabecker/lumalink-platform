#pragma once

#include <cstddef>

namespace httpadv::v1::transport
{
    enum class AvailabilityState
    {
        HasBytes,
        Exhausted,
        TemporarilyUnavailable,
        Error
    };

    struct AvailableResult
    {
        AvailabilityState state = AvailabilityState::Exhausted;
        std::size_t count = 0;
        int errorCode = 0;

        constexpr bool hasBytes() const
        {
            return state == AvailabilityState::HasBytes && count > 0;
        }

        constexpr bool isExhausted() const
        {
            return state == AvailabilityState::Exhausted;
        }

        constexpr bool isTemporarilyUnavailable() const
        {
            return state == AvailabilityState::TemporarilyUnavailable;
        }

        constexpr bool hasError() const
        {
            return state == AvailabilityState::Error;
        }
    };

    constexpr AvailableResult MakeAvailableResult(AvailabilityState state, std::size_t count = 0, int errorCode = 0)
    {
        return AvailableResult{state, count, errorCode};
    }

    constexpr AvailableResult AvailableBytes(std::size_t count)
    {
        return count > 0 ? MakeAvailableResult(AvailabilityState::HasBytes, count) : MakeAvailableResult(AvailabilityState::Exhausted);
    }

    constexpr AvailableResult ExhaustedResult()
    {
        return MakeAvailableResult(AvailabilityState::Exhausted);
    }

    constexpr AvailableResult TemporarilyUnavailableResult()
    {
        return MakeAvailableResult(AvailabilityState::TemporarilyUnavailable);
    }

    constexpr AvailableResult ErrorResult(int errorCode = 0)
    {
        return MakeAvailableResult(AvailabilityState::Error, 0, errorCode);
    }
}
