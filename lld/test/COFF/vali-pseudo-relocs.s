# REQUIRES: x86
# RUN: split-file %s %t.dir
# RUN: llvm-mc -triple=x86_64-windows-gnu -filetype=obj %t.dir/lib.s -o %t.lib64.obj
# RUN: lld-link /lldvpe /dll /noentry /export:variable,data /out:%t.lib64.dll /implib:%t.lib64.lib %t.lib64.obj
# RUN: llvm-mc -triple=x86_64-windows-gnu -filetype=obj %t.dir/main.s -o %t.main64.obj
# RUN: lld-link /lldvpe /out:%t.exe %t.main64.obj %t.lib64.lib
# RUN: %python %S/Inputs/vali-verify-image.py pseudo %t.exe
# RUN: llvm-mc -triple=x86_64-windows-gnu -defsym nolist=1 -filetype=obj %t.dir/main.s -o %t.nolist.obj
# RUN: not lld-link /lldvpe /runtime-pseudo-reloc:no /out:%t.exe %t.nolist.obj %t.lib64.lib 2>&1 | FileCheck %s --check-prefix=NO-RELOCS
# RUN: not lld-link /lldvpe /auto-import:no /out:%t.exe %t.main64.obj %t.lib64.lib 2>&1 | FileCheck %s --check-prefix=NO-IMPORT

# RUN: llvm-mc -triple=i686-windows-gnu -defsym i386=1 -filetype=obj %t.dir/lib.s -o %t.lib32.obj
# RUN: lld-link /lldvpe /dll /noentry /export:variable,data /out:%t.lib32.dll /implib:%t.lib32.lib %t.lib32.obj
# RUN: llvm-mc -triple=i686-windows-gnu -defsym i386=1 -filetype=obj %t.dir/main.s -o %t.main32.obj
# RUN: lld-link /lldvpe /out:%t.exe %t.main32.obj %t.lib32.lib
# RUN: %python %S/Inputs/vali-verify-image.py pseudo %t.exe

## Windows/MinGW output must not acquire Vali's directory, even when it has
## exactly the same pseudo-relocation list.
# RUN: llvm-mc -triple=x86_64-windows-gnu -filetype=obj %t.dir/mingw.s -o %t.mingw.obj
# RUN: lld-link /lldmingw /entry:__CrtConsoleEntry /out:%t.exe %t.main64.obj %t.lib64.lib %t.mingw.obj
# RUN: %python %S/Inputs/vali-verify-image.py no-directory %t.exe

# NO-RELOCS: requires pseudo relocations
# NO-IMPORT: error: {{(undefined symbol|relocation against symbol in discarded section)}}: variable

#--- lib.s
  .data
.ifdef i386
  .globl _variable
_variable:
.else
  .globl variable
variable:
.endif
  .long 42

#--- main.s
  .text
.ifdef i386
  .globl ___CrtConsoleEntry
___CrtConsoleEntry:
  movl _variable, %eax
  ret
  .data
  .long _variable
  .long ___RUNTIME_PSEUDO_RELOC_LIST__
  .long ___RUNTIME_PSEUDO_RELOC_LIST_END__
.else
  .globl __CrtConsoleEntry
__CrtConsoleEntry:
  movl variable(%rip), %eax
  ret
  .data
  .quad variable
.ifndef nolist
  .quad __RUNTIME_PSEUDO_RELOC_LIST__
  .quad __RUNTIME_PSEUDO_RELOC_LIST_END__
.endif
.endif

#--- mingw.s
  .text
  .globl _pei386_runtime_relocator
_pei386_runtime_relocator:
  ret
