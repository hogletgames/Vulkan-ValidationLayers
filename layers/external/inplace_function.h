// https://github.com/WG21-SG14/SG14/blob/master/SG14/inplace_function.h
// Doc: https://github.com/WG21-SG14/SG14/blob/master/Docs/Proposals/NonAllocatingStandardFunction.pdf

/*
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#ifndef SG14_INPLACE_FUNCTION_THROW
#define SG14_INPLACE_FUNCTION_THROW(x) throw(x)
#endif

namespace stdext {

namespace inplace_function_detail {

static constexpr size_t InplaceFunctionDefaultCapacity = 32;

template <class T>
struct wrapper {
    using type = T;
};

template <class R, class... Args>
struct vtable {
    using storage_ptr_t = void*;

    using invoke_ptr_t = R (*)(storage_ptr_t, Args&&...);
    using process_ptr_t = void (*)(storage_ptr_t, storage_ptr_t);
    using destructor_ptr_t = void (*)(storage_ptr_t);

    const invoke_ptr_t invoke_ptr;
    const process_ptr_t copy_ptr;
    const process_ptr_t relocate_ptr;
    const destructor_ptr_t destructor_ptr;

    explicit constexpr vtable() noexcept
        : invoke_ptr{[](storage_ptr_t, Args&&...) -> R {
              // Todo: we have exceptions disabled, so calling "throw" is not an option.
              // Returning R() is a hack, R must have a default constructor for this to works.
              // As of writing, it is ok to do this.
              /*SG14_INPLACE_FUNCTION_THROW(std::bad_function_call());*/
              return R();
          }},
          copy_ptr{[](storage_ptr_t, storage_ptr_t) -> void {}},
          relocate_ptr{[](storage_ptr_t, storage_ptr_t) -> void {}},
          destructor_ptr{[](storage_ptr_t) -> void {}} {}

    template <class C>
    explicit constexpr vtable(wrapper<C>) noexcept
        : invoke_ptr{[](storage_ptr_t storage_ptr, Args&&... args) -> R {
              return (*static_cast<C*>(storage_ptr))(static_cast<Args&&>(args)...);
          }},
          copy_ptr{[](storage_ptr_t dst_ptr, storage_ptr_t src_ptr) -> void { ::new (dst_ptr) C{(*static_cast<C*>(src_ptr))}; }},
          relocate_ptr{[](storage_ptr_t dst_ptr, storage_ptr_t src_ptr) -> void {
              ::new (dst_ptr) C{std::move(*static_cast<C*>(src_ptr))};
              static_cast<C*>(src_ptr)->~C();
          }},
          destructor_ptr{[](storage_ptr_t src_ptr) -> void { static_cast<C*>(src_ptr)->~C(); }} {}

    vtable(const vtable&) = delete;
    vtable(vtable&&) = delete;

    vtable& operator=(const vtable&) = delete;
    vtable& operator=(vtable&&) = delete;

    ~vtable() = default;
};

template <class R, class... Args>
#if __cplusplus >= 201703L
inline constexpr
#endif
    vtable<R, Args...>
        empty_vtable{};

template <size_t DstCap, size_t DstAlign, size_t SrcCap, size_t SrcAlign>
struct is_valid_inplace_dst : std::true_type {
    static_assert(DstCap >= SrcCap, "Can't squeeze larger inplace_function into a smaller one");

    static_assert(DstAlign % SrcAlign == 0, "Incompatible inplace_function alignments");
};

// C++11 MSVC compatible implementation of std::is_invocable_r.

template <class R>
void accept(R);

template <class, class R, class F, class... Args>
struct is_invocable_r_impl : std::false_type {};

template <class F, class... Args>
struct is_invocable_r_impl<decltype(std::declval<F>()(std::declval<Args>()...), void()), void, F, Args...> : std::true_type {};

template <class F, class... Args>
struct is_invocable_r_impl<decltype(std::declval<F>()(std::declval<Args>()...), void()), const void, F, Args...> : std::true_type {
};

template <class R, class F, class... Args>
struct is_invocable_r_impl<decltype(accept<R>(std::declval<F>()(std::declval<Args>()...))), R, F, Args...> : std::true_type {};

template <class R, class F, class... Args>
using is_invocable_r = is_invocable_r_impl<void, R, F, Args...>;

// Note about default alignment: This class used to employ
// std::aligned_storage<Capacity>, which is now deprecated and got removed. To
// avoid behavior changes, the new implementation mimics the default alignment
// of std::aligned_storage, as per the link below.
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/libc++/src/include/__type_traits/aligned_storage.h;l=49;drc=66b494f0101bb862e9e7b034f18645af4b1dd080
constexpr std::size_t GetDefaultAlignment(std::size_t capacity) {
    struct struct_double {
        long double lx;
    };
    struct struct_double4 {
        double lx[4];
    };
    std::size_t alignments[] = {alignof(unsigned char),      alignof(unsigned short), alignof(unsigned int), alignof(unsigned long),
                                alignof(unsigned long long), alignof(double),         alignof(long double),  alignof(int*),
                                alignof(struct_double),      alignof(struct_double4)};
    std::size_t max_alignment_within_capacity = 0;
    for (std::size_t alignment : alignments) {
        if (alignment <= capacity) {
            max_alignment_within_capacity = std::max(max_alignment_within_capacity, alignment);
        }
    }
    // The caller ensures this is non-zero via static_assert(), it's not possible to do it here.
    return max_alignment_within_capacity;
}

}  // namespace inplace_function_detail

