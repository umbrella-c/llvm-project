//===-------------------------- debug.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#define _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS 1
#include "cxxabi.h"
#include "__config"
#include "__debug"

_LIBCPP_BEGIN_NAMESPACE_STD


__c_node::~__c_node()
{
    free(beg_);
    if (__next_)
    {
        __next_->~__c_node();
        free(__next_);
    }
}

_LIBCPP_END_NAMESPACE_STD
