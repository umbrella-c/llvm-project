//===----------------------------------------------------------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
// REQUIRES: target={{.+-vali.*}}
// ADDITIONAL_COMPILE_FLAGS: -I %{libcxxabi}/src
#define _LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS
#include "private_typeinfo.h"

using Base = __cxxabiv1::__base_class_type_info;
static_assert(sizeof(decltype(Base::__offset_flags)) == sizeof(void*), "RTTI offset width");
static_assert(sizeof(Base) == 2 * sizeof(void*), "RTTI base descriptor stride");
static_assert(offsetof(Base, __offset_flags) == sizeof(void*), "RTTI flags offset");
constexpr Base negative = {nullptr, -6141}; // (-24 * 256) | public | virtual
static_assert((negative.__offset_flags >> Base::__offset_shift) == -24, "signed virtual offset");
#if __SIZEOF_POINTER__ == 8
constexpr Base large = {nullptr, (1LL << 40) | Base::__public_mask};
static_assert((large.__offset_flags >> Base::__offset_shift) == (1LL << 32), "no offset truncation");
#endif

#include "cxa_exception.h"
using Exception = __cxxabiv1::__cxa_exception;
using Dependent = __cxxabiv1::__cxa_dependent_exception;
static_assert(sizeof(Exception) == sizeof(Dependent), "exception header stride");
static_assert(offsetof(Exception, unwindHeader) == offsetof(Dependent, unwindHeader), "unwind header placement");
#if defined(__VALI64__)
static_assert(offsetof(Exception, referenceCount) == sizeof(void*), "64-bit exception reference count");
static_assert(offsetof(Dependent, primaryException) == sizeof(void*), "64-bit dependent exception pointer");
#endif
