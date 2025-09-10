ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    __guest_start = .;
    __text_start = .;
    .text : {
        KEEP(*(.text.boot))
        *(.text)
    }
    . = ALIGN(4096);
    __guest_end = .;
}
