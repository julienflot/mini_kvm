    .intel_syntax noprefix
    .code64

.text
.global _start
_start:
    lea rsi, msg
    call print
end:
    hlt

.global print
print:
    push rbp
    mov rbp, rsp
    
    mov rax, 0
    mov rbx, 0
    mov rdx, 0x3f8
print_loop:
    mov al, [rsi + rbx] # only load on byte
    cmp ax, 0
    je print_exit
    out dx, ax
    inc rbx
    jmp print_loop
print_exit:
    pop rbp
    ret

.data
msg: .ascii "Hello world\n\0"
