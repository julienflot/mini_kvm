ENTRY(_start)
OUTPUT_FORMAT(binary)
OUTPUT(bios.img)

SECTIONS
{
    . = 0x1000;
    __bios_start = .;

    .text : ALIGN(8) {
        __text_start = .;
        KEEP(*(.text._start));
        *(.text);
        __text_end = .;
    }

    .data : ALIGN(8) {
        __data_start = .;
        *(.data);
        __data_end = .;
    }
    
    . = ALIGN(4096);
    __bios_end = .;
}
