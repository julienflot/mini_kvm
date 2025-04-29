#include "debug.h"
#include "logger.h"

#include <linux/kvm.h>

void dump_regs(const struct kvm_regs *regs) {

    TRACE(
        "rax 0x%llx, rbx 0x%llx, rcx 0x%llx, rdx 0x%llx,", regs->rax, regs->rbx, regs->rcx,
        regs->rdx);
    TRACE(
        "rsi 0x%llx, rdi 0x%llx, rsp 0x%llx, rbp 0x%llx,", regs->rsi, regs->rdi, regs->rsp,
        regs->rbp);
    TRACE(
        "r8 0x%llx, r9 0x%llx, r10 0x%llx, r11 0x%llx,", regs->r8, regs->r9, regs->r10, regs->r11);
    TRACE(
        "r12 0x%llx, r13 0x%llx, r14 0x%llx, r15 0x%llx,", regs->r12, regs->r13, regs->r14,
        regs->r15);
    TRACE("rip 0x%llx, rflags 0x%llx", regs->rip, regs->rflags);
}

// TODO: add dumpd for segments or others fields
void dump_sregs(const struct kvm_sregs *sregs) {
    TRACE(
        "cr0 0x%llx, cr2 0x%llx, cr3 0x%llx, cr4 0x%llx, cr8 0x%llx,", sregs->cr0, sregs->cr2,
        sregs->cr3, sregs->cr4, sregs->cr8);
}
