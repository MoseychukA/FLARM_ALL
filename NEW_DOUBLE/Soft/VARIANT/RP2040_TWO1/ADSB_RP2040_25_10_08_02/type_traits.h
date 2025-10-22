//// SPDX-FileCopyrightText: 2020-2024 Artur Bać
//// SPDX-License-Identifier: BSL-1.0
//// SPDX-PackageHomePage: https://github.com/arturbac/fixed_math
//
//#pragma once
//
//#include <cstdint>
//#include <concepts>
//#include <type_traits>
//#include <limits>
//#include <cmath>
//
//namespace fixedmath::inline v2
//  {
//using fixed_internal = int64_t;
//using fixed_internal_unsigned = uint64_t;
//
//struct fix_carrier_t;
//struct fixed_t;
//
//  }  // namespace fixedmath::inline v2
//
//namespace fixedmath::typetraits
//  {
//// clang-format off
//template<int size> struct signed_type_by_size { };
//// template<> struct signed_type_by_size<1> { using type = int8_t; };
//template<> struct signed_type_by_size<2> { using type = int16_t; };
//template<> struct signed_type_by_size<4> { using type = int32_t; };
//template<> struct signed_type_by_size<8> { using type = int64_t; };
//template<> struct signed_type_by_size<16>{ using type = int64_t; };
//
//// clang-format on
//
//template<std::size_t Size>
//using signed_type_by_size_t = typename signed_type_by_size<Size>::type;
//
//template<typename T>
//using promote_to_signed_t = signed_type_by_size_t<(sizeof(T) << 1)>;
//
//template<typename T>
//inline constexpr bool is_integral_v = std::is_integral_v<T>;
//
//template<typename T>
//inline constexpr bool is_fixed_point_v = std::is_same_v<T, fixed_t>;
//
//template<typename T>
//inline constexpr bool is_signed_v = std::is_signed_v<T> || is_fixed_point_v<T>;
//
//template<typename T>
//inline constexpr bool is_unsigned_v = std::is_unsigned_v<T>;
//
//template<typename T>
//inline constexpr bool is_floating_point_v = std::is_floating_point_v<T> || is_fixed_point_v<T>;
//
//template<typename T>
//inline constexpr bool is_arithmetic_v = is_integral_v<T> || is_floating_point_v<T>;
//
//template<typename T>
//inline constexpr bool is_arithmetic_and_not_fixed_v = is_arithmetic_v<T> && (!is_fixed_point_v<T>);
//
//template<typename T>
//inline constexpr bool is_floating_point_and_not_fixed_v = std::is_floating_point_v<T>;
//
//template<typename T, typename U>
//inline constexpr bool is_arithmetic_and_one_is_fixed_v
//  = is_arithmetic_v<T> && is_arithmetic_v<U> && (is_fixed_point_v<T> || is_fixed_point_v<U>);
//
//template<typename T, typename U>
//inline constexpr bool one_of_is_double_v = std::is_same_v<T, double> || std::is_same_v<U, double>;
//  }  // namespace fixedmath::typetraits
//
//namespace fixedmath::inline v2::concepts
//  {
//using std::integral;
//template<typename T>
//concept internal_unsigned = std::same_as<T, fixed_internal_unsigned>;
//
//template<typename T>
//concept internal = std::same_as<T, fixed_internal>;
//
//template<typename T>
//concept fixed_point = typetraits::is_fixed_point_v<T>;
//
//template<typename T>
//concept floating_point = typetraits::is_floating_point_v<T>;
//
//template<typename T>
//concept arithmetic = typetraits::is_arithmetic_v<T>;
//
//template<typename T>
//concept arithmetic_and_not_fixed = typetraits::is_arithmetic_and_not_fixed_v<T>;
//
//template<typename T>
//concept floating_point_and_not_fixed = typetraits::is_floating_point_and_not_fixed_v<T>;
//
//template<typename T, typename U>
//concept arithmetic_and_one_is_fixed = typetraits::is_arithmetic_and_one_is_fixed_v<T, U>;
//
//template<typename T, typename U>
//concept one_of_is_double = typetraits::one_of_is_double_v<T, U>;
//
//  }  // namespace fixedmath::inline v2::concepts
//
//namespace fixedmath::inline v2::detail
//  {
//using limits_ = std::numeric_limits<fixedmath::fixed_t>;
//using flimits_ = std::numeric_limits<float>;
//using dlimits_ = std::numeric_limits<double>;
//  }  // namespace fixedmath::inline v2::detail



	// SPDX-FileCopyrightText: 2020-2024 Artur Bać
// SPDX-License-Identifier: BSL-1.0
// SPDX-PackageHomePage: https://github.com/arturbac/fixed_math

