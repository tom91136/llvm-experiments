#pragma once

#include <cstddef>
#include <new>

#include "rt_protected.hpp"
#include "rt_reflect.hpp"

#define __ALLOC __RT_PROTECT [[clang::annotate("__rt_alloc")]] __attribute__((noinline))
#define __FREE __RT_PROTECT [[clang::annotate("__rt_free")]]

#if __cpp_exceptions == 199711
  #define __THROW_OF_ABORT(e) throw e
#else
  #define __THROW_OF_ABORT(e) std::abort()
#endif

// NOLINTBEGIN(misc-definitions-in-headers)

extern "C" __ALLOC void *malloc(size_t size) {
  auto ptr = __RT_ALTERNATIVE(malloc)(size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapMalloc);
  return ptr;
}

extern "C" __ALLOC void *calloc(size_t nmemb, size_t size) {
  auto ptr = __RT_ALTERNATIVE(calloc)(nmemb, size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCalloc);
  return ptr;
}

extern "C" __ALLOC void *realloc(void *ptr, size_t size) {
  if (ptr) ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapFree);
  auto ptr1 = __RT_ALTERNATIVE(realloc)(ptr, size);
  ::ptr_reflect::_rt_record(ptr1, size, ::ptr_reflect::_rt_Type::HeapRealloc);
  return ptr1;
}

extern "C" __ALLOC void *memalign(size_t alignment, size_t size) {
  auto ptr = __RT_ALTERNATIVE(memalign)(alignment, size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapMemalign);
  return ptr;
}

extern "C" __ALLOC void *aligned_alloc(size_t alignment, size_t size) {
  auto ptr = __RT_ALTERNATIVE(memalign)(alignment, size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapAlignedAlloc);
  return ptr;
}

extern "C" __FREE void free(void *ptr) {
  (__RT_ALTERNATIVE(free)(ptr));
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapFree);
}

__ALLOC void *operator new(size_t size) {
  auto *ptr = __RT_ALTERNATIVE(malloc)(size);
  if (!ptr) __THROW_OF_ABORT(std::bad_alloc{});
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new(size_t size, std::align_val_t a) {
  auto *ptr = __RT_ALTERNATIVE(memalign)(static_cast<size_t>(a), size);
  if (!ptr) __THROW_OF_ABORT(std::bad_alloc{});
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new(size_t size, const std::nothrow_t &) noexcept {
  auto ptr = __RT_ALTERNATIVE(malloc)(size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new(size_t size, std::align_val_t a, const std::nothrow_t &) noexcept {
  auto ptr = __RT_ALTERNATIVE(malloc)(size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new[](size_t size) {
  auto *ptr = __RT_ALTERNATIVE(malloc)(size);
  if (!ptr) __THROW_OF_ABORT(std::bad_alloc{});
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new[](size_t size, std::align_val_t a) {
  auto *ptr = __RT_ALTERNATIVE(memalign)(static_cast<size_t>(a), size);
  if (!ptr) __THROW_OF_ABORT(std::bad_alloc{});
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new[](size_t size, const std::nothrow_t &) noexcept {
  auto ptr = __RT_ALTERNATIVE(malloc)(size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__ALLOC void *operator new[](size_t size, std::align_val_t a, const std::nothrow_t &) noexcept {
  auto ptr = __RT_ALTERNATIVE(memalign)(static_cast<size_t>(a), size);
  ::ptr_reflect::_rt_record(ptr, size, ::ptr_reflect::_rt_Type::HeapCXXNew);
  return ptr;
}

__FREE void operator delete(void *ptr) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete[](void *ptr) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete(void *ptr, std::align_val_t) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete[](void *ptr, std::align_val_t) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete(void *ptr, size_t) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete[](void *ptr, size_t) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete(void *ptr, size_t, std::align_val_t) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete[](void *ptr, size_t, std::align_val_t) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete(void *ptr, const std::nothrow_t &) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete[](void *ptr, const std::nothrow_t &) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete(void *ptr, std::align_val_t, const std::nothrow_t &) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}
__FREE void operator delete[](void *ptr, std::align_val_t, const std::nothrow_t &) noexcept {
  ::ptr_reflect::_rt_release(ptr, ::ptr_reflect::_rt_Type::HeapCXXDelete);
  (__RT_ALTERNATIVE(free)(ptr));
}

// NOLINTEND(misc-definitions-in-headers)