template <class Signature, size_t Capacity = inplace_function_detail::InplaceFunctionDefaultCapacity,
          size_t Alignment = inplace_function_detail::GetDefaultAlignment(Capacity)>
class inplace_function;  // unspecified

namespace inplace_function_detail {
template <class>
struct is_inplace_function : std::false_type {};
template <class Sig, size_t Cap, size_t Align>
struct is_inplace_function<inplace_function<Sig, Cap, Align>> : std::true_type {};
}  // namespace inplace_function_detail

template <class R, class... Args, size_t Capacity, size_t Alignment>
class inplace_function<R(Args...), Capacity, Alignment> {
    static_assert(Alignment > 0);
    using vtable_t = inplace_function_detail::vtable<R, Args...>;
    using vtable_ptr_t = const vtable_t*;

    template <class, size_t, size_t>
    friend class inplace_function;

  public:
    using capacity = std::integral_constant<size_t, Capacity>;
    using alignment = std::integral_constant<size_t, Alignment>;

    inplace_function() noexcept : vtable_ptr_{std::addressof(inplace_function_detail::empty_vtable<R, Args...>)} {}

    template <class T, class C = std::decay_t<T>,
              class = std::enable_if_t<!inplace_function_detail::is_inplace_function<C>::value &&
                                       inplace_function_detail::is_invocable_r<R, C&, Args...>::value>>
    inplace_function(T&& closure) {
        static_assert(std::is_copy_constructible<C>::value, "inplace_function cannot be constructed from non-copyable type");

        static_assert(sizeof(C) <= Capacity, "inplace_function cannot be constructed from object with this (large) size");

        static_assert(Alignment % alignof(C) == 0,
                      "inplace_function cannot be constructed from object with this (large) alignment");

        static const vtable_t vt{inplace_function_detail::wrapper<C>{}};
        vtable_ptr_ = std::addressof(vt);

        ::new (storage_) C{std::forward<T>(closure)};
    }

    template <size_t Cap, size_t Align>
    inplace_function(const inplace_function<R(Args...), Cap, Align>& other)
        : inplace_function(other.vtable_ptr_, other.vtable_ptr_->copy_ptr, other.storage_) {
        static_assert(inplace_function_detail::is_valid_inplace_dst<Capacity, Alignment, Cap, Align>::value,
                      "conversion not allowed");
    }

    template <size_t Cap, size_t Align>
    inplace_function(inplace_function<R(Args...), Cap, Align>&& other) noexcept
        : inplace_function(other.vtable_ptr_, other.vtable_ptr_->relocate_ptr, other.storage_) {
        static_assert(inplace_function_detail::is_valid_inplace_dst<Capacity, Alignment, Cap, Align>::value,
                      "conversion not allowed");

        other.vtable_ptr_ = std::addressof(inplace_function_detail::empty_vtable<R, Args...>);
    }

    inplace_function(std::nullptr_t) noexcept : vtable_ptr_{std::addressof(inplace_function_detail::empty_vtable<R, Args...>)} {}

    inplace_function(const inplace_function& other) : vtable_ptr_{other.vtable_ptr_} {
        vtable_ptr_->copy_ptr(storage_, other.storage_);
    }

    inplace_function(inplace_function&& other) noexcept
        : vtable_ptr_{std::exchange(other.vtable_ptr_, std::addressof(inplace_function_detail::empty_vtable<R, Args...>))} {
        vtable_ptr_->relocate_ptr(storage_, other.storage_);
    }

    inplace_function& operator=(std::nullptr_t) noexcept {
        vtable_ptr_->destructor_ptr(storage_);
        vtable_ptr_ = std::addressof(inplace_function_detail::empty_vtable<R, Args...>);
        return *this;
    }

    inplace_function& operator=(inplace_function other) noexcept {
        vtable_ptr_->destructor_ptr(storage_);

        vtable_ptr_ = std::exchange(other.vtable_ptr_, std::addressof(inplace_function_detail::empty_vtable<R, Args...>));
        vtable_ptr_->relocate_ptr(storage_, other.storage_);
        return *this;
    }

    ~inplace_function() { vtable_ptr_->destructor_ptr(storage_); }

    R operator()(Args... args) const { return vtable_ptr_->invoke_ptr(storage_, std::forward<Args>(args)...); }

    constexpr bool operator==(std::nullptr_t) const noexcept { return !operator bool(); }

    constexpr bool operator!=(std::nullptr_t) const noexcept { return operator bool(); }

    explicit constexpr operator bool() const noexcept {
        return vtable_ptr_ != std::addressof(inplace_function_detail::empty_vtable<R, Args...>);
    }

    void swap(inplace_function& other) noexcept {
        if (this == std::addressof(other)) return;

        alignas(Alignment) std::byte tmp[Capacity];
        vtable_ptr_->relocate_ptr(tmp, storage_);

        other.vtable_ptr_->relocate_ptr(storage_, other.storage_);

        vtable_ptr_->relocate_ptr(other.storage_, tmp);

        std::swap(vtable_ptr_, other.vtable_ptr_);
    }

    friend void swap(inplace_function& lhs, inplace_function& rhs) noexcept { lhs.swap(rhs); }

  private:
    vtable_ptr_t vtable_ptr_;
    alignas(Alignment) mutable std::byte storage_[Capacity];

    inplace_function(vtable_ptr_t vtable_ptr, typename vtable_t::process_ptr_t process_ptr,
                     typename vtable_t::storage_ptr_t storage_ptr)
        : vtable_ptr_{vtable_ptr} {
        process_ptr(storage_, storage_ptr);
    }
};

}  // namespace stdext
