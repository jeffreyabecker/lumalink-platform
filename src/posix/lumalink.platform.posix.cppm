module;

#include "posix/PosixTime.h"
#include "posix/PosixSocketTransport.h"
#include "posix/PosixFileAdapter.h"

export module lumalink.platform.posix;

import lumalink.platform;

export namespace lumalink::platform::posix
{
    using ::lumalink::platform::posix::PosixClock;
    using ::lumalink::platform::posix::PosixFs;
    using ::lumalink::platform::posix::PosixMonotonicClock;
    using ::lumalink::platform::posix::PosixSocketTransportFactory;
    using ::lumalink::platform::posix::PosixUtcClock;
}