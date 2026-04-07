/* Minimal, header-only `tcb::span` replacement sufficient for platform needs.
   This avoids relying on C++20 `std::span` so the codebase builds on older toolchains.

   Note: This is intentionally small — expand if more std::span features are required. */

#include <cstddef>
#include <iterator>

namespace tcb {
	static constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

	template <typename T, std::size_t N = dynamic_extent>
	class span {
	public:
		using element_type = T;
		using value_type = std::remove_cv_t<T>;
		using size_type = std::size_t;
		using pointer = T*;
		using const_pointer = const T*;
		using iterator = T*;
		using const_iterator = const T*;

		constexpr span() noexcept : data_(nullptr), size_(0) {}
		constexpr span(T* data, size_type size) noexcept : data_(data), size_(size) {}

		constexpr pointer data() const noexcept { return data_; }
		constexpr size_type size() const noexcept { return size_; }
		constexpr bool empty() const noexcept { return size_ == 0; }

		constexpr iterator begin() const noexcept { return data_; }
		constexpr iterator end() const noexcept { return data_ + size_; }

		constexpr T& operator[](size_type idx) const noexcept { return data_[idx]; }

	private:
		T* data_;
		size_type size_;
	};
}

namespace lumalink::platform::util {
	using tcb::span;
	using tcb::dynamic_extent;
}