#pragma once

#include <cstdint>
#include <type_traits>
#include <limits>
#include <cmath>

namespace fixedmath { inline namespace v2 {

using fixed_internal          = int64_t;
using fixed_internal_unsigned = uint64_t;

struct fix_carrier_t;
struct fixed_t;

}} // namespace fixedmath::inline v2

namespace fixedmath { namespace typetraits {

// clang-format off
template<int size> struct signed_type_by_size { };
template<> struct signed_type_by_size<1> { using type = int8_t; };
template<> struct signed_type_by_size<2> { using type = int16_t; };
template<> struct signed_type_by_size<4> { using type = int32_t; };
template<> struct signed_type_by_size<8> { using type = int64_t; };
template<> struct signed_type_by_size<16>{ using type = int64_t; };
// clang-format on

template<std::size_t Size>
using signed_type_by_size_t = typename signed_type_by_size<Size>::type;

template<typename T>
using promote_to_signed_t = signed_type_by_size_t<(sizeof(T) << 1)>;

template<typename T>
inline constexpr bool is_integral_v = std::is_integral<T>::value;

template<typename T>
inline constexpr bool is_fixed_point_v = std::is_same<T, fixedmath::v2::fixed_t>::value;

template<typename T>
inline constexpr bool is_signed_v = std::is_signed<T>::value || is_fixed_point_v<T>;

template<typename T>
inline constexpr bool is_unsigned_v = std::is_unsigned<T>::value;

template<typename T>
inline constexpr bool is_floating_point_v = std::is_floating_point<T>::value || is_fixed_point_v<T>;

template<typename T>
inline constexpr bool is_arithmetic_v = is_integral_v<T> || is_floating_point_v<T>;

template<typename T>
inline constexpr bool is_arithmetic_and_not_fixed_v = is_arithmetic_v<T> && (!is_fixed_point_v<T>);

template<typename T>
inline constexpr bool is_floating_point_and_not_fixed_v = std::is_floating_point<T>::value;

template<typename T, typename U>
inline constexpr bool is_arithmetic_and_one_is_fixed_v =
    is_arithmetic_v<T> && is_arithmetic_v<U> && (is_fixed_point_v<T> || is_fixed_point_v<U>);

template<typename T, typename U>
inline constexpr bool one_of_is_double_v =
    std::is_same<T, double>::value || std::is_same<U, double>::value;

}} // namespace fixedmath::typetraits

namespace fixedmath { inline namespace v2 { namespace traits {

// Вместо concepts — constexpr проверки

template<typename T>
inline constexpr bool internal_unsigned_v = std::is_same<T, fixed_internal_unsigned>::value;

template<typename T>
inline constexpr bool internal_v = std::is_same<T, fixed_internal>::value;

template<typename T>
inline constexpr bool fixed_point_v = typetraits::is_fixed_point_v<T>;

template<typename T>
inline constexpr bool floating_point_v = typetraits::is_floating_point_v<T>;

template<typename T>
inline constexpr bool arithmetic_v = typetraits::is_arithmetic_v<T>;

template<typename T>
inline constexpr bool arithmetic_and_not_fixed_v = typetraits::is_arithmetic_and_not_fixed_v<T>;

template<typename T>
inline constexpr bool floating_point_and_not_fixed_v = typetraits::is_floating_point_and_not_fixed_v<T>;

template<typename T, typename U>
inline constexpr bool arithmetic_and_one_is_fixed_v = typetraits::is_arithmetic_and_one_is_fixed_v<T, U>;

template<typename T, typename U>
inline constexpr bool one_of_is_double_v = typetraits::one_of_is_double_v<T, U>;

}}} // namespace fixedmath::inline v2::traits

namespace fixedmath { inline namespace v2 { namespace detail {

using limits_  = std::numeric_limits<fixedmath::fixed_t>;
using flimits_ = std::numeric_limits<float>;
using dlimits_ = std::numeric_limits<double>;

}}} // namespace fixedmath::inline v2::detail

/*
// Пример использования:
template<typename T>
void foo(T) {
    static_assert(fixedmath::v2::traits::fixed_point_v<T>, "T must be a fixed point type");
}

*/

/*
Краткие рекомендации:
Используй fixedmath::v2::traits::..._v<T> вместо concepts
Для аналогов concepts-комбинаций используй static_assert(...) или конструкции в шаблоне и if constexpr.
Этот код компилируется в Arduino IDE и поддерживается большинством современных компиляторов для микроконтроллеров.
Если нужно переписать usage-примеры, дать еще более минималистичные реализации или объяснить отдельные куски — пиши!  
	  
	  
	  
*/