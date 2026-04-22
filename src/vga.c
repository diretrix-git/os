#include "vga.h"
#include "types.h"
#include "pit.h"

/* VGA text buffer at 0xB8000 — 80x25 cells, 2 bytes each */
static volatile uint16_t* const VGA_BUFFER = (volatile uint16_t*)0xB8000;

/* Cursor state */
static uint32_t vga_row   = 0;
static uint32_t vga_col   = 0;
static uint8_t  vga_color = 0x07;  /* light grey on black */

/* I/O port helpers for hardware cursor */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port)); return r;
}

static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* ── Hardware cursor (blinking underscore) ──────────────────────────────── */

void vga_enable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 13); /* cursor start scanline */
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15); /* cursor end scanline */
}

void vga_disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

static void vga_update_hw_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_cursor(uint32_t row, uint32_t col) {
    vga_row = row;
    vga_col = col;
    vga_update_hw_cursor();
}

void vga_get_cursor(uint32_t* row, uint32_t* col) {
    *row = vga_row;
    *col = vga_col;
}

/* ── Color ──────────────────────────────────────────────────────────────── */

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

uint8_t vga_get_color(void) { return vga_color; }

/* ── Direct cell write (bypasses cursor, used for status bar) ───────────── */

void vga_write_at(uint32_t row, uint32_t col, char c, uint8_t color) {
    VGA_BUFFER[row * VGA_WIDTH + col] = vga_entry((unsigned char)c, color);
}

void vga_print_at(uint32_t row, uint32_t col, const char* s, uint8_t color) {
    while (*s && col < VGA_WIDTH) {
        vga_write_at(row, col++, *s++, color);
    }
}

/* ── Status bar ─────────────────────────────────────────────────────────── */
/* Drawn on row 0 — shell content starts from row 1 */

#define STATUS_COLOR 0x17   /* white on blue */

void vga_draw_statusbar(void) {
    /* Fill entire row 0 with blue background */
    for (uint32_t c = 0; c < VGA_WIDTH; c++)
        vga_write_at(0, c, ' ', STATUS_COLOR);

    /* Left: OS name */
    vga_print_at(0, 1, "Vamos OS v1.0", STATUS_COLOR);

    /* Right: tick counter as uptime indicator */
    uint32_t ticks = get_tick_count();
    uint32_t secs  = ticks / 100;
    char buf[16];
    /* format "UP: XXXs" */
    buf[0] = 'U'; buf[1] = 'P'; buf[2] = ':'; buf[3] = ' ';
    int i = 4;
    if (secs == 0) {
        buf[i++] = '0';
    } else {
        char tmp[8]; int j = 0;
        uint32_t n = secs;
        while (n) { tmp[j++] = (char)('0' + n % 10); n /= 10; }
        while (j > 0) buf[i++] = tmp[--j];
    }
    buf[i++] = 's'; buf[i] = '\0';
    /* right-align at col 70 */
    vga_print_at(0, 70, buf, STATUS_COLOR);
}

/* ── Clear ──────────────────────────────────────────────────────────────── */

void vga_clear(void) {
    uint16_t blank = vga_entry(' ', vga_color);
    /* Clear rows 1-24 only (row 0 = status bar) */
    for (uint32_t i = VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = blank;
    vga_row = 1;
    vga_col = 0;
    vga_update_hw_cursor();
}

void vga_init(void) {
    vga_color = 0x07;
    vga_row   = 0;
    vga_col   = 0;
    /* Clear entire screen including row 0 */
    uint16_t blank = vga_entry(' ', vga_color);
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = blank;
    vga_enable_cursor();
    vga_update_hw_cursor();
}

/* ── Scroll (rows 1-24 only, preserves status bar) ─────────────────────── */

static void vga_scroll(void) {
    for (uint32_t row = 1; row < VGA_HEIGHT - 1; row++)
        for (uint32_t col = 0; col < VGA_WIDTH; col++)
            VGA_BUFFER[row * VGA_WIDTH + col] =
                VGA_BUFFER[(row + 1) * VGA_WIDTH + col];

    uint16_t blank = vga_entry(' ', vga_color);
    for (uint32_t col = 0; col < VGA_WIDTH; col++)
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;

    vga_row = VGA_HEIGHT - 1;
    vga_col = 0;
}

/* ── putchar ────────────────────────────────────────────────────────────── */

void vga_putchar(char c) {
    /* Never write on row 0 (status bar) */
    if (vga_row == 0) vga_row = 1;

    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
        } else if (vga_row > 1) {
            vga_row--;
            vga_col = VGA_WIDTH - 1;
        }
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
    } else {
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char)c, vga_color);
        vga_col++;
        if (vga_col >= VGA_WIDTH) { vga_col = 0; vga_row++; }
    }

    if (vga_row >= VGA_HEIGHT) vga_scroll();
    vga_update_hw_cursor();
}

void vga_print(const char* s) {
    while (*s) vga_putchar(*s++);
}

void vga_print_color(const char* s, uint8_t color) {
    uint8_t saved = vga_color;
    vga_color = color;
    while (*s) vga_putchar(*s++);
    vga_color = saved;
}
