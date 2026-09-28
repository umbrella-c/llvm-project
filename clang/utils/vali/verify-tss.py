#!/usr/bin/env python3
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Run Vali's TSS and hashtable with concurrent host workers and sanitizers.

Only platform types, logical identity and locks are host adapters. This tests
container lifetime/concurrency; it does not qualify Vali's kernel or scheduler.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--vali', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
shims = {
    'threads.h': '''#pragma once
#include <limits.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
typedef unsigned thrd_t;
typedef unsigned tss_t;
typedef void (*tss_dtor_t)(void*);
#define TSS_DTOR_ITERATIONS 4
#define TSS_KEY_INVALID UINT_MAX
#define UUID_INVALID 0
#define EOK 0
#define thrd_success 0
#define thrd_error -1
#define thrd_nomem 3
#define _In_
#define _CODE_BEGIN
#define _CODE_END
#define CRTDECL(T,F) T F
int tss_create(tss_t*, tss_dtor_t);
void tss_delete(tss_t);
void *tss_get(tss_t);
int tss_set(tss_t, void*);
''',
    'internal/_tls.h': 'void *__tls_current(void);\n',
    'internal/_utils.h': 'unsigned __crt_thread_id(void);\n',
    'ddk/utils.h': '#define TRACE(...)\n',
    'os/spinlock.h': '''#pragma once
#include <pthread.h>
#include <assert.h>
typedef pthread_mutex_t spinlock_t;
#define _SPN_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER
static inline void spinlock_acquire(spinlock_t* l) { assert(!pthread_mutex_lock(l)); }
static inline void spinlock_release(spinlock_t* l) { assert(!pthread_mutex_unlock(l)); }
''',
    'ds/ds.h': '''#include <stdlib.h>
#define dsalloc malloc
#define dsfree free
#define dstrace(...)
''',
}
with tempfile.TemporaryDirectory(prefix='vali-tss-') as tmp:
    tmp = Path(tmp)
    for name, data in shims.items():
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data)
    binary = tmp / 'tss'
    subprocess.run([os.environ.get('CC', 'clang'), '-std=gnu11', '-D_XOPEN_SOURCE=700',
                    '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-pthread', '-I' + str(tmp), '-I' + str(a.vali / 'librt/libds/include'),
                    str(a.vali / 'librt/libc/threads/tss.c'),
                    str(a.vali / 'librt/libds/hashtable.c'),
                    str(root / 'clang/test/CodeGen/Vali/Inputs/tls/tss-host.c'),
                    '-o', str(binary)], check=True)
    env = dict(os.environ)
    env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
    subprocess.run([binary], env=env, check=True, timeout=30)
print('PASS concurrent Vali TSS isolation, reentrant destructors and key lifetime (ASan/UBSan)')
