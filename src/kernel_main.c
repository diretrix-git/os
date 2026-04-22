#include "multiboot.h"
#include "types.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "vga.h"
#include "serial.h"
#include "keyboard.h"
#include "scheduler.h"
#include "shell.h"
#include "kernel.h"

/* ── Simple delay using a busy loop ────────────────────────────────────── */
static void delay(uint32_t count) {
    volatile uint32_t i;
    for (i = 0; i < count; i++)
        __asm__ volatile("nop");
}

/* ── Typing animation: print string char by char with delay ────────────── */
static void type_print(const char* s, uint8_t color, uint32_t speed) {
    while (*s) {
        vga_print_color((char[]){*s, '\0'}, color);
        delay(speed);
        s++;
    }
}

/* ── Boot screen ────────────────────────────────────────────────────────── */
static void boot_screen(void) {
    /* Full black screen first */
    vga_set_color(0, 0);
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        ((volatile uint16_t*)0xB8000)[i] = 0x0000;

    delay(3000000);

    /* Draw centered ASCII art logo — "Vamos OS" */
    vga_set_cursor(3, 17);
    type_print(" __   __                              ___  ____  ", 0x0B, 200000);
    vga_set_cursor(4, 17);
    type_print(" \\ \\ / /__ _ _ __ ___   ___  ___    / _ \\/ ___| ", 0x0B, 200000);
    vga_set_cursor(5, 17);
    type_print("  \\ V / _` | '_ ` _ \\ / _ \\/ __|  | | | \\___ \\ ", 0x0B, 200000);
    vga_set_cursor(6, 17);
    type_print("   | | (_| | | | | | | (_) \\__ \\  | |_| |___) |", 0x0B, 200000);
    vga_set_cursor(7, 17);
    type_print("   |_|\\__,_|_| |_| |_|\\___/|___/   \\___/|____/ ", 0x0B, 200000);

    vga_set_cursor(9, 28);
    type_print("A Simple x86 Kernel", 0x07, 150000);
    vga_set_cursor(10, 30);
    type_print("Built in C & Assembly", 0x08, 150000);

    delay(5000000);

    /* Loading bar */
    vga_set_cursor(13, 20);
    vga_print_color("Loading", 0x0F);

    const char* steps[] = {
        " GDT", " IDT", " PIC", " PIT", " PMM", " KBD", " SCH"
    };
    for (int i = 0; i < 7; i++) {
        type_print(steps[i], 0x0A, 300000);
        vga_print_color(".", 0x0E);
        delay(4000000);
    }

    vga_set_cursor(14, 20);
    type_print("[ DONE ] Vamos OS ready.", 0x0A, 200000);

    delay(8000000);
}

void kernel_main(multiboot_info_t* mb) {

    /* 1. GDT must be first — reloads segment registers */
    gdt_init();

    /* 2. VGA init */
    vga_init();
    serial_init();
    serial_print("Vamos OS booting...\n");

    /* 3. Boot screen with animation */
    boot_screen();

    /* 4. Now init remaining hardware */
    idt_init();
    serial_print("IDT ok\n");

    pic_init();
    serial_print("PIC ok\n");

    pit_init(100);
    serial_print("PIT ok\n");

    pmm_init(mb);
    serial_print("PMM ok\n");

    keyboard_init();
    serial_print("Keyboard ok\n");

    scheduler_init();
    serial_print("Scheduler ok\n");

    __asm__ volatile("sti");
    serial_print("Interrupts enabled\n");

    /* 5. Clear screen and draw status bar, then launch shell */
    vga_set_color(7, 0);
    vga_clear();
    vga_draw_statusbar();

    shell_run();

    kernel_panic("kernel_main returned");
}
