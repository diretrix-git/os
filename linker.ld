/* Linker script for MyOS kernel */

ENTRY(_kernel_entry)

SECTIONS
{
    /* Kernel loads at 1MB */
    . = 0x100000;
    
    /* Code section */
    .text : ALIGN(4K)
    {
        *(.text.startup)
        *(.text*)
    }
    
    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }
    
    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data*)
    }
    
    /* Uninitialized data (BSS) */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }
    
    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
