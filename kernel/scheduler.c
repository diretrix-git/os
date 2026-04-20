#include "scheduler.h"
#include "process.h"
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

static inline int serial_transmit_empty(void)
{
    return inb(0x3F8 + 5) & 0x20;
}

static void serial_putchar(char c)
{
    while (!serial_transmit_empty())
        ;
    outb(0x3F8, (uint8_t)c);
}

static void serial_print(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            serial_putchar('\r');
        serial_putchar(*s++);
    }
}

/* ------------------------------------------------------------------ */

#define MAX_PROCS 16

static struct process *queue[MAX_PROCS];
static int count = 0;
static int cur_idx = 0;
static struct process *current_process = 0;

/* ------------------------------------------------------------------ */

void scheduler_init(void)
{
    serial_print("[SCHED] Init\n");
    count = 0;
    cur_idx = 0;
    current_process = 0;
    for (int i = 0; i < MAX_PROCS; i++)
        queue[i] = 0;
    serial_print("[SCHED] Done\n");
}

void scheduler_add(struct process *proc)
{
    if (count >= MAX_PROCS)
        return;
    queue[count++] = proc;
    serial_print("[SCHED] Added: ");
    serial_print(proc->name);
    serial_print("\n");
}
void scheduler_remove(struct process *proc)
{
    for (int i = 0; i < count; i++)
    {
        if (queue[i] == proc)
        {
            queue[i] = queue[--count];
            queue[count] = 0;
            return;
        }
    }
}

/* Find next READY process, round-robin */
static struct process *next_ready(void)
{
    if (count == 0)
        return 0;
    int start = (cur_idx < 0) ? 0 : cur_idx;
    for (int i = 0; i < count; i++)
    {
        int idx = (start + 1 + i) % count;
        struct process *p = queue[idx];
        if (p && p->state == PROCESS_READY)
        {
            cur_idx = idx;
            return p;
        }
    }
    /* Nothing else ready — resume current if still running/ready */
    if (current_process &&
        (current_process->state == PROCESS_RUNNING ||
         current_process->state == PROCESS_READY))
        return current_process;
    return 0;
}

/* Called from timer IRQ every N ticks */
void scheduler_tick(void)
{
    if (count == 0)
        return;

    struct process *next = next_ready();
    if (!next || next == current_process)
        return;

    if (current_process && current_process->state == PROCESS_RUNNING)
        current_process->state = PROCESS_READY;

    next->state = PROCESS_RUNNING;
    scheduler_switch(next);
}
/* Cooperative yield — call from a process to give up the CPU */
void scheduler_yield(void)
{
    if (!current_process)
        return;
    if (current_process->state == PROCESS_RUNNING)
        current_process->state = PROCESS_READY;

    struct process *next = next_ready();
    if (!next || next == current_process)
    {
        if (current_process)
            current_process->state = PROCESS_RUNNING;
        return;
    }
    next->state = PROCESS_RUNNING;
    scheduler_switch(next);
}

struct process *scheduler_get_current(void)
{
    return current_process;
}

/* ------------------------------------------------------------------ *
 * scheduler_switch                                                    *
 *                                                                     *
 * IMPORTANT: update current_process BEFORE calling switch_context.   *
 * After switch_context returns we are running in the new process's   *
 * context, so any writes that happen after the call belong to the    *
 * resumed process, not to the one we just switched away from.        *
 * ------------------------------------------------------------------ */
void scheduler_switch(struct process *proc)
{
    if (!proc)
        return;

    /* First-ever switch: no old context to save */
    if (!current_process)
    {
        current_process = proc;
        process_set_current(proc);
        /* Jump directly into the new process via a "fake" switch.
         * We pass a dummy old context — it will never be restored. */
        struct cpu_context dummy = {0};
        switch_context(&dummy, &proc->context);
        return;
    }

    if (current_process == proc)
        return;

    /* Update the pointer BEFORE the switch so both sides see it correctly */
    struct process *old = current_process;
    current_process = proc;
    process_set_current(proc);

    switch_context(&old->context, &proc->context);
    /*
     * Execution resumes here the next time `old` is switched back in.
     * At that point current_process already points to `old` again
     * (set by whichever call path resumed us), so nothing extra needed.
     */
}