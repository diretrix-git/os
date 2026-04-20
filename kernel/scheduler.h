#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

void scheduler_init(void);
void scheduler_add(struct process *proc);
void scheduler_remove(struct process *proc);
void scheduler_tick(void);
void scheduler_yield(void);          /* cooperative yield */
struct process *scheduler_get_current(void);
void scheduler_switch(struct process *proc);

/* Assembly */
void switch_context(struct cpu_context *old_ctx, struct cpu_context *new_ctx);

#endif