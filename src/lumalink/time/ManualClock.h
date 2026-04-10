#pragma once

#include "lumalink/time/Clock.h"
#include "lumalink/time/ClockTypes.h"

namespace lumalink::platform::time
{
    class ManualClock : public IClock, public ISettableUtcClock
    {
    public:
        ManualClock() = default;

        MonotonicMillis monotonicNow() const override
        {
            return monotonic_;
        }

        OptionalUnixTime utcNow() const override
        {
            return utc_;
        }

        void setUtcTime(const UnixTime& time) override
        {
            utc_ = time;
        }

        void clearUtcTime() override
        {
            utc_ = std::nullopt;
        }

        void advanceMillis(std::uint64_t deltaMillis)
        {
            monotonic_.value += deltaMillis;
            if (utc_.has_value())
            {
                const std::uint64_t totalMs =
                    static_cast<std::uint64_t>(utc_->subsecondMillis) + deltaMillis;
                utc_->seconds         += static_cast<std::int64_t>(totalMs / 1000U);
                utc_->subsecondMillis  = static_cast<std::uint16_t>(totalMs % 1000U);
            }
        }

        void setMonotonicMillis(std::uint64_t value)
        {
            monotonic_.value = value;
        }

    private:
        MonotonicMillis  monotonic_{};
        OptionalUnixTime utc_{};
    };
}