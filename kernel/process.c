#include "process.h"
#include "pmm.h"

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

#define SERIAL_PORT 0x3F8

static struct process process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static struct process *current_process = 0;

struct process *process_create(const char *name, void (*entry)())
{
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '1'); outb(SERIAL_PORT, '\n');
    /* Find free slot */
    struct process *proc = 0;
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_table[i].pid == 0 || process_table[i].state == PROCESS_TERMINATED)
        {
            proc = &process_table[i];
            break;
        }
    }
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '2'); outb(SERIAL_PORT, '\n');

    if (!proc)
    {
        return 0; /* No free slots */
    }
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '3'); outb(SERIAL_PORT, '\n');

    /* Allocate stack */
    uint32_t stack = pmm_alloc_frame();
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '4'); outb(SERIAL_PORT, '\n');
    if (!stack)
    {
        return 0;
    }
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '5'); outb(SERIAL_PORT, '\n');

    /* Initialize process */
    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->stack_bottom = stack;
    proc->stack_top = stack + PROCESS_STACK_SIZE;

    /* Copy name */
    for (int i = 0; i < 31 && name[i]; i++)
    {
        proc->name[i] = name[i];
    }
    proc->name[31] = 0;
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '6'); outb(SERIAL_PORT, '\n');

    /* Set up initial CPU context */
    /* This context will be restored by scheduler on first switch */
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '1'); outb(SERIAL_PORT, '\n');
    proc->context.eax = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '2'); outb(SERIAL_PORT, '\n');
    proc->context.ebx = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '3'); outb(SERIAL_PORT, '\n');
    proc->context.ecx = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '4'); outb(SERIAL_PORT, '\n');
    proc->context.edx = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '5'); outb(SERIAL_PORT, '\n');
    proc->context.esi = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '6'); outb(SERIAL_PORT, '\n');
    proc->context.edi = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '7'); outb(SERIAL_PORT, '\n');
    proc->context.ebp = 0;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '8'); outb(SERIAL_PORT, '\n');
    proc->context.esp = proc->stack_top;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, '9'); outb(SERIAL_PORT, '\n');
    proc->context.eip = (uint32_t)entry;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, 'A'); outb(SERIAL_PORT, '\n');
    proc->context.eflags = 0x202; /* Interrupts enabled */
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, 'B'); outb(SERIAL_PORT, '\n');
    proc->context.cs = 0x08;
    outb(SERIAL_PORT, 'X'); outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '\n');
    proc->context.ds = 0x10;
    outb(SERIAL_PORT, 'C'); outb(SERIAL_PORT, '7'); outb(SERIAL_PORT, '\n');

    /* Set up initial stack frame for the new process */
    /* The stack will have a fake context that switch_context will "restore" */
    uint32_t *stack_ptr = (uint32_t*)proc->stack_top;
    
    /* Push registers in reverse order of how switch_context will pop them */
    /* switch_context pops: ebp, edi, esi, ebx, then ret to eip */
    stack_ptr -= 1; *stack_ptr = 0;           /* ebp (fake) */
    stack_ptr -= 1; *stack_ptr = 0;           /* edi (fake) */
    stack_ptr -= 1; *stack_ptr = 0;           /* esi (fake) */
    stack_ptr -= 1; *stack_ptr = 0;           /* ebx (fake) */
    
    proc->context.esp = (uint32_t)stack_ptr;
    outb(SERIAL_PORT, 'S'); outb(SERIAL_PORT, 'T'); outb(SERIAL_PORT, 'K'); outb(SERIAL_PORT, '\n');

    return proc;
}

void process_exit(void)
{
    if (current_process)
    {
        current_process->state = PROCESS_TERMINATED;
        current_process->pid = 0;
        /* Free stack */
        pmm_free_frame(current_process->stack_bottom);
    }

    /* Halt forever */
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

struct process *process_get_current(void)
{
    return current_process;
}

void process_set_current(struct process *proc)
{
    current_process = proc;
}
