# REQUIRES: x86-registered-target
# RUN: llvm-mc -triple i386-uml-vali -filetype=obj -dwarf-version=4 %s -o %t32.o
# RUN: llvm-mc -triple x86_64-uml-vali -defsym=X64=1 -filetype=obj -dwarf-version=4 %s -o %t64.o
# RUN: llvm-readobj --file-headers --sections --symbols --relocations %t32.o | FileCheck %s --check-prefixes=COMMON,X32
# RUN: llvm-readobj --file-headers --sections --symbols --relocations %t64.o | FileCheck %s --check-prefixes=COMMON,X64
# RUN: llvm-dwarfdump --verify %t32.o
# RUN: llvm-dwarfdump --verify %t64.o
# RUN: llvm-dwarfdump --eh-frame %t32.o | FileCheck %s --check-prefix=CFI32
# RUN: llvm-dwarfdump --eh-frame %t64.o | FileCheck %s --check-prefix=CFI64

# RUN: llvm-mc -triple x86_64-uml-vali -defsym=X64=1 -filetype=asm -dwarf-version=4 %s -o %t.s
# RUN: llvm-mc -triple x86_64-uml-vali -filetype=obj -dwarf-version=4 %t.s -o %t-roundtrip.o
# RUN: llvm-readobj --file-headers --sections --symbols --relocations %t-roundtrip.o | FileCheck %s --check-prefixes=COMMON,X64

# VPE uses COFF relocatable encoding and DWARF CFI on both x86 variants.
.file 1 "vali-object.c"
.text
.globl entry
.def entry; .scl 2; .type 32; .endef
entry:
.cfi_startproc
.loc 1 1 0
call external
ret
.cfi_endproc

.data
.p2align 3
.globl pointer
pointer:
.ifdef X64
.quad external
.else
.long external
.endif
.rva entry
.secidx entry
.secrel32 entry
.weak weak_symbol
.long weak_symbol
.comm common_symbol,16,4
.lcomm local_common,12,8

.section .text$inline,"xr",discard,inline
.globl inline
inline:
ret
.section .rdata$inline,"dr",associative,inline
.long inline
.section .gcc_except_table,"dr"
.long 0

# X32: Format: COFF-i386
# X64: Format: COFF-x86-64
# COMMON: Name: .text
# COMMON: Name: .gcc_except_table
# COMMON: Name: .eh_frame
# COMMON: IMAGE_SCN_MEM_WRITE
# COMMON: Name: .debug_line
# COMMON: IMAGE_SCN_MEM_DISCARDABLE
# X32: IMAGE_REL_I386_REL32 external
# X64: IMAGE_REL_AMD64_REL32 external
# X32: IMAGE_REL_I386_DIR32 external
# X64: IMAGE_REL_AMD64_ADDR64 external
# X32: IMAGE_REL_I386_DIR32NB entry
# X64: IMAGE_REL_AMD64_ADDR32NB entry
# X32: IMAGE_REL_I386_SECTION entry
# X64: IMAGE_REL_AMD64_SECTION entry
# X32: IMAGE_REL_I386_SECREL entry
# X64: IMAGE_REL_AMD64_SECREL entry
# COMMON: Selection: Any
# COMMON: Selection: Associative
# COMMON: Name: weak_symbol
# COMMON: StorageClass: WeakExternal
# COMMON: Name: common_symbol
# COMMON: Value: 16
# CFI32: Data alignment factor: -4
# CFI32: Return address column: 8
# CFI32: FDE
# CFI64: Data alignment factor: -4
# CFI64: Return address column: 16
# CFI64: FDE
