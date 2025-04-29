#ifndef MINI_KVM_DEBUG_H
#define MINI_KVM_DEBUG_H

#include <linux/kvm.h>

void dump_regs(const struct kvm_regs *regs);
void dump_sregs(const struct kvm_sregs *sregs);

#endif /* MINI_KVM_DEBUG_H */
