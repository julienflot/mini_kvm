.globl _start
    .code32

_start:
    xor %eax, %eax # set ax to 0

loop:
    out %eax, $0x10
    inc %eax
    jmp loop
