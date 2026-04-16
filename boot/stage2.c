/*
 * Stage 2 Bootloader - C Implementation
 *
 * Responsibilities:
 * 1. Switch CPU from 16-bit real mode to 32-bit protected mode
 * 2. Load the Global Descriptor Table (GDT)
 * 3. Enable the A20 line for full 32-bit addressing
 * 4. Load the kernel from disk
 * 5. Jump to the kernel entry point
 */

#include <stdint.h>

/* =============================================================================
 * VGA Text Mode - For debugging during boot
 * ============================================================================= */
#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define COM1_PORT 0x3F8

#define KERNEL_LBA 11
#define KERNEL_SECTORS 2089 /* Actual kernel size in sectors for current build */

#define VGA_BUFFER ((volatile uint16_t *)VGA_ADDR)
#define VGA_ROW (*(volatile int *)0x90000)
#define VGA_COL (*(volatile int *)0x90004)

static inline void outb(uint16_t port, uint8_t value);
static inline uint8_t inb(uint16_t port);
static void vga_print(const char *str);
static void vga_print_hex(uint32_t value);

static inline void serial_init(void)
{
    outb(COM1_PORT + 1, 0x00); /* Disable interrupts */
    outb(COM1_PORT + 3, 0x80); /* Enable DLAB */
    outb(COM1_PORT + 0, 0x03); /* Baud rate divisor low (38400) */
    outb(COM1_PORT + 1, 0x00); /* Baud rate divisor high */
    outb(COM1_PORT + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7); /* FIFO enabled, clear, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static inline int serial_is_transmit_empty(void)
{
    return inb(COM1_PORT + 5) & 0x20;
}

static void serial_putchar(char c)
{
    while (!serial_is_transmit_empty())
        ;
    outb(COM1_PORT, (uint8_t)c);
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

static void debug_print(const char *str)
{
    vga_print(str);
    serial_print(str);
}

static void serial_print_hex(uint32_t value)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--)
        serial_putchar(hex_chars[(value >> (i * 4)) & 0xF]);
}

static void debug_print_hex(uint32_t value)
{
    vga_print_hex(value);
    serial_print_hex(value);
}

static void vga_putchar(char c)
{
    if (c == '\n')
    {
        VGA_COL = 0;
        VGA_ROW++;
    }
    else if (c == '\r')
    {
        VGA_COL = 0;
    }
    else
    {
        VGA_BUFFER[VGA_ROW * VGA_WIDTH + VGA_COL] = 0x0700 | c;
        VGA_COL++;
    }

    if (VGA_ROW >= VGA_HEIGHT)
    {
        /* Simple scroll - clear screen */
        for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        {
            VGA_BUFFER[i] = 0x0700;
        }
        VGA_ROW = 0;
        VGA_COL = 0;
    }
}

static void vga_print(const char *str)
{
    while (*str)
    {
        vga_putchar(*str++);
    }
}

static void vga_print_hex(uint32_t value)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--)
    {
        vga_putchar(hex_chars[(value >> (i * 4)) & 0xF]);
    }
}

/* =============================================================================
 * Port I/O Helpers
 * ============================================================================= */
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

static inline uint16_t inw(uint16_t port)
{
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static void wait_ide_ready(void)
{
    while (inb(0x1F7) & 0x80)
        ; /* Wait while controller is busy */
}

/* =============================================================================
 * GDT (Global Descriptor Table)
 * ============================================================================= */
#define GDT_ENTRIES 3

struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdtp __attribute__((unused));

/* GDT Access byte flags */
#define GDT_ACCESS_PRESENT 0x80
#define GDT_ACCESS_RING0 0x00
#define GDT_ACCESS_RING3 0x60
#define GDT_ACCESS_SEGMENT 0x10
#define GDT_ACCESS_CODE 0x0A
#define GDT_ACCESS_DATA 0x02

/* GDT Granularity flags */
#define GDT_GRAN_4KB 0x80
#define GDT_GRAN_32BIT 0x40

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) __attribute__((unused));
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

/* =============================================================================
 * Simple disk read in protected mode (using port I/O to IDE controller)
 * This is a simplified PIO read for the first few sectors.
 * ============================================================================= */
static void read_sectors(uint32_t lba, uint8_t count, uint32_t buffer)
{
    wait_ide_ready();

    /* Select drive 0, set LBA mode */
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));

    /* Set sector count */
    outb(0x1F2, count);

    /* Set LBA address (low 24 bits) */
    outb(0x1F3, (lba >> 0) & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);

    /* Send READ SECTORS command */
    outb(0x1F7, 0x20);

    /* Read sectors */
    wait_ide_ready();
    uint16_t *buf = (uint16_t *)buffer;
    for (int s = 0; s < count; s++)
    {
        wait_ide_ready();
        for (int i = 0; i < 256; i++)
        {
            *buf++ = inw(0x1F0);
        }
    }
}

static void read_sectors_lba(uint32_t lba, uint32_t count, uint32_t buffer)
{
    while (count > 0)
    {
        uint8_t chunk = count > 255 ? 255 : (uint8_t)count;
        read_sectors(lba, chunk, buffer);
        lba += chunk;
        buffer += chunk * 512;
        count -= chunk;
    }
}

void stage2_main(void) __attribute__((section(".text.startup")));

void stage2_main(void)
{
    serial_init();
    VGA_ROW = 0;
    VGA_COL = 0;
    debug_print("Stage 2: Protected mode initialized\n");

    debug_print("Stage 2: Loading kernel...\n");
    read_sectors_lba(KERNEL_LBA, KERNEL_SECTORS, 0x100000);
    debug_print("Stage 2: Kernel loaded to 0x100000\n");

    uint32_t *kernel_start = (uint32_t *)0x100000;
    debug_print("Stage 2: First word: ");
    debug_print_hex(*kernel_start);
    debug_print("\n");

    debug_print("Stage 2: Jumping to kernel at 0x100000...\n");

    typedef void (*kernel_entry_t)(void);
    kernel_entry_t kernel_entry = (kernel_entry_t)0x100000;
    kernel_entry();

    debug_print("Stage 2: ERROR - Kernel returned!\n");
    while (1)
    {
        __asm__ volatile("hlt");
    }
}
