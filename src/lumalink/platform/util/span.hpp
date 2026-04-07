/*
This is an implementation of C++20's std::span
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/n4820.pdf
*/

//          Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef TCB_SPAN_HPP_INCLUDED
#define TCB_SPAN_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifndef TCB_SPAN_NO_EXCEPTIONS
// Attempt to discover whether we're being compiled with exception support
#if !(defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
#define TCB_SPAN_NO_EXCEPTIONS
#endif
#endif

#ifndef TCB_SPAN_NO_EXCEPTIONS
#include <cstdio>
#include <stdexcept>
#endif

// Various feature test macros

#ifndef TCB_SPAN_NAMESPACE_NAME
#define TCB_SPAN_NAMESPACE_NAME tcb
#endif

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define TCB_SPAN_HAVE_CPP17
#endif

#if __cplusplus >= 201402L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
#define TCB_SPAN_HAVE_CPP14
#endif

namespace TCB_SPAN_NAMESPACE_NAME {

// Establish default contract checking behavior
#if !defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION) &&                          \
	!defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION) &&                      \
	!defined(TCB_SPAN_NO_CONTRACT_CHECKING)
#if defined(NDEBUG) || !defined(TCB_SPAN_HAVE_CPP14)
#define TCB_SPAN_NO_CONTRACT_CHECKING
#else
#define TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION
#endif
#endif

#if defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION)
struct contract_violation_error : std::logic_error {
	explicit contract_violation_error(const char* msg) : std::logic_error(msg)
	{}
};

inline void contract_violation(const char* msg)
{
	throw contract_violation_error(msg);
}

#elif defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION)
[[noreturn]] inline void contract_violation(const char* /*unused*/)
{
	std::terminate();
}
#endif

#if !defined(TCB_SPAN_NO_CONTRACT_CHECKING)
#define TCB_SPAN_STRINGIFY(cond) #cond
#define TCB_SPAN_EXPECT(cond)                                                  \
	cond ? (void) 0 : contract_violation("Expected " TCB_SPAN_STRINGIFY(cond))
#else
#define TCB_SPAN_EXPECT(cond)
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_inline_variables)
#define TCB_SPAN_INLINE_VAR inline
#else
#define TCB_SPAN_INLINE_VAR
#endif

#if defined(TCB_SPAN_HAVE_CPP14) ||                                            \
	(defined(__cpp_constexpr) && __cpp_constexpr >= 201304)
#define TCB_SPAN_CONSTEXPR14 constexpr
#else
#define TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP14_CONSTEXPR) &&                                  \
	(!defined(_MSC_VER) || _MSC_VER > 1900)
#define TCB_SPAN_CONSTEXPR_ASSIGN constexpr
#else
#define TCB_SPAN_CONSTEXPR_ASSIGN
#endif

#if defined(TCB_SPAN_NO_CONTRACT_CHECKING)
#define TCB_SPAN_CONSTEXPR11 constexpr
#else
#define TCB_SPAN_CONSTEXPR11 TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_deduction_guides)
#define TCB_SPAN_HAVE_DEDUCTION_GUIDES
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_byte)
#define TCB_SPAN_HAVE_STD_BYTE
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_array_constexpr)
#define TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC
#endif

#if defined(TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC)
#define TCB_SPAN_ARRAY_CONSTEXPR constexpr
#else
#define TCB_SPAN_ARRAY_CONSTEXPR
#endif

#ifdef TCB_SPAN_HAVE_STD_BYTE
using byte = std::byte;
#else
using byte = unsigned char;
#endif

#if defined(TCB_SPAN_HAVE_CPP17)
#define TCB_SPAN_NODISCARD [[nodiscard]]
#else
#define TCB_SPAN_NODISCARD
#endif

TCB_SPAN_INLINE_VAR constexpr std::size_t dynamic_extent = SIZE_MAX;

template <typename ElementType, std::size_t Extent = dynamic_extent>
class span;

namespace detail {

template <typename E, std::size_t S>
struct span_storage {
	constexpr span_storage() noexcept = default;

	constexpr span_storage(E* p_ptr, std::size_t /*unused*/) noexcept
	   : ptr(p_ptr)
	{}

	E* ptr = nullptr;
	static constexpr std::size_t size = S;
};

template <typename E>
struct span_storage<E, dynamic_extent> {
	constexpr span_storage() noexcept = default;

	constexpr span_storage(E* p_ptr, std::size_t p_size) noexcept
		: ptr(p_ptr), size(p_size)
	{}

	E* ptr = nullptr;
	std::size_t size = 0;
};

// Reimplementation of C++17 std::size() and std::data()
#if defined(TCB_SPAN_HAVE_CPP17) ||                                            \
	defined(__cpp_lib_nonmember_container_access)
using std::data;
using std::size;
#else
template <class C>
constexpr auto size(const C& c) -> decltype(c.size())
{
	return c.size();
}

template <class T, std::size_t N>
constexpr std::size_t size(const T (&)[N]) noexcept
{
	return N;
}

template <class C>
constexpr auto data(C& c) -> decltype(c.data())
{
	return c.data();
}

template <class C>
#endif


