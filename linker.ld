/* linker.ld — Sora OS kernel linker script */
/* Places kernel at physical address 0x100000 (1 MB mark) */

ENTRY(kernel_start)

SECTIONS
{
    . = 0x100000;

    /* Multiboot header + kernel_start entry code — must be first */
    .multiboot ALIGN(4) :
    {
        *(.multiboot)
    }

    .text ALIGN(4096) :
    {
        *(.text*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata*)
    }

    .data ALIGN(4096) :
    {
        *(.data*)
    }

    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss*)
    }

    kernel_end = .;
}
