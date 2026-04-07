#pragma once

#include "../../../src/httpadv/v1/transport/ByteStream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
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
            using namespace httpadv::v1::transport;

            inline std::string ReadByteSourceAsStdString(IByteSource &source)
            {
                std::string result;
                std::uint8_t buffer[256] = {};

                while (true)
                {
                    const std::size_t bytesRead = source.read(httpadv::v1::util::span<std::uint8_t>(buffer, sizeof(buffer)));
                    if (bytesRead == 0)
                    {
                        break;
                    }

                    result.append(reinterpret_cast<const char *>(buffer), bytesRead);
                }

                return result;
            }

            inline std::string DrainByteSourceWithAvailability(IByteSource &source, std::size_t maxUnavailablePolls = 8)
            {
                std::string result;
                std::uint8_t buffer[256] = {};
                std::size_t unavailablePolls = 0;

                while (true)
                {
                    const AvailableResult available = source.available();
                    if (available.hasBytes())
                    {
                        const std::size_t bytesRead = source.read(httpadv::v1::util::span<std::uint8_t>(buffer, (std::min)(available.count, sizeof(buffer))));
                        if (bytesRead == 0)
                        {
                            break;
                        }

                        result.append(reinterpret_cast<const char *>(buffer), bytesRead);
                        unavailablePolls = 0;
                        continue;
                    }

                    if (available.isTemporarilyUnavailable())
                    {
                        ++unavailablePolls;
                        if (unavailablePolls > maxUnavailablePolls)
                        {
                            break;
                        }

                        continue;
                    }

                    break;
                }

                return result;
            }

            class ScriptedByteSource : public IByteSource
            {
            public:
                struct Step
                {
                    std::string text;
                    bool temporarilyUnavailable = false;
                    bool unavailableReported = false;
                };

                ScriptedByteSource() = default;

                explicit ScriptedByteSource(std::initializer_list<std::pair<const char *, bool>> steps)
                {
                    for (const auto &step : steps)
                    {
                        steps_.push_back({step.first != nullptr ? step.first : "", step.second, false});
                    }
                }

                static ScriptedByteSource FromText(const char *text)
                {
                    ScriptedByteSource source;
                    source.steps_.push_back({text != nullptr ? text : "", false, false});
                    return source;
                }

                static ScriptedByteSource FromText(std::string_view text)
                {
                    ScriptedByteSource source;
                    source.steps_.push_back({std::string(text), false, false});
                    return source;
                }

                AvailableResult available() override
                {
                    advancePastConsumedSteps();
                    if (stepIndex_ >= steps_.size())
                    {
                        return ExhaustedResult();
                    }

                    Step &step = steps_[stepIndex_];
                    if (step.temporarilyUnavailable)
                    {
                        if (!step.unavailableReported)
                        {
                            step.unavailableReported = true;
                            return TemporarilyUnavailableResult();
                        }

                        ++stepIndex_;
                        positionInStep_ = 0;
                        return available();
                    }

                    return AvailableBytes(step.text.size() - positionInStep_);
                }

                std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) override
                {
                    return copyInto(buffer, true);
                }

                std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) override
                {
                    return copyInto(buffer, false);
                }

            private:
                std::vector<Step> steps_;
                std::size_t stepIndex_ = 0;
                std::size_t positionInStep_ = 0;

                void advancePastConsumedSteps()
                {
                    while (stepIndex_ < steps_.size())
                    {
                        Step &step = steps_[stepIndex_];
                        if (step.temporarilyUnavailable)
                        {
                            if (step.unavailableReported)
                            {
                                ++stepIndex_;
                                positionInStep_ = 0;
                                continue;
                            }

                            break;
                        }

                        if (positionInStep_ >= step.text.size())
                        {
                            ++stepIndex_;
                            positionInStep_ = 0;
                            continue;
                        }

                        break;
                    }
                }

                std::size_t copyInto(httpadv::v1::util::span<std::uint8_t> buffer, bool consume)
                {
                    advancePastConsumedSteps();
                    if (buffer.empty() || stepIndex_ >= steps_.size() || steps_[stepIndex_].temporarilyUnavailable)
                    {
                        return 0;
                    }

                    const Step &step = steps_[stepIndex_];
                    std::size_t totalRead = 0;
                    const std::size_t startOffset = positionInStep_;
                    while (totalRead < buffer.size() && (startOffset + totalRead) < step.text.size())
                    {
                        buffer[totalRead] = static_cast<std::uint8_t>(step.text[startOffset + totalRead]);
                        ++totalRead;
                    }

                    if (consume)
                    {
                        positionInStep_ += totalRead;
                        advancePastConsumedSteps();
                    }

                    return totalRead;
                }
            };

            class RecordingByteChannel : public IByteChannel
            {
            public:
                using IByteSink::write;

                RecordingByteChannel() = default;

                explicit RecordingByteChannel(std::initializer_list<std::pair<const char *, bool>> steps)
                    : readable_(steps)
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
                    written_.insert(written_.end(), buffer.begin(), buffer.end());
                    writeSizes_.push_back(buffer.size());
                    return buffer.size();
                }

                void flush() override
                {
                    ++flushCount_;
                }

                const std::vector<std::uint8_t> &writtenBytes() const
                {
                    return written_;
                }

                std::string writtenText() const
                {
                    return std::string(written_.begin(), written_.end());
                }

                const std::vector<std::size_t> &writeSizes() const
                {
                    return writeSizes_;
                }

                std::size_t flushCount() const
                {
                    return flushCount_;
                }

            private:
                ScriptedByteSource readable_;
                std::vector<std::uint8_t> written_;
                std::vector<std::size_t> writeSizes_;
                std::size_t flushCount_ = 0;
            };
        }
    }
}
