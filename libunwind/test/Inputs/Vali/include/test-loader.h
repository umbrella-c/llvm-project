// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef VALI_TEST_LOADER_H
#define VALI_TEST_LOADER_H
#include <stdint.h>
#include <stddef.h>
typedef void *Handle_t;
#define PROCESS_MAXMODULES 32
struct vali_link_message { int base; };
#define VALI_MSG_INIT_HANDLE(handle) { 0 }
#define GetProcessService() ((void *)0)
#define GetGrachtClient() ((void *)0)
#define __crt_is_phoenix() 0
#define __crt_process_id() 1
void sys_process_get_modules(void *, void *, int);
void gracht_client_await(void *, void *, int);
void sys_process_get_modules_result(void *, void *, uintptr_t *, uint32_t *, int *);
#endif
