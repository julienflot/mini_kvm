.intel_syntax noprefix
.code16

.text

.globl _start
_start:
    mov bx, 0 
loop:
    mov ax, [msg + bx]
    cmp ax, 0
    je end
    out 0x10, ax
    inc bx
    jmp loop

end:
    hlt

.data
msg: .ascii "Hello world\n\0"
