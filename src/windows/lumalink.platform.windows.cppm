module;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "windows/WindowsTime.h"
#include "windows/WindowsSocketTransport.h"
#include "windows/WindowsFileAdapter.h"

export module lumalink.platform.windows;

import lumalink.platform;

export namespace lumalink::platform::windows
{
    using ::lumalink::platform::windows::WindowsClock;
    using ::lumalink::platform::windows::WindowsFs;
    using ::lumalink::platform::windows::WindowsMonotonicClock;
    using ::lumalink::platform::windows::WindowsSocketTransportFactory;
    using ::lumalink::platform::windows::WindowsUtcClock;
}