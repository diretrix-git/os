#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "process.h"
#include "scheduler.h"
#include "shell.h"

void kernel_main(void);

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline int serial_is_transmit_empty(void)
{
    return inb(0x3F8 + 5) & 0x20;
}

static void serial_putchar(char c)
{
    while (!serial_is_transmit_empty())
        ;
    outb(0x3F8, (uint8_t)c);
}

static void serial_print(const char *str)
{
    while (*str)
    {
        char c = *str++;
        if (c == '\n')
            serial_putchar('\r');
        serial_putchar(c);
    }
}

static volatile int thread1_ticks = 0;
static volatile int thread2_ticks = 0;

void thread1_entry(void)
{
    while (1)
    {
        thread1_ticks++;
        if (thread1_ticks % 100 == 0)
        {
            vga_print("[Thread1] Running... ticks=");
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
            vga_print("[Thread2] Running... ticks=");
            vga_print_int(thread2_ticks);
            vga_print("\n");
        }
        __asm__ volatile("hlt");
    }
}

void kernel_entry(void) __attribute__((section(".text.startup")));

void kernel_entry(void)
{
    kernel_main();
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

void kernel_main(void)
{
    vga_init();
    serial_print("Kernel entry OK\n");
    vga_print("Kernel entry OK\n");
    serial_print("Booting MyOS...\n");
    vga_print("Booting MyOS...\n");

    serial_print("GDT init...\n");
    /* Skip kernel GDT reload for now; stage2 already set a flat protected-mode GDT. */
    /* gdt_init(); */
    serial_print("GDT loaded\n");
    vga_print("GDT loaded\n");

    serial_print("IDT init...\n");
    idt_init();
    serial_print("IDT loaded\n");
    vga_print("IDT loaded\n");

    serial_print("PIC init...\n");
    pic_init();
    serial_print("PIC initialized\n");
    vga_print("PIC initialized\n");

    serial_print("Timer init...\n");
    timer_init(100);
    serial_print("Timer started (100Hz)\n");
    vga_print("Timer started (100Hz)\n");

    serial_print("Keyboard init...\n");
    keyboard_init();
    serial_print("Keyboard ready\n");
    vga_print("Keyboard ready\n");

    serial_print("PMM init...\n");
    pmm_init(32 * 1024 * 1024);
    serial_print("PMM ready\n");
    vga_print("PMM ready: ");
    vga_print_int(pmm_free_frames());
    vga_print(" frames free\n");

    paging_init();
    vga_print("Paging enabled\n");

    scheduler_init();
    vga_print("Scheduler ready\n");

    __asm__ volatile("sti");
    vga_print("Interrupts enabled\n");

    vga_print("Creating demo threads...\n");
    struct process *t1 = process_create("Thread1", thread1_entry);
    struct process *t2 = process_create("Thread2", thread2_entry);

    if (t1)
        scheduler_add(t1);
    if (t2)
        scheduler_add(t2);

    vga_print("\n");

    shell_init();
    shell_run();

    while (1)
    {
        __asm__ volatile("hlt");
    }
}
