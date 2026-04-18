#include "pmm.h"

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

#define PMM_TOTAL_MEMORY (32 * 1024 * 1024) /* 32 MB */
#define PMM_FRAME_SIZE 4096
#define PMM_TOTAL_FRAMES (PMM_TOTAL_MEMORY / PMM_FRAME_SIZE)
#define PMM_BITMAP_SIZE ((PMM_TOTAL_FRAMES + 7) / 8)

static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];
static uint32_t pmm_total_frames = 0;
static uint32_t pmm_frames_used = 0;

static void pmm_set_frame(uint32_t frame)
{
    pmm_bitmap[frame / 8] |= (1 << (frame % 8));
}

static void pmm_unset_frame(uint32_t frame)
{
    pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static int pmm_test_frame(uint32_t frame) __attribute__((unused));
static int pmm_test_frame(uint32_t frame)
{
    return pmm_bitmap[frame / 8] & (1 << (frame % 8));
}

static int pmm_find_free_frame(void)
{
    for (uint32_t i = 0; i < pmm_total_frames / 8; i++)
    {
        if (pmm_bitmap[i] != 0xFF)
        {
            for (int j = 0; j < 8; j++)
            {
                if (!(pmm_bitmap[i] & (1 << j)))
                {
                    return i * 8 + j;
                }
            }
        }
    }
    return -1;
}

void pmm_init(uint32_t total_memory)
{
    serial_print("[PMM] Start\n");
    serial_print("[PMM] Calculating frames\n");
    
    pmm_total_frames = total_memory / PMM_FRAME_SIZE;
    pmm_frames_used = 0;
    
    serial_print("[PMM] Clearing bitmap\n");
    
    volatile uint8_t *bitmap_ptr = (volatile uint8_t *)pmm_bitmap;
    
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++)
    {
        bitmap_ptr[i] = 0;
    }
    
    serial_print("[PMM] Marking first 1MB\n");
    
    uint32_t frames_to_mark = 0x100000 / PMM_FRAME_SIZE;
    
    for (uint32_t i = 0; i < frames_to_mark; i++)
    {
        pmm_set_frame(i);
        pmm_frames_used++;
    }
    
    serial_print("[PMM] Complete\n");
}

uint32_t pmm_alloc_frame(void)
{
    int frame = pmm_find_free_frame();

    if (frame == -1)
    {
        return 0; /* Out of memory */
    }

    pmm_set_frame(frame);
    pmm_frames_used++;

    return frame * PMM_FRAME_SIZE;
}

void pmm_free_frame(uint32_t addr)
{
    uint32_t frame = addr / PMM_FRAME_SIZE;
    pmm_unset_frame(frame);
    pmm_frames_used--;
}

uint32_t pmm_used_frames(void)
{
    return pmm_frames_used;
}

uint32_t pmm_free_frames(void)
{
    return pmm_total_frames - pmm_frames_used;
}
