// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Required by Vali libcrt; runtime initialization is handled by CRT arrays.
extern "C" void dllmain(int) {}
