// Template base must match the origin of the COFF section-relative offsets.
.section .tls,"dw"
.globl template_start
template_start:
.section .tls$ZZZ,"dw"
.globl template_end
template_end:
