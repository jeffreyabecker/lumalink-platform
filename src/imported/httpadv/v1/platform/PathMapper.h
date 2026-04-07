#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace httpadv::v1::platform
{
    namespace detail
    {
        struct PosixPathMapperTraits
        {
            static constexpr char ScopedSeparator = '/';

            static std::string normalizeScopedPath(std::string_view path)
            {
                std::string normalized(path);
                std::replace(normalized.begin(), normalized.end(), '\\', '/');
                return normalized;
            }

            static bool isScopedSeparator(char value)
            {
                return value == '/';
            }

            static std::size_t minimumRootLength(const std::string &normalized)
            {
                return normalized.empty() ? 0u : 1u;
            }
        };

        struct WindowsPathMapperTraits
        {
            static constexpr char ScopedSeparator = '\\';

            static std::string normalizeScopedPath(std::string_view path)
            {
                std::string normalized(path);
                std::replace(normalized.begin(), normalized.end(), '/', '\\');
                return normalized;
            }

            static bool isScopedSeparator(char value)
            {
                return value == '/' || value == '\\';
            }

            static std::size_t minimumRootLength(const std::string &normalized)
            {
                if (normalized.size() >= 2 && normalized[1] == ':')
                {
                    return 3u;
                }

                return normalized.empty() ? 0u : 1u;
            }
        };

        template <typename Traits>
        class PathMapperBase
        {
        public:
            PathMapperBase() = default;

            explicit PathMapperBase(std::string_view scopedRootPath)
                : rootPath_(NormalizeScopedRootPath(scopedRootPath))
            {
            }

            static bool HasDriveLetterPrefix(std::string_view path)
            {
                return path.size() >= 2 &&
                       ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
                       path[1] == ':';
            }

            static std::string NormalizePath(std::string_view path)
            {
                std::string normalized(path);
                std::replace(normalized.begin(), normalized.end(), '\\', '/');

                if (HasDriveLetterPrefix(normalized))
                {
                    normalized.erase(0, 2);
                }

                std::vector<std::string_view> segments;
                std::size_t start = 0;
                while (start <= normalized.size())
                {
                    const std::size_t separator = normalized.find('/', start);
                    const std::size_t segmentEnd = separator == std::string::npos ? normalized.size() : separator;
                    const std::string_view segment(normalized.data() + start, segmentEnd - start);

                    if (!segment.empty() && segment != ".")
                    {
                        if (segment == "..")
                        {
                            if (!segments.empty())
                            {
                                segments.pop_back();
                            }
                        }
                        else
                        {
                            segments.push_back(segment);
                        }
                    }

                    if (separator == std::string::npos)
                    {
                        break;
                    }

                    start = separator + 1;
                }

                std::string joined;
                for (const std::string_view segment : segments)
                {
                    if (!joined.empty())
                    {
                        joined.push_back('/');
                    }

                    joined.append(segment.data(), segment.size());
                }

                return joined;
            }

            static std::string NormalizeRootPath(std::string_view path)
            {
                std::string normalized = WithLeadingSlash(path);
                while (normalized.size() > 1 && normalized.back() == '/')
                {
                    normalized.pop_back();
                }

                return normalized;
            }

            static std::string WithLeadingSlash(std::string_view path)
            {
                const std::string normalized = NormalizePath(path);
                if (normalized.empty())
                {
                    return "/";
                }

                return "/" + normalized;
            }

            static std::string Join(std::string_view base, std::string_view name)
            {
                const std::string normalizedBase = WithLeadingSlash(base);
                if (normalizedBase == "/")
                {
                    return "/" + NormalizePath(name);
                }

                return normalizedBase + "/" + NormalizePath(name);
            }

            static std::string NormalizeScopedPath(std::string_view path)
            {
                return Traits::normalizeScopedPath(path);
            }

            static std::string NormalizeScopedRootPath(std::string_view path)
            {
                std::string normalized = NormalizeScopedPath(path);
                const std::size_t minimumLength = Traits::minimumRootLength(normalized);
                while (normalized.size() > minimumLength && Traits::isScopedSeparator(normalized.back()))
                {
                    normalized.pop_back();
                }

                return normalized;
            }

            static std::string JoinScopedPath(std::string_view base, std::string_view name)
            {
                if (base.empty())
                {
                    return NormalizeScopedPath(name);
                }

                std::string joined(base);
                if (!Traits::isScopedSeparator(joined.back()))
                {
                    joined.push_back(Traits::ScopedSeparator);
                }

                joined.append(NormalizeScopedPath(name));
                return joined;
            }

            static std::string_view BasenameView(std::string_view path)
            {
                const std::size_t separator = path.find_last_of("\\/");
                if (separator == std::string_view::npos)
                {
                    return path;
                }

                return path.substr(separator + 1);
            }

            bool hasRootPath() const
            {
                return !rootPath_.empty();
            }

            const std::string &rootPath() const
            {
                return rootPath_;
            }

            std::string resolveScopedPath(std::string_view path) const
            {
                if (rootPath_.empty())
                {
                    return NormalizeScopedPath(path);
                }

                const std::string relativePath = NormalizePath(path);
                if (relativePath.empty())
                {
                    return rootPath_;
                }

                return JoinScopedPath(rootPath_, relativePath);
            }

            std::string exposePath(std::string_view path) const
            {
                if (rootPath_.empty())
                {
                    return std::string(path);
                }

                return WithLeadingSlash(path);
            }

        private:
            std::string rootPath_{};
        };
    }

    class PosixPathMapper : public detail::PathMapperBase<detail::PosixPathMapperTraits>
    {
    public:
        using detail::PathMapperBase<detail::PosixPathMapperTraits>::PathMapperBase;
    };

    class WindowsPathMapper : public detail::PathMapperBase<detail::WindowsPathMapperTraits>
    {
    public:
        using detail::PathMapperBase<detail::WindowsPathMapperTraits>::PathMapperBase;
    };
}