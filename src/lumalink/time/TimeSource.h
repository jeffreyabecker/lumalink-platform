#pragma once

#include "lumalink/time/ClockTypes.h"
#include "lumalink/time/Clock.h"

#include <expected>

namespace lumalink::platform::time
{
    enum class UtcTimeError
    {
        Unavailable
    };

    using UtcTimeFetchResult = std::expected<UnixTime, UtcTimeError>;

    class IUtcTimeSource
    {
    public:
        virtual ~IUtcTimeSource() = default;
        virtual UtcTimeFetchResult fetchUtcTime() = 0;
    };

    class IUtcSynchronizer
    {
    public:
        virtual ~IUtcSynchronizer() = default;
        virtual UtcTimeFetchResult synchronize() = 0;
    };

    class UtcSynchronizer : public IUtcSynchronizer
    {
    public:
        UtcSynchronizer(IUtcTimeSource& source, ISettableUtcClock& clock)
            : source_(source), clock_(clock)
        {
        }

        UtcTimeFetchResult synchronize() override
        {
            const UtcTimeFetchResult result = source_.fetchUtcTime();
            if (result.has_value())
            {
                clock_.setUtcTime(result.value());
            }
            return result;
        }

    private:
        IUtcTimeSource&    source_;
        ISettableUtcClock& clock_;
    };
}