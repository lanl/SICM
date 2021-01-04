#include <mutex>
#include <new>

#ifdef __cplusplus
extern "C" {
#endif

void* sh_alloc(int id, size_t sz);
void sh_free(void* ptr);
void sh_sized_free(void* ptr, size_t size);

#ifdef __GNUC__
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)   !!(x)
#  define unlikely(x) !!(x)
#endif

#ifdef __cplusplus
}
#endif

void	*operator new(std::size_t size);
void	*operator new[](std::size_t size);
void	*operator new(std::size_t size, const std::nothrow_t &) noexcept;
void	*operator new[](std::size_t size, const std::nothrow_t &) noexcept;
void	operator delete(void *ptr) noexcept;
void	operator delete[](void *ptr) noexcept;
void	operator delete(void *ptr, const std::nothrow_t &) noexcept;
void	operator delete[](void *ptr, const std::nothrow_t &) noexcept;

#if __cpp_sized_deallocation >= 201309
/* C++14's sized-delete operators. */
void	operator delete(void *ptr, std::size_t size) noexcept;
void	operator delete[](void *ptr, std::size_t size) noexcept;
#endif

static void *
handleOOM(std::size_t size, bool nothrow) {
	void *ptr = nullptr;

	while (ptr == nullptr) {
		std::new_handler handler;
		// GCC-4.8 and clang 4.0 do not have std::get_new_handler.
		{
			static std::mutex mtx;
			std::lock_guard<std::mutex> lock(mtx);

			handler = std::set_new_handler(nullptr);
			std::set_new_handler(handler);
		}
		if (handler == nullptr)
			break;

		try {
			handler();
		} catch (const std::bad_alloc &) {
			break;
		}

		ptr = sh_alloc(0, size);
	}

	if (ptr == nullptr && !nothrow)
		std::__throw_bad_alloc();
	return ptr;
}

template <bool IsNoExcept>
void *
newImpl(std::size_t size) noexcept(IsNoExcept) {
	void *ptr = sh_alloc(0, size);
	if (likely(ptr != nullptr))
		return ptr;

	return handleOOM(size, IsNoExcept);
}

void *
operator new(std::size_t size) {
	return newImpl<false>(size);
}

void *
operator new[](std::size_t size) {
	return newImpl<false>(size);
}

void *
operator new(std::size_t size, const std::nothrow_t &) noexcept {
	return newImpl<true>(size);
}

void *
operator new[](std::size_t size, const std::nothrow_t &) noexcept {
	return newImpl<true>(size);
}

void
operator delete(void *ptr) noexcept {
	sh_free(ptr);
}

void
operator delete[](void *ptr) noexcept {
	sh_free(ptr);
}

void
operator delete(void *ptr, const std::nothrow_t &) noexcept {
	sh_free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept {
	sh_free(ptr);
}

#if __cpp_sized_deallocation >= 201309

void
operator delete(void *ptr, std::size_t size) noexcept {
	if (unlikely(ptr == nullptr)) {
		return;
	}
  sh_sized_free(ptr, size);
}

void operator delete[](void *ptr, std::size_t size) noexcept {
	if (unlikely(ptr == nullptr)) {
		return;
	}
  sh_sized_free(ptr, size);
}

#endif  // __cpp_sized_deallocation
