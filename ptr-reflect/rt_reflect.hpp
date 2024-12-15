#pragma once

#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef __RT_IMPL
  #include <atomic>
  #include <chrono>
  #include <cstdio>
  #include <cstdlib>
  #include <mutex>
  #include <shared_mutex>

  #include "rt_hashmap.hpp"
  #include "rt_protected.hpp"

  #ifdef _WIN32
    #include <windows.h>
using pid_t = DWORD;
  #else
    #include <unistd.h>
using pid_t = pid_t;
  #endif

#endif

// NOLINTBEGIN(misc-definitions-in-headers)

namespace ptr_reflect {

extern "C" enum class _rt_Type : uint8_t {
  StackAlloc = 1,
  StackFree,

  HeapMalloc,
  HeapCalloc,
  HeapRealloc,
  HeapMemalign,
  HeapAlignedAlloc,
  HeapFree,
  HeapCXXNew,
  HeapCXXDelete,
};

extern "C" struct _rt_PtrInfo {
  uintptr_t ptr;
  size_t size;
  _rt_Type type;
};

template <typename E> constexpr typename std::underlying_type<E>::type to_integral(E e) {
  return static_cast<typename std::underlying_type<E>::type>(e);
}

constexpr const char *to_string(_rt_Type t) {
  switch (t) {
    case _rt_Type::StackAlloc: return "StackAlloc";
    case _rt_Type::StackFree: return "StackFree";

    case _rt_Type::HeapMalloc: return "HeapMalloc";
    case _rt_Type::HeapCalloc: return "HeapCalloc";
    case _rt_Type::HeapRealloc: return "HeapRealloc";
    case _rt_Type::HeapMemalign: return "HeapMemalign";
    case _rt_Type::HeapAlignedAlloc: return "HeapAlignedAlloc";
    case _rt_Type::HeapFree: return "HeapFree";
    case _rt_Type::HeapCXXNew: return "HeapCXXNew";
    case _rt_Type::HeapCXXDelete: return "HeapCXXDelete";
  }
  return "Unknown";
}

_rt_PtrInfo *query(void *ptr);
size_t *querySize(void *ptr);

#ifdef __RT_IMPL

namespace details {

using namespace std::chrono;

__RT_PROTECT int safe_vasprintf(char **buffer, const char *format, va_list args) {
  va_list args0;
  va_copy(args0, args);
  int size = std::vsnprintf(nullptr, 0, format, args0);
  va_end(args0);

  if (size < 0) {
    va_end(args);
    *buffer = nullptr;
    return -1;
  }
  *buffer = static_cast<char *>(__RT_ALTERNATIVE(calloc)(1, size + 1));
  std::vsnprintf(*buffer, size + 1, format, args);
  va_end(args);
  return size;
}

__RT_PROTECT __attribute__((format(printf, 2, 3))) int safe_snprintf(char **buffer, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int size = safe_vasprintf(buffer, format, args);
  va_end(args);
  return size;
}

__RT_PROTECT __attribute__((format(printf, 2, 3))) int safe_fprintf(FILE *stream, const char *format, ...) {
  if (!stream) return 0;
  va_list args;
  va_start(args, format);
  char *buffer{};
  int size = safe_vasprintf(&buffer, format, args);
  va_end(args);
  if (size > 0) {
    auto written = static_cast<int>(fwrite(buffer, 1, size, stream));
    __RT_ALTERNATIVE(free)(buffer);
    return written;
  }
  return size;
}

__RT_PROTECT void fail() {
  std::fflush(stderr);
  std::abort();
}

struct PtrRecord {
  time_point<steady_clock> point;
  _rt_PtrInfo info;
};

class ReflectService {

  struct RecordCommand {
    time_point<steady_clock> point;
    _rt_PtrInfo info;
  };

  struct ReleaseCommand {
    time_point<steady_clock> point;
    uintptr_t ptr;
    _rt_Type type;
  };

  std::atomic_bool &interpose;
  UnorderedMap<uintptr_t, PtrRecord> data;
  std::shared_mutex mutex{};
  time_point<steady_clock> start;
  std::FILE *trace{};

  __RT_PROTECT PtrRecord *queryUnsafe(uintptr_t ptr, bool allowSubrange) {
    PtrRecord *result = data.find(ptr);
    if (result) return result;
    if (allowSubrange) {
      data.walk([&](uintptr_t, PtrRecord *value) {
        if (ptr >= value->info.ptr && ptr < value->info.ptr + value->info.size) {
          result = value;
          return true;
        }
        return false;
      });
      return result;
    }
    return nullptr;
  }

public:
  __RT_PROTECT ReflectService(std::atomic_bool &interpose)
      : interpose(interpose), data([](auto x) { return x; }), start(steady_clock::now()) {
    char *name{};
    safe_snprintf(&name, "trace_%d.json",
  #ifdef _WIN32
                  GetCurrentProcessId()
  #else
                  getpid()
  #endif
    );
    trace = std::fopen(name, "w");
    __RT_ALTERNATIVE(free)(name);

    safe_fprintf(trace, "[\n");
    safe_fprintf(stderr, "[PtrReflect] started\n");
    interpose = true;
  }

