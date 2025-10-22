
#pragma once

#include "types.h"
#include "type_traits.h"
#include <limits>
#include <type_traits>
#include <bit>

namespace fixedmath {
    inline namespace v2 {
        namespace detail {

            // Левый сдвиг для signed
            template<int digits, typename T>
            constexpr
                typename std::enable_if<std::is_signed<T>::value, fixed_internal>::type
                unsigned_shift_left_signed(T value) noexcept {
                return static_cast<fixed_internal>(
                    (static_cast<fixed_internal_unsigned>(value) << digits)
                    | (static_cast<fixed_internal_unsigned>(value) & (fixed_internal_unsigned(1) << 63u))
                    );
            }

            // Левый сдвиг для unsigned
            template<int digits, typename T>
            constexpr
                typename std::enable_if<std::is_unsigned<T>::value, fixed_internal>::type
                unsigned_shift_left_unsigned(T value) noexcept {
                return static_cast<fixed_internal>(static_cast<fixed_internal_unsigned>(value) << digits);
            }

            // Promote типа к знаковому
            template<typename T>
            constexpr
                typename std::enable_if<std::is_integral<T>::value, typename typetraits::promote_to_signed_t<T>>::type
                promote_type_to_signed(T value) noexcept {
                typedef typename typetraits::promote_to_signed_t<T> signed_type;
                if (std::is_signed<T>::value)
                    return value;
                else
                    return static_cast<signed_type>(value);
            }

            // Наибольшая степень 4 (через clz)
            template<typename T>
            constexpr fixed_internal highest_pwr4_clz(T value) noexcept {
                static_assert(std::is_unsigned<T>::value, "T must be unsigned");
                if (value != 0) {
                    int clz = std::countl_zero(value);
                    clz = (64 - clz);
                    if ((clz & 1) == 0)
                        clz -= 1;
                    return fixed_internal(1ll << (clz - 1));
                }
                return 0;
            }

            // Наибольшая степень 4 (итеративно)
            template<typename T>
            constexpr fixed_internal highest_pwr4(T value) noexcept {
                static_assert(std::is_unsigned<T>::value, "T must be unsigned");
                fixed_internal_unsigned pwr4 = 1ll << 62;
                while (pwr4 > value)
                    pwr4 >>= 2;
                return fixed_internal(pwr4);
            }

            // Умножение fixed_internal (замедлив внутренний multiply)
            template<int precision>
            constexpr fixed_internal mul_(fixed_internal x, fixed_internal y) noexcept {
                return (x * y) >> precision;
            }

            // Деление fixed_internal
            template<int precision>
            constexpr fixed_internal div_(fixed_internal x, fixed_internal y) noexcept {
                return (x << precision) / y;
            }

            // Преобразование целого к fixed_internal
            template<int precision, typename T>
            constexpr fixed_internal fix_(T x) noexcept {
                static_assert(std::is_integral<T>::value, "T must be integral");
                return fixed_internal(x) << precision;
            }

            // Установка знака
            constexpr fixed_t set_sign(bool sign_, fixed_internal result) {
                if (!sign_)
                    return as_fixed(result);
                return as_fixed(-result);
            }

            // swap для арифметики
            template<typename T>
            inline
                typename std::enable_if<std::is_arithmetic<T>::value>::type
                swap(T& a, T& b) noexcept {
                T temp = a;
                a = b;
                b = temp;
            }

        }
    }
} // namespace fixedmath::inline v2::detail






//// SPDX-FileCopyrightText: 2020-2024 Artur Bać
//// SPDX-License-Identifier: BSL-1.0
//// SPDX-PackageHomePage: https://github.com/arturbac/fixed_math
//
//#pragma once
//
//#include "types.h"
//#include "type_traits.h"
//#include <bit>
//
//namespace fixedmath::inline v2::detail
//  {
//
//template<int digits>
//[[gnu::const, gnu::always_inline]]
//constexpr auto unsigned_shift_left_signed(std::signed_integral auto value) noexcept -> fixed_internal
//  {
//  return static_cast<fixed_internal>(
//    (static_cast<fixed_internal_unsigned>(value) << digits)
//    | (static_cast<fixed_internal_unsigned>(value) & (fixed_internal_unsigned(1) << 63u))
//  );
//  }
//
//template<int digits>
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto unsigned_shift_left_unsigned(std::unsigned_integral auto value) noexcept -> fixed_internal
//  {
//  return static_cast<fixed_internal>(static_cast<fixed_internal_unsigned>(value) << digits);
//  }
//
//template<std::integral value_type>
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto promote_type_to_signed(value_type value) noexcept
//  {
//  if constexpr(std::is_signed_v<value_type>)
//    return value;
//  else
//    {
//    using signed_type = typetraits::promote_to_signed_t<value_type>;
//    return static_cast<signed_type>(value);
//    }
//  }
//
//[[nodiscard, gnu::const, gnu::always_inline]]
/////\returns the highest power of 4 that is less than or equal to \ref value
//constexpr auto highest_pwr4_clz(concepts::internal_unsigned auto value) noexcept -> fixed_internal
//  {
//  if(value != 0) [[likely]]
//    {
//    int clz{std::countl_zero(value)};
//
//    clz = (64 - clz);
//    if((clz & 1) == 0)
//      clz -= 1;
//
//    return fixed_internal(1ll << (clz - 1));
//    }
//  return 0;
//  }
//
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto highest_pwr4(std::unsigned_integral auto value) noexcept -> fixed_internal
//  {
//  // one starts at the highest power of four <= than the argument.
//  fixed_internal_unsigned pwr4{1ll << 62};  // second-to-top bit set
//
//  while(pwr4 > value)
//    pwr4 >>= 2;
//  return fixed_internal(pwr4);
//  }
//
//template<int precision>
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto mul_(concepts::internal auto x, concepts::internal auto y) noexcept -> fixed_internal
//  {
//  return (x * y) >> precision;
//  }
//
//template<int precision>
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto div_(concepts::internal auto x, concepts::internal auto y) noexcept -> fixed_internal
//  {
//  return (x << precision) / y;
//  }
//
//template<int precision>
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto fix_(std::integral auto x) noexcept -> fixed_internal
//  {
//  return fixed_internal(x) << precision;
//  }
//
//[[nodiscard, gnu::const, gnu::always_inline]]
//constexpr auto set_sign(bool sign_, concepts::internal auto result) -> fixed_t
//  {
//  if(!sign_)
//    return as_fixed(result);
//  return as_fixed(-result);
//  }
//
//template<concepts::arithmetic T>
//constexpr void swap(T & a, T & b) noexcept
//  {
//  T temp = a;
//  a = b;
//  b = temp;
//  }
//  }  // namespace fixedmath::inline v2::detail
//
