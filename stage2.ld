ENTRY(_stage2_main)

SECTIONS
{
    . = 0x8000;

    .text : {
        *(.text.startup)
        *(.text*)
    }

    .rdata : {
        *(.rdata*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }
}