  __RT_PROTECT bool blockingRecord(const _rt_PtrInfo &info, const time_point<steady_clock> now = steady_clock::now()) {
    std::unique_lock lock(mutex);
    // safe_fprintf(stderr, "[PtrReflect] record %p(size=%ld, type=%s)\n", reinterpret_cast<void *>(info.ptr), info.size,
    //              to_string(info.type));

    auto inserted = data.emplace(info.ptr, PtrRecord{now, info});
    if (!inserted) {
      safe_fprintf(stderr, "[PtrReflect] failed to insert %p (size=%ld, type=%s)\n", reinterpret_cast<void *>(info.ptr), info.size,
                   to_string(info.type));
      fail();
    }
    return true;
  }

  __RT_PROTECT bool blockingRelease(uintptr_t ptr, _rt_Type type, const time_point<steady_clock> now = steady_clock::now()) {
    std::unique_lock lock(mutex);
    // safe_fprintf(stderr, "[PtrReflect] release %p (type=%s)\n", reinterpret_cast<void *>(ptr), to_string(type));
    static int i = 0;
    if (auto it = queryUnsafe(ptr, false)) {
      const auto [recordPoint, info] = *it;
      safe_fprintf(trace,
                   "  {"
                   "\"name\": \"0x%lx (%ld)\","
                   "\"cat\": \"%s\", "
                   "\"ph\": \"X\", "
                   "\"ts\": %" PRId64 " , "
                   "\"dur\": %" PRId64 ", \"pid\": %d, \"tid\": %d},\n",
                   info.ptr, info.size, to_string(info.type),                                      //
                   duration_cast<microseconds>(recordPoint.time_since_epoch()).count(),            //
                   duration_cast<microseconds>(now - recordPoint).count(), to_integral(info.type), //
                   0);
      data.erase(ptr);
      i++;
      if (i % 100 == 0) std::fflush(trace);
      return true;
    }
    safe_fprintf(stderr, "[PtrReflect] failed to release %p (type=%s)\n", reinterpret_cast<void *>(ptr), to_string(type));
    // raise(SIGTRAP);
    // fail();
    return true;
  }

  __RT_PROTECT ~ReflectService() {
    interpose = false;
    const auto now = steady_clock::now();
    safe_fprintf(trace,
                 "  {"
                 "\"name\": \"runtime\","
                 "\"cat\": \"global\", "
                 "\"ph\": \"X\", "
                 "\"ts\": %" PRId64 ", "
                 "\"dur\": %" PRId64 ", \"pid\": 0, \"tid\": %d}\n",            //
                 duration_cast<microseconds>(start.time_since_epoch()).count(), //
                 duration_cast<microseconds>(now - start).count(),              //
                 0);
    safe_fprintf(trace, "]");
    std::fclose(trace);
    safe_fprintf(stderr, "[PtrReflect] terminated\n");
  }

  __RT_PROTECT _rt_PtrInfo *blockingQuery(void *ptr) {
    std::shared_lock lock(mutex);
    if (auto it = queryUnsafe(reinterpret_cast<uintptr_t>(ptr), true)) return &it->info;
    return {};
  }

  __RT_PROTECT size_t *blockingQuerySize(void *ptr) {
    std::shared_lock lock(mutex);
    if (auto it = queryUnsafe(reinterpret_cast<uintptr_t>(ptr), true)) return &it->info.size;
    return {};
  }
};

inline std::atomic_bool serviceInit{false};
__RT_PROTECT inline ReflectService *_rt_get() {
  static ReflectService s(serviceInit);
  return &s;
}
inline auto _ = _rt_get();

} // namespace details

extern "C" __RT_PROTECT __attribute__((noinline)) void _rt_record(void *ptr, size_t size, _rt_Type type) {
  if (!details::serviceInit.load()) return;
  details::_rt_get()->blockingRecord(_rt_PtrInfo{reinterpret_cast<uintptr_t>(ptr), size, type});
}
extern "C" __RT_PROTECT __attribute__((noinline)) void _rt_release(void *ptr, _rt_Type type) {
  if (!ptr) return;
  if (!details::serviceInit.load()) return;
  details::_rt_get()->blockingRelease(reinterpret_cast<uintptr_t>(ptr), type);
}

__RT_PROTECT _rt_PtrInfo *reflect(void *ptr) { return details::_rt_get()->blockingQuery(ptr); }
__RT_PROTECT size_t *reflectSize(void *ptr) { return details::_rt_get()->blockingQuerySize(ptr); }

#endif
}; // namespace ptr_reflect

// NOLINTEND(misc-definitions-in-headers)