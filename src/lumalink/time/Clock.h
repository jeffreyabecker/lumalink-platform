#pragma once

#include "lumalink/time/ClockTypes.h"

namespace lumalink::platform::time
{
    class IMonotonicClock
    {
    public:
        virtual ~IMonotonicClock() = default;
        virtual MonotonicMillis monotonicNow() const = 0;
    };

    class IUtcClock
    {
    public:
        virtual ~IUtcClock() = default;
        virtual OptionalUnixTime utcNow() const = 0;
    };

    class IClock : public IMonotonicClock, public IUtcClock
    {
    public:
        ~IClock() override = default;
    };

    class ISettableUtcClock : public IUtcClock
    {
    public:
        ~ISettableUtcClock() override = default;
        virtual void setUtcTime(const UnixTime& time) = 0;
        virtual void clearUtcTime() = 0;
    };
}