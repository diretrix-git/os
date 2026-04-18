#include "scheduler.h"
#include "vga.h"

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

#define MAX_PROCESSES 16

static struct process *process_queue[MAX_PROCESSES];
static int process_count = 0;
static int current_index = 0;
static struct process *current_process = 0;

void scheduler_init(void)
{
    serial_print("[SCHED] Init\n");
    process_count = 0;
    current_index = 0;
    current_process = 0;
    process_queue[0] = 0;
    process_queue[1] = 0;
    process_queue[2] = 0;
    process_queue[3] = 0;
    process_queue[4] = 0;
    process_queue[5] = 0;
    process_queue[6] = 0;
    process_queue[7] = 0;
    process_queue[8] = 0;
    process_queue[9] = 0;
    process_queue[10] = 0;
    process_queue[11] = 0;
    process_queue[12] = 0;
    process_queue[13] = 0;
    process_queue[14] = 0;
    process_queue[15] = 0;
    serial_print("[SCHED] Done\n");
}

void scheduler_add(struct process *proc)
{
    if (process_count >= MAX_PROCESSES)
    {
        return;
    }

    process_queue[process_count++] = proc;
    serial_print("[SCHED] Added process ");
}

void scheduler_tick(void)
{
    if (process_count == 0)
    {
        return;
    }

    current_index = (current_index + 1) % process_count;
    struct process *next_proc = process_queue[current_index];

    if (next_proc == current_process || next_proc->state != PROCESS_READY)
    {
        return;
    }

    scheduler_switch(next_proc);
}

struct process *scheduler_get_current(void)
{
    return current_process;
}

void scheduler_switch(struct process *proc)
{
    if (!current_process)
    {
        current_process = proc;
        process_set_current(proc);
        return;
    }

    if (current_process == proc)
    {
        return;
    }

    switch_context(&current_process->context, &proc->context);

    current_process = proc;
    process_set_current(proc);
}
