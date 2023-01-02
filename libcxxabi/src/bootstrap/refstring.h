//===------------------------ __refstring ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_REFSTRING_H
#define _LIBCPP_REFSTRING_H

#include <__config>
#include <stdexcept>
#include <cstddef>
#include <cstring>

_LIBCPP_BEGIN_NAMESPACE_STD

enum __libcpp_atomic_order {
    _AO_Relaxed = __ATOMIC_RELAXED,
    _AO_Consume = __ATOMIC_CONSUME,
    _AO_Acquire = __ATOMIC_ACQUIRE,
    _AO_Release = __ATOMIC_RELEASE,
    _AO_Acq_Rel = __ATOMIC_ACQ_REL,
    _AO_Seq     = __ATOMIC_SEQ_CST
};

template <class _ValueType, class _AddType>
inline _LIBCPP_INLINE_VISIBILITY
_ValueType __libcpp_atomic_add(_ValueType* __val, _AddType __a,
                               int __order = _AO_Seq)
{
    return __atomic_add_fetch(__val, __a, __order);
}

namespace __refstring_imp { namespace {
typedef int count_t;

struct _Rep_base {
    std::size_t len;
    std::size_t cap;
    count_t     count;
};

inline _Rep_base* rep_from_data(const char *data_) noexcept {
    char *data = const_cast<char *>(data_);
    return reinterpret_cast<_Rep_base *>(data - sizeof(_Rep_base));
}

inline char * data_from_rep(_Rep_base *rep) noexcept {
    char *data = reinterpret_cast<char *>(rep);
    return data + sizeof(*rep);
}

}} // namespace __refstring_imp

using namespace __refstring_imp;

inline
__libcpp_refstring::__libcpp_refstring(const char* msg) {
    std::size_t len = strlen(msg);
    _Rep_base* rep = static_cast<_Rep_base *>(::operator new(sizeof(*rep) + len + 1));
    rep->len = len;
    rep->cap = len;
    rep->count = 0;
    char *data = data_from_rep(rep);
    std::memcpy(data, msg, len + 1);
    __imp_ = data;
}

inline
__libcpp_refstring::__libcpp_refstring(const __libcpp_refstring &s) _NOEXCEPT
    : __imp_(s.__imp_)
{
    if (__uses_refcount())
        __libcpp_atomic_add(&rep_from_data(__imp_)->count, 1);
}

inline
__libcpp_refstring& __libcpp_refstring::operator=(__libcpp_refstring const& s) _NOEXCEPT {
    bool adjust_old_count = __uses_refcount();
    struct _Rep_base *old_rep = rep_from_data(__imp_);
    __imp_ = s.__imp_;
    if (__uses_refcount())
        __libcpp_atomic_add(&rep_from_data(__imp_)->count, 1);
    if (adjust_old_count)
    {
        if (__libcpp_atomic_add(&old_rep->count, count_t(-1)) < 0)
        {
            ::operator delete(old_rep);
        }
    }
    return *this;
}

inline
__libcpp_refstring::~__libcpp_refstring() {
    if (__uses_refcount()) {
        _Rep_base* rep = rep_from_data(__imp_);
        if (__libcpp_atomic_add(&rep->count, count_t(-1)) < 0) {
            ::operator delete(rep);
        }
    }
}

inline
bool __libcpp_refstring::__uses_refcount() const {
    return true;
}

_LIBCPP_END_NAMESPACE_STD

#endif //_LIBCPP_REFSTRING_H
