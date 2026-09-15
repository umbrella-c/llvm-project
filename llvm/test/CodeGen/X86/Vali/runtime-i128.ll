; RUN: llc -mtriple=x86_64-uml-vali %s -o - | FileCheck %s
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t.obj
; RUN: llvm-readobj --relocations %t.obj | FileCheck %s --check-prefix=OBJ
;
; Vali uses the Win64 C ABI for compiler-rt's __int128 helpers: indirect
; integer arguments and an XMM0 integer result, without Windows SEH.

; CHECK-LABEL: sdiv128:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: leaq {{[0-9]+}}(%rsp), %rdx
; CHECK: callq __divti3
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __divti3
define i64 @sdiv128(ptr %a, ptr %b) {
  %x = load i128, ptr %a, align 16
  %y = load i128, ptr %b, align 16
  %v = sdiv i128 %x, %y
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: udiv128:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: leaq {{[0-9]+}}(%rsp), %rdx
; CHECK: callq __udivti3
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __udivti3
define i64 @udiv128(ptr %a, ptr %b) {
  %x = load i128, ptr %a, align 16
  %y = load i128, ptr %b, align 16
  %v = udiv i128 %x, %y
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: srem128:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: leaq {{[0-9]+}}(%rsp), %rdx
; CHECK: callq __modti3
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __modti3
define i64 @srem128(ptr %a, ptr %b) {
  %x = load i128, ptr %a, align 16
  %y = load i128, ptr %b, align 16
  %v = srem i128 %x, %y
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: urem128:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: leaq {{[0-9]+}}(%rsp), %rdx
; CHECK: callq __umodti3
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __umodti3
define i64 @urem128(ptr %a, ptr %b) {
  %x = load i128, ptr %a, align 16
  %y = load i128, ptr %b, align 16
  %v = urem i128 %x, %y
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: fptosi_float:
; CHECK: callq __fixsfti
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __fixsfti
define i64 @fptosi_float(float %a) {
  %v = fptosi float %a to i128
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: fptoui_float:
; CHECK: callq __fixunssfti
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __fixunssfti
define i64 @fptoui_float(float %a) {
  %v = fptoui float %a to i128
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: sitofp_float:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: callq __floattisf
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __floattisf
define float @sitofp_float(ptr %a) {
  %x = load i128, ptr %a, align 16
  %r = sitofp i128 %x to float
  ret float %r
}

; CHECK-LABEL: uitofp_float:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: callq __floatuntisf
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __floatuntisf
define float @uitofp_float(ptr %a) {
  %x = load i128, ptr %a, align 16
  %r = uitofp i128 %x to float
  ret float %r
}

; CHECK-LABEL: fptosi_double:
; CHECK: callq __fixdfti
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __fixdfti
define i64 @fptosi_double(double %a) {
  %v = fptosi double %a to i128
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: fptoui_double:
; CHECK: callq __fixunsdfti
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __fixunsdfti
define i64 @fptoui_double(double %a) {
  %v = fptoui double %a to i128
  %r = trunc i128 %v to i64
  ret i64 %r
}

; CHECK-LABEL: sitofp_double:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: callq __floattidf
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __floattidf
define double @sitofp_double(ptr %a) {
  %x = load i128, ptr %a, align 16
  %r = sitofp i128 %x to double
  ret double %r
}

; CHECK-LABEL: uitofp_double:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: callq __floatuntidf
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __floatuntidf
define double @uitofp_double(ptr %a) {
  %x = load i128, ptr %a, align 16
  %r = uitofp i128 %x to double
  ret double %r
}

; CHECK-LABEL: strict_fptosi:
; CHECK: callq __fixdfti
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __fixdfti
define i64 @strict_fptosi(double %a) strictfp {
  %v = call i128 @llvm.experimental.constrained.fptosi.i128.f64(double %a, metadata !"fpexcept.strict")
  %r = trunc i128 %v to i64
  ret i64 %r
}
declare i128 @llvm.experimental.constrained.fptosi.i128.f64(double, metadata)

; CHECK-LABEL: strict_fptoui:
; CHECK: callq __fixunsdfti
; CHECK: movq %xmm0, %rax
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __fixunsdfti
define i64 @strict_fptoui(double %a) strictfp {
  %v = call i128 @llvm.experimental.constrained.fptoui.i128.f64(double %a, metadata !"fpexcept.strict")
  %r = trunc i128 %v to i64
  ret i64 %r
}
declare i128 @llvm.experimental.constrained.fptoui.i128.f64(double, metadata)

; CHECK-LABEL: strict_sitofp:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: callq __floattidf
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __floattidf
define double @strict_sitofp(ptr %a) strictfp {
  %x = load i128, ptr %a, align 16
  %r = call double @llvm.experimental.constrained.sitofp.f64.i128(i128 %x, metadata !"round.dynamic", metadata !"fpexcept.strict")
  ret double %r
}
declare double @llvm.experimental.constrained.sitofp.f64.i128(i128, metadata, metadata)

; CHECK-LABEL: strict_uitofp:
; CHECK: leaq {{[0-9]+}}(%rsp), %rcx
; CHECK: callq __floatuntidf
; CHECK: retq
; OBJ: IMAGE_REL_AMD64_REL32 __floatuntidf
define double @strict_uitofp(ptr %a) strictfp {
  %x = load i128, ptr %a, align 16
  %r = call double @llvm.experimental.constrained.uitofp.f64.i128(i128 %x, metadata !"round.dynamic", metadata !"fpexcept.strict")
  ret double %r
}
declare double @llvm.experimental.constrained.uitofp.f64.i128(i128, metadata, metadata)
