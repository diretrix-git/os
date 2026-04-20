#include "process.h"
#include "pmm.h"
#include "scheduler.h"

static struct process process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static struct process *current_process = 0;

// static inline void outb(uint16_t port, uint8_t value)
// {
//     __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
// }

// #define SERIAL_PORT 0x3F8

struct process *process_create(const char *name, void (*entry)(void))
{
    /* Find a free slot */
    struct process *proc = 0;
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_table[i].pid == 0 ||
            process_table[i].state == PROCESS_TERMINATED)
        {
            proc = &process_table[i];
            break;
        }
    }
    if (!proc)
        return 0;

    /* Allocate a 4 KB stack frame */
    uint32_t stack = pmm_alloc_frame();
    if (!stack)
        return 0;

    /* Basic fields */
    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->stack_bottom = stack;
    proc->stack_top = stack + PROCESS_STACK_SIZE;

    /* Copy name */
    int i;
    for (i = 0; i < 31 && name[i]; i++)
        proc->name[i] = name[i];
    proc->name[i] = 0;

    /* ----------------------------------------------------------------
     * Build the initial stack frame that switch_context will restore.
     *
     * switch_context (new version) does:
     *
     *   mov esp, [new_ctx->esp]   ; switch to new stack
     *   push [new_ctx->eip]       ; push entry point
     *   ret                       ; jump to it
     *
     * So we only need ctx->esp and ctx->eip set correctly.
     * We still set up a fake callee-save frame on the stack so that
     * if the process ever calls switch_context itself the pops work.
     * ---------------------------------------------------------------- */
    uint32_t *sp = (uint32_t *)proc->stack_top;

    /* Fake callee-save frame (ebp, ebx, esi, edi) — all zero */
    *(--sp) = 0; /* ebp */
    *(--sp) = 0; /* ebx */
    *(--sp) = 0; /* esi */
    *(--sp) = 0; /* edi */

    /* Context fields used by switch_context */
    proc->context.esp = (uint32_t)sp;
    proc->context.eip = (uint32_t)entry;

    /* Remaining context fields (informational, not used by switch_context) */
    proc->context.eax = 0;
    proc->context.ebx = 0;
    proc->context.ecx = 0;
    proc->context.edx = 0;
    proc->context.esi = 0;
    proc->context.edi = 0;
    proc->context.ebp = 0;
    proc->context.eflags = 0x202; /* IF=1, reserved bit set */
    proc->context.cs = 0x08;
    proc->context.ds = 0x10;

    return proc;
}

void process_exit(void)
{
    if (current_process)
    {
        current_process->state = PROCESS_TERMINATED;
        pmm_free_frame(current_process->stack_bottom);
        current_process->pid = 0;
    }
    /* Don't halt — yield back so other processes can run */
    scheduler_yield();
    /* If we somehow return, halt */
    while (1)
        __asm__ volatile("hlt");
}

struct process *process_get_current(void)
{
    return current_process;
}

void process_set_current(struct process *proc)
{
    current_process = proc;
}