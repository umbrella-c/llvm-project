//=== llvm/Support/Vali/ThreadLocal.inc - Vali Thread Local Data -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Vali specific ThreadLocal class.
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <threads.h>
#include <stdlib.h>

namespace llvm {
using namespace sys;

ThreadLocalImpl::ThreadLocalImpl() : data() {
  static_assert(sizeof(tss_t) <= sizeof(data), "size too big");
  tss_t* key = reinterpret_cast<tss_t*>(&data);
  int errorcode = tss_create(key, nullptr);
  assert(errorcode == thrd_success);
  (void)errorcode;
}

ThreadLocalImpl::~ThreadLocalImpl() {
  tss_t* key = reinterpret_cast<tss_t*>(&data);
  tss_delete(*key);
}

void ThreadLocalImpl::setInstance(const void* d) {
  tss_t* key = reinterpret_cast<tss_t*>(&data);
  int errorcode = tss_set(*key, const_cast<void*>(d));
  assert(errorcode == thrd_success);
  (void)errorcode;
}

void *ThreadLocalImpl::getInstance() {
  tss_t* key = reinterpret_cast<tss_t*>(&data);
  return tss_get(*key);
}

void ThreadLocalImpl::removeInstance() {
  setInstance(nullptr);
}

}
