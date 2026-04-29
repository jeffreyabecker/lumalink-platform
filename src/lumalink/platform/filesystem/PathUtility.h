#pragma once

#include "../buffers/ByteStream.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lumalink::platform::filesystem
{
    struct PathUtility
    {
        static std::string_view getFileName(std::string_view path)
        {
            const std::size_t separator = path.find_last_of('/');
            if (separator == std::string_view::npos)
            {
                return path;
            }

            return path.substr(separator + 1);
        }
        static std::string_view getDirName(std::string_view path)
        {
            const std::size_t separator = path.find_last_of('/');
            if (separator == std::string_view::npos)
            {
                return std::string_view();
            }

            return path.substr(0, separator);
        }
        template <typename... Paths>
        static std::string join(Paths &&...paths)
        {
            std::size_t totalSize = 0;
            bool isFirstSegment = true;
            (..., [&totalSize, &isFirstSegment](auto &&path)
             {
                std::string_view pathSegment(path);
                if (!pathSegment.empty() && !isFirstSegment && pathSegment.front() == '/')
                {
                    totalSize += pathSegment.length() - 1;
                }
                else
                {
                    totalSize += pathSegment.length();
                }
                totalSize += 1; // for separator
                isFirstSegment = false; }(std::forward<Paths>(paths)));

            std::string result;
            result.reserve(totalSize);

            isFirstSegment = true;
            (..., [&result, &isFirstSegment](auto &&path)
             {
                if (!result.empty() && result.back() != '/') {
                    result += '/';
                }
                std::string_view pathSegment(path);
                if (!pathSegment.empty() && !isFirstSegment && pathSegment.front() == '/') {
                    pathSegment.remove_prefix(1);
                }
                result += pathSegment;
                isFirstSegment = false; }(std::forward<Paths>(paths)));

            return result;
        }
        static std::string_view getExtension(std::string_view path)
        {
            const std::size_t separator = path.find_last_of('/');
            const std::size_t dot = path.find_last_of('.');
            if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator))
            {
                return std::string_view();
            }

            return path.substr(dot + 1);
        }
        static std::string_view removeExtension(std::string_view path)
        {
            const std::size_t separator = path.find_last_of('/');
            const std::size_t dot = path.find_last_of('.');
            if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator))
            {
                return path;
            }

            return path.substr(0, dot);
        }
        static std::string_view makeRelative(std::string_view path, std::string_view base, bool includeLeadingSeparator = false)
        {
            if (base.empty())
            {
                return path;
            }

            if (path.size() > base.size() && path.compare(0, base.size(), base) == 0 &&
                (path[base.size()] == '/' || base.back() == '/'))
            {
                return path.substr(base.size() + (base.back() == '/' ? 0 : 1) - (includeLeadingSeparator ? 1 : 0));
            }

            return path;
        }
    };
}
