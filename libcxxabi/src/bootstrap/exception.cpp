// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#define _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS 1
#include "cxxabi.h"
#include <exception>

namespace std {

bool uncaught_exception() _NOEXCEPT { return uncaught_exceptions() > 0; }

int uncaught_exceptions() _NOEXCEPT
{
# if _LIBCPPABI_VERSION > 1001
    return (int)__cxxabiv1::__cxa_uncaught_exceptions();
# else
    return __cxxabiv1::__cxa_uncaught_exception() ? 1 : 0;
# endif
}

} // namespace std
