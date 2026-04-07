#pragma once

#include "../time/Clock.h"
#include "../time/ClockTypes.h"

#if defined(ARDUINO)

#include <Arduino.h>

#if __has_include(<WiFi.h>)
#include <WiFi.h>
#endif

#include <ctime>

namespace lumalink::platform::arduino
{
    namespace detail
    {
        static constexpr std::time_t kMinimumSaneUnixTime = 10'000'000;
    }

    class ArduinoMonotonicClock : public lumalink::platform::time::IMonotonicClock
    {
    public:
        lumalink::platform::time::MonotonicMillis monotonicNow() const override
        {
            return {static_cast<std::uint64_t>(::millis())};
        }
    };

    class ArduinoUtcClock : public lumalink::platform::time::IUtcClock
    {
    public:
        lumalink::platform::time::OptionalUnixTime utcNow() const override
        {
            const std::time_t now = ::time(nullptr);
            if (now < detail::kMinimumSaneUnixTime)
            {
                return std::nullopt;
            }

            lumalink::platform::time::UnixTime result;
            result.seconds         = static_cast<std::int64_t>(now);
            result.subsecondMillis = 0;
            return result;
        }

#if __has_include(<WiFiNTP.h>)
        bool beginNtp(const char* primaryServer = "pool.ntp.org",
                      const char* secondaryServer = "time.nist.gov",
                      int timeoutSeconds = 3600)
        {
            if (primaryServer == nullptr || primaryServer[0] == '\0')
            {
                return false;
            }

            if (secondaryServer != nullptr && secondaryServer[0] != '\0')
            {
                NTP.begin(primaryServer, secondaryServer, timeoutSeconds);
            }
            else
            {
                NTP.begin(primaryServer, timeoutSeconds);
            }

            return true;
        }

        bool waitForSync(std::uint32_t timeoutMs = 10'000)
        {
            return NTP.waitSet(timeoutMs);
        }

        bool ntpRunning() const
        {
            return NTP.running();
        }
#else
        bool beginNtp(const char* primaryServer = nullptr,
                      const char* secondaryServer = nullptr,
                      int timeoutSeconds = 0)
        {
            (void)primaryServer;
            (void)secondaryServer;
            (void)timeoutSeconds;
            return false;
        }

        bool waitForSync(std::uint32_t timeoutMs = 0)
        {
            (void)timeoutMs;
            return false;
        }

        bool ntpRunning() const
        {
            return false;
        }
#endif
    };

    class ArduinoClock : public lumalink::platform::time::IClock
    {
    public:
        lumalink::platform::time::MonotonicMillis monotonicNow() const override
        {
            return monotonic_.monotonicNow();
        }

        lumalink::platform::time::OptionalUnixTime utcNow() const override
        {
            return utc_.utcNow();
        }

#if __has_include(<WiFiNTP.h>)
        bool beginNtp(const char* primaryServer = "pool.ntp.org",
                      const char* secondaryServer = "time.nist.gov",
                      int timeoutSeconds = 3600)
        {
            return utc_.beginNtp(primaryServer, secondaryServer, timeoutSeconds);
        }

        bool waitForSync(std::uint32_t timeoutMs = 10'000)
        {
            return utc_.waitForSync(timeoutMs);
        }

        bool ntpRunning() const
        {
            return utc_.ntpRunning();
        }
#else
        bool beginNtp(const char* primaryServer = nullptr,
                      const char* secondaryServer = nullptr,
                      int timeoutSeconds = 0)
        {
            return utc_.beginNtp(primaryServer, secondaryServer, timeoutSeconds);
        }

        bool waitForSync(std::uint32_t timeoutMs = 0)
        {
            return utc_.waitForSync(timeoutMs);
        }

        bool ntpRunning() const
        {
            return utc_.ntpRunning();
        }
#endif

    private:
        ArduinoMonotonicClock monotonic_{};
        ArduinoUtcClock       utc_{};
    };
}

#endif