#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "ps2.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "process.h"
#include "scheduler.h"
#include "shell.h"

void kernel_main(void);

/* BSS boundaries provided by linker.ld */
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

/* ------------------------------------------------------------------ */
/* Serial helpers (used before VGA is up)                             */
/* ------------------------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline int serial_tx_empty(void)  { return inb(0x3F8 + 5) & 0x20; }
static void serial_putchar(char c)
{
    while (!serial_tx_empty()) ;
    outb(0x3F8, (uint8_t)c);
}
static void serial_print(const char *s)
{
    while (*s) { if (*s == '\n') serial_putchar('\r'); serial_putchar(*s++); }
}

/* ------------------------------------------------------------------ */
/* Demo threads                                                        */
/* ------------------------------------------------------------------ */
static volatile int thread1_ticks = 0;
static volatile int thread2_ticks = 0;

void thread1_entry(void)
{
    while (1)
    {
        thread1_ticks++;
        if (thread1_ticks % 100 == 0)
        {
            vga_print("[Thread1] ticks=");
            vga_print_int(thread1_ticks);
            vga_print("\n");
        }
        __asm__ volatile("hlt");
    }
}

void thread2_entry(void)
{
    while (1)
    {
        thread2_ticks++;
        if (thread2_ticks % 150 == 0)
        {
            vga_print("[Thread2] ticks=");
            vga_print_int(thread2_ticks);
            vga_print("\n");
        }
        __asm__ volatile("hlt");
    }
}

/* ------------------------------------------------------------------ */
/* Kernel entry point — called by stage2, runs before kernel_main     */
/* ------------------------------------------------------------------ */
void kernel_entry(void) __attribute__((section(".text.startup"), used));

void kernel_entry(void)
{
    /* Zero the BSS segment BEFORE any C code uses static/global variables */
    for (uint8_t *p = _bss_start; p < _bss_end; p++)
        *p = 0;

    kernel_main();

    /* Should never return */
    while (1)
        __asm__ volatile("hlt");
}

/* ------------------------------------------------------------------ */
/* Main kernel initialisation                                          */
/* ------------------------------------------------------------------ */
void kernel_main(void)
{
    vga_init();
    serial_print("Kernel entry OK\n");
    vga_print("Kernel entry OK\n");
    vga_print("Booting MyOS...\n");

    /* GDT — stage2 already set a flat GDT; skip reload for now */
    vga_print("GDT loaded\n");

    serial_print("IDT init...\n");
    idt_init();
    vga_print("IDT loaded\n");

    serial_print("PIC init...\n");
    pic_init();
    vga_print("PIC initialized\n");

    serial_print("PS/2 init...\n");
    ps2_controller_init();
    vga_print("PS/2 Controller ready\n");

    serial_print("Timer init...\n");
    timer_init(100);
    vga_print("Timer started (100Hz)\n");

    serial_print("Keyboard init...\n");
    keyboard_init();
    vga_print("Keyboard ready\n");

    serial_print("PMM init...\n");
    pmm_init(32 * 1024 * 1024);
    vga_print("PMM ready: ");
    vga_print_int(pmm_free_frames());
    vga_print(" frames free\n");

    serial_print("Paging init...\n");
    paging_init();
    vga_print("Paging enabled\n");

    serial_print("Scheduler init...\n");
    scheduler_init();
    vga_print("Scheduler ready\n");

    serial_print("Enabling interrupts...\n");
    __asm__ volatile("sti");
    vga_print("Interrupts enabled\n");

    /* Create demo threads */
    vga_print("Creating demo threads...\n");
    struct process *t1 = process_create("Thread1", thread1_entry);
    struct process *t2 = process_create("Thread2", thread2_entry);
    if (t1) { scheduler_add(t1); vga_print("Thread1 added\n"); }
    if (t2) { scheduler_add(t2); vga_print("Thread2 added\n"); }

    vga_print("\n");

    /* Start the interactive shell */
    shell_init();
    shell_run();

    while (1)
        __asm__ volatile("hlt");
}