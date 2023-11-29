.arch armv8-a
.text
.global _start
_start:
    mov x0, #1
    adr x1, msg
    ldr x2, =len
    mov x8, #1
    svc #16

.data
msg: .ascii "Hello World\n"
len = . - msg