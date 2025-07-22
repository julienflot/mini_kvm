# Mini KVM

## Build and Run

Just run in the root folder of the project to build the project
```sh
make 
```

## Invocations 

### `mini_kvm run`

```
--name name: set the name of the VM.
--disk path: path to the disk image.
--cpus number: number of vcpu to allocate.
--mem size: ram quantity allocated.
--log [path]: log internal inforamtion
```

### `mini_kvm pause`

```
name: name of the VM.
```

### `mini_kvm resume`

```
name: name of the VM.
```

### `mini_kvm show`

```
--regs: show registers.
--vcpus: show vcpus threads ID.
--state: show VM state (stopped, running, paused).
--stats: show VM stats.
```

# References :

- [KVM API Reference](https://www.kernel.org/doc/html/latest/virt/kvm/api.html)
- [Simple KVM setup](https://github.com/soulxu/kvmsample)
- [KVM tool](https://github.com/kvmtool/kvmtool)
- [Linker script explaination](https://jsandler18.github.io/explanations/linker_ld.html)
