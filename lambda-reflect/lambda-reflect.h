#include <cassert>

namespace lambda_reflect {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"

template <typename T, typename F> //
constexpr T get([[maybe_unused]] F &f, [[maybe_unused]] const char *field) {
  assert(false && "impl not replaced");
} // NOLINT(*-reserved-identifier)

template <typename T, typename F> //
constexpr void set([[maybe_unused]] F &f, [[maybe_unused]] const char *field, const T &value) {
  assert(false && "impl not replaced");
} // NOLINT(*-reserved-identifier)

#pragma clang diagnostic pop

} // namespace lambda_reflect
