#pragma once

#include "lumalink/LumaLinkPlatform.h"

#if defined(_WIN32)
#include "windows/WindowsFactory.h"
#else
#include "posix/PosixFactory.h"
#endif