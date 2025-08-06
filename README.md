# Mini KVM

## Build and Run

This project use CMake as it main build system so to build the project run the following commands :

```sh
mkdir build && cd build 
cmake ..
make
```

To build a release version :
```sh
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Invocations 

### `mini_kvm run`

```
--name name: set the name of the VM.
--vcpus number: number of vcpu to allocate.
--mem size: ram quantity allocated.
--kernel path: path to a kernel image.
--log [path]: log internal inforamtion.
```

### `mini_kvm pause`

```
name: name of the VM.
```

### `mini_kvm resume`

```
--name: name of the VM.
```

### `mini_kvm shutdown`

```
--name: name of the VM.
```

### `mini_kvm status`

With no arguments other than the name, this sub command will print the current state of the VM.

```
--name: name of the VM.
--regs: show registers.
--vcpus: select which vcpus the command will target.
```

# References :

- [KVM API Reference](https://www.kernel.org/doc/html/latest/virt/kvm/api.html)
- [Simple KVM setup](https://github.com/soulxu/kvmsample)
- [KVM tool](https://github.com/kvmtool/kvmtool)
- [Linker script explaination](https://jsandler18.github.io/explanations/linker_ld.html)
