# Mini KVM

## Build and Run

Just run in the root folder of the project to build the project
```sh
make 
```

For running the VM there is a make rule : 
```sh
make run
```

Or run by hand :
```sh
./build/mini_kvm
```

## Misc 

To filter logs output :
```sh
LOGGER_LEVEL=WARN ./build/mini_kvm
```

# References :

- [KVM API Reference](https://www.kernel.org/doc/html/latest/virt/kvm/api.html)
- [Simple KVM setup](https://github.com/soulxu/kvmsample)
- [Linker script explaination](https://jsandler18.github.io/explanations/linker_ld.html)
