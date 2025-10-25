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
--name/-n:  set the name of the virtual machine
--log/-l:   enable logging, can specify an output file with --log=output.txt
--mem/-m:   memory allocated to the virtual machine in bytes
--vcpu/-v:  number of vcpus dedicated to the virtual machine
--help/-h:  print this message
```

### `mini_kvm pause`

```
--name/-n:  set the name of the virtual machine
```

### `mini_kvm resume`

```
--name/-n:  set the name of the virtual machine
```

### `mini_kvm shutdown`

```
--name/-n:  set the name of the virtual machine
```

### `mini_kvm status`

With no arguments other than the name, this sub command will print the current state of the VM.

```
--name/-n:  set the name of the virtual machine
--regs/-r:  request register state
--vcpus/-v: specify a target VCPU list
--mem/-m:   dump memory format is start_addr,[,end_addr][,word_size][,bytes_per_line]
```

# References :

- [KVM API Reference](https://www.kernel.org/doc/html/latest/virt/kvm/api.html)
- [Simple KVM setup](https://github.com/soulxu/kvmsample)
- [KVM tool](https://github.com/kvmtool/kvmtool)
- [Linker script explaination](https://jsandler18.github.io/explanations/linker_ld.html)
