#ifndef MINI_KVM_CONSTS_H
#define MINI_KVM_CONSTS_H

#define MINI_KVM_FS_ROOT_PATH "/tmp/mini_kvm"
#define MINI_KVM_MAX_VCPUS 64

#define PLM4_ADDR 0x1000
#define PAGE_SIZE 0x1000
#define MINIMUM_MEMORY_REQUIRED 0x5000
#define BOOTLOADER_ADDR 0x4000

// control and msr constants
#define CR0_PE 0x1
#define CR0_PG 0x80000000
#define CR4_PAE (1 << 5)
#define EFER_LME (1 << 8)
#define EFER_LMA (1 << 10)

// paging constants
#define PT_ADDR_MASK 0xffffffffff000
#define PT_RW (1 << 1)
#define PT_PRESENT (1 << 0)
#define PT_PAGE_SIZE (1 << 7)

#endif /*MINI_KVM_CONSTS_H*/
