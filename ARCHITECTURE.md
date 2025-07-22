# Mini KVM architecture

Mini is designed to run simple and lightweight Linux virtual machines. 
This project is a toy project that I used to learn about Operating systems, C and Linux programming, etc.
This file is here to remind me how did I organized the code structure and which design choice have made (and possibly why for avoiding looking at my code and say "why did I wrote that ... 30min later .. Oh that is why").


## Specifcations

This binary should be able to perform the following operations :
- boot a x86 Linux image with KVM.
- load a disk file.
- allocate N vcpus.
- optional logging of the internals.
- expose a monitor to inspect VM resources.
- change VM state : start, pause, resume, etc.
- output the serial monitor to the current terminal

## Files structure

- `main.c` : entry point, do the sub command parsing and pass CLI arguments to the sub command handler.
- `commands.h` : contains all function definitions for sub commands handling.
- `commands/run.{h,c}` : implementation of the run sub command.
- `kvm/kvm.{c, h}` : contains all function related to the KVM API (VM creation, VCPU setup and machine configuration).
- `utils/` : contains every utilities and misc functions for the project (errors definitions, logging, etc).

## Code path

> **TODO**
