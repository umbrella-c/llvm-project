//===------------------------ memory.cpp ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#define _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS 1
#include "cxxabi.h"
#include "memory"

_LIBCPP_BEGIN_NAMESPACE_STD

enum __libcpp_atomic_order {
    _AO_Relaxed = __ATOMIC_RELAXED,
    _AO_Consume = __ATOMIC_CONSUME,
    _AO_Acquire = __ATOMIC_ACQUIRE,
    _AO_Release = __ATOMIC_RELEASE,
    _AO_Acq_Rel = __ATOMIC_ACQ_REL,
    _AO_Seq     = __ATOMIC_SEQ_CST
};

template <class _ValueType>
inline _LIBCPP_INLINE_VISIBILITY
_ValueType __libcpp_atomic_load(_ValueType const* __val,
                                int = 0)
{
    return *__val;
}

template <class _ValueType>
inline _LIBCPP_INLINE_VISIBILITY
bool __libcpp_atomic_compare_exchange(_ValueType* __val,
    _ValueType* __expected, _ValueType __after,
    int = 0, int = 0)
{
    if (*__val == *__expected) {
        *__val = __after;
        return true;
    }
    *__expected = *__val;
    return false;
}

bad_weak_ptr::~bad_weak_ptr() _NOEXCEPT {}

const char*
bad_weak_ptr::what() const _NOEXCEPT
{
    return "bad_weak_ptr";
}

__shared_count::~__shared_count()
{
}

__shared_weak_count::~__shared_weak_count()
{
}

const void*
__shared_weak_count::__get_deleter(const type_info&) const _NOEXCEPT
{
    return nullptr;
}

void
__shared_weak_count::__release_weak() _NOEXCEPT
{
    // NOTE: The acquire load here is an optimization of the very
    // common case where a shared pointer is being destructed while
    // having no other contended references.
    //
    // BENEFIT: We avoid expensive atomic stores like XADD and STREX
    // in a common case.  Those instructions are slow and do nasty
    // things to caches.
    //
    // IS THIS SAFE?  Yes.  During weak destruction, if we see that we
    // are the last reference, we know that no-one else is accessing
    // us. If someone were accessing us, then they would be doing so
    // while the last shared / weak_ptr was being destructed, and
    // that's undefined anyway.
    //
    // If we see anything other than a 0, then we have possible
    // contention, and need to use an atomicrmw primitive.
    // The same arguments don't apply for increment, where it is legal
    // (though inadvisable) to share shared_ptr references between
    // threads, and have them all get copied at once.  The argument
    // also doesn't apply for __release_shared, because an outstanding
    // weak_ptr::lock() could read / modify the shared count.
    if (__libcpp_atomic_load(&__shared_weak_owners_, _AO_Acquire) == 0)
    {
        // no need to do this store, because we are about
        // to destroy everything.
        //__libcpp_atomic_store(&__shared_weak_owners_, -1, _AO_Release);
        __on_zero_shared_weak();
    }
    else if (__libcpp_atomic_refcount_decrement(__shared_weak_owners_) == -1)
        __on_zero_shared_weak();
}

__shared_weak_count*
__shared_weak_count::lock() _NOEXCEPT
{
    long object_owners = __libcpp_atomic_load(&__shared_owners_);
    while (object_owners != -1)
    {
        if (__libcpp_atomic_compare_exchange(&__shared_owners_,
                                             &object_owners,
                                             object_owners+1))
            return this;
    }
    return nullptr;
}

_LIBCPP_END_NAMESPACE_STD
