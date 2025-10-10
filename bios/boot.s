    .intel_syntax noprefix
    .code16

.text
.global _start
_start:
    lea si, msg
    call print
end:
    hlt

.global print
print:
    push bp
    mov bp, sp
    
    mov ax, 0
    mov bx, 0
    mov dx, 0x3f8
print_loop:
    mov al, [si + bx] # only load on byte
    cmp ax, 0
    je print_exit
    out dx, ax
    inc bx
    jmp print_loop
print_exit:
    pop bp
    ret

.data
msg: .ascii "Hello world\n\0"
