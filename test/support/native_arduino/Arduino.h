#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <string>
#include <type_traits>
#include <utility>

using byte = uint8_t;

class String
{
public:
    String() = default;
    String(const char *value) : value_(value != nullptr ? value : "") {}
    String(const char *value, std::size_t length) : value_(value != nullptr ? std::string(value, length) : std::string()) {}
    String(const std::string &value) : value_(value) {}
    String(std::string &&value) noexcept : value_(std::move(value)) {}
    String(char value) : value_(1, value) {}

    template <typename Integer, typename = std::enable_if_t<std::is_integral_v<Integer>>>
    String(Integer value) : value_(std::to_string(value)) {}

    String(const String &) = default;
    String(String &&) noexcept = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) noexcept = default;

    const char *c_str() const
    {
        return value_.c_str();
    }

    std::size_t length() const
    {
        return value_.size();
    }

    bool isEmpty() const
    {
        return value_.empty();
    }

    char operator[](std::size_t index) const
    {
        return value_[index];
    }

    std::size_t indexOf(char value, std::size_t fromIndex = 0) const
    {
        const std::size_t result = value_.find(value, fromIndex);
        return result == std::string::npos ? static_cast<std::size_t>(-1) : result;
    }

    std::size_t indexOf(const char *value, std::size_t fromIndex = 0) const
    {
        const std::size_t result = value_.find(value != nullptr ? value : "", fromIndex);
        return result == std::string::npos ? static_cast<std::size_t>(-1) : result;
    }

    void reserve(std::size_t size)
    {
        value_.reserve(size);
    }

    void toLowerCase()
    {
        for (char &ch : value_)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    void toUpperCase()
    {
        for (char &ch : value_)
        {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
    }

    std::string::const_iterator begin() const
    {
        return value_.begin();
    }

    std::string::const_iterator end() const
    {
        return value_.end();
    }

    String &operator+=(char value)
    {
        value_ += value;
        return *this;
    }

    String &operator+=(const char *value)
    {
        value_ += (value != nullptr ? value : "");
        return *this;
    }

    String &operator+=(const String &value)
    {
        value_ += value.value_;
        return *this;
    }

    friend bool operator==(const String &left, const String &right)
    {
        return left.value_ == right.value_;
    }

    friend bool operator!=(const String &left, const String &right)
    {
        return !(left == right);
    }

    friend bool operator<(const String &left, const String &right)
    {
        return left.value_ < right.value_;
    }

    friend String operator+(String left, const String &right)
    {
        left += right;
        return left;
    }

    friend String operator+(String left, const char *right)
    {
        left += right;
        return left;
    }

    friend String operator+(const char *left, const String &right)
    {
        String result(left);
        result += right;
        return result;
    }

private:
    std::string value_;
};

namespace arduino
{
    using String = ::String;
}

#ifndef F
#define F(x) x
#endif